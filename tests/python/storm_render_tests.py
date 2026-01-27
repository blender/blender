#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
from pathlib import Path
try:
    # Render report is not always available and leads to errors in the console logs that can be ignored.
    from modules import render_report

    class StormReport(render_report.Report):
        def __init__(self, title, output_dir, oiiotool, variation=None, blocklist=[]):
            super().__init__(title, output_dir, oiiotool, variation=variation, blocklist=blocklist)
            self.gpu_backend = variation

        def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
            return arguments_cb(filepath, base_output_filepath, gpu_backend=self.gpu_backend)

except ImportError:
    # render_report can only be loaded when running the render tests. It errors when
    # this script is run during preparation steps.
    pass

# Unsupported or broken scenarios for the Storm render engine
BLOCKLIST_HYDRA = [
    # Corrupted output around borders
    "image.*_half.*.blend",
    "image.*_float.*.blend",
    # Differences between devices/drivers causing this to fail
    "image.blend",
    # VDB rendering is incorrect on Metal
    "overlapping_octrees.blend",
    # No number of sample support, so will not converge to gray as expected
    "white_noise_256spp.blend",
]

BLOCKLIST_USD = [
    # Corrupted output around borders
    "image.*_half.*.blend",
    "image.*_float.*.blend",
    # Nondeterministic exporting of lights in the scene
    "light_tree_node_subtended_angle.blend",
    # VDB rendering is incorrect on Metal
    "overlapping_octrees.blend",
    # No number of sample support, so will not converge to gray as expected
    "white_noise_256spp.blend",
]

# Metal support in Storm is no as good as OpenGL, though this needs to be
# retested with newer OpenUSD versions as there are improvements.
BLOCKLIST_METAL = [
    # Thinfilm
    "principled.*thinfilm.*.blend",
    "metallic.*thinfilm.*.blend",
    # Transparency
    "transparent.blend",
    "transparent_shadow.blend",
    "transparent_shadow_hair.blend",
    "transparent_shadow_hair_blur.blend",
    "shadow_all_max_bounces.blend",
    "underwater_caustics.blend",
    "shadow_link_transparency.blend",
    "principled_bsdf_transmission.blend",
    "light_path_is_shadow_ray.blend",
    "light_path_is_transmission_ray.blend",
    "light_path_ray_depth.blend",
    "light_path_ray_length.blend",
    "transparent_spatial_splits.blend",
    # Volume
    "light_link_surface_in_volume.blend",
    "openvdb.*.blend",
    "principled_bsdf_interior",
    # Other
    "autosmooth_custom_normals.blend",
]

# AMD seems to have similar limitations as Metal for transparency.
BLOCKLIST_AMD = BLOCKLIST_METAL + [
    "volume_tricubic_interpolation.blend",
]

# Minor difference in texture coordinate for white noise hash.
BLOCKLIST_OPENGL_INTEL_LINUX = [
    "hair_reflection.blend",
    "hair_transmission.blend",
    "principled_bsdf_emission.blend",
    "principled_bsdf_sheen.blend",
]

# Some Vulkan tests are broken for all vendors.
BLOCKLIST_VULKAN = [
    # Integrator; image 100% transparent
    "transparent_shadow_limit_0.blend",
    "transparent_shadow_limit_1.blend",
    "transparent_shadow_limit_1024.blend",
    "transparent_shadow_limit_401.blend",

    # Light linking; image black
    "shadow_link_simple_point_cloud.blend",

    # Mesh; some spheres (Intel Linux) or all spheres (every other vendor) not rendered
    "normal_types.blend",
    "normal_types_motion.blend",

    # Shader; image 100% transparent
    "normal.blend",
]
BLOCKLIST_VULKAN_HYDRA = [
    # Motion blur; sporadic black image on NVIDIA and Intel
    "multi_step_motion_blur.blend",
]
BLOCKLIST_VULKAN_USD = [
    # Motion blur; image black
    "bvh_steps_curve_segments_0.blend",
    "bvh_steps_curve_segments_3.blend",
    "bvh_steps_line_segments_0.blend",
    "bvh_steps_line_segments_3.blend",
    "mblur_deform_autosmooth.blend",
    "mblur_deform_simple.blend",
]

# A very large amount of tests is missing objects. Blacklist all tests for now.
BLOCKLIST_VULKAN_INTEL_LINUX = [
    ".*.blend",
]

