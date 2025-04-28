#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import importlib.util
import os
import platform
import subprocess
import sys
from pathlib import Path
import sys

from modules import render_report


class OverlayReport(render_report.Report):
    def __init__(self, title, output_dir, oiiotool, variation=None, blocklist=[]):
        super().__init__(title, output_dir, oiiotool, variation=variation, blocklist=blocklist)
        self.gpu_backend = variation

    def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
        return arguments_cb(filepath, base_output_filepath, gpu_backend=self.gpu_backend)


def get_arguments(filepath, output_filepath, gpu_backend):
    arguments = [
        "--no-window-focus",
        "--window-geometry",
        "0", "0", "128", "128",
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error"]

    if gpu_backend:
        arguments.extend(["--gpu-backend", gpu_backend])

    # Windows separators get messed up when passing them inside the python expression
    output_filepath = output_filepath.replace("\\", "/")

    script_name = Path(filepath).stem + ".py"
    current_dir = os.path.dirname(os.path.realpath(__file__))
    script_filepath = os.path.join(current_dir, "overlay", script_name)

    arguments.extend([
        filepath,
        "--python-expr",
        f'import bpy; bpy.context.scene.render.filepath = "{output_filepath}"',
        "-P",
        script_filepath])

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

    report = OverlayReport("Overlay", args.outdir, args.oiiotool, variation=args.gpu_backend)
    if args.gpu_backend == "vulkan":
        report.set_compare_engine('overlay', 'opengl')
    else:
        report.set_compare_engine('workbench', 'opengl')
    report.set_pixelated(True)
    report.set_reference_dir("overlay_renders")

    test_dir_name = Path(args.testdir).name
    if test_dir_name.startswith('hair') and platform.system() == "Darwin":
        report.set_fail_threshold(0.050)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
