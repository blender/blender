#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import pathlib
import subprocess
import sys
from pathlib import Path


def setup():
    import bpy

    for scene in bpy.data.scenes:
        scene.render.engine = 'BLENDER_EEVEE'

    # Enable Eevee features
    scene = bpy.context.scene
    eevee = scene.eevee

    eevee.use_soft_shadows = True

    eevee.use_ssr = True
    eevee.use_ssr_refraction = True

    eevee.use_gtao = True
    eevee.gtao_distance = 1

    eevee.use_volumetric_shadows = True
    eevee.volumetric_tile_size = '2'

    for mat in bpy.data.materials:
        # This needs to be enabled case by case,
        # otherwise we loose SSR and GTAO everywhere.
        # mat.use_screen_refraction = True
        mat.use_sss_translucency = True

    cubemap = None
    grid = None
    # Does not work in edit mode
    try:
        # Simple probe setup
        bpy.ops.object.lightprobe_add(type='CUBEMAP', location=(0.5, 0, 1.5))
        cubemap = bpy.context.selected_objects[0]
        cubemap.scale = (2.5, 2.5, 1.0)
        cubemap.data.falloff = 0
        cubemap.data.clip_start = 2.4

        bpy.ops.object.lightprobe_add(type='GRID', location=(0, 0, 0.25))
        grid = bpy.context.selected_objects[0]
        grid.scale = (1.735, 1.735, 1.735)
        grid.data.grid_resolution_x = 3
        grid.data.grid_resolution_y = 3
        grid.data.grid_resolution_z = 2
    except:
        pass

    try:
        # Try to only include the plane in reflections
        plane = bpy.data.objects['Plane']

        collection = bpy.data.collections.new("Reflection")
        collection.objects.link(plane)
        # Add all lights to light the plane
        if not invert:
            for light in bpy.data.objects:
                if light.type == 'LIGHT':
                    collection.objects.link(light)

        # Add collection to the scene
        scene.collection.children.link(collection)

        cubemap.data.visibility_collection = collection
    except:
        pass

    eevee.gi_diffuse_bounces = 1
    eevee.gi_cubemap_resolution = '128'
    eevee.gi_visibility_resolution = '16'
    eevee.gi_irradiance_smoothing = 0

    bpy.ops.scene.light_cache_bake()


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


def get_gpu_device_type(blender):
    command = [
        blender,
        "-noaudio",
        "--background"
        "--factory-startup",
        "--python",
        str(pathlib.Path(__file__).parent / "gpu_info.py")
    ]
    try:
        completed_process = subprocess.run(command, stdout=subprocess.PIPE)
        for line in completed_process.stdout.read_text():
            if line.startswith("GPU_DEVICE_TYPE:"):
                vendor = line.split(':')[1]
                return vendor
    except BaseException as e:
        return None
    return None


def get_arguments(filepath, output_filepath):
    return [
        "--background",
        "-noaudio",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
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

    gpu_device_type = get_gpu_device_type(blender)
    reference_override_dir = None
    if gpu_device_type == "AMD":
        reference_override_dir = "eevee_renders/amd"

    from modules import render_report
    report = render_report.Report("Eevee", output_dir, idiff)
    report.set_pixelated(True)
    report.set_reference_dir("eevee_renders")
    report.set_reference_override_dir(reference_override_dir)
    report.set_compare_engine('cycles', 'CPU')

    test_dir_name = Path(test_dir).name
    if test_dir_name.startswith('image'):
        report.set_fail_threshold(0.051)

    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
