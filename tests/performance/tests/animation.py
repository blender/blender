# Apache License, Version 2.0

import api
import os


def _run(args):
    import bpy
    import time

    start_time = time.time()

    scene = bpy.context.scene
    for i in range(scene.frame_start, scene.frame_end):
        scene.frame_set(scene.frame_start)

    elapsed_time = time.time() - start_time

    result = {'time': elapsed_time}
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
        result, _ = env.run_in_blender(_run, args)
        return result


def generate(env):
    filepaths = env.find_blend_files('animation')
    return [AnimationTest(filepath) for filepath in filepaths]
