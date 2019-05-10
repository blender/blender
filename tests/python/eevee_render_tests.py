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

    eevee.use_ssr = True
    eevee.use_ssr_refraction = True
    eevee.use_gtao = True

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


def get_arguments(filepath, output_filepath):
    return [
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
    report = render_report.Report("Eevee", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("eevee_renders")
    report.set_compare_engines('eevee', 'cycles')
    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
