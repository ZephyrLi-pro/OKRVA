#!/usr/bin/env python3
"""Clone OKRVA, then run ./install.sh in a Bianbu 2.3.5 K1 desktop terminal."""
from __future__ import annotations
import argparse
import configparser
import http.client
import json
import os
from pathlib import Path
import platform
import pwd
import shutil
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
import io
import re
import wave

from common import InstallError,Runner,json_write,sha
from recipes import Recipes
from resources import Resources
from resources import FSTS
from deployment import Deployment,check_existing
from libreoffice_runtime import check_libreoffice,runtime_settings


PHASES=('all','check','deps','fetch','build','install','start','doctor')
COMPONENTS=('all','llama','asr','tts','qt','kws','gesture','kylinbot')


class Context:
    def __init__(self,args):
        self.__dict__.update(vars(args))
        self.repo=Path(__file__).resolve().parents[2]
        self.work=Path(args.work_dir).expanduser().resolve()
        self.lock=json.loads((self.repo/'deploy/installer/lock.json').read_text())
        self.username=pwd.getpwuid(os.getuid()).pw_name
        self.runtime=Path(os.environ.get('XDG_RUNTIME_DIR',f'/run/user/{os.getuid()}'))
        self.display=os.environ.get('WAYLAND_DISPLAY','wayland-0')
        self.runner=Runner(self.work)
        self.revision=None
        office=runtime_settings()
        self.libreoffice_launcher=office['launcher'];self.impress_pipe=office['pipe']


def platform_info():
    fields={}
    for line in Path('/etc/os-release').read_text().splitlines():
        if '=' in line:
            key,value=line.split('=',1);fields[key]=value.strip('"')
    return fields


def preflight(c):
    if os.getuid()==0:raise InstallError('请用桌面普通用户运行 ./install.sh，脚本在必要步骤自动调用sudo。')
    info=platform_info()
    if platform.machine()!='riscv64' or info.get('ID')!='bianbu' or not info.get('VERSION_ID','').startswith('2.3.'):
        raise InstallError('当前配方适用于K1 / riscv64 / Bianbu 2.3.x（基线2.3.5），请勿在其他发行版执行。')
    if os.cpu_count() is None or os.cpu_count()<8:raise InstallError('该配方需要K1的8核CPU布局。')
    if c.phase in ('all','start') and not Path('/dev/tcm').exists():
        raise InstallError('缺少/dev/tcm，当前方案需要K1系统镜像提供的TCM驱动。')
    compatible=Path('/sys/firmware/devicetree/base/compatible')
    if compatible.exists() and b'k1' not in compatible.read_bytes().lower():
        raise InstallError('未识别为K1设备；IME/TCM参数不能直接用于其他芯片。')
    if any(x.isspace() for x in str(c.work)):raise InstallError('构建目录不能含空白字符，请用 --work-dir 指定。')
    if c.work.is_relative_to(c.repo):raise InstallError('--work-dir必须在源码仓库之外。')
    if c.phase in ('all','build','fetch'):
        probe=c.work
        while not probe.exists():probe=probe.parent
        free=shutil.disk_usage(probe).free
        if free<12*1024**3:raise InstallError('构建与模型缓存至少需要12GiB可用空间，建议20GiB以上；可用 --work-dir 选择其他磁盘。')
    if c.phase in ('all','install'):check_existing(Path.home())
    office=None
    if c.phase in ('all','check','install','start'):
        office=check_libreoffice(Path.home(),c.libreoffice_launcher,c.impress_pipe)
    if c.phase in ('all','start') and not c.no_start:
        display=Path(c.display) if Path(c.display).is_absolute() else c.runtime/c.display
        if not display.exists() or not (c.runtime/'bus').exists():
            raise InstallError('请先登录Bianbu Wayland桌面并在桌面终端运行；仅构建安装可加 --no-start。')
    return dict(os=info.get('PRETTY_NAME'),architecture=platform.machine(),user=c.username,
                work_dir=str(c.work),runtime_dir=str(c.runtime),libreoffice=office)


