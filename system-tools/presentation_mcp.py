#!/usr/bin/env python3
"""Scoped presentation tools: basename search, page snapshots and verified notes.

The model never chooses a filesystem path, D-Bus method or executable. Page
snapshots stay bound to the reader session, original page and full text hash.
Only opening a presentation may start the already configured reader service.
"""
from __future__ import annotations

import argparse
import asyncio
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import stat
import subprocess
import sys
import time
import uuid

SERVICE='org.riscv.AIReader'
OBJECT='/org/riscv/AIReader'
INTERFACE='org.riscv.AIReader1'
EXTENSIONS={'.ppt','.pptx','.odp'}
MAX_LINE=256*1024
MAX_TEXT=12000
MAX_NOTES=1500


class PresentationError(ValueError):pass


def tool(name,description,properties):
    return {'name':name,'description':description,'inputSchema':{'type':'object',
        'properties':properties,'required':list(properties),'additionalProperties':False}}


SCHEMAS=[
    tool('open_presentation','按文件名打开允许目录里的PPT；query为空、PPT或最近:时只用最近真实打开记录，没有有效历史则明确说明，不猜选默认文件。不接收文件路径。',
        {'query':{'type':'string','maxLength':120}}),
    tool('get_current_slide','读取LibreOffice Impress真实当前页全文、已有备注及快照ID；为演讲备注保存锁定文档和页码，不做OCR。',{}),
    tool('save_speaker_notes','将AI生成的演讲备注追加到本进程快照对应的原始PPTX页面并直接保存；保留原有备注并回读校验。保存后保持普通大幻灯片视图，当前LibreOffice版本支持底部备注栏时同步显示备注。不接收路径或任意页码，不制作备份。',
        {'snapshot_id':{'type':'string','pattern':'^[0-9a-f]{32}$'},
         'notes':{'type':'string','minLength':50,'maxLength':MAX_NOTES}}),
]


def user_directory(kind,home=None):
    home=Path.home() if home is None else Path(home)
    key='XDG_DOCUMENTS_DIR' if kind=='documents' else 'XDG_DESKTOP_DIR'
    config=home/'.config/user-dirs.dirs'
    if config.is_file():
        for line in config.read_text().splitlines():
            if not line.startswith(key+'='):continue
            parts=shlex.split(line.split('=',1)[1])
            if len(parts)!=1:raise PresentationError('用户目录配置无效。')
            value=parts[0]
            if value.startswith('$HOME/'):value=str(home/value[6:])
            if '$' in value or not Path(value).is_absolute():raise PresentationError('用户目录配置含不支持的变量。')
            return Path(value)
    return home/('Documents' if kind=='documents' else 'Desktop')


def sha(text):return hashlib.sha256(text.encode('utf-8')).hexdigest()


def identity(info):return (info.st_dev,info.st_ino)


def signature(info):return (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns)


class ReaderClient:
    """Fixed session-bus API. Nothing here runs a shell or arbitrary program."""
    def __init__(self):
        self.address='unix:path=/run/user/'+str(os.getuid())+'/bus'

    async def _call(self,member,signature_,body):
        try:
            from dbus_next import Message,MessageType,MessageFlag
            from dbus_next.aio import MessageBus
        except ImportError as error:
            raise PresentationError('系统缺少python3-dbus-next，未执行PPT操作。') from error
        state_only=member=='GetState'
        bus=await asyncio.wait_for(MessageBus(bus_address=self.address).connect(),timeout=2 if state_only else 240)
        try:
            owner=None
            if state_only:
                lookup=await asyncio.wait_for(bus.call(Message(destination='org.freedesktop.DBus',
                    path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',
                    member='GetNameOwner',signature='s',body=[SERVICE],
                    flags=MessageFlag.NO_AUTOSTART)),timeout=2)
                if (lookup is None or lookup.message_type==MessageType.ERROR or len(lookup.body)!=1
                    or not isinstance(lookup.body[0],str) or not re.fullmatch(r':\d+\.\d+',lookup.body[0])):
                    raise PresentationError('没有可核实的最近PPT阅读会话。')
                owner=lookup.body[0]
            reply=await asyncio.wait_for(bus.call(Message(destination=owner or SERVICE,path=OBJECT,
                interface=INTERFACE,member=member,signature=signature_,body=body,
                flags=MessageFlag.NO_AUTOSTART)),timeout=2 if state_only else 240)
            if reply is None or reply.message_type==MessageType.ERROR:
                detail='；'.join(str(v) for v in reply.body) if reply is not None else '没有回执'
                raise PresentationError('LibreOffice Impress未完成操作：'+detail[:400])
            if owner is not None and reply.sender!=owner:
                raise PresentationError('最近PPT回执的会话身份不匹配。')
            if len(reply.body)!=1 or not isinstance(reply.body[0],str):
                raise PresentationError('LibreOffice Impress回执格式无效。')
            value=json.loads(reply.body[0])
            if not isinstance(value,dict):raise PresentationError('LibreOffice Impress回执必须为对象。')
            if owner is not None:value['reader_owner']=owner
            return value
        finally:bus.disconnect()

    def start(self):
        environment=dict(os.environ,DBUS_SESSION_BUS_ADDRESS=self.address,
                         XDG_RUNTIME_DIR='/run/user/'+str(os.getuid()))
        try:
            result=subprocess.run(['/usr/bin/systemctl','--user','start','riscv-ai-reader.service'],
                env=environment,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                text=True,timeout=330,check=False)
        except (OSError,subprocess.TimeoutExpired) as error:
            raise PresentationError('LibreOffice Impress启动失败或超时。') from error
        if result.returncode:raise PresentationError('LibreOffice Impress启动失败：'+result.stderr.strip()[:300])

    def open(self,path):
        # Starting an already active service is harmless; Type=notify waits for
        # the configured reader's readiness before the fixed API is invoked.
        self.start()
        return asyncio.run(self._call('OpenPresentation','s',[path]))

    def recent(self):
        # The managed bridge starts lazily: it restores validated history
        # metadata without opening LibreOffice or a configured presentation.
        self.start()
        return self.state()

    def state(self):
        # Read-only discovery never activates the service or trusts a status
        # file left by an older owner. Unavailability falls back to scoped search.
        try:return asyncio.run(self._call('GetState','',[]))
        except Exception as error:
            raise PresentationError('最近PPT阅读会话不可用。') from error

    def snapshot(self):return asyncio.run(self._call('GetSlideSnapshot','',[]))

    def save(self,session_id,page,text_sha256,notes):
        return asyncio.run(self._call('SaveSpeakerNotes','suss',[session_id,page,text_sha256,notes]))


