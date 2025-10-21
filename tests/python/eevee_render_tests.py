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
        def __init__(self, title, output_dir, oiiotool, variation=None, blocklist=[]):
            super().__init__(title, output_dir, oiiotool, variation=variation, blocklist=blocklist)
            self.gpu_backend = variation

        def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
            return arguments_cb(filepath, base_output_filepath, gpu_backend=self.gpu_backend)

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
    "principled_bsdf_thinfilm_transmission.blend",
    "ray_offset.blend",
    # Blocked due to difference in border texel handling between platforms (to be fixed).
    "render_passes_thinfilm_color.blend",
    # Blocked due to a significant amount of transparency that have a different nosie pattern between devices.
    "light_path_is_shadow_ray.blend",
    # Blocked as the test seems to alternate between two different states
    "light_path_is_diffuse_ray.blend",
]

BLOCKLIST_METAL = [
    # Blocked due to difference in volume lightprobe bakes (to be fixed).
    "light_path_is_volume_scatter_ray.blend",
    # Blocked due to difference in volume lightprobe bakes(maybe?) (to be fixed).
    "volume_zero_extinction_channel.blend",
    # Blocked due to difference in mipmap interpolation (to be fixed).
    "environment_mirror_ball.blend",
    # Blocked due to difference in mipmap interpolation / anisotropic filtering (to be fixed).
    "image.blend",
    # Blocked due to subtle differences in DOF
    "osl_camera_advanced.blend",
]

BLOCKLIST_VULKAN = [
    # Blocked due to difference in screen space tracing (to be fixed).
    "sss_reflection_clamp.blend",
    # Blocked due to difference in screen space tracing (to be investigated).
    "image.blend"
]


def setup():
    import bpy

    for scene in bpy.data.scenes:
        scene.render.engine = 'BLENDER_EEVEE'

        # Enable Eevee features
        eevee = scene.eevee

        # Overscan of 50 will doesn't offset the final image, and adds more information for screen based ray tracing.
        eevee.use_overscan = True
        eevee.overscan_size = 50.0

        # Ambient Occlusion Pass
        for view_layer in scene.view_layers:
            view_layer.eevee.ambient_occlusion_distance = 1

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

        if bpy.data.objects.get('Volume_Probe_Baked') is not None:
            # Some file already have pre existing probe setup with baked data.
            pass
        # Does not work in edit mode
        elif bpy.context.mode == 'OBJECT':
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
        "-E", "BLENDER_EEVEE",
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

    blocklist = BLOCKLIST
    if args.gpu_backend == "metal":
        blocklist += BLOCKLIST_METAL
    elif args.gpu_backend == "vulkan":
        blocklist += BLOCKLIST_VULKAN

    report = EEVEEReport("EEVEE", args.outdir, args.oiiotool, variation=args.gpu_backend, blocklist=blocklist)
    if args.gpu_backend == "vulkan":
        report.set_compare_engine('eevee', 'opengl')
    else:
        report.set_compare_engine('cycles', 'CPU')

    report.set_pixelated(True)
    report.set_reference_dir("eevee_renders")

    test_dir_name = Path(args.testdir).name
    if test_dir_name.startswith('image_mapping'):
        # Platform dependent border values. To be fixed
        report.set_fail_threshold(0.2)
    elif test_dir_name.startswith('image'):
        report.set_fail_threshold(0.051)
    elif test_dir_name.startswith('displacement'):
        # metal shadow and wireframe difference. To be fixed.
        report.set_fail_threshold(0.07)
    elif test_dir_name.startswith('bsdf'):
        # metallic thinfilm tests
        report.set_fail_threshold(0.03)
    elif test_dir_name.startswith('principled_bsdf'):
        # principled bsdf transmission test
        report.set_fail_threshold(0.02)

    # Noise pattern changes depending on platform. Mostly caused by transparency.
    # TODO(fclem): See if we can just increase number of samples per file.
    if test_dir_name.startswith('render_layer'):
        # shadow pass, rlayer flag
        report.set_fail_threshold(0.08)
    elif test_dir_name.startswith('hair'):
        # hair close up
        report.set_fail_threshold(0.0275)
    elif test_dir_name.startswith('integrator'):
        # Noise difference in transparent materials
        report.set_fail_threshold(0.05)
    elif test_dir_name.startswith('pointcloud'):
        # points transparent
        report.set_fail_threshold(0.06)
    elif test_dir_name.startswith('light_linking'):
        # Noise difference in transparent material
        report.set_fail_threshold(0.05)
    elif test_dir_name.startswith('light'):
        # Noise difference in background
        report.set_fail_threshold(0.03)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)
    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
