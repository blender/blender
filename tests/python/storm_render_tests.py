#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import sys
from pathlib import Path

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
]

# Minor difference in texture coordinate for white noise hash.
BLOCKLIST_INTEL = [
    "hair_reflection.blend",
    "hair_transmission.blend",
    "principled_bsdf_emission.blend",
    "principled_bsdf_sheen.blend",
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


def get_arguments(filepath, output_filepath):
    return [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-P",
        os.path.realpath(__file__),
        "-o", output_filepath,
        "-F", "PNG",
        "-f", "1"]


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
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    from modules import render_report

    if sys.platform == "darwin":
        blocklist = BLOCKLIST_METAL
    else:
        gpu_vendor = render_report.get_gpu_device_vendor(args.blender)
        if gpu_vendor == "AMD":
            blocklist = BLOCKLIST_AMD
        elif gpu_vendor == "INTEL":
            blocklist = BLOCKLIST_INTEL
        else:
            blocklist = []

    if args.export_method == 'HYDRA':
        report = render_report.Report("Storm Hydra", args.outdir, args.oiiotool, blocklist=blocklist + BLOCKLIST_HYDRA)
        report.set_reference_dir("storm_hydra_renders")
        report.set_compare_engine('cycles', 'CPU')
    else:
        report = render_report.Report("Storm USD", args.outdir, args.oiiotool, blocklist=blocklist + BLOCKLIST_USD)
        report.set_reference_dir("storm_usd_renders")
        report.set_compare_engine('storm_hydra')

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
        # Ignore differences in rasterization of hair on Mesa drivers
        report.set_fail_threshold(0.02)
        report.set_fail_percent(1.8)

    test_dir_name = Path(args.testdir).name

    os.environ['BLENDER_HYDRA_EXPORT_METHOD'] = args.export_method

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if not inside_blender and __name__ == "__main__":
    main()