class PresentationTools:
    def __init__(self,documents,desktop,demo=None,default=None,reader=None,clock=time.monotonic):
        self.roots=[];self.root_identities={}
        for candidate in (documents,desktop,demo):
            if candidate is None:continue
            root=Path(candidate)
            if not root.is_absolute() or '..' in root.parts:raise PresentationError('PPT搜索目录必须是绝对目录。')
            self._no_links(root)
            info=root.stat()
            if not stat.S_ISDIR(info.st_mode):raise PresentationError('PPT搜索目录不可用。')
            root=Path(os.path.normpath(root))
            if root not in self.roots:self.roots.append(root);self.root_identities[root]=identity(info)
        if not self.roots:raise PresentationError('没有配置PPT搜索目录。')
        # Retain the legacy argument for callers, without selecting or even
        # requiring that old configured file to exist.
        self.default=Path(default) if default else None
        self.reader=reader or ReaderClient();self.clock=clock;self.snapshots={}

    @staticmethod
    def _no_links(path):
        current=Path(path.anchor)
        for part in path.parts[1:]:
            current=current/part
            if stat.S_ISLNK(current.lstat().st_mode):raise PresentationError('PPT路径不能包含符号链接。')

    def _check_roots(self):
        for root in self.roots:
            self._no_links(root)
            info=root.stat()
            if not stat.S_ISDIR(info.st_mode) or identity(info)!=self.root_identities[root]:
                raise PresentationError('PPT搜索目录发生改变，请重新启动工具。')

    def _allowed(self,path):
        self._check_roots()
        path=Path(path)
        if not path.is_absolute() or '..' in path.parts or path!=Path(os.path.normpath(path)):
            raise PresentationError('PPT路径无效。')
        if not any(path.is_relative_to(root) for root in self.roots):
            raise PresentationError('PPT不在已配置的文档、桌面或演示目录中。')
        self._no_links(path);info=path.stat()
        if not stat.S_ISREG(info.st_mode) or path.suffix.casefold() not in EXTENSIONS:
            raise PresentationError('该路径不是支持的PPT/PPTX/ODP普通文件。')
        if not 0<info.st_size<=512*1024*1024:raise PresentationError('演示文件为空或超过512MB上限。')
        return path

    def _files(self):
        self._check_roots();found=set();seen=0
        for root in self.roots:
            for directory,folders,files in os.walk(root,followlinks=False):
                folders[:]=sorted(name for name in folders if not name.startswith('.') and not (Path(directory)/name).is_symlink())
                for name in files:
                    seen+=1
                    if seen>5000:raise PresentationError('搜索目录文件过多，请缩小已配置的PPT目录。')
                    path=Path(directory)/name
                    if name.startswith('.') or path.is_symlink() or path.suffix.casefold() not in EXTENSIONS:continue
                    found.add(self._allowed(path))
        return sorted(found,key=lambda p:str(p).casefold())

    def schemas(self):return copy.deepcopy(SCHEMAS)

    def call(self,name,arguments):
        fields={'open_presentation':{'query'},'get_current_slide':set(),
                'save_speaker_notes':{'snapshot_id','notes'}}
        if name not in fields or not isinstance(arguments,dict) or set(arguments)!=fields[name]:
            raise PresentationError('PPT工具名称或参数无效。')
        method={'open_presentation':self.open,'get_current_slide':self.current,
                'save_speaker_notes':self.save}[name]
        return json.dumps(method(**arguments),ensure_ascii=False,separators=(',',':'))

    def _recent(self):
        try:
            value=self.reader.recent()
            if (not isinstance(value,dict) or type(value.get('active')) is not bool
                or not isinstance(value.get('reader_owner'),str)
                or not re.fullmatch(r':\d+\.\d+',value['reader_owner'])
                or not isinstance(value.get('session_id'),str) or not 0<len(value['session_id'])<=128
                or not isinstance(value.get('source_path'),str)
                or type(value.get('page')) is not int or type(value.get('total')) is not int):
                return None
            if value['active']:
                if value.get('mode') not in ('document','notes','slideshow') or not 1<=value['page']<=value['total']:
                    return None
            elif value.get('mode')!='closed' or value['page']!=0 or value['total']!=0:
                return None
            target=self._allowed(Path(value['source_path']))
            return target,'reader_current' if value['active'] else 'reader_recent'
        except (PresentationError,OSError,ValueError,TypeError,RuntimeError,TimeoutError):
            return None

    def open(self,query):
        if not isinstance(query,str) or len(query)>120:
            raise PresentationError('请提供演示文稿名称或关键词，不能提供文件路径。')
        query=query.strip()
        if query=='最近:':query=''
        if re.search(r'[/\\:~\x00-\x1f]',query) or '..' in query:
            raise PresentationError('请提供演示文稿名称或关键词，不能提供文件路径。')
        self._check_roots()
        generic=query.casefold() in ('','ppt','pptx','演示文稿','默认ppt','默认演示文稿')
        if generic:
            selected=self._recent()
            if selected is None:
                return {'ok':False,'opened':False,'reason':'no_recent',
                        'message':'没有可用的最近打开记录，请先打开一次PPT。'}
            target,selection_reason=selected
        else:
            files=self._files()
            exact=[p for p in files if query.casefold() in (p.name.casefold(),p.stem.casefold())]
            matches=exact or [p for p in files if query.casefold() in p.stem.casefold()]
            if not matches:raise PresentationError('未找到匹配的演示文稿，请说出文档或桌面里的PPT名称。')
            if len(matches)>1:raise PresentationError('找到多份匹配的演示文稿，请补充明确名称。')
            target=matches[0]
            selection_reason='exact_name' if exact else 'unique_name_match'
        target=self._allowed(target);selected_signature=signature(target.stat())
        value=self.reader.open(str(target))
        if (value.get('active') is not True or value.get('source_path')!=str(target)
            or not isinstance(value.get('session_id'),str) or not value['session_id']
            or type(value.get('page')) is not int or type(value.get('total')) is not int
            or not 1<=value['page']<=value['total']
            or signature(self._allowed(target).stat())!=selected_signature):
            raise PresentationError('未确认LibreOffice Impress已打开目标PPT或文件身份已改变。')
        return {'ok':True,'opened':True,'path':str(target),'page':value['page'],
                'total':value['total'],'session_id':value['session_id'],
                'selection_reason':selection_reason}

    def current(self):
        value=self.reader.snapshot()
        if (value.get('ok') is not True or not isinstance(value.get('session_id'),str) or not value['session_id']
            or not isinstance(value.get('source_path'),str) or type(value.get('page')) is not int
            or type(value.get('total')) is not int or not 1<=value['page']<=value['total']
            or not isinstance(value.get('text'),str) or not value['text'].strip()
            or len(value['text'])>MAX_TEXT or value.get('text_sha256')!=sha(value['text'])
            or not isinstance(value.get('notes'),str) or len(value['notes'])>20000
            or not all(isinstance(value.get(key),str) for key in ('title','page_title'))):
            raise PresentationError('当前页快照无效、正文为空或超过12000字，未生成备注。')
        path=self._allowed(Path(value['source_path']))
        now=self.clock()
        self.snapshots={k:s for k,s in self.snapshots.items() if s['expires']>now}
        if len(self.snapshots)>=32:raise PresentationError('当前页快照过多，请稍后重试。')
        snapshot_id=uuid.uuid4().hex
        self.snapshots[snapshot_id]={'value':copy.deepcopy(value),'signature':signature(path.stat()),
            'expires':now+3600,'attempted':False,'saved':None,'notes':None}
        return dict(value,snapshot_id=snapshot_id,tool='ppt__get_current_slide')

    def save(self,snapshot_id,notes):
        if not isinstance(snapshot_id,str) or not re.fullmatch(r'[0-9a-f]{32}',snapshot_id):
            raise PresentationError('当前页快照ID无效。')
        if (not isinstance(notes,str) or not 50<=len(notes)<=MAX_NOTES or not notes.strip()
            or '\x00' in notes or any(ord(c)<32 and c not in '\n\t\r' for c in notes)):
            raise PresentationError('演讲备注须为50至1500字的有效文本。')
        state=self.snapshots.get(snapshot_id)
        if state is None or state['expires']<=self.clock():raise PresentationError('当前页快照不存在或已过期，请重新生成。')
        if state['attempted']:
            if state['saved'] is not None and state['notes']==notes:return copy.deepcopy(state['saved'])
            raise PresentationError('该备注保存已尝试，未自动重复写入；请先核对PPT后重新生成。')
        source=state['value'];path=self._allowed(Path(source['source_path']))
        if path.suffix.casefold()!='.pptx':raise PresentationError('演讲备注保存目前仅支持PPTX，请先另存为PPTX。')
        if signature(path.stat())!=state['signature']:raise PresentationError('PPT文件在生成备注期间已改变，未保存，请重新生成。')
        state['attempted']=True;state['notes']=notes
        value=self.reader.save(source['session_id'],source['page'],source['text_sha256'],notes)
        final=value.get('notes')
        if (value.get('ok') is not True or value.get('verified') is not True
            or type(value.get('notes_visible')) is not bool or type(value.get('notes_pane_supported')) is not bool
            or value['notes_visible'] is not value['notes_pane_supported']
            or value.get('display_mode')!='normal' or value.get('normal_view_visible') is not True
            or value.get('path')!=str(path) or type(value.get('page')) is not int or value['page']!=source['page']
            or not isinstance(final,str) or not final.endswith(notes)
            or source['notes'] and not final.startswith(source['notes'])
            or value.get('notes_sha256')!=sha(final)):
            raise PresentationError('备注可能已写入，但未通过保存回读确认；已停止，不会自动重复保存。')
        result=dict(value,snapshot_id=snapshot_id,tool='ppt__save_speaker_notes')
        state['saved']=copy.deepcopy(result)
        return result


