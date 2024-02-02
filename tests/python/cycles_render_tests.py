#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2015-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import shlex
import sys
from pathlib import Path

# List of .blend files that are known to be failing and are not ready to be
# tested, or that only make sense on some devices. Accepts regular expressions.
BLACKLIST_ALL = [
    # Blacklisted due overlapping object differences between platforms.
    "hair_geom_reflection.blend",
    "hair_geom_transmission.blend",
    "hair_instancer_uv.blend",
    "principled_hair_directcoloring.blend",
    "visibility_particles.blend",
]

BLACKLIST_OSL = [
    # OSL only supported on CPU.
    '.*_osl.blend',
    'osl_.*.blend',
]

BLACKLIST_OPTIX = [
    # Ray intersection precision issues
    'T50164.blend',
    'T43865.blend',
]

BLACKLIST_METAL = [
    # MNEE only works on Metal with macOS >= 13
    "underwater_caustics.blend",
]

BLACKLIST_GPU = [
    # Uninvestigated differences with GPU.
    'image_log.blend',
    'T40964.blend',
    'T45609.blend',
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
    # Inconsistent handling of overlapping objects.
    "T41143.blend",
    "visibility_particles.blend",
    # No path guiding on GPU.
    "guiding*.blend",
]


def get_arguments(filepath, output_filepath):
    dirname = os.path.dirname(filepath)
    basedir = os.path.dirname(dirname)
    subject = os.path.basename(dirname)

    args = [
        "--background",
        "-noaudio",
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

    if subject == 'bake':
        args.extend(['--python', os.path.join(basedir, "util", "render_bake.py")])
    elif subject == 'denoise_animation':
        args.extend(['--python', os.path.join(basedir, "util", "render_denoise.py")])
    else:
        args.extend(["-f", "1"])

    return args


def create_argparse():
    parser = argparse.ArgumentParser()
    parser.add_argument("-blender", nargs="+")
    parser.add_argument("-testdir", nargs=1)
    parser.add_argument("-outdir", nargs=1)
    parser.add_argument("-oiiotool", nargs=1)
    parser.add_argument("-device", nargs=1)
    parser.add_argument("-blacklist", nargs="*")
    parser.add_argument('--batch', default=False, action='store_true')
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    oiiotool = args.oiiotool[0]
    output_dir = args.outdir[0]
    device = args.device[0]

    blacklist = BLACKLIST_ALL
    if device != 'CPU':
        blacklist += BLACKLIST_GPU
    if device != 'CPU' or 'OSL' in args.blacklist:
        blacklist += BLACKLIST_OSL
    if device == 'OPTIX':
        blacklist += BLACKLIST_OPTIX
    if device == 'METAL':
        blacklist += BLACKLIST_METAL

    from modules import render_report
    report = render_report.Report('Cycles', output_dir, oiiotool, device, blacklist)
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
    test_dir_name = Path(test_dir).name
    if test_dir_name in ('motion_blur', 'integrator', ):
        report.set_fail_threshold(0.032)
    if test_dir_name == "denoise":
        report.set_fail_threshold(0.25)

    ok = report.run(test_dir, blender, get_arguments, batch=args.batch)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
