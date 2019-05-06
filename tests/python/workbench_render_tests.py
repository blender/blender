#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
import shlex
import shutil
import subprocess
import sys


def setup():
    import bpy

    scene = bpy.context.scene
    scene.display.shading.color_type = 'TEXTURE'


# When run from inside Blender, render and exit.
try:
    import bpy
    inside_blender = True
except ImportError:
    inside_blender = False

if inside_blender:
    try:
        setup()
    except Exception as e:
        print(e)
        sys.exit(1)


def render_file(filepath, output_filepath):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)
    subject = os.path.basename(dirname)

    frame_filepath = output_filepath + '0001.png'

    command = [
        BLENDER,
        "--background",
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        filepath,
        "-E", "BLENDER_WORKBENCH",
        "-P",
        os.path.realpath(__file__),
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1"]

    try:
        # Success
        output = subprocess.check_output(command)
        if os.path.exists(frame_filepath):
            shutil.copy(frame_filepath, output_filepath)
            os.remove(frame_filepath)
        if VERBOSE:
            print(" ".join(command))
            print(output.decode("utf-8"))
        return None
    except subprocess.CalledProcessError as e:
        # Error
        if os.path.exists(frame_filepath):
            os.remove(frame_filepath)
        if VERBOSE:
            print(" ".join(command))
            print(e.output.decode("utf-8"))
        if b"Error: engine not found" in e.output:
            return "NO_ENGINE"
        elif b"blender probably wont start" in e.output:
            return "NO_START"
        return "CRASH"
    except BaseException as e:
        # Crash
        if os.path.exists(frame_filepath):
            os.remove(frame_filepath)
        if VERBOSE:
            print(" ".join(command))
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
    report = render_report.Report("Workbench Test Report", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("workbench_renders")
    report.set_compare_engines('workbench', 'eevee')
    ok = report.run(test_dir, render_file)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
