#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
from pathlib import Path


BLOCKLIST = [
    "hdr_simple_export_hlg_12bit.blend",
    "hdr_simple_export_pq_12bit.blend",
    "sdr_simple_export_p3_aces_10bit.blend",
    "hdr_simple_still_test_file.blend",
]


def get_compositor_device_setter_script(execution_device):
    return f"import bpy; bpy.data.scenes[0].render.compositor_device = '{execution_device}'"


def get_arguments(filepath, output_filepath, backend):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)

    args = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--console-crash-handler",
        "--debug-exit-on-error"]

    execution_device = "CPU"
    if backend != "CPU":
        execution_device = "GPU"
        args.extend(["--gpu-backend", backend])

    args.extend([
        filepath,
        "--python-expr", get_compositor_device_setter_script(execution_device),
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1",
    ])

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
    parser.add_argument('--gpu-backend')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    from modules import render_report
    backend = args.gpu_backend if args.gpu_backend else "CPU"
    report_title = f"Sequencer {backend.upper()}"
    report = render_report.Report(report_title, args.outdir, args.oiiotool, blocklist=BLOCKLIST)
    report.set_pixelated(True)
    # Default error tolerances are quite large, lower them.
    report.set_fail_threshold(2.0 / 255.0)
    report.set_fail_percent(0.01)
    report.set_reference_dir("reference")

    def arguments_callback(filepath, output_filepath): return get_arguments(filepath, output_filepath, backend)
    ok = report.run(args.testdir, args.blender, arguments_callback, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
