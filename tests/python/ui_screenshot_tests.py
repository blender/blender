#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2018-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys


def screenshot():
    import bpy

    output_path = sys.argv[-1]

    # Force redraw and take screenshot.
    bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
    bpy.ops.screen.screenshot(filepath=output_path)

    bpy.ops.wm.quit_blender()


# When run from inside Blender, take screenshot and exit.
try:
    import bpy
    inside_blender = True
except ImportError:
    inside_blender = False

if inside_blender:
    screenshot()
    sys.exit(0)


def get_arguments(filepath, output_filepath):
    return [
        "--no-window-focus",
        "--window-geometry",
        "0", "0", "1024", "768",
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-P",
        os.path.realpath(__file__),
        "--",
        output_filepath + '0001.png']


def create_argparse():
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True)
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--oiiotool", required=True)
    parser.add_argument('--batch', default=False, action='store_true')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    from modules import render_report
    report = render_report.Report("Screenshot", args.outdir, args.oiiotool)
    ok = report.run(args.testdir, args.blender, get_arguments)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
