#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2022 Blender Foundation
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
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-o", output_filepath,
        "-f", "1",
        "-F", "PNG"]

    return args


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-outdir", nargs=1)
    parser.add_argument("-idiff", nargs=1)
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    idiff = args.idiff[0]
    output_dir = args.outdir[0]

    from modules import render_report
    report = render_report.Report("Sequencer", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("reference")

    test_dir_name = Path(test_dir).name
    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
