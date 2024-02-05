#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
from pathlib import Path


def setup():
    import bpy
    import addon_utils

    addon_utils.enable("hydra_storm")

    for scene in bpy.data.scenes:
        scene.render.engine = 'HYDRA_STORM'
        scene.hydra.export_method = os.environ['BLENDER_HYDRA_EXPORT_METHOD']


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
    parser.add_argument("-oiiotool", nargs=1)
    parser.add_argument("-export_method", nargs=1)
    parser.add_argument('--batch', default=False, action='store_true')
    parser.add_argument('--fail-silently', default=False, action='store_true')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    oiiotool = args.oiiotool[0]
    output_dir = args.outdir[0]
    export_method = args.export_method[0]

    from modules import render_report

    if export_method == 'HYDRA':
        report = render_report.Report("Storm Hydra", output_dir, oiiotool)
        report.set_reference_dir("storm_hydra_renders")
        report.set_compare_engine('cycles', 'CPU')
    else:
        report = render_report.Report("Storm USD", output_dir, oiiotool)
        report.set_reference_dir("storm_usd_renders")
        report.set_compare_engine('storm_hydra')

    report.set_pixelated(True)

    test_dir_name = Path(test_dir).name

    os.environ['BLENDER_HYDRA_EXPORT_METHOD'] = export_method

    ok = report.run(test_dir, blender, get_arguments, batch=args.batch, fail_silently=args.fail_silently)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
