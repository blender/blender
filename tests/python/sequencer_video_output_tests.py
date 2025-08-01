#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
import subprocess

from pathlib import Path
from modules import render_report


def get_movie_file_suffix(filepath):
    """
    Get suffix used for the video output.
    The script does not have access to the .blend file content, so deduct it from the .blend filename.
    """

    return Path(filepath).stem.split("_")[-1]


def get_arguments(filepath, output_filepath):
    suffix = get_movie_file_suffix(filepath)

    args = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-o", f"{output_filepath}.{suffix}",
        "-a",
    ]

    return args


def create_argparse():
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True)
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--oiiotool", required=True)
    parser.add_argument("--batch", default=False, action="store_true")
    return parser


class VideoOutputReport(render_report.Report):
    def postprocess_test(self, blender, test):
        suffix = get_movie_file_suffix(test.filepath)

        video_file = Path(f"{test.tmp_out_img_base}.{suffix}").as_posix()

        # If oiiotool supports the FFmpeg this could be used instead.
        """
        command = (
            self.oiiotool,
            "-i", video_file,
            "-o", test.tmp_out_img,
        )

        try:
            subprocess.check_output(command)
        except subprocess.CalledProcessError as e:
            pass
        """

        # Blender's render pipeline always appends frame suffix unless # is present in the file path.
        # Here we need the file name to match exactly, so we trick Blender by going 0001 -> #### mask
        # allowing render piepline to expand it back to 0001.
        out_filepath = test.tmp_out_img.replace("0001", "####")

        python_expr = (
            f"""
import bpy
scene = bpy.context.scene
scene.render.resolution_x = 1920
scene.render.resolution_y = 1080
scene.render.resolution_percentage = 25
ed = scene.sequence_editor_create()
strip = ed.strips.new_movie(name='input', filepath='{video_file}', channel=1, frame_start=1)
strip.colorspace_settings.name = 'Non-Color'
""")

        command = (
            blender,
            "--background",
            "--factory-startup",
            "--enable-autoexec",
            "--python-expr", python_expr,
            "-o", out_filepath,
            "-F", "PNG",
            "-f", "1",
        )

        try:
            subprocess.check_output(command)
        except subprocess.CalledProcessError as e:
            pass


def main():
    parser = create_argparse()
    args = parser.parse_args()

    report = VideoOutputReport("Sequencer", args.outdir, args.oiiotool)
    report.set_pixelated(True)
    # Default error tolerances are quite large, lower them.
    report.set_fail_threshold(2.0 / 255.0)
    report.set_fail_percent(0.01)
    report.set_reference_dir("reference")

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
