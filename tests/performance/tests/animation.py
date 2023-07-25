# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import api


def _run(args):
    import bpy
    import time

    start_time = time.time()
    elapsed_time = 0.0
    num_frames = 0

    while elapsed_time < 10.0:
        scene = bpy.context.scene
        for i in range(scene.frame_start, scene.frame_end + 1):
            scene.frame_set(i)

        num_frames += scene.frame_end + 1 - scene.frame_start
        elapsed_time = time.time() - start_time

    time_per_frame = elapsed_time / num_frames

    result = {'time': time_per_frame}
    return result


class AnimationTest(api.Test):
    def __init__(self, filepath):
        self.filepath = filepath

    def name(self):
        return self.filepath.stem

    def category(self):
        return "animation"

    def run(self, env, device_id):
        args = {}
        result, _ = env.run_in_blender(_run, args, [self.filepath])
        return result


def generate(env):
    filepaths = env.find_blend_files('animation/*')
    return [AnimationTest(filepath) for filepath in filepaths]
