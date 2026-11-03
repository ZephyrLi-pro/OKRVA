#!/usr/bin/env python3
"""Native-text document MCP. Model never supplies arbitrary filesystem paths.

Reads regular documents below the configured Documents root, saves new reports
only below Desktop, never follows source symlinks or overwrites existing files.
No inference or OCR lives here. Native text extraction reuses DocumentReader.
"""
from __future__ import annotations
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shlex
import signal
import stat
import subprocess
import sys
import tempfile
import time
import uuid
from urllib.parse import unquote
import configparser
import shutil

EXTENSIONS={'.txt','.md','.pdf','.doc','.docx','.wps','.ppt','.pptx'}
MAX_BYTES=20*1024*1024
MAX_TEXT=24000
MAX_LINE=256*1024
REPORT_NAME=re.compile(r'文档摘要-(?:[A-Za-z0-9_\-\u4e00-\u9fff]{1,40}-)?[0-9]{8}-[0-9]{6}-[0-9a-f]{8}\.md')
REPORT_HEADER='# 最近文档摘要\n摘要方式：模型选择原文关键句；不含OCR。\n'.encode()


class DocumentError(ValueError):pass


def tool(name,description,properties):
    return {'name':name,'description':description,'inputSchema':{'type':'object',
        'properties':properties,'required':list(properties),'additionalProperties':False}}


SCHEMAS=[
    tool('recent_documents','按真实修改时间查找文档目录内最近的N份文件；返回来源ID及时间。不支持的格式明确报错，不跳过较新的文件。',
         {'directory':{'type':'string','enum':['documents']},'count':{'type':'integer','minimum':1,'maximum':5},
          'order':{'type':'string','enum':['modified_desc']}}),
    tool('read_document','读取本次recent_documents返回的文档ID；复用DocumentReader提取TXT/MD/PDF/Office原生文本，不做OCR。',
         {'batch_id':{'type':'string'},'document_id':{'type':'string'}}),
    tool('save_report','将本批次每份文档的摘要分别保存为桌面上的独立Markdown文件；一份原文对应一个文件，逐份回读校验，不覆盖文件。',
         {'batch_id':{'type':'string'},'entries':{'type':'array','minItems':1,'maxItems':5,'items':{
             'type':'object','properties':{'document_id':{'type':'string'},'points':{'type':'array','minItems':1,'maxItems':3,'items':{'type':'string'}}},
             'required':['document_id','points'],'additionalProperties':False}}}),
]
SCHEMAS += [
    tool('list_reports','只读列出桌面由助手生成的摘要文件及来源ID；不递归，不包括其他文档或链接。',{}),
    tool('trash_reports','将本次list_reports选中的摘要ID移入回收站并验证；不能永久删除，不能操作任意路径。',
         {'selection_id':{'type':'string'},'report_ids':{'type':'array','minItems':1,'maxItems':20,'items':{'type':'string'}}}),
]


def user_directory(kind,home=None):
    home=Path.home() if home is None else Path(home)
    config=home/'.config/user-dirs.dirs'
    key='XDG_DOCUMENTS_DIR' if kind=='documents' else 'XDG_DESKTOP_DIR'
    if config.is_file():
        for line in config.read_text().splitlines():
            if line.startswith(key+'='):
                parts=shlex.split(line.split('=',1)[1])
                if len(parts)!=1:raise DocumentError('用户目录配置无效。')
                value=parts[0]
                if value.startswith('$HOME/'):value=str(home/value[6:])
                if '$' in value or not Path(value).is_absolute():raise DocumentError('用户目录配置含不支持的变量。')
                return Path(value).resolve(strict=True)
    return (home/('Documents' if kind=='documents' else 'Desktop')).resolve(strict=True)


def signature(info):return (info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns)


def normalize_native_text(text):
    """Join soft-wrapped native text, preserving paragraphs and slide metadata."""
    result=[];pending=''
    def flush():
        nonlocal pending
        if pending:result.append(pending);pending=''
    for raw in text.splitlines():
        line=raw.strip()
        if not line:flush();result.append('');continue
        if re.fullmatch(r'===== Slide [0-9]+ =====',line):flush();result.append(line);continue
        bullet=re.match(r'^[•●▪‣]\s*',line)
        if bullet:flush();pending=line[bullet.end():];continue
        if pending and not re.search(r'[。！？!?；;.]$',pending):
            separator='' if re.search(r'[\u4e00-\u9fff]$',pending) and re.match(r'[\u4e00-\u9fff]',line) else ' '
            pending+=separator+line
        else:flush();pending=line
    flush()
    return '\n'.join(result).strip()


