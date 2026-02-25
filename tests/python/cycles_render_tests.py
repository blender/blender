#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import platform
import os
import shlex
import sys
from pathlib import Path
from modules import render_report

# List of .blend files that are known to be failing and are not ready to be
# tested, or that only make sense on some devices. Accepts regular expressions.
BLOCKLIST_ALL = [
    # Blocked due to overlapping object differences between platforms.
    "hair_geom_reflection.blend",
    "hair_geom_transmission.blend",
    "hair_instancer_uv.blend",
    "principled_hair_directcoloring.blend",
    "visibility_particles.blend",
    # Tests for EEVEE-only setting (duplicates from the Cycles perspective)
    "camera_depth_of_field_jittered.blend",
    "shadow_resolution.blend",
    "shadow_min_pool_size.blend",
    "shadow_resolution_scale.blend",
    "shader_to_rgb_transparent.blend",
]

# Blocklist for device + build configuration that does not support OSL at all.
BLOCKLIST_OSL_NONE = [
    '.*_osl.blend',
    'osl_.*.blend',
]

# Blocklist for OSL with limited OSL tests for fast test execution.
BLOCKLIST_OSL_LIMITED = []

# Blocklist for tests that fail when running all tests with OSL backend.
# Most of these tests are blocked due to expected differences between SVM and OSL.
# Due to the expected differences there are usually a SVM and OSL version of the test.
# So blocking these tests doesn't lose any test permutations.
BLOCKLIST_OSL_ALL = BLOCKLIST_OSL_LIMITED + [
    # AOVs are not supported. See 73266
    'aov_.*.blend',
    'render_passes_aov.*.blend',
    # Tests that need investigating into why they're failing:
    # Noise differences due to Principled BSDF mixing/layering used in some of these scenes
    'render_passes_.*.blend',
    # OSL can not specify parameters when reading attribute, which we need for stochastic sampling
    'volume_tricubic_interpolation.blend',
]

BLOCKLIST_OPTIX = [
    # Ray intersection precision issues
    'big_triangles_50164.blend',
    'big_plane_43865.blend',
]

# Blocklist for OSL tests that fail with the OptiX OSL backend.
BLOCKLIST_OPTIX_OSL_LIMITED = [
    # OptiX OSL doesn't support the trace function
    'osl_trace_shader.blend',
    # Noise functions do not return color with OptiX OSL
    'osl_camera_advanced.blend',
]

# Blocklist for SVM tests that fail when forced to run with OptiX OSL
BLOCKLIST_OPTIX_OSL_ALL = BLOCKLIST_OPTIX_OSL_LIMITED + [
    # OptiX OSL does support AO, Bevel or Raycast
    'ambient_occlusion.*.blend',
    'bake_bevel.blend',
    'bevel.blend',
    'raycast.*.blend',
    'principled_bsdf_bevel_emission_137420.blend',
    # Dicing tests use wireframe node which doesn't appear to be supported with OptiX OSL
    'dicing_camera.blend',
    'object_dicing.blend',
    'offscreen_dicing.blend',
    'panorama_dicing.blend',
    # Error during rendering. Need to investigate why.
    'points_volume.blend',
]


BLOCKLIST_METAL = []

BLOCKLIST_METAL_RT = [
    # Metal RT uses different parameterization for linear curves.
    # See discussion in #146072
    # https://projects.blender.org/blender/blender/issues/146072#issuecomment-1699788
    'hair_linear_close_up.blend',
]

if platform.system() == "Darwin":
    version, _, _ = platform.mac_ver()
    major_version = version.split(".")[0]
    if int(major_version) < 13:
        BLOCKLIST_METAL += [
            # MNEE only works on Metal with macOS >= 13
            "underwater_caustics.blend",
        ]

