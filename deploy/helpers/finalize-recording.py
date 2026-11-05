#!/usr/bin/env python3
"""Normalize the K1 hardware encoder's MP4 packets without re-encoding."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

def main():
    source=Path(sys.argv[1]).absolute()
    if source.is_symlink() or not source.is_file() or not source.stat().st_size:
        raise ValueError('录制文件不可用')
    fd,name=tempfile.mkstemp(prefix='.'+source.stem+'-',suffix='.mp4',dir=source.parent)
    os.close(fd);target=Path(name)
    try:
        timeout=max(40,min(600,40+source.stat().st_size//(5*1024*1024)))
        command=['ffmpeg','-v','error','-i',str(source),'-map','0','-c','copy',
            '-bsf:v','filter_units=remove_types=7|8,setts=pts=N/25/TB:dts=N/25/TB:duration=1/25/TB',
            '-movflags','+faststart','-y',str(target)]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=timeout)
        if result.returncode:raise RuntimeError('视频封装处理失败，保留原录制文件')
        info=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries',
            'stream=codec_name,codec_type,width,height,r_frame_rate','-of','json',str(target)],timeout=15))
        videos=[s for s in info['streams'] if s['codec_type']=='video']
        audios=[s for s in info['streams'] if s['codec_type']=='audio']
        if (len(videos)!=1 or not audios or videos[0]['codec_name']!='h264'
                or videos[0]['width']!=1280 or videos[0]['height']!=720 or videos[0]['r_frame_rate']!='25/1'):
            raise ValueError('输出参数未通过720p/25帧/音轨检查，保留原录制文件')
        decoded=subprocess.run(['ffmpeg','-v','error','-i',str(target),'-t','1','-map','0:v:0',
            '-f','null','-'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
        if decoded.returncode or decoded.stderr.strip():
            raise ValueError('输出解码检查失败，保留原录制文件')
        shutil.copymode(source,target);os.replace(target,source)
        print('封装检查通过：1280×720，25帧/秒，H.264视频及混合音轨。')
    finally:
        target.unlink(missing_ok=True)

if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError,KeyError) as error:
        print(str(error),file=sys.stderr);raise SystemExit(1)