BLOCKLIST_VULKAN_NVIDIA = [
    # Principled bsdf; missing objects
    "principled_bsdf_emission.blend",
    "principled_bsdf_sheen.blend",

    # Shader; failed non-deterministically on workers when tested
    "texture_coordinate_camera.blend",
    "texture_coordinate_object.blend",
    "texture_coordinate_generated.blend",
    "texture_coordinate_normal.blend",

    # Render layer; failed non-deterministically on workers when tested
    "freestyle_stroke_material.blend"

    # Hair; failed non-deterministically on workers when tested
    "hair_instancer_uv.blend"
]

BLOCKLIST_VULKAN_AMD = [
    # Hair; failed non-deterministically on workers when tested
    "hair_instancer_uv.blend"
]


def setup():
    import bpy
    import addon_utils

    addon_utils.enable("hydra_storm")

    for scene in bpy.data.scenes:
        scene.render.engine = 'HYDRA_STORM'
        scene.hydra.export_method = os.environ['BLENDER_HYDRA_EXPORT_METHOD']


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
    parser.add_argument("--export_method", required=True)
    parser.add_argument('--batch', default=False, action='store_true')
    parser.add_argument('--gpu-backend')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blocklist = []
    if args.gpu_backend == "metal":
        blocklist += BLOCKLIST_METAL
    elif args.gpu_backend == "vulkan":
        blocklist += BLOCKLIST_VULKAN
        if args.export_method == 'HYDRA':
            blocklist += BLOCKLIST_VULKAN_HYDRA
        else:
            blocklist += BLOCKLIST_VULKAN_USD
        gpu_vendor = render_report.get_gpu_device_vendor(args.blender)
        if gpu_vendor == "NVIDIA":
            blocklist += BLOCKLIST_VULKAN_NVIDIA
        elif gpu_vendor == "AMD":
            blocklist += BLOCKLIST_VULKAN_AMD
        elif gpu_vendor == "INTEL" and sys.platform == "linux":
            blocklist += BLOCKLIST_VULKAN_INTEL_LINUX
    else:
        gpu_vendor = render_report.get_gpu_device_vendor(args.blender)
        if gpu_vendor == "AMD":
            blocklist += BLOCKLIST_AMD
        elif gpu_vendor == "INTEL" and sys.platform == "linux":
            blocklist += BLOCKLIST_OPENGL_INTEL_LINUX

    if args.export_method == 'HYDRA':
        report = StormReport(
            "Storm Hydra",
            args.outdir,
            args.oiiotool,
            variation=args.gpu_backend,
            blocklist=blocklist +
            BLOCKLIST_HYDRA)
        report.set_reference_dir("storm_hydra_renders")
        if args.gpu_backend == "vulkan":
            report.set_compare_engine('storm_hydra', 'opengl')
        else:
            report.set_compare_engine('cycles', 'CPU')
    else:
        report = StormReport(
            "Storm USD",
            args.outdir,
            args.oiiotool,
            variation=args.gpu_backend,
            blocklist=blocklist +
            BLOCKLIST_USD)
        report.set_reference_dir("storm_usd_renders")
        report.set_compare_engine('storm_hydra')
        if args.gpu_backend == "metal":
            report.set_compare_engine('storm_hydra', 'metal')
        elif args.gpu_backend == "vulkan":
            report.set_compare_engine('storm_hydra', 'vulkan')
        elif args.gpu_backend == "opengl":
            report.set_compare_engine('storm_hydra', 'opengl')
        else:
            report.set_compare_engine('cycles', 'CPU')

    report.set_pixelated(True)

    # Try to account for image filtering differences from OS/drivers
    test_dir_name = Path(args.testdir).name
    if (test_dir_name in {'image_mapping'}):
        report.set_fail_threshold(0.028)
        report.set_fail_percent(1.3)
    if (test_dir_name in {'image_colorspace'}):
        report.set_fail_threshold(0.032)
        report.set_fail_percent(1.5)
    if (test_dir_name in {'mesh'}):
        report.set_fail_threshold(0.036)
        report.set_fail_percent(1.3)
    if (test_dir_name in {'sss', 'hair'}):
        # Ignore differences in rasterization of hair on Vulkan and Mesa drivers
        report.set_fail_threshold(0.036)
        report.set_fail_percent(2.3)

    test_dir_name = Path(args.testdir).name

    os.environ['BLENDER_HYDRA_EXPORT_METHOD'] = args.export_method

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
