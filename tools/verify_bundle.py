#!/usr/bin/env python3
"""Verify the submission files without importing code or touching device services."""
import hashlib
from pathlib import Path

def main():
    root = Path(__file__).resolve().parents[1]
    rows = (root / 'SHA256SUMS').read_text(encoding='utf-8').splitlines()
    failures = []
    listed = set()
    for row in rows:
        expected, relative = row.split('  ', 1)
        target = root / relative
        if not target.resolve().is_relative_to(root):
            raise SystemExit('Unsafe manifest path')
        listed.add(relative)
        if not target.is_file():
            failures.append(relative + ': missing')
        elif hashlib.sha256(target.read_bytes()).hexdigest() != expected:
            failures.append(relative + ': SHA256 mismatch')
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() and p.name != 'SHA256SUMS' and '__pycache__' not in p.parts and '.git' not in p.parts}
    failures.extend(name + ': not in manifest' for name in sorted(actual - listed))
    if failures:
        raise SystemExit('\n'.join(failures))
    print(f'PASS: {len(rows)} files verified. No services or configuration changed.')

if __name__ == '__main__':
    main()
