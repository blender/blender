#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
import shlex
import shutil
import subprocess
import sys


def render_files(filepaths, output_filepaths):
    command = [BLENDER, "--background"]

    # OSL and GPU examples
    # custom_args += ["--python-expr", "import bpy; bpy.context.scene.cycles.shading_system = True"]
    # custom_args += ["--python-expr", "import bpy; bpy.context.scene.cycles.device = 'GPU'"]
    custom_args = os.getenv('CYCLESTEST_ARGS')
    custom_args = shlex.split(custom_args) if custom_args else []

    for filepath, output_filepath in zip(filepaths, output_filepaths):
        dirname = os.path.dirname(filepath)
        basedir = os.path.dirname(dirname)
        subject = os.path.basename(dirname)

        frame_filepath = output_filepath + '0001.png'

        common_args = [
            "-noaudio",
            "--factory-startup",
            "--enable-autoexec",
            filepath,
            "-E", "CYCLES",
            "-o", output_filepath,
            "-F", "PNG"]

        common_args += custom_args

        if subject == 'bake':
            command.extend(common_args)
            command.extend(['--python', os.path.join(basedir, "util", "render_bake.py")])
        elif subject == 'denoise_animation':
            command.extend(common_args)
            command.extend(['--python', os.path.join(basedir, "util", "render_denoise.py")])
        else:
            command.extend(common_args)
            command.extend(["-f", "1"])

    error = None
    try:
        # Success
        output = subprocess.check_output(command)
        if VERBOSE:
            print(" ".join(command))
            print(output.decode("utf-8"))
    except subprocess.CalledProcessError as e:
        # Error
        if VERBOSE:
            print(" ".join(command))
            print(e.output.decode("utf-8"))
        error = "CRASH"
    except BaseException as e:
        # Crash
        if VERBOSE:
            print(" ".join(command))
            print(e.decode("utf-8"))
        error = "CRASH"

    # Detect missing filepaths and consider those errors
    errors = []
    for output_filepath in output_filepaths:
        frame_filepath = output_filepath + '0001.png'
        if os.path.exists(frame_filepath):
            shutil.copy(frame_filepath, output_filepath)
            os.remove(frame_filepath)
            errors.append(None)
        else:
            errors.append(error)
            error = 'SKIPPED'

    return errors

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
    report = render_report.Report("Cycles Test Report", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("cycles_renders")
    report.set_compare_engines('cycles', 'eevee')
    ok = report.run(test_dir, render_files)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
