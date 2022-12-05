#!/usr/bin/env python3
# macOS utility to remove all rpaths and add a new one.

import subprocess
import sys

rpath = sys.argv[1]
file = sys.argv[2]

# Find existing rpaths and delete them one by one.
p = subprocess.run(['otool', '-l', file], capture_output=True)
tokens = p.stdout.split()

for i, token in enumerate(tokens):
    if token == b'LC_RPATH':
        old_rpath = tokens[i + 4]
        subprocess.run(['install_name_tool', '-delete_rpath', old_rpath, file])

subprocess.run(['install_name_tool', '-add_rpath', rpath, file])