def show_plan(c):
    print('OKRVA 同款K1联网构建安装计划')
    print('目标: Bianbu 2.3.x / K1 / riscv64；普通桌面用户；基线2.3.5')
    print('当前: '+platform.machine()+' / '+platform_info().get('PRETTY_NAME','unknown'))
    print('仓库: '+str(c.repo));print('构建、下载和日志: '+str(c.work))
    for text in (
        '1. 检查平台、磁盘、桌面会话、已有配置和预先准备的LibreOffice24.8启动器。',
        '2. 安装Bianbu依赖和固定版本SpacemiT运行库；麒麟SDK包逐个校验SHA256。',
        '3. 下载并校验0.6B、1.7B、SenseVoice、Matcha、KWS/VAD和手势模型。',
        '4. 固定外部源码版本，应用项目后端补丁；使用独立Qt GLES开发目录。',
        '5. 构建小K、KylinBot、ASR/TTS、GGML后端、KWS/VAD、手势和文档读取器。',
        '6. 安装程序与模型；生成每台设备独立的ASR/Gateway凭据。',
        '7. 检测摄像头及USB音频，配置单轮唤醒和用户级服务。',
        '8. 启动并检查模型、动态库、接口、配对和服务状态。'):
        print(text)
    print('此为只读计划；没有下载、安装、修改配置或重启服务。')


def json_get(url):
    with urllib.request.urlopen(url,timeout=4) as response:return json.load(response)


class UnixHTTP(http.client.HTTPConnection):
    def __init__(self,path):super().__init__('localhost',timeout=5);self.path=path
    def connect(self):
        self.sock=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);self.sock.settimeout(5);self.sock.connect(str(self.path))


def inference_smoke(c):
    def post(url,payload,timeout=180,headers=None):
        if isinstance(payload,dict):
            payload=json.dumps(payload,ensure_ascii=False).encode();headers={'Content-Type':'application/json'}
        request=urllib.request.Request(url,data=payload,headers=headers or {})
        with urllib.request.urlopen(request,timeout=timeout) as response:return response.read()
    value=json.loads(post('http://127.0.0.1:18080/v1/chat/completions',dict(
        model='qwen-local',messages=[{'role':'user','content':'请计算1+1，只回答一个数字。'}],
        max_tokens=32,temperature=0,stream=False,chat_template_kwargs={'enable_thinking':False})))
    answer=value['choices'][0]['message']['content'].strip()
    if not re.fullmatch(r'(?:1\s*\+\s*1\s*[=＝]\s*|答案是[：:]?\s*)?2[。.!]?',answer):raise InstallError('LLM算术自检不符合预期')
    audio=post('http://127.0.0.1:18083/v1/audio/speech',
        dict(model='vits-zh',voice='zh-0',input='你好，我是KylinBot，本地系统助手。'))
    with wave.open(io.BytesIO(audio),'rb') as wav:
        seconds=wav.getnframes()/wav.getframerate();pcm=wav.readframes(wav.getnframes())
        if seconds<.2 or not any(pcm):raise InstallError('TTS未生成有效音频')
    env=Path.home()/'.config/kylinbot-asr/env'
    token=next(line.split('=',1)[1].strip().strip('"') for line in env.read_text().splitlines() if line.startswith('KYLINBOT_ASR_TOKEN='))
    boundary='OKRVASelfCheckBoundary'
    body=(f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="selfcheck.wav"\r\nContent-Type: audio/wav\r\n\r\n'.encode()
          +audio+f'\r\n--{boundary}--\r\n'.encode())
    result=json.loads(post('http://127.0.0.1:18081/v1/audio/transcriptions',body,
        headers={'Content-Type':'multipart/form-data; boundary='+boundary,'Authorization':'Bearer '+token}))
    if not isinstance(result.get('text'),str) or not result['text'].strip():raise InstallError('ASR合成音频自检没有识别结果')
    return dict(llm_arithmetic=True,tts_wave_seconds=round(seconds,2),asr_nonempty=True)


