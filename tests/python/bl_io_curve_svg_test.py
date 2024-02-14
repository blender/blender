#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
from pathlib import Path


def get_arguments(filepath, output_filepath):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)

    args = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-E", "CYCLES",
        "-o", output_filepath,
        "-F", "PNG",
        "--python", os.path.join(basedir, "util", "import_svg.py"),
        "-f", "1",
    ]

    return args


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-outdir", nargs=1)
    parser.add_argument("-oiiotool", nargs=1)
    parser.add_argument('--batch', default=False, action='store_true')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    oiiotool = args.oiiotool[0]
    output_dir = args.outdir[0]

    from modules import render_report
    report = render_report.Report('IO Curve SVG', output_dir, oiiotool)
    report.set_pixelated(True)

    test_dir_name = Path(test_dir).name
    if test_dir_name == 'complex':
        report.set_fail_percent(0.01)

    ok = report.run(test_dir, blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
