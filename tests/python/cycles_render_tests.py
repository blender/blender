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
    # Temporarily blocked for 4.4 lib upgrade, due to PNG alpha minor difference.
    "image_log_osl.blend",
]

# Blocklist that disables OSL specific tests for configurations that do not support OSL backend.
BLOCKLIST_EXPLICIT_OSL = [
    '.*_osl.blend',
    'osl_.*.blend',
]

# Blocklist for SVM tests that are forced to run with OSL to test consistency between the two backends.
BLOCKLIST_OSL = [
    # Block tests that fail with OSL due to differences from SVM.
    # Note: Most of the tests below are expected to be different between OSL and SVM
    # As such many of these tests have both a SVM and OSL file. Blocking the SVM
    # tests here doesn't loose any test permutations.
    #
    # AOVs are not supported. See 73266
    'aov_position.blend',
    'render_passes_aov.*.blend',
    # Image sampling is different from SVM. There are OSL variants of these tests
    'image_byte.*.blend',
    'image_float.*.blend',
    'image_half.*.blend',
    'image_mapping_.*_closest.blend',
    'image_mapping_.*_cubic.blend',
    'image_mapping_.*_linear.blend',
    'image_alpha_blend.blend',
    'image_alpha_channel_packed.blend',
    'image_alpha_ignore.blend',
    'image_log.blend',
    'image_non_color.blend',
    'image_mapping_udim.blend',
    # OSL handles bump + displacement differently from SVM. There are OSL variants of these tests
    'both_displacement.blend',
    'bump_with_displacement.blend',
    # Ray portal test uses bump + displacement
    'ray_portal.blend',
    # TODO: Tests that need investigating into why they're failing, and how to fix that.
    # Noise differences due to Principled BSDF mixing/layering used in some of these scenes
    'render_passes_.*.blend',
]

BLOCKLIST_OPTIX = [
    # Ray intersection precision issues
    'big_triangles_50164.blend',
    'big_plane_43865.blend',
]

BLOCKLIST_OPTIX_OSL = [
    # OPTIX OSL doesn't support trace function needed for AO and bevel
    'bake_bevel.blend',
    'ambient_occlusion.*.blend',
    'bevel.blend',
    'osl_trace_shader.blend',
    # Bump evaluation is not implemented yet. See 104276
    'compare_bump.blend',
    'both_displacement.blend',
    'bump_with_displacement.blend',
    'ray_portal.blend',
    # The 3D texture doesn't have the right mappings
    'point_density_.*_object.blend',
    # Dicing tests use wireframe node which doesn't appear to be supported with OptiX OSL
    'dicing_camera.blend',
    'offscreen_dicing.blend',
    'panorama_dicing.blend',
    # The mapping of the UDIM texture is incorrect. Need to investigate why.
    'image_mapping_udim_packed.blend',
    # Error during rendering. Need to investigate why.
    'points_volume.blend',
]


BLOCKLIST_METAL = []

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
    'image_log.blend',
    'glass_mix_40964.blend',
    'filter_glossy_refraction_45609.blend',
    'smoke_color.blend',
    'bevel_mblur.blend',
    # Inconsistency between Embree and Hair primitive on GPU.
    'denoise_hair.blend',
    'hair_basemesh_intercept.blend',
    'hair_instancer_uv.blend',
    'hair_length_info.blend',
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
    def __init__(self, title, output_dir, oiiotool, device=None, blocklist=[], osl=False):
        # Split device name in format "<device_type>[-<RT>]" into individual
        # tokens, setting the RT suffix to an empty string if its not specified.
        self.device, suffix = (device.split("-") + [""])[:2]
        self.use_hwrt = (suffix == "RT")
        self.osl = osl

        variation = self.device
        if suffix:
            variation += ' ' + suffix
        if self.osl:
            variation += ' OSL'

        super().__init__(title, output_dir, oiiotool, variation, blocklist)

    def _get_render_arguments(self, arguments_cb, filepath, base_output_filepath):
        return arguments_cb(filepath, base_output_filepath, self.use_hwrt, self.osl)

    def _get_arguments_suffix(self):
        return ['--', '--cycles-device', self.device] if self.device else []


def get_arguments(filepath, output_filepath, use_hwrt=False, osl=False):
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
    parser.add_argument("--blocklist", nargs="*", default=[])
    parser.add_argument("--osl", default=False, action='store_true')
    parser.add_argument('--batch', default=False, action='store_true')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    device = args.device

    blocklist = BLOCKLIST_ALL
    if device != 'CPU':
        blocklist += BLOCKLIST_GPU
    if device != 'CPU' or 'OSL' in args.blocklist:
        blocklist += BLOCKLIST_EXPLICIT_OSL
    if device == 'OPTIX':
        blocklist += BLOCKLIST_OPTIX
        if args.osl:
            blocklist += BLOCKLIST_OPTIX_OSL
    if device == 'METAL':
        blocklist += BLOCKLIST_METAL
    if args.osl:
        blocklist += BLOCKLIST_OSL

    report = CyclesReport('Cycles', args.outdir, args.oiiotool, device, blocklist, args.osl)
    report.set_pixelated(True)
    report.set_reference_dir("cycles_renders")
    if device == 'CPU':
        report.set_compare_engine('eevee')
    else:
        report.set_compare_engine('cycles', 'CPU')

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

    test_dir_name = Path(args.testdir).name
    if (test_dir_name in {'motion_blur', 'integrator', "displacement"}) or \
       ((args.osl) and (test_dir_name in {'shader', 'hair'})):
        report.set_fail_threshold(0.032)

    # Layer mixing is different between SVM and OSL, so a few tests have
    # noticably different noise causing OSL Principled BSDF tests to fail.
    if ((args.osl) and (test_dir_name == 'principled_bsdf')):
        report.set_fail_threshold(0.06)

    ok = report.run(args.testdir, args.blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
