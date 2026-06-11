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
    # Blocked due to stochastic diffuse/transmission layering resulting in non-deterministic surfel lighting.
    "principled_bsdf_transmission.blend",
    # Blocked due to platform-dependent noise differences (likely floating-point/fast-math differences).
    "raycast_bump.blend",
    # Blocked due to platform-dependent uninitialized pixels.
    "image_mapping_udim.blend",
    # Redundant with hair_linear_close_up.
    "hair_ribbon_close_up.blend",
    # Redundant with transparent_shadow.
    "transparent_shadow_limit_.*",
    # Redundant with transparent_shadow_hair.
    "transparent_shadow_hair_blur.blend",
    # Unsupported feature. Redundant tests. (except osl_camera_advanced which tests triangular bokeh)
    "osl_camera_advanced_manual_dof.blend",
    "osl_camera_advanced_manual_dof_138188.blend",
    "osl_camera_cubemap.blend",
    "osl_camera_cubemap_auto_derivatives.blend",
    "osl_camera_offset_in_volume.blend",
    # Extreme texture values interpolate differently on different GPUs.
    "image_log.blend",
    # Exhibit the LTC light leaking issue. To be enabeld back after fixing.
    "light_path_glossy_depth.blend",
    # Exhibit non-deterministic behavior because of tracing outside the spotlight 45° cone.
    "light_path_is_camera_ray.blend",
    # Exhibit non-deterministic (to be fixed).
    "background_scene.blend",
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
]

BLOCKLIST_VULKAN = [
    # Blocked due to UB in the background pixels (to be fixed).
    "lightprobe_planar.blend",
    # Blocked due to texture interpolation differences (to be investigated).
    "principled_blackbody.blend",
    # Blocked due to difference in indirect light (AMD linux) (to be investigated).
    "sss_concave_clamp.blend",
    # Blocked due to difference in screen space tracing (to be investigated).
    "image.blend",
]

BLOCKLIST_OPENGL = [
]

BLOCKLIST_INTEL = [
]

BLOCKLIST_AMD_WINDOWS_VK = [
    # Fails inside driver during XML serialization (See #159880).
    "implicit_volume.blend"
]

BLOCKLIST_INTEL_WINDOWS_GL = [
    # Fails sporadically and causes all subsequent volume tests to fail (See #153612).
    "volume_instance.blend"
]

BLOCKLIST_NVIDIA_GL = [
    # Non-deterministic behavior. Unkown reason, the pool size doesn't seem to be exceeded.
    "shadow_min_pool_size.blend",
]


