#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import platform
import sys
from pathlib import Path
try:
    # Render report is not always available and leads to errors in the console logs that can be ignored.
    from modules import render_report

    class WorkbenchReport(render_report.Report):
        def __init__(self, title, output_dir, oiiotool, device=None, blocklist=[]):
            super().__init__(title, output_dir, oiiotool, device=device, blocklist=blocklist)
            self.gpu_backend = device

        def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
            return arguments_cb(filepath, base_output_filepath, gpu_backend=self.device)

except ImportError:
    # render_report can only be loaded when running the render tests. It errors when
    # this script is run during preparation steps.
    pass


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


def get_arguments(filepath, output_filepath, gpu_backend):
    arguments = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error"]

    if gpu_backend:
        arguments.extend(["--gpu-backend", gpu_backend])

    arguments.extend([
        filepath,
        "-E", "BLENDER_WORKBENCH",
        "-P",
        os.path.realpath(__file__),
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1"])

    return arguments


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-outdir", nargs=1)
    parser.add_argument("-oiiotool", nargs=1)
    parser.add_argument('--batch', default=False, action='store_true')
    parser.add_argument('--fail-silently', default=False, action='store_true')
    parser.add_argument('--gpu-backend', nargs=1)
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    oiiotool = args.oiiotool[0]
    output_dir = args.outdir[0]
    gpu_backend = args.gpu_backend[0]

    from modules import render_report
    report = WorkbenchReport("Workbench", output_dir, oiiotool, device=gpu_backend)
    if gpu_backend == "vulkan":
        report.set_compare_engine('workbench', 'opengl')
    else:
        report.set_compare_engine('eevee_next', 'opengl')
    report.set_pixelated(True)
    report.set_reference_dir("workbench_renders")

    test_dir_name = Path(test_dir).name
    if test_dir_name.startswith('hair') and platform.system() == "Darwin":
        report.set_fail_threshold(0.050)

    ok = report.run(test_dir, blender, get_arguments, batch=args.batch, fail_silently=args.fail_silently)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
