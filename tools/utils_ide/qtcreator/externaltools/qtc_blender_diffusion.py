#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Takes 1 arg

 qtc_blender_diffusion.py <file> <row>

Currently GCC is assumed
"""

import sys
import os
import subprocess

SOURCE_FILE = sys.argv[-2]
SOURCE_ROW = sys.argv[-1]

BASE_URL = "https://developer.blender.org/diffusion/B/browse"


def main():
    dirname, _filename = os.path.split(SOURCE_FILE)

    process = subprocess.Popen(
        ["git", "rev-parse", "--symbolic-full-name", "--abbrev-ref",
         "@{u}"], stdout=subprocess.PIPE, cwd=dirname, universal_newlines=True)
    output = process.communicate()[0]
    branchname = output.rstrip().rsplit('/', 1)[-1]

    process = subprocess.Popen(
        ["git", "rev-parse", "--show-toplevel"],
        stdout=subprocess.PIPE, cwd=dirname, universal_newlines=True)
    output = process.communicate()[0]
    toplevel = output.rstrip()
    filepath = os.path.relpath(SOURCE_FILE, toplevel)

    url = '/'.join([BASE_URL, branchname, filepath]) + "$" + SOURCE_ROW

    print(url)

    # Maybe handy, but also annoying?
    if "--browse" in sys.argv:
        import webbrowser
        webbrowser.open(url)


if __name__ == "__main__":
    main()
