#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys


# When run from inside Blender, render and exit.
try:
    import bpy
    inside_blender = True
except ImportError:
    inside_blender = False

ENABLE_REALTIME_COMPOSITOR_SCRIPT = "import bpy; " \
    "bpy.context.preferences.experimental.use_experimental_compositors = True; " \
    "bpy.data.scenes[0].node_tree.execution_mode = 'REALTIME'"


def get_arguments(filepath, output_filepath):
    return [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-P", os.path.realpath(__file__),
        "--python-expr", ENABLE_REALTIME_COMPOSITOR_SCRIPT,
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1"
    ]


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-outdir", nargs=1)
    parser.add_argument("-oiiotool", nargs=1)
    parser.add_argument('--batch', default=False, action='store_true')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    oiiotool = args.oiiotool[0]
    output_dir = args.outdir[0]

    from modules import render_report
    report = render_report.Report("Compositor Realtime", output_dir, oiiotool)
    report.set_reference_dir("compositor_realtime_renders")

    ok = report.run(test_dir, blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
