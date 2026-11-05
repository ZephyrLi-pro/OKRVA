"""Shared deployment operations. No commands run at import time."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shlex
import shutil
import subprocess
import tarfile
import tempfile
import time
import zipfile


class InstallError(RuntimeError):
    pass


def sha(path: Path) -> str:
    digest=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(4*1024*1024),b''):digest.update(chunk)
    return digest.hexdigest()


def atomic_write(path: Path, data: str | bytes, mode=0o644):
    path.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent,prefix='.'+path.name+'-',delete=False) as f:
        temporary=Path(f.name)
        os.fchmod(f.fileno(),mode)
        f.write(data.encode() if isinstance(data,str) else data)
        f.flush();os.fsync(f.fileno())
    os.replace(temporary,path)


def json_write(path, value, mode=0o600):
    atomic_write(path,json.dumps(value,ensure_ascii=False,indent=2)+'\n',mode)


def relative_path(value: str) -> Path:
    path=PurePosixPath(value)
    if not path.parts or path.is_absolute() or '..' in path.parts:
        raise InstallError('非法相对路径: '+value)
    return Path(*path.parts)


def validate_archive(path: Path, destination: Path):
    """Inspect every member before extracting any file, including link targets."""
    with tarfile.open(path) as archive:
        members=archive.getmembers()
        base=destination.resolve()
        for member in members:
            rel=relative_path(member.name)
            target=(base/rel).resolve()
            if not target.is_relative_to(base):raise InstallError('压缩包路径越界')
            if member.isdev() or member.isfifo():raise InstallError('压缩包含设备文件')
            if member.issym() or member.islnk():
                link=Path(member.linkname)
                origin=target.parent if member.issym() else base
                if link.is_absolute() or not (origin/link).resolve().is_relative_to(base):
                    raise InstallError('压缩包链接越界')
        return members


def unpack(path: Path, destination: Path):
    validate_archive(path,destination)
    if destination.exists():raise InstallError('解包目标已存在: '+str(destination))
    destination.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.unpack-',dir=destination.parent) as tmp:
        staged=Path(tmp)/'data';staged.mkdir()
        with tarfile.open(path) as archive:archive.extractall(staged,filter='data')
        staged.rename(destination)


def unpack_zip(path: Path,destination: Path):
    with zipfile.ZipFile(path) as archive:
        members=archive.infolist()
        for member in members:
            rel=relative_path(member.filename)
            if not (destination/rel).resolve().is_relative_to(destination.resolve()):
                raise InstallError('ZIP路径越界')
            if (member.external_attr >> 16) & 0o170000 == 0o120000:raise InstallError('ZIP不能包含符号链接')
        destination.mkdir(parents=True,exist_ok=True)
        archive.extractall(destination)


class Runner:
    def __init__(self, work: Path, dry=False):
        self.work=work;self.dry=dry;self.counter=0

    def run(self,args,*,cwd=None,env=None,label='command',capture=False):
        args=[str(x) for x in args]
        print('+ '+shlex.join(args),flush=True)
        if self.dry:return ''
        if capture:
            value=subprocess.run(args,cwd=cwd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if value.returncode:raise InstallError(f'{label} 失败: {value.stderr[-2500:]}')
            return value.stdout
        logs=self.work/'logs';logs.mkdir(parents=True,exist_ok=True)
        self.counter+=1;log=logs/f'{time.time_ns()}-{self.counter}-{label}.log'
        with log.open('w') as out:
            process=subprocess.Popen(args,cwd=cwd,env=env,stdout=out,stderr=subprocess.STDOUT)
            while True:
                try:code=process.wait(timeout=20);break
                except subprocess.TimeoutExpired:print(f'  {label} 仍在运行，日志: {log}',flush=True)
                except KeyboardInterrupt:
                    process.terminate()
                    try:process.wait(timeout=10)
                    except subprocess.TimeoutExpired:process.kill();process.wait()
                    raise
        if code:
            tail='\n'.join(log.read_text(errors='replace').splitlines()[-25:])
            raise InstallError(f'{label} 失败，退出码{code}\n{tail}\n完整日志: {log}')
        return ''

    def download(self,url,expected=None):
        if not url.startswith('https://'):raise InstallError('下载必须使用HTTPS')
        # Keep resumable files isolated by URL, even if basenames are equal.
        key=hashlib.sha256(url.encode()).hexdigest()[:12]
        name=url.split('?',1)[0].rsplit('/',1)[1]
        target=self.work/'downloads'/(key+'-'+name)
        if self.dry:
            print('下载并校验: '+url);return target
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.is_file():
            if expected and sha(target)!=expected:raise InstallError('下载缓存校验失败: '+str(target))
            return target
        partial=target.with_name(target.name+'.part')
        self.run(['curl','--fail','--location','--retry','3','--retry-delay','2',
                  '--connect-timeout','20','--continue-at','-','--output',partial,url],label='download')
        if expected and sha(partial)!=expected:raise InstallError('下载SHA256不匹配: '+str(partial))
        os.replace(partial,target)
        return target


def copy_file(source: Path,destination: Path,mode=None):
    destination.parent.mkdir(parents=True,exist_ok=True)
    if destination.is_file() and sha(destination)==sha(source):return
    with tempfile.NamedTemporaryFile(dir=destination.parent,prefix='.'+destination.name+'-',delete=False) as f:
        temporary=Path(f.name)
    shutil.copyfile(source,temporary)
    temporary.chmod(mode if mode is not None else source.stat().st_mode & 0o777)
    os.replace(temporary,destination)


def install_model(source: Path,destination: Path,expected: str):
    if sha(source)!=expected:raise InstallError('模型源校验失败: '+str(source))
    if destination.exists() and sha(destination)!=expected:
        raise InstallError('保留已有不同内容的模型，请先确认: '+str(destination))
    copy_file(source,destination,0o644)


def extract_members(archive_path: Path,members: list[dict],output: Path):
    """Copy only named regular model files; validate all before publishing."""
    output.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive_path) as archive:
        available=archive.getmembers()
        selected=[]
        for item in members:
            matches=[m for m in available if m.isfile() and (m.name==item['member'] or m.name.endswith('/'+item['member']))]
            if len(matches)!=1:raise InstallError('模型包成员缺失或重名: '+item['member'])
            destination=output/relative_path(item['name'])
            raw=archive.extractfile(matches[0])
            assert raw is not None
            destination.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=destination.parent,delete=False) as f:
                temporary=Path(f.name);shutil.copyfileobj(raw,f)
            if sha(temporary)!=item['sha256']:raise InstallError('模型成员校验失败: '+item['member'])
            selected.append((temporary,destination))
        for temporary,destination in selected:os.replace(temporary,destination)


def split_dependencies(value: str):
    """Names only; Bianbu APT resolves versions for distribution packages."""
    import re
    result=[]
    for group in value.split(','):
        choices=[]
        for option in group.split('|'):
            match=re.match(r'\s*([a-zA-Z0-9+.-]+)(?::[a-zA-Z0-9-]+)?',option)
            if match:choices.append(match.group(1))
        if choices:result.append(choices)
    return result
