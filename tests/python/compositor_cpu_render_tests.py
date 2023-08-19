#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2022 Blender Authors
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


def get_arguments(filepath, output_filepath):
    return [
        "--background",
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-P",
        os.path.realpath(__file__),
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1"]


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
    report = render_report.Report("Compositor CPU", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("compositor_cpu_renders")

    if os.path.basename(test_dir) == 'filter':
        # Temporary change to pass OpenImageDenoise test with both 1.3 and 1.4.
        report.set_fail_threshold(0.05)
    elif os.path.basename(test_dir) == 'matte':
        # The node_keying_matte.blend test is very sensitive to the exact values in the
        # input image. It makes it hard to precisely match results on different systems
        # (with and without SSE, i.e.), especially when OCIO has different precision for
        # the exponent transform on different platforms.
        report.set_fail_threshold(0.05)

    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
