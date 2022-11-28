# SPDX-License-Identifier: Apache-2.0

import api
import os


def _run(args):
    import bpy
    import time

    # Evaluate objects once first, to avoid any possible lazy evaluation later.
    bpy.context.view_layer.update()

    # Tag all objects with geometry nodes modifiers to be recalculated.
    for ob in bpy.context.view_layer.objects:
        for modifier in ob.modifiers:
            if modifier.type == 'NODES':
                ob.update_tag()
                break

    start_time = time.time()
    bpy.context.view_layer.update()
    elapsed_time = time.time() - start_time

    result = {'time': elapsed_time}
    return result


class GeometryNodesTest(api.Test):
    def __init__(self, filepath):
        self.filepath = filepath

    def name(self):
        return self.filepath.stem

    def category(self):
        return "geometry_nodes"

    def run(self, env, device_id):
        args = {}

        result, _ = env.run_in_blender(_run, args, [self.filepath])

        return result


def generate(env):
    filepaths = env.find_blend_files('geometry_nodes/*')
    return [GeometryNodesTest(filepath) for filepath in filepaths]