BLOCKLIST_GPU = [
    # Uninvestigated differences with GPU.
    'glass_mix_40964.blend',
    'filter_glossy_refraction_45609.blend',
    'bevel_mblur.blend',
    # Inconsistency between Embree and Hair primitive on GPU.
    'denoise_hair.blend',
    'hair_basemesh_intercept.blend',
    'hair_instancer_uv.blend',
    'hair_info.blend',
    'hair_particle_random.blend',
    "hair_transmission.blend",
    'principled_hair_.*.blend',
    'transparent_shadow_hair.*.blend',
    "microfacet_hair_orientation.blend",
    # Inconsistent handling of overlapping objects.
    "sobol_uniform_41143.blend",
    "visibility_particles.blend",
    # No path guiding on GPU.
    "guiding*.blend",
]


class CyclesReport(render_report.Report):
    def __init__(
            self,
            title,
            test_dir_name,
            output_dir,
            oiiotool,
            device=None,
            blocklist=[],
            osl=False):
        # Split device name in format "<device_type>[-<RT>]" into individual
        # tokens, setting the RT suffix to an empty string if its not specified.
        self.device, suffix = (device.split("-") + [""])[:2]
        self.use_hwrt = (suffix == "RT")
        self.osl = osl
        self.extra_args = []

        variation = self.device
        if suffix:
            variation += ' ' + suffix
        if self.osl:
            variation += ' OSL'

        super().__init__(title, output_dir, oiiotool, variation, blocklist)

        self.set_pixelated(True)
        self.set_reference_dir("cycles_renders")
        if device == 'CPU':
            self.set_compare_engine('eevee')
        else:
            self.set_compare_engine('cycles', 'CPU')

    def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
        return arguments_cb(
            filepath,
            base_output_filepath,
            self.use_hwrt,
            self.osl,
            self.extra_args)

    def _get_arguments_suffix(self):
        return ['--', '--cycles-device', self.device] if self.device else []


def get_arguments(filepath, output_filepath, use_hwrt, osl, extra_args):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)
    subject = os.path.basename(dirname)

    args = [
        "--background",
        "--factory-startup",
        "--enable-autoexec",
        "--debug-memory",
        "--debug-exit-on-error",
        filepath,
        "-E", "CYCLES",
        "-o", output_filepath,
        "-F", "PNG"]

    # OSL and GPU examples
    # custom_args += ["--python-expr", "import bpy; bpy.context.scene.cycles.shading_system = True"]
    # custom_args += ["--python-expr", "import bpy; bpy.context.scene.cycles.device = 'GPU'"]
    custom_args = os.getenv('CYCLESTEST_ARGS')
    if custom_args:
        args.extend(shlex.split(custom_args))

    spp_multiplier = os.getenv('CYCLESTEST_SPP_MULTIPLIER')
    if spp_multiplier:
        args.extend(["--python-expr", f"import bpy; bpy.context.scene.cycles.samples *= {spp_multiplier}"])

    cycles_pref = "bpy.context.preferences.addons['cycles'].preferences"
    use_hwrt_bool_value = "True" if use_hwrt else "False"
    use_hwrt_on_off_value = "'ON'" if use_hwrt else "'OFF'"
    args.extend([
        "--python-expr",
        (f"import bpy;"
         f"{cycles_pref}.use_hiprt = {use_hwrt_bool_value};"
         f"{cycles_pref}.use_oneapirt = {use_hwrt_bool_value};"
         f"{cycles_pref}.metalrt = {use_hwrt_on_off_value}")
    ])

    if osl:
        args.extend(["--python-expr", "import bpy; bpy.context.scene.cycles.shading_system = True"])

    args.extend(extra_args)

    if subject == 'bake':
        args.extend(['--python', os.path.join(basedir, "util", "render_bake.py")])
    elif subject == 'denoise_animation':
        args.extend(['--python', os.path.join(basedir, "util", "render_denoise.py")])
    else:
        args.extend(["-f", "1"])

    return args


def create_argparse():
    parser = argparse.ArgumentParser(
        description="Run test script for each blend file in TESTDIR, comparing the render result with known output."
    )
    parser.add_argument("--blender", required=True)
    parser.add_argument("--testdir", required=True)
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--oiiotool", required=True)
    parser.add_argument("--device", required=True)
    parser.add_argument("--osl", default='none', type=str, choices=["none", "limited", "all"])
    parser.add_argument('--batch', default=False, action='store_true')
    return parser


