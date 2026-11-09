"""Install current artifacts, generate per-device credentials and user services."""
from __future__ import annotations
import configparser
import hashlib
import io
import json
import os
from pathlib import Path
import re
import secrets
import shlex
import shutil
import subprocess
import tomllib

from common import InstallError,atomic_write,copy_file,install_model,json_write,sha,unpack_zip
from resources import Resources,FSTS,KWS
from libreoffice_runtime import check_libreoffice,runtime_settings

CONFIG_NAMES=('.kylinbot/config.toml','.config/kylin-aiassistant/settings.ini',
              '.config/kylin-aiassistant/tts.ini','.config/kylinbot-asr/env',
              '.config/riscv-ai-adapter/config.toml')


def load_state(home):
    p=home/'.local/state/okrva-installer/installed.json'
    return json.loads(p.read_text()) if p.is_file() else dict(files={})


def check_existing(home):
    state=load_state(home)
    for name in CONFIG_NAMES:
        p=home/name
        if p.exists() and name not in state.get('files',{}):
            raise InstallError('检测到已有或手动修改的配置，已保留: '+str(p)+
                '\n该安装入口面向新机器；请先运行 --phase doctor 检查现有部署。')


def user_directories(home):
    result={}
    for kind in ('DOCUMENTS','DESKTOP'):
        value=subprocess.check_output(['xdg-user-dir',kind],text=True).strip()
        p=Path(value).resolve()
        if not p.is_relative_to(home.resolve()) or p==home.resolve():
            raise InstallError('请先配置独立的用户文档与桌面目录: xdg-user-dirs-update')
        result[kind]=p
    return result


def toml_text(value):return json.dumps(str(value),ensure_ascii=False)[1:-1]


def render_bot(template,home,runtime,documents,desktop,asr_token,gateway_token):
    text=template.replace('@HOME@',toml_text(home)).replace('REPLACE_WITH_YOUR_OWN_VALUE',asr_token)
    text=text.replace('paired_tokens = []','paired_tokens = ["'+hashlib.sha256(gateway_token.encode()).hexdigest()+'"]',1)
    text=text.replace('/run/user/1000',toml_text(runtime))
    text=text.replace(toml_text(home)+'/\\u6587\\u6863',toml_text(documents))
    text=text.replace(toml_text(home)+'/文档',toml_text(documents)).replace(toml_text(home)+'/Desktop',toml_text(desktop))
    tomllib.loads(text)
    return text


def discover_camera(explicit=None):
    if explicit:
        p=Path(explicit)
        if not p.exists():raise InstallError('指定的摄像头不存在: '+str(p))
        return str(p)
    candidates=sorted(Path('/dev/v4l/by-id').glob('*video-index0'))
    if len(candidates)==1:return str(candidates[0])
    if not candidates and Path('/dev/video0').exists():return '/dev/video0'
    if len(candidates)>1:raise InstallError('发现多个摄像头，请用 --camera /dev/v4l/by-id/... 指定')
    return '/dev/video0'


def units_for_device(repo,home,camera,ppt,display='wayland-0',launcher=None,impress_pipe=None):
    office=runtime_settings(home,launcher,impress_pipe)
    units={}
    for p in (repo/'deploy/systemd').glob('*.service'):
        text=p.read_text().replace('WAYLAND_DISPLAY=wayland-0','WAYLAND_DISPLAY='+display.replace('%','%%'))
        if p.name=='ukui-air-gesture.service':
            text=re.sub(r'^Environment="AIR_GESTURE_CAMERA=.*"$',
                'Environment="AIR_GESTURE_CAMERA='+camera.replace('%','%%').replace('"','\\"')+'"',text,flags=re.M)
        elif p.name=='riscv-ai-reader.service':
            for name,value in (('RVAI_LIBREOFFICE_LAUNCHER',office['launcher']),('RVAI_IMPRESS_PIPE',office['pipe'])):
                line='Environment='+json.dumps(name+'='+value.replace('%','%%'),ensure_ascii=False)
                text=re.sub(r'^Environment="'+name+r'=.*"$',lambda _:line,text,flags=re.M)
            command=['/usr/bin/python3','-u','-B','%h/riscv-ai-adapter/tools/reading/reader.py']
            # systemd command quoting differs from shell quoting.
            line='ExecStart='+' '.join(json.dumps(a,ensure_ascii=False) for a in command)
            text=re.sub(r'^ExecStart=.*$',lambda _:line,text,flags=re.M)
        units[p.name]=text
    return units


