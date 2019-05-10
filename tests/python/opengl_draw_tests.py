#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
import subprocess
import sys


def screenshot():
    import bpy

    output_path = sys.argv[-1]

    # Force redraw and take screenshot.
    bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
    bpy.ops.screen.screenshot(filepath=output_path, full=True)

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
        filepath,
        "-P",
        os.path.realpath(__file__),
        "--",
        output_filepath + '0001.png']


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
    report = render_report.Report("OpenGL Draw", output_dir, idiff)
    ok = report.run(test_dir, blender, get_arguments)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
