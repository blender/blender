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
try:
    # Render report is not always available and leads to errors in the console logs that can be ignored.
    from modules import render_report

    class EEVEEReport(render_report.Report):
        def __init__(self, title, output_dir, oiiotool, device=None, blocklist=[]):
            super().__init__(title, output_dir, oiiotool, device=device, blocklist=blocklist)
            self.gpu_backend = device

        def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
            return arguments_cb(filepath, base_output_filepath, gpu_backend=self.device)

except ImportError:
    # render_report can only be loaded when running the render tests. It errors when
    # this script is run during preparation steps.
    pass

# List of .blend files that are known to be failing and are not ready to be
# tested, or that only make sense on some devices. Accepts regular expressions.
BLOCKLIST = [
    # Blocked due to point cloud volume differences between platforms (to be fixed).
    "points_volume.blend",
    # Blocked due to GBuffer encoding of small IOR difference between platforms (to be fixed).
    "principled_thinfilm_transmission.blend",
]


def setup():
    import bpy

    for scene in bpy.data.scenes:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'

        # Enable Eevee features
        eevee = scene.eevee

        # Overscan of 50 will doesn't offset the final image, and adds more information for screen based ray tracing.
        eevee.use_overscan = True
        eevee.overscan_size = 50.0

        # Ambient Occlusion Pass
        eevee.gtao_distance = 1

        # Lights
        eevee.light_threshold = 0.001

        # Hair
        scene.render.hair_type = 'STRIP'

        # Shadow
        eevee.shadow_step_count = 16

        # Volumetric
        eevee.volumetric_tile_size = '2'
        eevee.volumetric_start = 1.0
        eevee.volumetric_end = 50.0
        eevee.volumetric_samples = 128
        eevee.use_volumetric_shadows = True
        eevee.clamp_volume_indirect = 0.0

        # Motion Blur
        if scene.render.use_motion_blur:
            eevee.motion_blur_steps = 10

        # Ray-tracing
        eevee.use_raytracing = True
        eevee.ray_tracing_method = 'SCREEN'
        ray_tracing = eevee.ray_tracing_options
        ray_tracing.resolution_scale = "1"
        ray_tracing.screen_trace_quality = 1.0
        ray_tracing.screen_trace_thickness = 1.0

        # Light-probes
        eevee.gi_cubemap_resolution = '256'

        # Only include the plane in probes
        for ob in scene.objects:
            if ob.type == 'LIGHT':
                # Set maximum resolution
                ob.data.shadow_maximum_resolution = 0.0

            if ob.name != 'Plane' and ob.type != 'LIGHT':
                ob.hide_probe_volume = True
                ob.hide_probe_sphere = True
                ob.hide_probe_plane = True

            # Counteract the versioning from legacy EEVEE. Should be changed per file at some point.
            for mat_slot in ob.material_slots:
                if mat_slot.material:
                    mat_slot.material.thickness_mode = 'SPHERE'

        # Does not work in edit mode
        if bpy.context.mode == 'OBJECT':
            # Simple probe setup
            bpy.ops.object.lightprobe_add(type='SPHERE', location=(0.0, 0.1, 1.0))
            cubemap = bpy.context.selected_objects[0]
            cubemap.scale = (5.0, 5.0, 2.0)
            cubemap.data.falloff = 0.0
            cubemap.data.clip_start = 0.8
            cubemap.data.influence_distance = 1.2

            bpy.ops.object.lightprobe_add(type='VOLUME', location=(0.0, 0.0, 2.0))
            grid = bpy.context.selected_objects[0]
            grid.scale = (8.0, 4.5, 4.5)
            grid.data.resolution_x = 32
            grid.data.resolution_y = 16
            grid.data.resolution_z = 8
            grid.data.bake_samples = 128
            grid.data.capture_world = True
            grid.data.surfel_density = 100
            # Make lighting smoother for most of the case.
            grid.data.dilation_threshold = 1.0
            bpy.ops.object.lightprobe_cache_bake(subset='ACTIVE')


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
    # TODO: This always fails.
    command = [
        blender,
        "--background",
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
    except Exception:
        return None
    return None


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
        "-E", "BLENDER_EEVEE_NEXT",
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

    gpu_device_type = get_gpu_device_type(blender)
    reference_override_dir = None
    if gpu_device_type == "AMD":
        reference_override_dir = "eevee_next_renders/amd"

    report = EEVEEReport("Eevee Next", output_dir, oiiotool, device=gpu_backend, blocklist=BLOCKLIST)
    if gpu_backend == "vulkan":
        report.set_compare_engine('eevee_next', 'opengl')
    else:
        report.set_compare_engine('cycles', 'CPU')

    report.set_pixelated(True)
    report.set_reference_dir("eevee_next_renders")
    report.set_reference_override_dir(reference_override_dir)

    test_dir_name = Path(test_dir).name
    if test_dir_name.startswith('image_mapping'):
        # Platform dependent border values. To be fixed
        report.set_fail_threshold(0.2)
    elif test_dir_name.startswith('image'):
        report.set_fail_threshold(0.051)

    # Noise pattern changes depending on platform. Mostly caused by transparency.
    # TODO(fclem): See if we can just increase number of samples per file.
    if test_dir_name.startswith('render_layer'):
        # shadow pass, rlayer flag
        report.set_fail_threshold(0.035)
    elif test_dir_name.startswith('hair'):
        # hair close up
        report.set_fail_threshold(0.0275)
    elif test_dir_name.startswith('integrator'):
        # shadow all max bounces
        report.set_fail_threshold(0.0275)
    elif test_dir_name.startswith('pointcloud'):
        # points transparent
        report.set_fail_threshold(0.06)

    ok = report.run(test_dir, blender, get_arguments, batch=args.batch, fail_silently=args.fail_silently)
    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
