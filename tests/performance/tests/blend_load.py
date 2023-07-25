# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import api
import os
import pathlib


def _run(filepath):
    import bpy
    import time

    # Load once to ensure it's cached by OS
    bpy.ops.wm.open_mainfile(filepath=filepath)
    bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

    # Measure loading the second time
    start_time = time.time()
    bpy.ops.wm.open_mainfile(filepath=filepath)
    elapsed_time = time.time() - start_time

    result = {'time': elapsed_time}
    return result


class BlendLoadTest(api.Test):
    def __init__(self, filepath):
        self.filepath = filepath

    def name(self):
        return self.filepath.stem

    def category(self):
        return "blend_load"

    def run(self, env, device_id):
        result, _ = env.run_in_blender(_run, str(self.filepath))
        return result


def generate(env):
    filepaths = env.find_blend_files('*/*')
    return [BlendLoadTest(filepath) for filepath in filepaths]
