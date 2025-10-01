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
        def __init__(self, title, output_dir, oiiotool, variation=None, blocklist=[]):
            super().__init__(title, output_dir, oiiotool, variation=variation, blocklist=blocklist)
            self.gpu_backend = variation

        def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
            return arguments_cb(filepath, base_output_filepath, gpu_backend=self.gpu_backend)

except ImportError:
    # render_report can only be loaded when running the render tests. It errors when
    # this script is run during preparation steps.
    pass

BLOCKLIST_VULKAN = [
    # Blocked due behavior differences. mix(0.05, INF, 0.0) will result a NaN in Vulkan, but INF in OpenGL.
    # The INF is part of the EXR image.
    "image_log.blend",
    "image_log_osl.blend",
]


def setup():
    import bpy

    for scene in bpy.data.scenes:
        scene.render.engine = 'BLENDER_WORKBENCH'
        scene.display.shading.light = 'STUDIO'
        scene.display.shading.color_type = 'TEXTURE'

        # Hair
        scene.render.hair_type = 'STRIP'


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
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True)
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--oiiotool", required=True)
    parser.add_argument('--batch', default=False, action='store_true')
    parser.add_argument('--gpu-backend')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blocklist = []
    if args.gpu_backend == "vulkan":
        blocklist += BLOCKLIST_VULKAN

    report = WorkbenchReport("Workbench", args.outdir, args.oiiotool, variation=args.gpu_backend, blocklist=blocklist)
    if args.gpu_backend == "vulkan":
        report.set_compare_engine('workbench', 'opengl')
    else:
        report.set_compare_engine('eevee', 'opengl')
    report.set_pixelated(True)
    report.set_reference_dir("workbench_renders")

    test_dir_name = Path(args.testdir).name
    if test_dir_name.startswith('hair') and platform.system() == "Darwin":
        report.set_fail_threshold(0.050)
    if test_dir_name.startswith('openvdb'):
        report.set_fail_threshold(0.04)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
