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


def render_file(filepath, output_filepath):
    command = (
        BLENDER,
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
        output_filepath)

    try:
        # Success
        output = subprocess.check_output(command)
        if VERBOSE:
            print(output.decode("utf-8"))
        return None
    except subprocess.CalledProcessError as e:
        # Error
        if os.path.exists(output_filepath):
            os.remove(output_filepath)
        if VERBOSE:
            print(e.output.decode("utf-8"))
        return "CRASH"
    except BaseException as e:
        # Crash
        if os.path.exists(output_filepath):
            os.remove(output_filepath)
        if VERBOSE:
            print(e)
        return "CRASH"


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

    global BLENDER, VERBOSE

    BLENDER = args.blender[0]
    VERBOSE = os.environ.get("BLENDER_VERBOSE") is not None

    test_dir = args.testdir[0]
    idiff = args.idiff[0]
    output_dir = args.outdir[0]

    from modules import render_report
    report = render_report.Report("OpenGL Draw Test Report", output_dir, idiff)
    ok = report.run(test_dir, render_file)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
