#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
from pathlib import Path


BLOCKLIST = [
    "hdr_simple_export_hlg_12bit.blend",
    "hdr_simple_export_pq_12bit.blend",
    "hdr_simple_still_test_file.blend",
]


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
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1",
    ]

    return args


def create_argparse():
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True)
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--oiiotool", required=True)
    parser.add_argument("--batch", default=False, action="store_true")
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    from modules import render_report
    report = render_report.Report("Sequencer", args.outdir, args.oiiotool, blocklist=BLOCKLIST)
    report.set_pixelated(True)
    # Default error tolerances are quite large, lower them.
    report.set_fail_threshold(2.0 / 255.0)
    report.set_fail_percent(0.01)
    report.set_reference_dir("reference")

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
