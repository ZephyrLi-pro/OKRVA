#!/usr/bin/env python3
"""Apply reviewed changes to an explicitly supplied, matching third-party checkout."""
import argparse,hashlib,json,subprocess
from pathlib import Path
def main():
    p=argparse.ArgumentParser();p.add_argument('component',choices=('llama','tts'));p.add_argument('source',type=Path);args=p.parse_args()
    bundle=Path(__file__).resolve().parents[1]
    kit=bundle/'patches'/('spacemit-llama' if args.component=='llama' else 'spacemit-tts')
    source=args.source.resolve(strict=True);manifest=json.loads((kit/'source-manifest.json').read_text())
    digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
    rows=manifest['files']
    assert all((source/r['path']).resolve().is_relative_to(source) for r in rows),'Unsafe patch path'
    if all(digest(source/r['path'])==r['after_sha256'] for r in rows):print('Already matches the exported source.');return
    mismatches=[r['path'] for r in rows if digest(source/r['path'])!=r['before_sha256']]
    if mismatches:raise SystemExit('Source does not match the pinned base; no patch applied: '+', '.join(mismatches))
    patch=kit/('current-runtime.patch' if args.component=='llama' else 'current-vendor.patch')
    command=['git','-c','core.autocrlf=false','apply']
    subprocess.run(command+['--check',str(patch)],cwd=source,check=True)
    subprocess.run(command+[str(patch)],cwd=source,check=True)
    assert all(digest(source/r['path'])==r['after_sha256'] for r in rows),'Final hash mismatch'
    print('Applied and verified '+str(len(rows))+' source files.')
if __name__=='__main__':main()
