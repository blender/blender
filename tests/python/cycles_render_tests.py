#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
import shlex
import shutil
import subprocess
import sys


def get_arguments(filepath, output_filepath):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)
    subject = os.path.basename(dirname)

    args = [
        "--background",
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        filepath,
        "-E", "CYCLES",
        "-o", output_filepath,
        "-F", "PNG"]

    # OSL and GPU examples
    # custom_args += ["--python-expr", "import bpy; bpy.context.scene.cycles.shading_system = True"]
    # custom_args += ["--python-expr", "import bpy; bpy.context.scene.cycles.device = 'GPU'"]
    custom_args = os.getenv('CYCLESTEST_ARGS')
    if custom_args:
        args.extend(shlex.split(custom_args))

    if subject == 'bake':
        args.extend(['--python', os.path.join(basedir, "util", "render_bake.py")])
    elif subject == 'denoise_animation':
        args.extend(['--python', os.path.join(basedir, "util", "render_denoise.py")])
    else:
        args.extend(["-f", "1"])

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
    report = render_report.Report("Cycles", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("cycles_renders")
    report.set_compare_engines('cycles', 'eevee')
    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
