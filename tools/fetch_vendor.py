#!/usr/bin/env python3
"""Fetch an exact external source revision into a NEW directory; never install."""
import argparse
import subprocess
from pathlib import Path

SOURCES = {
    'llama': ('https://github.com/spacemit-com/llama.cpp', '17ce6aa07bd38057040ca43da8660ee496c3cb6d'),
    'tts': ('https://github.com/spacemit-com/model-zoo-tts', 'a13613dfa41d5dd971d1f897eb4b9e668540f2cb'),
    'asr': ('https://github.com/spacemit-com/model-zoo-asr', 'dd0bed4d4d9f7425f11adccc2efbca3e2d330aae'),
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('component', choices=SOURCES)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    destination = args.destination.resolve()
    if destination.exists():
        raise SystemExit('Destination already exists; nothing changed.')
    url, revision = SOURCES[args.component]
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'clone', '--no-checkout', '--filter=blob:none', url, str(destination)], check=True)
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'checkout', '--detach', revision], cwd=destination, check=True)
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=destination, text=True).strip()
    if actual != revision:
        raise SystemExit('Pinned revision mismatch; do not build.')
    subprocess.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=destination, check=True)
    print(f'Fetched {args.component} {revision} into {destination}; no installation or service changes.')

if __name__ == '__main__':
    main()