def setup():
    import bpy

    for scene in bpy.data.scenes:
        scene.render.engine = 'BLENDER_EEVEE'

        skip_hair_setup = scene.get("EEVEE_skip_hair_setup", False)
        skip_probes_setup = scene.get("EEVEE_skip_probes_setup", False)
        skip_raytracing_setup = scene.get("EEVEE_skip_raytracing_setup", False)
        skip_shadow_setup = scene.get("EEVEE_skip_shadow_setup", False)
        skip_subsurface_setup = scene.get("EEVEE_skip_subsurface_setup", False)

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
        if not skip_hair_setup:
            scene.render.hair_type = 'STRIP'

        # Shadow
        if not skip_shadow_setup:
            eevee.shadow_step_count = 16
            eevee.shadow_pool_size = '1024'

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

        if not skip_raytracing_setup:
            # Ray-tracing
            eevee.use_raytracing = True
            eevee.ray_tracing_method = 'SCREEN'
            ray_tracing = eevee.ray_tracing_options
            ray_tracing.resolution_scale = "1"
            ray_tracing.screen_trace_quality = 1.0
            ray_tracing.screen_trace_thickness = 1.0
            # Fast GI
            eevee.fast_gi_quality = 0.8

        # Light-probes
        if not skip_probes_setup:
            eevee.gi_cubemap_resolution = '256'

        # Light-path intensity
        eevee.direct_light_intensity = 1.0
        eevee.indirect_light_intensity = 1.0

        for ob in scene.objects:
            if ob.type == 'LIGHT' and not skip_shadow_setup:
                # Set maximum resolution
                ob.data.shadow_maximum_resolution = 0.0

            # Only include the plane in probes
            if ob.name != 'Plane' and ob.type != 'LIGHT' and not skip_probes_setup:
                ob.hide_probe_volume = True
                ob.hide_probe_sphere = True

            # Counteract the versioning from legacy EEVEE. Should be changed per file at some point.
            if not skip_subsurface_setup:
                for mat_slot in ob.material_slots:
                    if mat_slot.material:
                        mat_slot.material.thickness_mode = 'SPHERE'

        if bpy.data.objects.get('Volume_Probe_Baked') is not None:
            # Some file already have pre existing probe setup with baked data.
            pass
        # Does not work in edit mode
        elif bpy.context.mode == 'OBJECT' and not skip_probes_setup:
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
    elif args.gpu_backend == "opengl":
        blocklist += BLOCKLIST_OPENGL

    gpu_vendor = render_report.get_gpu_device_vendor(args.blender)
    if os.getenv("BLENDER_TEST_IGNORE_VENDOR_BLOCKLIST") is None:
        if gpu_vendor == "INTEL":
            blocklist += BLOCKLIST_INTEL
        if gpu_vendor == "INTEL" and sys.platform == "win32" and args.gpu_backend == "opengl":
            blocklist += BLOCKLIST_INTEL_WINDOWS_GL
        if gpu_vendor == "NVIDIA" and args.gpu_backend == "opengl":
            blocklist += BLOCKLIST_NVIDIA_GL
        if gpu_vendor == "AMD" and sys.platform == "win32" and args.gpu_backend == "vulkan":
            blocklist += BLOCKLIST_AMD_WINDOWS_VK

    report = EEVEEReport("EEVEE", args.outdir, args.oiiotool, variation=args.gpu_backend, blocklist=blocklist)
    if args.gpu_backend == "vulkan":
        report.set_compare_engine('eevee', 'opengl')
    else:
        report.set_compare_engine('cycles', 'CPU')

    report.set_pixelated(True)
    report.set_reference_dir("eevee_renders")
    # Default settings are too lose. EEVEE renders have much less noise than a path tracer.
    report.set_fail_percent(0.08)
    report.set_fail_threshold(4.0 / 255.0)

    test_dir_name = Path(args.testdir).name
    if gpu_vendor == "NVIDIA" and args.gpu_backend == "opengl":
        # References are supposed to be generated on OpenGL Nvidia. Tighten the threshold for this platform.
        report.set_fail_percent(0.049)
        report.set_fail_threshold(2.0 / 255.0)
        # Some different GPU arch have different texture sampling behavior.
        if test_dir_name in {"image_mapping"}:
            report.set_fail_percent(0.08)
            report.set_fail_threshold(4.0 / 255.0)
        elif test_dir_name in {"displacement"}:
            report.set_fail_percent(0.11)
            report.set_fail_threshold(5.0 / 255.0)
        elif test_dir_name in {"texture"}:
            report.set_fail_percent(0.14)
            report.set_fail_threshold(6.0 / 255.0)
    elif test_dir_name.startswith('camera'):
        # camera_stereo_panoramic have some platform specific small differences
        report.set_fail_percent(0.14)
        report.set_fail_threshold(6.0 / 255.0)
    elif test_dir_name.startswith('image_colorspace'):
        # image_log has hot pixels that result in platform differences.
        report.set_fail_percent(0.15)
    elif test_dir_name.startswith('displacement'):
        # Raster difference across hardware (subdiv geometry).
        report.set_fail_percent(0.1)
        report.set_fail_threshold(7.0 / 255.0)
        if args.gpu_backend == "metal":
            # Difference in shadows in true_displacement_image and vector_displacement_tangent.
            report.set_fail_percent(0.21)
        elif gpu_vendor == "AMD":
            # Difference in bump_normal_texture likely caused by different derivatives.
            report.set_fail_percent(0.29)
    elif test_dir_name.startswith('transparency'):
        # Dithered transparency uses platform dependent noise pattern.
        report.set_fail_percent(0.22)
        report.set_fail_threshold(10.0 / 255.0)
    elif test_dir_name.startswith('instancing'):
        # Small pointcloud has platform dependent raster pattern
        report.set_fail_threshold(8.0 / 255.0)
        if args.gpu_backend == "metal":
            report.set_fail_percent(0.33)
    elif test_dir_name.startswith('hair'):
        # hair_close_up has differences of line rasterization on linux.
        if gpu_vendor == "INTEL":
            report.set_fail_percent(0.13)
    elif test_dir_name.startswith('principled_bsdf'):
        # principled_bsdf_thinfilm_metallic has some weird behavior in reflection of
        # black surfaces. to be investigated
        report.set_fail_percent(0.09)
    elif test_dir_name.startswith('integrator'):
        # Noise difference in transparent materials (mostly transparent_spatial_splits)
        report.set_fail_threshold(8.0 / 255.0)
        if gpu_vendor == "INTEL":
            # light_path_is_singular_ray has some fireflies.
            report.set_fail_percent(0.11)
        if gpu_vendor == "AMD":
            # light_path_is_singular_ray has some fireflies.
            report.set_fail_threshold(9.0 / 255.0)
    elif test_dir_name.startswith('light_linking'):
        # Noise difference in transparent materials (mostly shadow_link_transparency) and volume
        report.set_fail_threshold(8.0 / 255.0)
    elif test_dir_name.startswith('texture'):
        # Texture sampling and noise function different per platform.
        # Also voronoi_f1 test uses `pow()` which has different precision depending on platform.
        report.set_fail_percent(0.46)
        report.set_fail_threshold(6.0 / 255.0)
    elif test_dir_name.startswith('shader'):
        # normal_mapping_light_leak fireflies.
        # fresnel_layer_weight high values are accumulated differently on different platform.
        report.set_fail_percent(0.2)
        if gpu_vendor == "INTEL":
            # mix_color uses implementation dependent function.
            report.set_fail_percent(0.41)
    elif test_dir_name.startswith('render_layer'):
        # Because of aov_transparency noise pattern
        report.set_fail_percent(0.5)
        report.set_fail_threshold(8.0 / 255.0)
    elif test_dir_name.startswith('grease_pencil'):
        # TAA dependent look? To be investigated.
        report.set_fail_percent(0.1)
        report.set_fail_threshold(6.0 / 255.0)
    elif test_dir_name.startswith('raycast') and gpu_vendor == "AMD":
        # Some slight contour differences on AMD
        report.set_fail_percent(0.37)
        report.set_fail_threshold(10.0 / 255.0)
    elif test_dir_name.startswith('pointcloud'):
        # Only because of points_transparent
        report.set_fail_threshold(8.0 / 255.0)
    elif test_dir_name.startswith('motion_blur'):
        # Failure can be subtle, tighten threshold
        report.set_fail_percent(0.04)
        report.set_fail_threshold(2.0 / 255.0)
        if args.gpu_backend == "metal":
            # subd_motion_blur has some differences in bump on M1.
            report.set_fail_percent(0.06)
        if args.gpu_backend == "opengl" and gpu_vendor == "AMD":
            # large_combined_motion has 1 hot pixel difference in rasterization.
            report.set_fail_percent(0.043)
            report.set_fail_threshold(3.0 / 255.0)
    elif test_dir_name.startswith('lightprobe') and args.gpu_backend == "metal":
        # Some shadow difference, to be investigated
        report.set_fail_percent(0.09)
        report.set_fail_threshold(6.0 / 255.0)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)
    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
