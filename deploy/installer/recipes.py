"""Native K1 build recipes, with a private GLES SDK for Qt applications."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from common import InstallError,atomic_write,copy_file,json_write,sha,split_dependencies,unpack

BASE_PACKAGES='''build-essential gcc-14 g++-14 cmake ninja-build pkg-config curl ca-certificates
git python3 python3-dev python3-numpy python3-dbus python3-dbus-next python3-gi python3-gi-cairo
python3-pyqt5 python3-uno python3-opencv python3-lxml python3-pil python3-xlsxwriter python3-typing-extensions pybind11-dev
libsndfile1-dev libcurl4-openssl-dev libfftw3-dev libsamplerate0-dev libespeak-ng-dev espeak-ng nlohmann-json3-dev libjsoncpp-dev
espeak-ng-data libasound2-dev libssl-dev libdbus-1-dev libevdev-dev libglib2.0-dev
libsdl2-dev libx11-dev libxtst-dev libxcb1-dev libcmark-dev librsvg2-dev
libfreeimage-dev libfreeimageplus-dev libgif-dev libstb-dev
qtwayland5 libqt5gui5-gles libqt5multimedia5 libqt5multimediawidgets5 libqt5multimedia5-plugins
libqt5sql5-sqlite libqt5svg5 libqt5x11extras5 libqt5qml5 libqt5quickwidgets5 libqt5websockets5
qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qt-labs-settings
qml-module-qt-labs-folderlistmodel qml-module-qtquick-dialogs qml-module-qtquick-window2
qml-module-qtquick2 qml-module-qtgraphicaleffects qml-module-qtquick-templates2
libukui-log4qt1 libkf5windowsystem5 libkf5waylandclient5 libkf5globalaccel-bin
libreoffice-core libreoffice-writer libreoffice-impress poppler-utils fonts-noto-cjk
gir1.2-gtk-3.0 pipewire pipewire-pulse wireplumber pulseaudio-utils wlrctl
wf-recorder ffmpeg v4l-utils xdg-user-dirs acl'''.split()

QT_DEVELOPMENT='''qtbase5-gles-dev qtbase5-dev-tools qt5-qmake qt5-qmake-bin
qttools5-dev-tools qttools5-dev qtdeclarative5-dev qtdeclarative5-dev-tools
qtmultimedia5-dev libqt5websockets5-dev libqt5svg5-dev libqt5x11extras5-dev
qtquickcontrols2-5-dev libkf5windowsystem-dev libkf5globalaccel-dev libkf5wayland-dev
libkf5screen-dev libgsettings-qt-dev libukui-log4qt-dev libpolkit-qt5-1-dev'''.split()


def safe_apt_plan(text):
    if any(line.startswith('Remv ') for line in text.splitlines()):
        raise InstallError('APT计划会移除现有软件包，已停止，请检查Bianbu版本和软件源。')


def development_package(name):
    return (name.endswith(('-dev','-dev-tools')) or name in
            ('qt5-qmake','qt5-qmake-bin','qtchooser','extra-cmake-modules'))


def prefer_package(choices):
    if choices==['libc-dev']:return 'libc6-dev'
    if any(x in choices for x in ('qtbase5-dev','qtbase5-gles-dev')):return 'qtbase5-gles-dev'
    if any(x in choices for x in ('libqt5gui5','libqt5gui5t64','libqt5gui5-gles')):return 'libqt5gui5-gles'
    if any(x in choices for x in ('libqt5quick5','libqt5quick5-gles')):return 'libqt5quick5-gles'
    return choices[0]


def merge_system_sdk(original,target):
    # Keep extracted GLES CMake configs coherent; do not import GLX metadata.
    if original.parent.name=='cmake' and original.name.startswith('Qt5'):return
    for p in original.iterdir():
        q=target/p.name
        if not os.path.lexists(q):q.symlink_to(p)
        elif p.is_dir() and q.is_dir() and not q.is_symlink():merge_system_sdk(p,q)


class Recipes:
    def __init__(self,context):
        self.c=context;self.work=context.work;self.r=context.runner
        self.repo=context.repo;self.lock=context.lock
        self.source=None;self.sdk=None
        self.artifacts=self.work/'artifacts'

    def apt_install(self,names):
        names=sorted(set(map(str,names)))
        if not names:return
        args=['apt-get','--no-install-recommends','--no-remove','install',*names]
        plan=self.r.run(['sudo',*args,'--simulate'],capture=True,label='apt-plan')
        safe_apt_plan(plan)
        self.r.run(['sudo',*args,'-y'],label='apt-install')

    def dependencies(self):
        # Reuse Bianbu's own keyring; do not add the openKylin distribution repo.
        key=Path('/usr/share/keyrings/bianbu-archive-keyring.gpg')
        if not key.is_file():raise InstallError('缺少Bianbu官方软件源密钥，请使用Bianbu 2.3.5系统镜像。')
        existing=list(Path('/etc/apt/sources.list.d').glob('*.sources'))+list(Path('/etc/apt/sources.list.d').glob('*.list'))
        existing.append(Path('/etc/apt/sources.list'))
        present=any(p.is_file() and 'archive.spacemit.com/bianbu' in p.read_text() and
                    'bianbu-v2.3-updates' in p.read_text() for p in existing)
        source=self.work/'okrva-spacemit.sources'
        if not present:
            atomic_write(source,'''Types: deb
URIs: https://archive.spacemit.com/bianbu/
Suites: bianbu-v2.3-updates
Components: main
Signed-By: /usr/share/keyrings/bianbu-archive-keyring.gpg
''')
            self.r.run(['sudo','install','-m644',source,'/etc/apt/sources.list.d/okrva-spacemit.sources'],label='apt-source')
        self.r.run(['sudo','apt-get','update'],label='apt-update')
        self.apt_install(BASE_PACKAGES+[f'{name}={version}' for name,version in self.lock['apt_pins'].items()])
        packages=[]
        for item in self.lock['sdk_packages']:
            if item['kind']=='runtime':packages.append(self.r.download(item['url'],item['sha256']))
        self.r.run(['sudo','install','-d','-m755','/etc/kylin-config/basic'],label='sdk-config-directory')
        self.apt_install(packages)

    def stage_source(self):
        if self.source:return self.source
        # A stable source receipt prevents moving the original clone from
        # breaking the installed services. No .git, credentials or build cache.
        names=subprocess.check_output(['git','-C',str(self.repo),'ls-files','--cached','--others','--exclude-standard','-z']).decode().split('\0')
        files=[];digest=hashlib.sha256()
        for name in sorted(set(filter(None,names))):
            p=self.repo/name
            if not p.is_file():continue
            if p.is_symlink():raise InstallError('源码清单含符号链接，请检查: '+name)
            value=sha(p);files.append((name,value));digest.update((name+'\0'+value+'\n').encode())
        revision=digest.hexdigest()[:16]
        target=self.work/'source'/revision
        for name,value in files:
            source=self.repo/name;dest=target/name
            if dest.exists() and sha(dest)!=value:raise InstallError('构建源码快照被修改: '+str(dest))
            if not dest.exists():copy_file(source,dest)
        self.source=target;self.c.revision=revision
        json_write(self.work/'source-receipt.json',dict(revision=revision,files=dict(files)))
        return target

    def fetch_source(self,name):
        if name in ('llama','tts','asr'):
            from importlib.util import spec_from_file_location,module_from_spec
            spec=spec_from_file_location('okrva_vendor',self.repo/'tools/fetch_vendor.py');module=module_from_spec(spec);spec.loader.exec_module(module)
            url,revision=module.SOURCES[name]
        else:
            item=self.lock['sources'][name];url,revision=item['url'],item['revision']
        dest=self.work/'vendor'/f'{name}-{revision[:12]}'
        if not dest.exists():
            dest.parent.mkdir(parents=True,exist_ok=True)
            self.r.run(['git','init',dest],label='init-'+name)
            self.r.run(['git','-C',dest,'remote','add','origin',url],label='origin-'+name)
        if not (dest/'.git').exists():raise InstallError('外部源码目录不是安装器的Git缓存: '+str(dest))
        head=subprocess.run(['git','-C',str(dest),'rev-parse','HEAD'],text=True,capture_output=True)
        if head.returncode or head.stdout.strip()!=revision:
            self.r.run(['git','-C',dest,'fetch','--depth','1','origin',revision],label='fetch-'+name)
            self.r.run(['git','-C',dest,'-c','core.autocrlf=false','checkout','--detach',revision],label='checkout-'+name)
        self.r.run(['git','-C',dest,'submodule','update','--init','--recursive'],label='submodules-'+name)
        actual=self.r.run(['git','-C',dest,'rev-parse','HEAD'],capture=True).strip()
        if actual!=revision:raise InstallError('外部源码版本不匹配: '+name)
        return dest

    def private_qt_sdk(self):
        if self.sdk:return self.sdk
        aptdir=self.work/'qt-development';aptdir.mkdir(parents=True,exist_ok=True)
        pending=list(QT_DEVELOPMENT);done={};runtime=set()
        while pending:
            name=prefer_package([pending.pop()])
            if name in done:continue
            raw=self.r.run(['apt-cache','show','--no-all-versions',name],capture=True)
            fields={};key=None
            for line in raw.splitlines():
                if line.startswith((' ','\t')) and key:fields[key]+=' '+line.strip()
                elif ': ' in line:key,value=line.split(': ',1);fields[key]=value
            if 'Version' not in fields:raise InstallError('缺少开发包: '+name)
            version=fields['Version']
            matches=[]
            for p in aptdir.glob('*.deb'):
                # dpkg-deb is the authority, not a guessed epoch filename.
                if p.name.startswith(name+'_'):
                    meta=subprocess.check_output(['dpkg-deb','-f',str(p),'Version'],text=True).strip()
                    if meta==version:matches.append(p)
            if not matches:
                self.r.run(['apt-get','download',name+'='+version],cwd=aptdir,label='qt-download')
                matches=[p for p in aptdir.glob(name+'_*.deb') if subprocess.check_output(['dpkg-deb','-f',str(p),'Version'],text=True).strip()==version]
            if len(matches)!=1:raise InstallError('开发包缓存存在歧义: '+name)
            done[name]=matches[0]
            for choices in split_dependencies(fields.get('Depends','')+','+fields.get('Pre-Depends','')):
                dependency=prefer_package(choices)
                if development_package(dependency):pending.append(dependency)
                else:runtime.add(dependency)
        # Install runtime dependencies only; all Qt/KF development metadata is
        # extracted privately so desktop-GL Qt cannot replace the GLES runtime.
        self.apt_install(runtime)
        packages=list(done.values())
        packages += [self.r.download(p['url'],p['sha256']) for p in self.lock['sdk_packages'] if p['kind']=='development']
        key=hashlib.sha256(''.join(sorted(sha(p) for p in packages)).encode()).hexdigest()[:16]
        destination=self.work/'sdk'/key
        if not (destination/'complete.json').is_file():
            destination.parent.mkdir(parents=True,exist_ok=True)
            temporary=Path(tempfile.mkdtemp(prefix='.sdk-',dir=destination.parent))
            for package in packages:self.r.run(['dpkg-deb','-x',package,temporary],label='qt-extract')
            # Link missing system files only AFTER every package was extracted.
            # Never unpack another package through system-facing SDK symlinks.
            merge_system_sdk(Path('/usr'),temporary/'usr')
            # qmake modules from KDE may contain hard-coded /usr paths.
            # Keep pkg-config files original: PKG_CONFIG_SYSROOT_DIR handles them.
            for pri in (temporary/'usr/lib/riscv64-linux-gnu/qt5/mkspecs').rglob('*.pri'):
                if not pri.is_symlink():
                    text=pri.read_text();text=text.replace('= /usr/','= '+str(destination)+'/usr/')
                    pri.write_text(text)
            conf=f'''[Paths]
Prefix={destination}/usr
Headers=include/riscv64-linux-gnu/qt5
Libraries=lib/riscv64-linux-gnu
ArchData=lib/riscv64-linux-gnu/qt5
Data=share/qt5
HostBinaries=lib/qt5/bin
HostLibraries=lib/riscv64-linux-gnu
HostData=lib/riscv64-linux-gnu/qt5
'''
            atomic_write(temporary/'usr/lib/qt5/bin/qt.conf',conf)
            json_write(temporary/'complete.json',dict(packages={p.name:sha(p) for p in packages}),0o644)
            if destination.exists():raise InstallError('SDK目录不完整，请使用新的 --work-dir。')
            temporary.rename(destination)
        gui=destination/'usr/lib/riscv64-linux-gnu/cmake/Qt5Gui'
        if (gui/'Qt5Gui_QXcbGlxIntegrationPlugin.cmake').exists():
            raise InstallError('GLES开发目录混入了GLX插件配置')
        self.sdk=destination
        return destination

    def qt_environment(self):
        sdk=self.private_qt_sdk()
        return dict(os.environ,PKG_CONFIG_SYSROOT_DIR=str(sdk),
          PKG_CONFIG_LIBDIR=f'{sdk}/usr/lib/riscv64-linux-gnu/pkgconfig:{sdk}/usr/lib/pkgconfig:{sdk}/usr/share/pkgconfig',
          CPLUS_INCLUDE_PATH=f'{sdk}/usr/include/riscv64-linux-gnu:{sdk}/usr/include:{sdk}/usr/include/KF5:{sdk}/usr/include/KF5/KWindowSystem',
          LIBRARY_PATH=f'{sdk}/usr/lib/riscv64-linux-gnu:{sdk}/usr/lib')

    def qt(self):
        source=self.stage_source()/'kylin-aiassistant-lite';env=self.qt_environment()
        qmake=self.sdk/'usr/lib/qt5/bin/qmake'
        for name,project,extra in (
            ('smallk',source/'kylin-aiassistant/kylin-aiassistant.pro',[]),
            ('voice',source/'voice-assistant/voice-assistant.pro',[]),
            ('document-reader',source/'tools/document-reader-cli/reader-cli.pro',
             ['DOCUMENT_READER_ROOT='+str(source/'kylin-aiassistant/global')])):
            build=self.work/'build'/name;build.mkdir(parents=True,exist_ok=True)
            self.r.run([qmake,project,'PREFIX=/usr',*extra],cwd=build,env=env,label='qmake-'+name)
            self.r.run(['make','-j'+str(self.c.jobs)],cwd=build,env=env,label='build-'+name)
            if name!='document-reader':
                self.r.run(['make','install','INSTALL_ROOT='+str(self.artifacts/'rootfs')],cwd=build,env=env,label='stage-'+name)
        self.validate_elf(self.work/'build/smallk/kylin-aiassistant')
        self.validate_elf(self.work/'build/voice/voice-assistant')

    def validate_elf(self,binary):
        linked=self.r.run(['ldd',binary],capture=True)
        if 'not found' in linked:raise InstallError('运行库缺失: '+linked)
        dynamic=self.r.run(['readelf','-d',binary],capture=True)
        if str(self.work) in dynamic and any(x in dynamic for x in ('RPATH','RUNPATH')):
            raise InstallError('可执行文件仍引用临时构建库目录: '+str(binary))

    def llama(self):
        vendor=self.fetch_source('llama');build=self.work/'build/llama'
        self.r.run(['sh',self.repo/'tools/build_llama_backend.sh',vendor,build],
                   env=dict(os.environ,BUILD_JOBS=str(self.c.jobs)),label='build-llama')
        self.validate_elf(build/'bin/libggml-cpu.so')

    def asr(self):
        vendor=self.fetch_source('asr');build=self.work/'build/asr'
        knf=self.fetch_source('kaldi-native-fbank')
        shared=self.work/'thirdparty';shared.mkdir(exist_ok=True)
        self.link_source(shared/'kaldi-native-fbank',knf)
        env=dict(os.environ,SROBOTIS_THIRDPARTY_CACHE=str(shared))
        self.r.run(['cmake','-S',vendor,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
          '-DBUILD_STREAM_DEMO=OFF','-DBUILD_PYTHON_BINDINGS=ON','-DASR_MODEL_FETCH_OFF=ON',
          '-DBUILD_SHARED_LIBS=OFF','-DCMAKE_SKIP_BUILD_RPATH=ON',
          '-DUSE_SPACEMIT_EP=ON','-DONNXRUNTIME_INCLUDE_DIR=/usr/include',
          '-DONNXRUNTIME_LIB=/usr/lib/libonnxruntime.so','-DPython3_EXECUTABLE=/usr/bin/python3'],env=env,label='configure-asr')
        self.r.run(['cmake','--build',build,'--parallel',str(self.c.jobs),'--target','_spacemit_asr'],env=env,label='build-asr')
        modules=list((build/'python/spacemit_asr').glob('_spacemit_asr*.so'))
        if len(modules)!=1:raise InstallError('ASR Python绑定未生成')
        self.validate_elf(modules[0])

    @staticmethod
    def link_source(destination,source):
        if destination.is_symlink() and destination.resolve()==source.resolve():return
        if destination.exists() or destination.is_symlink():raise InstallError('外部依赖缓存冲突: '+str(destination))
        destination.symlink_to(source,target_is_directory=True)

    def tts(self):
        vendor=self.fetch_source('tts');build=self.work/'build/tts'
        self.r.run(['python3',self.repo/'tools/apply_backend_changes.py','tts',vendor],label='patch-tts')
        shared=self.work/'thirdparty';shared.mkdir(exist_ok=True)
        for name in ('cpp-pinyin','cppjieba','kaldifst'):self.link_source(shared/name,self.fetch_source(name))
        env=dict(os.environ,SROBOTIS_THIRDPARTY_CACHE=str(shared),BUILD_JOBS=str(self.c.jobs))
        self.r.run(['cmake','-S',vendor,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
          '-DTTS_USE_FST=ON','-DUSE_SPACEMIT_EP=OFF','-DBUILD_SIMPLE_DEMO=OFF',
          '-DBUILD_SHARED_LIBS=OFF','-DCMAKE_SKIP_BUILD_RPATH=ON',
          '-DBUILD_STREAM_DEMO=OFF','-DBUILD_PYTHON_BINDINGS=OFF',
          '-DONNXRUNTIME_INCLUDE_DIR=/usr/include','-DONNXRUNTIME_LIB=/usr/lib/libonnxruntime.so',
          '-DRVAI_MATCHA_BRIDGE_SOURCE='+str(self.repo/'riscv-ai-adapter/scripts/tts_matcha_bridge.cpp')],env=env,label='configure-tts')
        self.r.run(['cmake','--build',build,'--target','kylinbot_matcha','--parallel',str(self.c.jobs)],env=env,label='build-tts')
        self.validate_elf(build/'lib/libkylinbot-matcha.so')

    def kws(self):
        archive=self.r.download(self.lock['sherpa']['url'],self.lock['sherpa']['sha256'])
        target=self.work/'sherpa'
        if not target.exists():unpack(archive,target)
        folders=[p.parent for p in target.glob('*/lib') if (p/'libsherpa-onnx-c-api.so').exists()]
        if len(folders)!=1:raise InstallError('sherpa C API运行库缺失')
        runtime=folders[0];source=self.stage_source()/'kylinbot/tools'
        output=self.work/'build/kws';output.mkdir(parents=True,exist_ok=True)
        # Embed the final user path, never the build-cache path.
        library=Path.home()/'.local/share/kylinbot-asr/sherpa-v1.13.2/lib'
        for name,file in (('kylinbot-kws-gated',source/'bianbu-kws/kylinbot-kws-stream.c'),
                          ('kylinbot-vad-stream',source/'bianbu-vad/kylinbot-vad-stream.c')):
            self.r.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror',
                '-I'+str(source/'bianbu-vad/include'),file,'-L'+str(runtime/'lib'),
                '-Wl,-rpath,'+str(library),'-lsherpa-onnx-c-api','-o',output/name],label='build-'+name)
        json_write(self.work/'sherpa-location.json',dict(path=str(runtime)))

    def gesture(self):
        source=self.stage_source()/'ukui-air-gesture'
        from resources import Resources
        assets=Resources(self.c).gesture_files()
        for p in assets:copy_file(p,source/'app/python'/p.name)
        sdk=self.private_qt_sdk();env=self.qt_environment();build=self.work/'build/gesture'
        self.r.run(['cmake','-S',source,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
          '-DCMAKE_INSTALL_PREFIX=/usr/local','-DCMAKE_PREFIX_PATH='+str(sdk/'usr'),
          '-DQt5_DIR='+str(sdk/'usr/lib/riscv64-linux-gnu/cmake/Qt5')],env=env,label='configure-gesture')
        self.r.run(['cmake','--build',build,'--parallel',str(self.c.jobs)],env=env,label='build-gesture')
        self.validate_elf(build/'app/ukui-air-gesture')

    def kylinbot(self):
        source=self.stage_source()/'kylinbot'
        prefix=self.work/'rust'
        if not (prefix/'bin/cargo').is_file():
            item=self.lock['rust'];archive=self.r.download(item['url'],item['sha256'])
            unpacked=self.work/'rust-distribution'
            if not unpacked.exists():unpack(archive,unpacked)
            installers=list(unpacked.glob('*/install.sh'))
            if len(installers)!=1:raise InstallError('Rust发行包目录不完整')
            self.r.run(['sh',installers[0],'--prefix='+str(prefix),
                        '--components=rustc,cargo,rust-std-riscv64gc-unknown-linux-gnu','--disable-ldconfig'],label='rust-toolchain')
        env=dict(os.environ,PATH=str(prefix/'bin')+':'+os.environ['PATH'],
                 CARGO_HOME=str(self.work/'cargo'),CARGO_TARGET_DIR=str(self.work/'build/cargo'),
                 CARGO_BUILD_JOBS=str(self.c.jobs))
        # The deployed UI is native Qt/QML. Gateway APIs do not require the
        # separate React administration dashboard or its Node build toolchain.
        self.r.run(['cargo','build','--release','--locked','--no-default-features','--features',
            'agent-runtime,gateway,voice-wake,tui-onboarding,observability-prometheus,schema-export',
            '--bin','kylinbot'],cwd=source,env=env,label='build-kylinbot')
        self.validate_elf(self.work/'build/cargo/release/kylinbot')

    def build(self,component='all'):
        selected=('llama','asr','tts','qt','kws','gesture','kylinbot') if component=='all' else (component,)
        self.stage_source()
        for name in selected:
            print('\n构建组件: '+name,flush=True);getattr(self,name)()
        receipt=self.work/'last-build.json'
        current=json.loads(receipt.read_text()) if receipt.is_file() else {}
        previous=current.get('components',[]) if current.get('source_revision')==self.c.revision else []
        json_write(receipt,dict(source_revision=self.c.revision,components=sorted(set(previous)|set(selected))))