def test_volume_ray_marching(args, report):
    # Default volume rendering algorithm is null scattering, but we also want to test ray marching
    report.extra_args = ["--python-expr", "import bpy; bpy.context.scene.cycles.volume_biased = True"]
    report.set_reference_dir("cycles_ray_marching_renders")
    report.set_test_name_suffix("_ray_marching")
    return report.run(args.testdir, args.blender, get_arguments, batch=args.batch)


def test_texture_cache(args, report):
    # Use texture cache directory in output folder, and clear it to test auto generating.
    test_dir_name = Path(args.testdir).name
    texture_cache_dir = Path(args.outdir) / test_dir_name / "texture_cache"
    for tx_file in texture_cache_dir.glob("*.tx"):
        tx_file.unlink()

    report.extra_args = ["--python-expr",
                         "import bpy; "
                         "bpy.context.scene.render.use_texture_cache = True; "
                         "bpy.context.scene.render.use_auto_generate_texture_cache = True; "
                         f"bpy.context.preferences.filepaths.texture_cache_directory = r\"{texture_cache_dir}\""]
    report.set_reference_dir("cycles_tx_renders")
    report.set_test_name_suffix("_tx")
    return report.run(args.testdir, args.blender, get_arguments, batch=args.batch)


def main():
    parser = create_argparse()
    args = parser.parse_args()

    device = args.device

    blocklist = BLOCKLIST_ALL

    if args.osl == 'none':
        blocklist += BLOCKLIST_OSL_NONE
    elif args.osl == "limited":
        blocklist += BLOCKLIST_OSL_LIMITED
    else:
        blocklist += BLOCKLIST_OSL_ALL

    if device != 'CPU':
        blocklist += BLOCKLIST_GPU

    if device == 'OPTIX':
        blocklist += BLOCKLIST_OPTIX
        if args.osl == 'limited':
            blocklist += BLOCKLIST_OPTIX_OSL_LIMITED
        elif args.osl == 'all':
            blocklist += BLOCKLIST_OPTIX_OSL_ALL

    if device == 'METAL':
        blocklist += BLOCKLIST_METAL
    if device == 'METAL-RT':
        blocklist += BLOCKLIST_METAL
        blocklist += BLOCKLIST_METAL_RT

    test_dir_name = Path(args.testdir).name
    report = CyclesReport('Cycles', test_dir_name, args.outdir, args.oiiotool, device, blocklist, args.osl == 'all')

    # Increase threshold for motion blur, see #78777.
    #
    # underwater_caustics.blend gives quite different results on Linux and Intel macOS compared to
    # Windows and Arm macOS.
    #
    # OSL tests:
    # Blackbody is slightly different between SVM and OSL.
    # Microfacet hair renders slightly differently, and fails on Windows and Linux with OSL
    #
    # both_displacement.blend has slight differences between Linux and other platforms.

    if (test_dir_name in {'motion_blur', 'integrator', "displacement"}) or \
       ((args.osl == 'all') and (test_dir_name in {'shader', 'hair'})):
        report.set_fail_threshold(0.032)

    # Layer mixing is different between SVM and OSL, so a few tests have
    # noticeably different noise causing OSL Principled BSDF tests to fail.
    if ((args.osl == 'all') and (test_dir_name == 'principled_bsdf')):
        report.set_fail_threshold(0.06)

    # Volume scattering probability guiding renders differently on different platforms
    if (test_dir_name in {'shadow_catcher', 'light'}):
        report.set_fail_threshold(0.038)
    if (test_dir_name in {'light', 'camera'}):
        report.set_fail_threshold(0.02)
        report.set_fail_percent(4)
    if (test_dir_name in {'volume', 'openvdb'}):
        report.set_fail_threshold(0.048)
        report.set_fail_percent(3)
    # OSL blackbody output is a little different.
    if (test_dir_name in {'colorspace'}):
        report.set_fail_threshold(0.05)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    if (test_dir_name == 'volume'):
        ok = ok and test_volume_ray_marching(args, report)

    if (test_dir_name in {'image_colorspace', 'image_mapping'}):
        ok = ok and test_texture_cache(args, report)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
