#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import os
import sys


def set_view3d_context_override(context_override):
    """
    Set context override to become the first viewport in the active workspace

    The ``context_override`` is expected to be a copy of an actual current context
    obtained by `context.copy()`
    """

    for area in context_override["screen"].areas:
        if area.type != 'VIEW_3D':
            continue
        for space in area.spaces:
            if space.type != 'VIEW_3D':
                continue
            for region in area.regions:
                if region.type != 'WINDOW':
                    continue
                context_override["area"] = area
                context_override["region"] = region


def generate_stroke(context):
    """
    Generate stroke for the bpy.ops.sculpt.brush_stroke operator

    The generated stroke covers the full plane diagonal.
    """
    from mathutils import Vector

    template = {
        "name": "stroke",
        "mouse": (0.0, 0.0),
        "mouse_event": (0, 0),
        "location": (0.0, 0.0, 0.0),
        "is_start": True,
        "pressure": 1.0,
        "time": 1.0,
        "size": 1.0,
        "x_tilt": 0,
        "y_tilt": 0
    }

    num_steps = 250
    start = Vector((context['area'].width, context['area'].height))
    end = Vector((0, 0))
    delta = (end - start) / (num_steps - 1)

    stroke = []
    for i in range(num_steps):
        step = template.copy()
        step["mouse"] = start + delta * i
        step["mouse_event"] = start + delta * i
        stroke.append(step)

    return stroke


def setup():
    """
    Prepare the scene for rendering - generates objects then performs a stroke
    """

    import bpy
    context = bpy.context

    # Create an undo stack explicitly. This isn't created by default in background mode.
    bpy.ops.ed.undo_push()

    # Forcibly flip the object out of and back into sculpt mode to avoid poll errors due to non-initialized
    # tool runtime data.
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.mode_set(mode='SCULPT')

    context_override = context.copy()
    set_view3d_context_override(context_override)

    with context.temp_override(**context_override):
        bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)

    # Multires workaround - we need to leave sculpt mode currently to flush MDISP data so that the
    # render actually works.
    bpy.ops.object.mode_set(mode='OBJECT')


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
    dirname = os.path.dirname(filepath)

    args = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-E", "BLENDER_WORKBENCH",
        "-P", os.path.realpath(__file__),
        "-o", output_filepath,
        "-f", "1",
        "-F", "PNG"]

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


def main():
    parser = create_argparse()
    args = parser.parse_args()

    from modules import render_report
    report = render_report.Report("Sculpt", args.outdir, args.oiiotool)
    report.set_pixelated(True)
    report.set_fail_threshold(2.0 / 255.0)
    report.set_fail_percent(1.5)
    report.set_reference_dir("reference")

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
