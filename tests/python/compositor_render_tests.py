#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys


# When run from inside Blender, render and exit.
try:
    import bpy
    inside_blender = True
except ImportError:
    inside_blender = False


def get_compositor_device_setter_script(execution_device):
    return f"import bpy; bpy.data.scenes[0].render.compositor_device = '{execution_device}'"


def get_arguments(filepath, output_filepath, backend):
    arguments = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error"]

    execution_device = "CPU"
    if backend != "CPU":
        execution_device = "GPU"
        arguments.extend(["--gpu-backend", backend])

    arguments.extend([
        filepath,
        "-P", os.path.realpath(__file__),
        "--python-expr", get_compositor_device_setter_script(execution_device),
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1"])
    return arguments


def create_argparse():
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True)
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--oiiotool", required=True)
    parser.add_argument('--batch', default=False, action='store_true')
    parser.add_argument('--gpu-backend')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    from modules import render_report
    backend = args.gpu_backend if args.gpu_backend else "CPU"
    report_title = f"Compositor {backend.upper()}"
    report = render_report.Report(report_title, args.outdir, args.oiiotool)
    report.set_pixelated(True)
    report.set_reference_dir("compositor_renders")

    if os.path.basename(args.testdir) == 'filter':
        # Temporary change to pass OpenImageDenoise test with both 1.3 and 1.4.
        report.set_fail_threshold(0.05)
    elif os.path.basename(args.testdir) == 'mask' or os.path.basename(args.testdir) == 'keying':
        # The node_keying_matte.blend test is very sensitive to the exact values in the
        # input image. It makes it hard to precisely match results on different systems
        # (with and without SSE, i.e.), especially when OCIO has different precision for
        # the exponent transform on different platforms.
        report.set_fail_threshold(0.06)
        report.set_fail_percent(2)

    def arguments_callback(filepath, output_filepath): return get_arguments(filepath, output_filepath, backend)
    ok = report.run(args.testdir, args.blender, arguments_callback, batch=args.batch)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