def doctor(c,wait=0,smoke=False):
    deadline=time.monotonic()+wait;errors=[];report={}
    while True:
        errors=[];report={}
        for port,name in ((18081,'ASR'),(18083,'TTS'),(18080,'Adapter')):
            try:
                value=json_get(f'http://127.0.0.1:{port}/health')
                report[name]=value
                if name=='ASR' and value.get('provider')!='spacemit':errors.append('ASR尚未使用SpacemiT EP')
                if name=='TTS' and value.get('text_normalization')!='fst':errors.append('TTS文本规范化资源未就绪')
            except (OSError,ValueError) as error:errors.append(name+'接口未就绪: '+type(error).__name__)
        config=Path.home()/'.config/kylin-aiassistant/settings.ini'
        try:
            ini=configparser.ConfigParser(interpolation=None,strict=False);ini.read(config,encoding='utf-8-sig')
            token=ini['General']['gateway-token'].strip('"')
            connection=UnixHTTP(c.runtime/'kylin-bot/gateway.sock')
            connection.request('GET','/api/sessions',headers={'Authorization':'Bearer '+token})
            response=connection.getresponse();response.read(4*1024*1024);connection.close()
            if response.status!=200:errors.append('Gateway配对验证失败: HTTP '+str(response.status))
            else:report['Gateway']={'authenticated':True}
            report['voice_followup_enabled']=ini['General'].get('voice-followup-enabled','false').lower()=='true'
        except (OSError,KeyError,configparser.Error) as error:errors.append('Gateway尚未就绪: '+type(error).__name__)
        if not errors or time.monotonic()>=deadline:break
        time.sleep(1)
    try:
        report['LibreOffice']=check_libreoffice(Path.home(),
            getattr(c,'libreoffice_launcher',None),getattr(c,'impress_pipe',None))
    except InstallError as error:
        report['LibreOffice']={'version_check_passed':False}
        errors.append(str(error))
    # Read-only checks: no volume changes, app opening, microphone recording,
    # session deletion or user-file edits are part of doctor.
    libraries=['/usr/bin/kylin-aiassistant','/usr/bin/voice-assistant','/usr/local/bin/kylinbot','/usr/bin/wlrctl',
               '/usr/local/bin/ukui-air-gesture',str(Path.home()/'.local/lib/riscv-ai-adapter/tts-matcha-20261016/libkylinbot-matcha.so')]
    for name in libraries:
        if not Path(name).is_file():errors.append('缺少程序: '+name);continue
        linked=subprocess.run(['ldd',name],text=True,capture_output=True)
        if linked.returncode or 'not found' in linked.stdout:errors.append('动态库缺失: '+name)
    models=json.loads((c.repo/'dependencies/model-manifest.json').read_text())
    for item in models:
        p=Path(item['path'].replace('@HOME@',str(Path.home())))
        if not p.is_file() or sha(p)!=item['sha256']:errors.append('模型校验失败: '+item['filename'])
    tts=Path.home()/'.local/share/riscv-ai-adapter/tts-matcha-20261016'
    for name,expected in FSTS.items():
        p=tts/'models/tts/text_norm/v1/zh'/name
        if not p.is_file() or sha(p)!=expected:errors.append('TTS文本规范化数据校验失败: '+name)
    if not (tts/'thirdparty/cpp-pinyin/res/dict/mandarin').exists():errors.append('缺少TTS拼音词典')
    if not os.access('/dev/tcm',os.R_OK|os.W_OK):errors.append('当前桌面用户没有TCM访问权限')
    services=['kylinbot-asr-sensevoice.service','kylinbot-tts.service','riscv-ai-adapter.service',
              'kylinbot.service','kylinbot-kws-policy.service','okrva-aiassistant.service']
    report['services']={}
    for service in services:
        status=subprocess.run(['systemctl','--user','is-active',service],text=True,capture_output=True).stdout.strip()
        report['services'][service]=status
        if status!='active':errors.append('服务未运行: '+service)
    if smoke and not errors:
        try:report['inference_smoke']=inference_smoke(c)
        except (OSError,ValueError,KeyError,StopIteration,InstallError) as error:errors.append('推理自检失败: '+str(error))
    report['errors']=errors;report['passed']=not errors
    # Do not expose tokens or session bodies in stdout or deployment reports.
    c.work.mkdir(parents=True,exist_ok=True);json_write(c.work/'doctor.json',report)
    print(json.dumps(report,ensure_ascii=False,indent=2))
    if errors:raise InstallError('部署检查未通过，详见 '+str(c.work/'doctor.json'))
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase',choices=PHASES,default='all')
    parser.add_argument('--component',choices=COMPONENTS,default='all',help='fetch/build时选择单一组件')
    parser.add_argument('--work-dir',default='~/.cache/okrva-installer')
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--dry-run',action='store_true',help='只读显示计划，任意Linux主机均可执行')
    parser.add_argument('--no-start',action='store_true',help='安装后不启动服务')
    parser.add_argument('--smoke-test',action='store_true',help='doctor时额外测试LLM、TTS、ASR；不录音、不播放')
    parser.add_argument('--camera',help='摄像头稳定路径，多摄像头时需指定')
    parser.add_argument('--audio-input',help='pactl sources中的完整设备名')
    parser.add_argument('--audio-output',help='pactl sinks中的完整设备名')
    parser.add_argument('--ppt',help='阅读器默认打开的本地PPT；未指定则创建项目演示PPT')
    args=parser.parse_args()
    if not 1<=args.jobs<=8:parser.error('--jobs范围为1到8，默认2')
    c=Context(args)
    if args.dry_run:show_plan(c);return
    info=preflight(c);print(json.dumps(info,ensure_ascii=False,indent=2))
    if args.phase=='check':return
    if args.phase=='doctor':doctor(c,smoke=args.smoke_test);return
    if args.phase in ('all','build','install'):
        subprocess.run([sys.executable,str(c.repo/'tools/verify_bundle.py')],check=True)
    c.work.mkdir(parents=True,exist_ok=True)
    recipes=Recipes(c)
    phases=('deps','fetch','build','install','start','doctor') if args.phase=='all' else (args.phase,)
    for phase in phases:
        if args.no_start and phase in ('start','doctor'):continue
        print('\n===== '+phase+' =====',flush=True)
        if phase=='deps':recipes.dependencies()
        elif phase=='fetch':Resources(c).fetch(args.component)
        elif phase=='build':recipes.build(args.component)
        elif phase=='install':Deployment(c,recipes).install()
        elif phase=='start':Deployment(c,recipes).start()
        elif phase=='doctor':doctor(c,wait=120,smoke=True)
    if args.phase in ('all','install','start'):
        print('\n程序安装阶段完成。小K从应用菜单打开；PPT使用LibreOffice Impress，语音与手势桥可执行 systemctl --user start riscv-ai-reader.service。')
        print('PPT默认使用已检查的LibreOffice24.8运行入口；底部备注栏和实际当前页须在Impress窗口中验收。')
        print('默认单轮唤醒：每次提问前说“你好麒麟”，播报结束保留回答、等待下次唤醒。')
        if args.no_start:print('服务尚未启动；登录桌面后执行 ./install.sh --phase start，再执行 --phase doctor。')
    else:print('\n已完成阶段: '+args.phase)
    print('日志及检查结果: '+str(c.work))


if __name__=='__main__':
    try:main()
    except (InstallError,subprocess.CalledProcessError,OSError) as error:
        print('\n安装停止: '+str(error),file=sys.stderr);raise SystemExit(1)
    except KeyboardInterrupt:
        print('\n安装已中断；缓存保留，可重跑相同命令。',file=sys.stderr);raise SystemExit(130)
