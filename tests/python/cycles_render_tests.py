#!/usr/bin/env python3
# Apache License, Version 2.0

import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

# List of .blend files that are known to be failing and are not ready to be
# tested, or that only make sense on some devices. Accepts regular expressions.
BLACKLIST_OSL = [
    # OSL only supported on CPU.
    '.*_osl.blend',
    'osl_.*.blend',
]

BLACKLIST_OPTIX = [
    # No branched path on Optix.
    'T53854.blend',
    'T50164.blend',
    'portal.blend',
    'denoise_sss.blend',
    'denoise_passes.blend',
    'distant_light.blend',
    'aov_position.blend',
    'subsurface_branched_path.blend',
    'T43865.blend',
]

BLACKLIST_GPU = [
    # Missing equiangular sampling on GPU.
    'area_light.blend',
    'denoise_hair.blend',
    'point_density_.*.blend',
    'point_light.blend',
    'shadow_catcher_bpt_.*.blend',
    'sphere_light.blend',
    'spot_light.blend',
    'T48346.blend',
    'world_volume.blend',
    # Uninvestigated differences with GPU.
    'image_log.blend',
    'subsurface_behind_glass_branched.blend',
    'T40964.blend',
    'T45609.blend',
    'T48860.blend',
    'smoke_color.blend',
    'bevel_mblur.blend',
    # Inconsistency between Embree and Hair primitive on GPU.
    'hair_basemesh_intercept.blend',
    'hair_instancer_uv.blend',
    'hair_particle_random.blend',
    'principled_hair_.*.blend',
    'transparent_shadow_hair.*.blend',
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
    parser.add_argument("-idiff", nargs=1)
    parser.add_argument("-device", nargs=1)
    parser.add_argument("-blacklist", nargs="*")
    return parser


def main():
    parser = create_argparse()
    args = parser.parse_args()

    blender = args.blender[0]
    test_dir = args.testdir[0]
    idiff = args.idiff[0]
    output_dir = args.outdir[0]
    device = args.device[0]

    blacklist = []
    if device != 'CPU':
        blacklist += BLACKLIST_GPU
    if device != 'CPU' or 'OSL' in args.blacklist:
        blacklist += BLACKLIST_OSL
    if device == 'OPTIX':
        blacklist += BLACKLIST_OPTIX

    from modules import render_report
    report = render_report.Report('Cycles', output_dir, idiff, device, blacklist)
    report.set_pixelated(True)
    report.set_reference_dir("cycles_renders")
    if device == 'CPU':
        report.set_compare_engine('eevee')
    else:
        report.set_compare_engine('cycles', 'CPU')

    # Increase threshold for motion blur, see T78777.
    test_dir_name = Path(test_dir).name
    if test_dir_name == 'motion_blur':
        report.set_fail_threshold(0.032)

    ok = report.run(test_dir, blender, get_arguments, batch=True)

    sys.exit(not ok)


if __name__ == "__main__":
    main()
