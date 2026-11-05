"""Fetch the deployed models and text front-end data, verifying every file."""
from __future__ import annotations
import json
from pathlib import Path
from common import InstallError,copy_file,extract_members,json_write,sha

SPACEMIT='https://archive.spacemit.com/spacemit-ai/model_zoo/'
KWS='sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01'
FSTS={
 'date.fst':'eb8aa079ae3cb81d8f4404992f39d61a0cb990947512b5b8d1e54d1f6980e718',
 'phone.fst':'1ac2b6fa56b1442320c4de7db08353bab8963a2b57f365eebcdd3a2d3562f8d7',
 'number.fst':'743f402181fcfebf76cc2f0546b71fa26476e626fbe4e460fb7b4c3a7a8bd5bd',
}


class Resources:
    def __init__(self,context):
        self.c=context;self.r=context.runner;self.work=context.work
        self.models=json.loads((context.repo/'dependencies/model-manifest.json').read_text())
        self.assets=self.work/'assets'

    def expected(self,role,name):
        rows=[x for x in self.models if x['role']==role and x['filename']==name]
        if len(rows)!=1:raise InstallError('模型清单缺失: '+role+'/'+name)
        return rows[0]

    def direct(self,role,name,url):
        item=self.expected(role,name)
        fetched=self.r.download(url,item['sha256'])
        destination=self.assets/role/name;copy_file(fetched,destination,0o644)
        return destination

    def archive(self,role,names,url):
        output=self.assets/role
        if all((output/n).is_file() and sha(output/n)==self.expected(role,n)['sha256'] for n in names):return
        archive=self.r.download(url)
        extract_members(archive,[dict(member=n,name=n,sha256=self.expected(role,n)['sha256']) for n in names],output)

    def llama(self):
        self.direct('chat-and-planning','Qwen3-0.6B-Q4_K_M.gguf',SPACEMIT+'llm/Qwen3-0.6B-Q4_K_M.gguf')
        name='Qwen3-1.7B-Q4_0.gguf';output=self.assets/'reading-summary'/name
        expected=self.expected('reading-summary',name)['sha256']
        if not output.is_file() or sha(output)!=expected:
            item=self.c.lock['reading_model_input']
            source=self.r.download(item['url'],item['sha256'])
            with source.open('rb') as f:
                if f.read(4)!=b'GGUF':raise InstallError('阅读模型输入不是GGUF文件')
            output.parent.mkdir(parents=True,exist_ok=True)
            partial=output.with_name(output.name+'.partial')
            self.r.run(['/usr/bin/llama-quantize','--allow-requantize',source,partial,'Q4_0','2'],label='quantize-reading-model')
            if sha(partial)!=expected:raise InstallError('阅读模型量化结果不符合当前版本SHA256，未安装')
            partial.replace(output)

    def asr(self):
        self.archive('asr-sensevoice',['model_quant_optimized.onnx','tokens.txt','am.mvn'],SPACEMIT+'asr/sensevoice.tar.gz')
        self.direct('voice-vad','silero_vad.onnx',
            'https://raw.githubusercontent.com/snakers4/silero-vad/6478567951ae5c9979ad7b234185b5515f4be7a1/src/silero_vad/data/silero_vad.onnx')

    def tts(self):
        self.archive('tts-matcha',['model-steps-3.q.onnx','vocab_tts.txt'],
            SPACEMIT+'tts/matcha-tts/matcha-icefall-zh-en.tar.gz')
        self.direct('tts-matcha','vocos-16khz-univ.q.onnx',SPACEMIT+'tts/vocoder/vocos-16khz-univ.q.onnx')
        for name,expected in FSTS.items():
            fetched=self.r.download(SPACEMIT+'tts/text_norm/v1/zh/'+name,expected)
            copy_file(fetched,self.assets/'tts-fst'/name,0o644)

    def kws(self):
        names=[p+'-epoch-12-avg-2-chunk-16-left-64.int8.onnx' for p in ('encoder','decoder','joiner')]+['tokens.txt']
        self.archive('wake-keyword',names,'https://github.com/k2-fsa/sherpa-onnx/releases/download/kws-models/'+KWS+'.tar.bz2')
        source=self.c.repo/'assets/wake/keywords-nihao-kylin.txt'
        if sha(source)!=self.expected('wake-keyword',source.name)['sha256']:raise InstallError('唤醒词清单不匹配')
        copy_file(source,self.assets/'wake-keyword'/source.name)

    def gesture_files(self):
        result=[]
        for folder in ('palm_detection_mediapipe','handpose_estimation_mediapipe'):
            name=folder+'_2023feb.onnx'
            result.append(self.direct('gesture-opencv-ort',name,
                'https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models/'+folder+'/'+name))
        return result

    def gesture(self):self.gesture_files()

    def fetch(self,component='all'):
        names=('llama','asr','tts','kws','gesture') if component=='all' else (component,)
        for name in names:
            if hasattr(self,name):
                print('\n获取模型和运行数据: '+name,flush=True);getattr(self,name)()
        if component=='all':
            item=self.c.lock['python_pptx'];self.r.download(item['url'],item['sha256'])
        json_write(self.work/'resource-receipt.json',dict(
            files={str(p.relative_to(self.assets)):sha(p) for p in self.assets.rglob('*') if p.is_file()}))

    def validate_all(self):
        for item in self.models:
            p=self.assets/item['role']/item['filename']
            if not p.is_file() or sha(p)!=item['sha256']:raise InstallError('请先完成模型下载和校验: '+str(p))
        for name,expected in FSTS.items():
            p=self.assets/'tts-fst'/name
            if not p.is_file() or sha(p)!=expected:raise InstallError('缺少TTS文本规范化数据: '+name)