def extract_native(reader,snapshot):
    process=subprocess.Popen([reader,str(snapshot)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True)
    try:stdout,_=process.communicate(timeout=250)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid,signal.SIGKILL);process.communicate()
        raise DocumentError('文档原生文本提取超时。')
    value=json.loads(stdout)
    if process.returncode or value.get('ok') is not True:raise DocumentError('没有提取到可用的原生文本：'+str(value.get('error','未知错误'))[:300])
    return value


class DocumentTools:
    def __init__(self,documents,desktop,reader,clock=time.monotonic,trash_data_home=None):
        self.documents=Path(documents).resolve(strict=True)
        self.desktop=Path(desktop).resolve(strict=True)
        if not self.documents.is_dir() or not self.desktop.is_dir():raise DocumentError('文档/桌面目录不可用。')
        self.reader=str(Path(reader).resolve())
        self.desktop_identity=signature(self.desktop.stat())[:2]
        self.clock=clock;self.batches={}
        self.report_selections={}
        self.data_home=Path(trash_data_home or os.environ.get('XDG_DATA_HOME',str(Path.home()/'.local/share'))).absolute()

    def schemas(self):return SCHEMAS

    def call(self,name,args):
        definition=next((s for s in SCHEMAS if s['name']==name),None)
        if definition is None or not isinstance(args,dict) or set(args)!=set(definition['inputSchema']['properties']):
            raise DocumentError('工具名或参数字段无效。')
        if name=='recent_documents':result=self.recent(**args)
        elif name=='read_document':result=self.read(**args)
        elif name=='save_report':result=self.save(**args)
        elif name=='list_reports':result=self.list_reports()
        else:result=self.trash_reports(**args)
        return json.dumps(result,ensure_ascii=False)

    def report_info(self,name):
        if not isinstance(name,str) or not REPORT_NAME.fullmatch(name):raise DocumentError('摘要文件名不在支持范围。')
        if signature(self.desktop.stat())[:2]!=self.desktop_identity or self.desktop.is_symlink():raise DocumentError('桌面目录已变化。')
        path=self.desktop/name
        descriptor=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
        try:
            info=os.fstat(descriptor)
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.geteuid() or info.st_size>128*1024:raise DocumentError('文件不是助手生成的普通摘要。')
            with os.fdopen(descriptor,'rb',closefd=False) as source:data=source.read(128*1024+1)
            if not data.startswith(REPORT_HEADER) or signature(os.fstat(descriptor))!=signature(info):raise DocumentError('摘要文件格式或内容已变化。')
            return {'signature':signature(info),'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),'modified_ns':info.st_mtime_ns}
        finally:os.close(descriptor)

    def list_reports(self):
        rows=[]
        # No source-document reads; only bounded summary headers/bytes are used
        # to identify the assistant's own report format and bind later effects.
        for path in self.desktop.iterdir():
            if not REPORT_NAME.fullmatch(path.name) or path.is_symlink() or not path.is_file():continue
            try:info=self.report_info(path.name)
            except DocumentError:continue
            rows.append({'report_id':uuid.uuid4().hex,'name':path.name,**info})
            if len(rows)>100:raise DocumentError('桌面摘要超过100份，请先手动整理；没有返回不完整清单。')
        rows.sort(key=lambda r:(-r['modified_ns'],r['name']))
        self.report_selections={k:v for k,v in self.report_selections.items() if v['expires']>self.clock()}
        if len(self.report_selections)>=32:raise DocumentError('清理任务过多，请稍后重试。')
        selection_id=uuid.uuid4().hex
        self.report_selections[selection_id]={'rows':rows,'expires':self.clock()+600,'attempted':False}
        return {'selection_id':selection_id,'reports':[{k:v for k,v in row.items() if k not in ('signature','sha256')} for row in rows]}

    def trash_reports(self,selection_id,report_ids):
        selection=self.report_selections.get(selection_id) if isinstance(selection_id,str) else None
        if selection is None or selection['expires']<=self.clock() or selection['attempted']:raise DocumentError('清理清单不存在、已过期或已经执行；未重复操作。')
        if (not isinstance(report_ids,list) or not 1<=len(report_ids)<=20 or
            any(not isinstance(i,str) for i in report_ids) or len(set(report_ids))!=len(report_ids)):
            raise DocumentError('摘要选择无效。')
        by_id={row['report_id']:row for row in selection['rows']}
        if any(i not in by_id for i in report_ids):raise DocumentError('摘要ID不属于本次清单。')
        rows=[by_id[i] for i in report_ids]
        for row in rows:
            current=self.report_info(row['name'])
            if current['signature']!=row['signature'] or current['sha256']!=row['sha256']:raise DocumentError('摘要已改变，请重新列出后再选择；尚未移动文件。')
        gio=shutil.which('gio')
        if not gio:raise DocumentError('系统未安装gio回收站工具；不会改用永久删除。')
        trash=self.data_home/'Trash';infos=trash/'info';files=trash/'files'
        # GIO's home Trash gives verifiable originals and makes normal desktop
        # restore work. Refuse cross-filesystem Trash rather than guess a mount.
        if not self.data_home.exists() or self.data_home.stat().st_dev!=self.desktop.stat().st_dev:
            raise DocumentError('桌面与用户回收站不在同一文件系统，暂未接入该清理方式。')
        selection['attempted']=True;confirmed=[];failure=False
        for row in rows:
            try:
                current=self.report_info(row['name'])
                if current['signature']!=row['signature'] or current['sha256']!=row['sha256']:raise DocumentError('摘要已改变。')
                known=set(infos.iterdir()) if infos.is_dir() else set()
                path=self.desktop/row['name']
                result=subprocess.run([gio,'trash','--',str(path)],capture_output=True,timeout=25,
                    env=dict(os.environ,XDG_DATA_HOME=str(self.data_home)))
                if result.returncode or path.exists() or path.is_symlink():raise DocumentError('回收站移动未确认。')
                matches=[]
                for item in infos.iterdir():
                    if item in known or item.suffix!='.trashinfo' or item.is_symlink():continue
                    config=configparser.ConfigParser(interpolation=None);config.read(item,encoding='utf-8')
                    original=unquote(config.get('Trash Info','Path',fallback=''))
                    target=files/item.name[:-10]
                    if original==str(path) and not target.is_symlink() and target.is_file():
                        info=target.stat()
                        if info.st_size==row['size'] and hashlib.sha256(target.read_bytes()).hexdigest()==row['sha256']:
                            matches.append(item.name[:-10])
                if len(matches)!=1:raise DocumentError('回收站内容回读未确认。')
                confirmed.append({'report_id':row['report_id'],'name':row['name'],'trash_name':matches[0]})
            except (DocumentError,OSError,ValueError,configparser.Error,subprocess.TimeoutExpired):failure=True;break
        message=(f'已确认将{len(confirmed)}份摘要移入回收站，可在回收站恢复。' if confirmed else '')
        if failure:message+='本次清理未全部确认，已停止；没有自动重复执行，也没有永久删除。'
        return {'selection_id':selection_id,'verified':not failure,'requested':len(rows),'moved':confirmed,
            'count':len(confirmed),'message':message}

    def recent(self,directory,count,order):
        if directory!='documents' or order!='modified_desc' or type(count) is not int or not 1<=count<=5:
            raise DocumentError('文档查询参数无效。')
        found=[];visited=0;directories=0
        def inaccessible(_error):raise DocumentError('有子目录无法读取，不能保证最近文件列表完整。')
        for base,dirs,files in os.walk(self.documents,followlinks=False,onerror=inaccessible):
            directories+=1
            if directories>10000 or len(Path(base).relative_to(self.documents).parts)>32:raise DocumentError('文档目录范围过大，未返回不完整列表。')
            dirs[:]=[n for n in dirs if not n.startswith('.') and not (Path(base)/n).is_symlink()]
            for name in files:
                visited+=1
                if visited>10000:raise DocumentError('目录文件超过10000份，请缩小范围；没有返回不完整的最近列表。')
                path=Path(base)/name
                if name.startswith('.'):continue
                info=path.lstat()
                if stat.S_ISREG(info.st_mode):
                    found.append((path,info))
        found.sort(key=lambda row:(-row[1].st_mtime_ns,str(row[0].relative_to(self.documents))))
        if len(found)<count:raise DocumentError(f'符合条件的文档只有{len(found)}份，少于请求数量；尚未保存摘要。')
        self.batches={key:b for key,b in self.batches.items() if b['expires']>self.clock()}
        if len(self.batches)>=16:raise DocumentError('文档任务太多，请稍后再试。')
        batch_id=uuid.uuid4().hex
        rows=[dict(document_id=uuid.uuid4().hex,path=str(p),name=str(p.relative_to(self.documents)),
                   modified_ns=info.st_mtime_ns,size=info.st_size,signature=signature(info)) for p,info in found[:count]]
        self.batches[batch_id]=dict(expires=self.clock()+3600,rows=rows,read={},saved=None)
        return {'batch_id':batch_id,'documents':[{k:v for k,v in row.items() if k!='signature'} for row in rows]}

    def batch(self,batch_id):
        if not isinstance(batch_id,str) or batch_id not in self.batches:raise DocumentError('文档批次不存在。')
        result=self.batches[batch_id]
        if result['expires']<=self.clock():raise DocumentError('文档批次已过期。')
        return result

    def source(self,row):
        path=Path(row['path'])
        if path.is_symlink() or not path.resolve(strict=True).is_relative_to(self.documents):raise DocumentError('文档路径发生变化。')
        descriptor=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
        try:
            info=os.fstat(descriptor)
            if not stat.S_ISREG(info.st_mode) or signature(info)!=tuple(row['signature']):raise DocumentError('文档已修改，请重新查找；尚未保存摘要。')
            if info.st_size>MAX_BYTES:raise DocumentError('最近文件超过20MiB，未跳过它并用旧文件代替；本批次尚未保存。')
            with os.fdopen(descriptor,'rb',closefd=False) as source:data=source.read(MAX_BYTES+1)
            if signature(os.fstat(descriptor))!=tuple(row['signature']):raise DocumentError('读取期间文档发生变化。')
            return data
        finally:os.close(descriptor)

    def read(self,batch_id,document_id):
        batch=self.batch(batch_id)
        row=next((r for r in batch['rows'] if r['document_id']==document_id),None)
        if row is None:raise DocumentError('文档ID不属于本批次。')
        data=self.source(row)
        suffix=Path(row['path']).suffix.lower()
        if suffix not in EXTENSIONS:raise DocumentError('最近文件的类型不支持原生文本提取：'+suffix+'；本批次尚未保存摘要。')
        if suffix in ('.txt','.md'):
            try:text=data.decode('utf-8-sig')
            except UnicodeError:raise DocumentError('文本文件不是UTF-8编码，不能将乱码当作正文。')
            pages=0
        else:
            with tempfile.TemporaryDirectory(prefix='kylin-document-') as directory:
                snapshot=Path(directory)/('input'+suffix);snapshot.write_bytes(data)
                try:
                    value=extract_native(self.reader,snapshot)
                except (OSError,ValueError):raise DocumentError('文档原生文本提取失败或超时。')
                if not isinstance(value.get('text'),str):raise DocumentError('提取器没有返回有效正文。')
                text=normalize_native_text(value.get('text',''));pages=value.get('page_count',0)
        if not isinstance(text,str) or not text.strip() or len(text)>MAX_TEXT or '\x00' in text:
            raise DocumentError(f'正文为空、无效或超过{MAX_TEXT}字符；未截断并伪装成完整摘要。')
        # Recheck the real document after extracting the private snapshot.
        self.source(row)
        batch['read'][document_id]=text
        return {'batch_id':batch_id,'document_id':document_id,'name':row['name'],
                'modified_ns':row['modified_ns'],'text':text,'page_count':pages}

    def save(self,batch_id,entries):
        batch=self.batch(batch_id)
        if batch['saved'] is not None:raise DocumentError('本批次已保存，禁止重复写入。')
        if not isinstance(entries,list) or len(entries)!=len(batch['rows']):raise DocumentError('摘要数量与实际文档不符。')
        prepared=[]
        for entry,row in zip(entries,batch['rows']):
            if not isinstance(entry,dict) or set(entry)!={'document_id','points'} or entry['document_id']!=row['document_id']:
                raise DocumentError('摘要顺序或来源ID不符。')
            text=batch['read'].get(row['document_id']);points=entry['points']
            if text is None or not isinstance(points,list) or not 1<=len(points)<=3 or any(not isinstance(p,str) for p in points):
                raise DocumentError('文档未读取或摘要要点无效。')
            if len(set(points))!=len(points) or any(not point.strip() or len(point)>1200 or point not in text for point in points):
                raise DocumentError('摘要要点不在原文中，禁止保存未经核实的内容。')
            self.source(row)
            title=html.escape(row['name'].replace('\n',' ').replace('\r',' '))
            blocks=['# 最近文档摘要','摘要方式：模型选择原文关键句；不含OCR。','','## '+title,
                '来源：'+html.escape(row['path'].replace('\n',' ').replace('\r',' ')),
                '修改时间：'+time.strftime('%Y-%m-%d %H:%M:%S %z',time.localtime(row['modified_ns']/1e9)),'']
            blocks += ['- '+html.escape(point.replace('\n',' ')) for point in points]+['']
            prepared.append((row,('\n'.join(blocks)+'\n').encode()))
        directory_fd=os.open(self.desktop,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        reports=[];created=[];failure=False
        try:
            if signature(os.fstat(directory_fd))[:2]!=self.desktop_identity:raise DocumentError('桌面目录已变化。')
            # After the first effect can begin, every retry is rejected, even
            # if a later file fails. Keep partial files and truthful receipts.
            batch['saved']=reports
            for row,data in prepared:
                try:
                    self.source(row)
                    stem=re.sub(r'[^A-Za-z0-9_\-\u4e00-\u9fff]+','_',Path(row['name']).stem).strip('_-')[:40] or '文档'
                    target=self.desktop/('文档摘要-'+stem+'-'+time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8]+'.md')
                    descriptor=os.open(target.name,os.O_CREAT|os.O_EXCL|os.O_WRONLY|os.O_NOFOLLOW,0o600,dir_fd=directory_fd)
                    created.append(str(target))
                    with os.fdopen(descriptor,'wb') as out:out.write(data);out.flush();os.fsync(out.fileno())
                    os.fsync(directory_fd)
                    descriptor=os.open(target.name,os.O_RDONLY|os.O_NOFOLLOW,dir_fd=directory_fd)
                    with os.fdopen(descriptor,'rb') as source:verified=source.read()
                    if verified!=data:raise DocumentError('摘要回读不一致。')
                    reports.append({'document_id':row['document_id'],'source_name':row['name'],'path':str(target),
                        'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'verified':True})
                except (DocumentError,OSError):failure=True;break
            if not failure:
                try:
                    for row in batch['rows']:self.source(row)
                except (DocumentError,OSError):failure=True
        finally:os.close(directory_fd)
        complete=not failure and len(reports)==len(entries)
        message=(f'已分别生成{len(reports)}个独立摘要文件，保存到桌面，每份原文对应一个文件，均已回读确认。' if complete else
            f'已确认保存{len(reports)}个独立摘要文件；本批次未全部完成，部分文件可能已写入但未确认。已停止，没有自动重复保存。')
        return {'layout':'one_file_per_document','batch_id':batch_id,'expected_count':len(entries),
            'reports':reports,'created_paths':created,'count':len(reports),'verified':complete,'message':message}


def serve(tools):
    while True:
        line=sys.stdin.readline(MAX_LINE+1)
        if not line:return
        if len(line)>MAX_LINE:
            while line and not line.endswith('\n'):line=sys.stdin.readline(MAX_LINE+1)
            print(json.dumps({'jsonrpc':'2.0','id':None,'error':{'code':-32600,'message':'请求过大。'}}),flush=True)
            continue
        request_id=None
        try:
            message=json.loads(line)
            if not isinstance(message,dict) or message.get('jsonrpc')!='2.0':raise DocumentError('请求格式无效。')
            request_id=message.get('id')
            if request_id is None:continue
            if type(request_id) not in (str,int):raise DocumentError('请求ID无效。')
            method=message.get('method');params=message.get('params',{})
            if method=='initialize':result={'protocolVersion':'2024-11-05','capabilities':{'tools':{}},'serverInfo':{'name':'kylin-document-tools','version':'0.3.0'}}
            elif method=='tools/list':result={'tools':tools.schemas()}
            elif method=='ping':result={}
            elif method=='tools/call':
                if not isinstance(params,dict) or set(params)-{'name','arguments'}:raise DocumentError('工具调用格式无效。')
                text=tools.call(params.get('name'),params.get('arguments',{}))
                result={'content':[{'type':'text','text':text}],'isError':False}
            else:raise DocumentError('未知方法。')
            reply={'jsonrpc':'2.0','id':request_id,'result':result}
        except (DocumentError,OSError,ValueError,TypeError,KeyError) as error:
            reply={'jsonrpc':'2.0','id':request_id,'error':{'code':-32010,'message':str(error)[:500]}}
        print(json.dumps(reply,ensure_ascii=False),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--documents-root');parser.add_argument('--desktop-root')
    parser.add_argument('--reader-cli',required=True);options=parser.parse_args()
    serve(DocumentTools(options.documents_root or user_directory('documents'),
        options.desktop_root or user_directory('desktop'),options.reader_cli))
