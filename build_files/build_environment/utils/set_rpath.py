#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# macOS utility to remove all `rpaths` and add a new one.

__all__ = (
    "main",
)

import os
import pathlib
import re
import subprocess
import sys


# Strip version numbers from dependencies macOS notarizatiom fails
# with version symlinks.
def strip_lib_version(name):
    name = re.sub(r'(\.[0-9]+)+.dylib', '.dylib', name)
    name = re.sub(r'(\.[0-9]+)+.so', '.so', name)
    name = re.sub(r'(\.[0-9]+)+.cpython', '.cpython', name)
    return name


# Patch cmake config to match rename
def update_cmake_config(oldfile, newfile):
    for cmakefile in oldfile.parent.glob("cmake/*/*.cmake"):
        text = cmakefile.read_text()
        text = text.replace(oldfile.name, newfile.name)
        cmakefile.write_text(text)


def main():
    rpath = sys.argv[1]
    file = sys.argv[2]
    new_file = strip_lib_version(file)

    file = pathlib.Path(file)
    new_file = pathlib.Path(new_file)

    # Update CMake configuration files.
    update_cmake_config(file, new_file)

    # Remove if symbolic-link.
    if file.is_symlink():
        os.remove(file)
        sys.exit(0)

    # Find existing RPATHS and delete them one by one.
    p = subprocess.run(['otool', '-l', file], capture_output=True)
    tokens = p.stdout.split()

    for i, token in enumerate(tokens):
        if token == b'LC_RPATH':
            old_rpath = tokens[i + 4]
            subprocess.run(['install_name_tool', '-delete_rpath', old_rpath, file])

    subprocess.run(['install_name_tool', '-add_rpath', rpath, file])

    # Strip version from dependencies.
    p = subprocess.run(['otool', '-L', file], capture_output=True)
    tokens = p.stdout.split()
    for i, token in enumerate(tokens):
        token = token.decode("utf-8")
        if token.startswith("@rpath"):
            new_token = strip_lib_version(token)
            subprocess.run(['install_name_tool', '-change', token, new_token, file])

    # Strip version from library itself.
    new_id = '@rpath/' + new_file.name
    os.rename(file, new_file)
    subprocess.run(['install_name_tool', '-id', new_id, new_file])


if __name__ == "__main__":
    main()