def serve(tools):
    while True:
        line=sys.stdin.readline(MAX_LINE+1)
        if not line:return
        if len(line)>MAX_LINE:
            while line and not line.endswith('\n'):line=sys.stdin.readline(MAX_LINE+1)
            print(json.dumps({'jsonrpc':'2.0','id':None,'error':{'code':-32600,'message':'请求过大。'}}),flush=True)
            continue
        request_id=None;message=None
        try:
            message=json.loads(line)
            if not isinstance(message,dict) or message.get('jsonrpc')!='2.0':raise PresentationError('请求格式无效。')
            request_id=message.get('id')
            if request_id is None:continue
            if type(request_id) not in (str,int):raise PresentationError('请求ID无效。')
            method=message.get('method');params=message.get('params',{})
            if method=='initialize':result={'protocolVersion':'2024-11-05','capabilities':{'tools':{}},'serverInfo':{'name':'kylin-presentation-tools','version':'0.1.0'}}
            elif method=='tools/list':result={'tools':tools.schemas()}
            elif method=='ping':result={}
            elif method=='tools/call':
                if not isinstance(params,dict) or set(params)-{'name','arguments'}:raise PresentationError('工具调用格式无效。')
                result={'content':[{'type':'text','text':tools.call(params.get('name'),params.get('arguments',{}))}],'isError':False}
            else:raise PresentationError('未知方法。')
            reply={'jsonrpc':'2.0','id':request_id,'result':result}
        except (PresentationError,OSError,ValueError,TypeError,KeyError,RuntimeError,TimeoutError) as error:
            if isinstance(message,dict) and message.get('method')=='tools/call':
                reply={'jsonrpc':'2.0','id':request_id,'result':{'content':[{'type':'text','text':str(error)[:500]}],'isError':True}}
            else:reply={'jsonrpc':'2.0','id':request_id,'error':{'code':-32010,'message':str(error)[:500]}}
        print(json.dumps(reply,ensure_ascii=False),flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--documents-root');parser.add_argument('--desktop-root')
    parser.add_argument('--demo-root');parser.add_argument('--default-presentation')
    options=parser.parse_args()
    documents=options.documents_root or user_directory('documents')
    desktop=options.desktop_root or user_directory('desktop')
    demo=Path(options.demo_root) if options.demo_root else Path.home()/'scenario5-20261028'
    if not demo.exists() and not options.demo_root:demo=None
    default=options.default_presentation
    serve(PresentationTools(documents,desktop,demo,default))


if __name__=='__main__':main()
