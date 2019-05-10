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
        scene.render.engine = 'BLENDER_EEVEE'

    # Enable Eevee features
    scene = bpy.context.scene
    eevee = scene.eevee

    eevee.use_sss = True
    eevee.use_ssr = True
    eevee.use_ssr_refraction = True
    eevee.use_gtao = True
    eevee.use_dof = True

    eevee.use_volumetric = True
    eevee.use_volumetric_shadows = True
    eevee.volumetric_tile_size = '2'

    for mat in bpy.data.materials:
        mat.use_screen_refraction = True
        mat.use_sss_translucency = True


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
        "-E", "BLENDER_EEVEE",
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
    report = render_report.Report("Eevee Test Report", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("eevee_renders")
    report.set_compare_engines('eevee', 'cycles')
    ok = report.run(test_dir, render_file)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
