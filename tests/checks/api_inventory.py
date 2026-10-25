#!/usr/bin/env python3
"""Static API reference inventory; this is NOT executed line/branch coverage."""
import argparse
import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--library', type=Path, help='Optional Linux shared library; inspect exported symbols using nm')
args = parser.parse_args()

# Match public function declarations, not inline implementations or function pointers.
declaration = re.compile(r'(?m)^([\w \t*]+?)\b(BLB_\w+)\s*\([^;{}]*\)\s*;')
headers = {}
for header in sorted((ROOT / 'include' / 'bulba').rglob('*.h')):
    content = re.sub(r'/\*.*?\*/|//[^\n]*', '', header.read_text(), flags=re.S)
    for prefix, name in declaration.findall(content):
        first = prefix.strip().split()[0] if prefix.strip() else ''
        if not first or first in {'return', 'static', 'inline', 'typedef', 'else'}:
            continue
        headers.setdefault(name, str(header.relative_to(ROOT)))

references = {}
for group in ('checks', 'demos'):
    names = set()
    for source in (ROOT / 'tests' / group).glob('*.c'):
        content = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', '', source.read_text(), flags=re.S)
        names.update(re.findall(r'\b(BLB_\w+)\s*\(', content))
    references[group] = names

exports = None
if args.library:
    result = subprocess.run(['nm', '-D', '--defined-only', str(args.library)], check=True, capture_output=True, text=True)
    exports = set(re.findall(r'\b(BLB_\w+)\b', result.stdout))

writer = csv.writer(sys.stdout)
writer.writerow(['function', 'header', 'referenced_by_checks', 'referenced_by_demos', 'exported_if_inspected'])
for name, header in sorted(headers.items()):
    writer.writerow([name, header, name in references['checks'], name in references['demos'],
                     '' if exports is None else name in exports])
