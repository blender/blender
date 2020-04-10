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

    for scene in bpy.data.scenes:
        scene.render.engine = 'BLENDER_WORKBENCH'
        scene.display.shading.light = 'STUDIO'
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


def get_arguments(filepath, output_filepath):
    return [
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
    report = render_report.Report("Workbench", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("workbench_renders")
    report.set_compare_engines('workbench', 'eevee')
    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