class Deployment:
    def __init__(self,c,recipes):
        self.c=c;self.b=recipes;self.r=c.runner;self.home=Path.home();self.work=c.work
        self.state=load_state(self.home);self.files=dict(self.state.get('files',{}))

    def managed(self,path,data,mode=0o644):
        if not path.resolve().is_relative_to(self.home.resolve()):raise InstallError('用户文件路径越界')
        name=str(path.relative_to(self.home))
        raw=data.encode() if isinstance(data,str) else data
        if path.exists():
            current=sha(path)
            if current==hashlib.sha256(raw).hexdigest():
                self.files[name]=current;return False
            if name not in self.files:raise InstallError('保留未由安装器管理的文件: '+str(path))
            if current!=self.files[name]:
                print('保留安装后调整过的配置: '+str(path));return False
        # Record ownership before the atomic file replacement, so an interrupted
        # installation can resume without treating its own config as foreign.
        self.files[name]=hashlib.sha256(raw).hexdigest();self.state['files']=self.files
        json_write(self.home/'.local/state/okrva-installer/installed.json',self.state)
        atomic_write(path,raw,mode)
        return True

    def link(self,path,target):
        if path.is_symlink() and path.resolve()==target.resolve():return
        name=str(path.relative_to(self.home))
        if path.exists() or path.is_symlink():
            previous=self.state.get('links',{}).get(name)
            if not path.is_symlink() or os.readlink(path)!=previous:raise InstallError('保留现有目录: '+str(path))
            path.unlink()
        path.parent.mkdir(parents=True,exist_ok=True);path.symlink_to(target,target_is_directory=True)

    def copy_user(self,source,destination,mode=None):
        if destination.exists() and sha(destination)!=sha(source):
            name=str(destination.relative_to(self.home))
            if self.files.get(name)!=sha(destination):raise InstallError('保留现有文件: '+str(destination))
        copy_file(source,destination,mode);self.files[str(destination.relative_to(self.home))]=sha(destination)

    def example_ppt(self,release):
        destination=release/'examples/OKRVA-demo.pptx'
        if self.c.ppt:
            p=Path(self.c.ppt).expanduser().resolve()
            if not p.is_file() or p.suffix.lower() not in ('.ppt','.pptx'):raise InstallError('--ppt需要本地PPT文件')
            destination=release/'examples'/p.name;copy_file(p,destination)
        elif not destination.exists():
            import sys
            item=self.c.lock['python_pptx'];wheel=self.r.download(item['url'],item['sha256'])
            helper=self.work/'python-helper/python-pptx-1.0.2'
            if not (helper/'pptx/templates/default.pptx').is_file():unpack_zip(wheel,helper)
            sys.path.insert(0,str(helper))
            from pptx import Presentation
            p=Presentation()
            for title,body in (
                ('多模态智能终端','文本输入、语音输入和手势输入共同连接本地智能体。'),
                ('本地系统控制','KylinBot通过系统工具调整音量、查询状态和打开应用；执行结果需要实际回读确认。'),
                ('智能文档阅读','阅读器提取当前页原生文本，通过语音提出问题，由本地模型总结并播报。右挥下一页，左挥上一页。')):
                slide=p.slides.add_slide(p.slide_layouts[1]);slide.shapes.title.text=title;slide.placeholders[1].text=body
            destination.parent.mkdir(parents=True,exist_ok=True);p.save(destination)
        return destination

    def install(self):
        office=check_libreoffice(self.home,getattr(self.c,'libreoffice_launcher',None),getattr(self.c,'impress_pipe',None))
        check_existing(self.home)
        self.state['libreoffice']=office
        source=self.b.stage_source();revision=self.c.revision
        receipt=self.work/'last-build.json'
        record=json.loads(receipt.read_text()) if receipt.is_file() else {}
        components={'llama','asr','tts','qt','kws','gesture','kylinbot'}
        if record.get('source_revision')!=revision or not components.issubset(record.get('components',[])):
            raise InstallError('请先完整执行 --phase build，当前源码与构建回执不匹配')
        assets=Resources(self.c);assets.validate_all()
        required=['smallk/kylin-aiassistant','voice/voice-assistant','document-reader/kylin-document-reader',
            'llama/bin/libggml-cpu.so','asr/python/spacemit_asr','tts/lib/libkylinbot-matcha.so',
            'kws/kylinbot-kws-gated','kws/kylinbot-vad-stream','gesture/app/ukui-air-gesture','cargo/release/kylinbot']
        for name in required:
            if not (self.work/'build'/name).exists():raise InstallError('缺少构建产物: '+name)
        release=self.home/'.local/share/okrva/releases'/revision
        # Source remains as independent component directories, with no archives.
        if not (release/'.source-installed').is_file():
            shutil.copytree(source,release,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc','node_modules'))
            atomic_write(release/'.source-installed',revision+'\n')
        links={'riscv-ai-adapter':'riscv-ai-adapter','kylin-sys-tools':'system-tools',
          'kylin-aiassistant-lite':'kylin-aiassistant-lite','kylinbot':'kylinbot','ukui-air-gesture-bianbu':'ukui-air-gesture'}
        for alias,component in links.items():self.link(self.home/alias,release/component)

        rootfs=self.work/'artifacts/rootfs'
        copy_file(self.work/'build/cargo/release/kylinbot',rootfs/'usr/local/bin/kylinbot',0o755)
        copy_file(self.work/'build/gesture/app/ukui-air-gesture',rootfs/'usr/local/bin/ukui-air-gesture',0o755)
        for p in (source/'ukui-air-gesture/app/python').glob('*.py'):
            copy_file(p,rootfs/'usr/local/share/ukui-air-gesture/python'/p.name,0o644)
        copy_file(source/'ukui-air-gesture/app/python/LICENSE.opencv-zoo',rootfs/'usr/local/share/ukui-air-gesture/LICENSE.opencv-zoo',0o644)
        for p in (assets.assets/'gesture-opencv-ort').glob('*.onnx'):
            copy_file(p,rootfs/'usr/local/share/ukui-air-gesture/models'/p.name,0o644)
        atomic_write(rootfs/'etc/udev/rules.d/70-okrva-tcm.rules',
            'SUBSYSTEM=="misc", KERNEL=="tcm", GROUP="render", MODE="0660", TAG+="uaccess"\n'
            'SUBSYSTEM=="misc", KERNEL=="tcm_sync_mem", GROUP="render", MODE="0660", TAG+="uaccess"\n')
        atomic_write(rootfs/'etc/udev/rules.d/99-zz-okrva-uinput.rules',
            'KERNEL=="uinput", GROUP="input", MODE="0660", TAG+="uaccess"\n')
        manifest=[]
        from root_install import allowed
        for p in sorted(rootfs.rglob('*')):
            if not p.is_file():continue
            name=str(p.relative_to(rootfs))
            if not allowed(name):raise InstallError('构建安装路径未登记: '+name)
            manifest.append(dict(path=name,sha256=sha(p),mode=p.stat().st_mode & 0o777))
        json_write(self.work/'rootfs-manifest.json',manifest,0o644)
        self.r.run(['sudo','python3',self.c.repo/'deploy/installer/root_install.py',rootfs,self.work/'rootfs-manifest.json'],label='install-system-programs')
        self.r.run(['sudo','glib-compile-schemas','/usr/share/glib-2.0/schemas'],label='compile-schemas')
        self.r.run(['sudo','usermod','-aG','render,video,input',self.c.username],label='device-groups')
        self.r.run(['sudo','udevadm','control','--reload-rules'],label='udev-reload')
        for device in ('/dev/tcm','/dev/tcm_sync_mem','/dev/uinput'):
            if Path(device).exists():self.r.run(['sudo','setfacl','-m',f'u:{os.getuid()}:rw',device],label='device-access')

        # Models, libraries and native bindings have explicit installation maps.
        for item in assets.models:
            if item['role']=='gesture-opencv-ort':continue
            dest=Path(item['path'].replace('@HOME@',str(self.home)))
            install_model(assets.assets/item['role']/item['filename'],dest,item['sha256'])
        tts_share=self.home/'.local/share/riscv-ai-adapter/tts-matcha-20261016'
        for name,expected in FSTS.items():install_model(assets.assets/'tts-fst'/name,tts_share/'models/tts/text_norm/v1/zh'/name,expected)
        pinyin=self.b.fetch_source('cpp-pinyin')/'res'
        for p in pinyin.rglob('*'):
            if p.is_file():self.copy_user(p,tts_share/'thirdparty/cpp-pinyin/res'/p.relative_to(pinyin),0o644)
        llama=self.home/'.local/lib/riscv-ai-adapter/llama-q6-rvv-20261030'
        for p in (self.work/'build/llama/bin').glob('libggml*.so*'):
            if p.is_file():self.copy_user(p,llama/'lib'/p.name,0o644)
        self.copy_user(self.c.repo/'deploy/installer/llama-server-tcm.sh',llama/'llama-server-tcm',0o755)
        self.copy_user(self.work/'build/tts/lib/libkylinbot-matcha.so',self.home/'.local/lib/riscv-ai-adapter/tts-matcha-20261016/libkylinbot-matcha.so',0o755)
        self.copy_user(Path('/usr/bin/espeak-ng'),self.home/'.local/lib/riscv-ai-adapter/tts-matcha-20261016/bin/espeak-ng',0o755)
        for p in (self.work/'build/asr/python/spacemit_asr').rglob('*'):
            if p.is_file() and p.suffix in ('.so','.py'):self.copy_user(p,self.home/'src/model-zoo-asr/build-k1/python/spacemit_asr'/p.relative_to(self.work/'build/asr/python/spacemit_asr'))
        sherpa=Path(json.loads((self.work/'sherpa-location.json').read_text())['path'])
        for folder in ('lib','bin'):
            for p in (sherpa/folder).iterdir():
                if p.is_file() and (folder=='lib' or p.name=='sherpa-onnx-keyword-spotter'):
                    self.copy_user(p,self.home/'.local/share/kylinbot-asr/sherpa-v1.13.2'/folder/p.name,0o755 if folder=='bin' else 0o644)
        for name in ('kylinbot-kws-gated','kylinbot-vad-stream'):
            self.copy_user(self.work/'build/kws'/name,self.home/'.local/lib/kylinbot'/name,0o755)
        self.copy_user(source/'kylinbot/tools/bianbu-kws/kws-policy.py',self.home/'.local/lib/kylinbot/kws-policy.py',0o644)
        self.copy_user(source/'asr-service/sensevoice_server.py',self.home/'.local/share/kylinbot-asr/bin/sensevoice_server.py',0o644)
        self.copy_user(source/'assets/wake/wake_ack_zh_fast.wav',self.home/'.local/share/kylinbot/wake_ack_zh_fast.wav',0o644)
        self.copy_user(self.work/'build/document-reader/kylin-document-reader',self.home/'.local/libexec/kylin-document-reader',0o755)
        for name,installed in [('wait-ready.py','okrva-wait-ready.py'),('wait-gesture-ready.py','okrva-wait-gesture-ready.py'),
            ('clear-conversation-history.py','okrva-clear-conversation-history.py'),('finalize-recording.py','okrva-finalize-recording.py')]:
            self.copy_user(source/'deploy/helpers'/name,self.home/'.local/libexec'/installed,0o644)
        self.copy_user(source/'deploy/helpers/start-smallk.sh',self.home/'.local/bin/okrva-smallk',0o755)
        self.copy_user(source/'deploy/helpers/record-k1.sh',self.home/'record-k1.sh',0o755)
        self.configure(release,discover_camera(self.c.camera))
        self.state.update(source_revision=revision,files=self.files,links={alias:str(release/component) for alias,component in links.items()})
        json_write(self.home/'.local/state/okrva-installer/installed.json',self.state)

    def configure(self,release,camera):
        directory=user_directories(self.home)
        for p in directory.values():p.mkdir(parents=True,exist_ok=True)
        p=self.home/'.local/state/okrva-installer/credentials.json'
        if p.is_file():credentials=json.loads(p.read_text())
        else:
            credentials=dict(gateway_token='zc_'+secrets.token_hex(32),asr_token=secrets.token_urlsafe(32));json_write(p,credentials)
        ppt=self.example_ppt(release)
        bot_template=(release/'deploy/templates/kylinbot.toml.in').read_text()
        bot_template=bot_template.replace('@PPT_DIR@',toml_text(ppt.parent)).replace('@PPT@',toml_text(ppt))
        bot=render_bot(bot_template,self.home,self.c.runtime,
            directory['DOCUMENTS'],directory['DESKTOP'],credentials['asr_token'],credentials['gateway_token'])
        self.managed(self.home/'.kylinbot/config.toml',bot,0o600)
        adapter=(release/'deploy/templates/riscv-ai-adapter.toml.in').read_text().replace('@HOME@',toml_text(self.home))
        tomllib.loads(adapter);self.managed(self.home/'.config/riscv-ai-adapter/config.toml',adapter,0o600)
        self.managed(self.home/'.config/kylinbot-asr/env','KYLINBOT_ASR_TOKEN='+credentials['asr_token']+'\n',0o600)
        ini=configparser.ConfigParser(interpolation=None);ini.optionxform=str
        ini['General']={'gateway-host':'127.0.0.1:42617','gateway-token':credentials['gateway_token'],
          'history-id':'0','meeting-id':'0','is-first-install':'false','is-first-install-50':'false',
          'is-first-template':'true','is-hide-tray':'true','is-stroke-word':'true','scribe-auto-invoke':'true',
          'question-mode-enabled':'true','voice-callback-enabled':'true','voice-followup-enabled':'false',
          'voice-id':'0','win-mode':'3','is-sidebar-close':'true','is-side-mode-abreast':'false'}
        output=io.StringIO();ini.write(output,space_around_delimiters=False)
        self.managed(self.home/'.config/kylin-aiassistant/settings.ini',output.getvalue(),0o600)
        self.managed(self.home/'.config/kylin-aiassistant/tts.ini',
            '[TTS]\nurl=http://127.0.0.1:18080/v1/audio/speech\nmodel=vits-zh\nvoice=zh-0\n',0o600)
        workspace=self.home/'.kylinbot/workspace'
        for name in ('IDENTITY.md','SOUL.md'):
            self.managed(workspace/name,(release/'deploy/templates/workspace'/name).read_text(),0o644)
        for name,text in units_for_device(release,self.home,camera,ppt,self.c.display,
                getattr(self.c,'libreoffice_launcher',None),getattr(self.c,'impress_pipe',None)).items():
            self.managed(self.home/'.config/systemd/user'/name,text)
        dbus='[D-BUS Service]\nName=org.ukui.voice_assistant\nExec=/usr/bin/env SDL_AUDIODRIVER=pipewire /usr/bin/voice-assistant --dbus-activation\nSystemdService=okrva-voice-pipewire.service\n'
        self.managed(self.home/'.local/share/dbus-1/services/org.ukui.voice_assistant.service',dbus)
        self.managed(self.home/'.local/share/dbus-1/services/org.ukui.kylin_aiassistant.service',
            '[D-BUS Service]\nName=org.ukui.kylin_aiassistant\nExec=/usr/bin/kylin-aiassistant --silence\nSystemdService=okrva-aiassistant.service\n')
        desktop=(release/'deploy/templates/kylin-aiassistant.desktop.in').read_text().replace('@HOME@',str(self.home))
        self.managed(self.home/'.local/share/applications/kylin-aiassistant.desktop',desktop)
        self.managed(self.home/'.local/share/applications/riscv-ai-reader.desktop',
            (release/'riscv-ai-adapter/tools/reading/packaging/riscv-ai-reader.desktop').read_text())
        self.managed(self.home/'.config/autostart/okrva-smallk.desktop',
            '[Desktop Entry]\nType=Application\nName=OKRVA 小K\nExec=systemctl --user start okrva-aiassistant.service\nX-GNOME-Autostart-enabled=true\n')
        self.state.update(camera=camera,ppt=str(ppt),documents=str(directory['DOCUMENTS']),desktop=str(directory['DESKTOP']))
        (self.home/'.local/state/riscv-ai-reader').mkdir(parents=True,exist_ok=True)
        self.r.run(['systemctl','--user','daemon-reload'],label='user-units-reload')

    def audio(self):
        restarted=False
        data={kind:json.loads(subprocess.check_output(['pactl','-f','json','list',kind],text=True)) for kind in ('sources','sinks')}
        chosen={}
        for kind,explicit in (('sources',self.c.audio_input),('sinks',self.c.audio_output)):
            entries=[x for x in data[kind] if not x['name'].endswith('.monitor')]
            candidates=[x for x in entries if x.get('properties',{}).get('device.bus')=='usb' or '.usb-' in x['name']]
            selected=next((x for x in entries if x['name']==explicit),None) if explicit else (candidates[0] if len(candidates)==1 else None)
            if explicit and selected is None:raise InstallError('指定的音频设备不存在: '+explicit)
            if selected is None:
                print('保留现有默认音频；可在系统设置选择USB'+('麦克风' if kind=='sources' else '输出'))
                continue
            command='set-default-source' if kind=='sources' else 'set-default-sink'
            self.r.run(['pactl',command,selected['name']],label='audio-default');chosen[kind]=selected
        source=chosen.get('sources')
        if source:
            cards=json.loads(subprocess.check_output(['pactl','-f','json','list','cards'],text=True))
            card=next((x for x in cards if x['index']==source.get('card')),None)
            if card and card['name'].startswith('alsa_card.usb-'):
                # This camera family can report a nonsensical 0..0.39dB range.
                # Scope raw-volume handling to the selected USB device only.
                name=json.dumps(card['name'])
                lua='table.insert(alsa_monitor.rules, { matches = { { { "device.name", "equals", '+name+' } } }, apply_properties = { ["api.alsa.ignore-dB"] = true } })\n'
                changed=self.managed(self.home/'.config/wireplumber/main.lua.d/51-okrva-camera-mic.lua',lua)
                if changed:
                    self.r.run(['systemctl','--user','restart','wireplumber.service'],label='usb-audio-rule')
                    restarted=True
                    import time
                    deadline=time.monotonic()+20
                    while time.monotonic()<deadline:
                        seen=json.loads(subprocess.check_output(['pactl','-f','json','list','sources'],text=True))
                        if any(x['name']==source['name'] for x in seen):break
                        time.sleep(.25)
                    else:raise InstallError('USB麦克风未在音频服务恢复后重新出现')
                self.r.run(['pactl','set-default-source',source['name']],label='usb-input-restore')
                if chosen.get('sinks'):self.r.run(['pactl','set-default-sink',chosen['sinks']['name']],label='usb-output-restore')
            self.r.run(['pactl','set-source-mute',source['name'],'0'],label='microphone-unmute')
            self.r.run(['pactl','set-source-volume',source['name'],'100%'],label='microphone-volume')
        self.state['files']=self.files;json_write(self.home/'.local/state/okrva-installer/installed.json',self.state)
        return restarted

    def start(self):
        check_libreoffice(self.home,getattr(self.c,'libreoffice_launcher',None),getattr(self.c,'impress_pipe',None))
        bot_running=subprocess.run(['systemctl','--user','is-active','--quiet','kylinbot.service']).returncode==0
        audio_restarted=self.audio()
        self.r.run(['systemctl','--user','import-environment','DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR'],label='desktop-environment')
        units=['kylinbot-asr-sensevoice.service','kylinbot-tts.service','riscv-ai-adapter.service',
               'kylinbot-kws-policy.service','kylinbot.service']
        self.r.run(['systemctl','--user','enable',*units,'ukui-air-gesture.service'],label='enable-user-services')
        for unit in units:self.r.run(['systemctl','--user','start',unit],label='start-'+unit.split('.')[0])
        if audio_restarted and bot_running:
            self.r.run(['systemctl','--user','restart','kylinbot.service'],label='restore-wake-capture')
        if Path(self.state.get('camera','/dev/video0')).exists():
            self.r.run(['systemctl','--user','reset-failed','ukui-air-gesture.service'],label='reset-gesture-startup')
            self.r.run(['systemctl','--user','start','ukui-air-gesture.service'],label='start-gesture')
        else:print('摄像头尚未连接；连接后执行 ./install.sh --phase start')
        self.r.run(['systemctl','--user','start','okrva-aiassistant.service'],label='start-smallk')
