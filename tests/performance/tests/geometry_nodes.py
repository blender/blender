# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api


def _run(args):
    import bpy
    import time

    # Evaluate objects once first, to avoid any possible lazy evaluation later.
    bpy.context.view_layer.update()

    test_time_start = time.time()
    measured_times = []

    min_measurements = 5
    max_measurements = 100
    timeout = 5

    while True:
        # Tag all objects with geometry nodes modifiers to be recalculated.
        for ob in bpy.context.view_layer.objects:
            for modifier in ob.modifiers:
                if modifier.type == 'NODES':
                    ob.update_tag()
                    break

        start_time = time.time()
        bpy.context.view_layer.update()
        elapsed_time = time.time() - start_time
        measured_times.append(elapsed_time)

        if len(measured_times) >= min_measurements and test_time_start + timeout < time.time():
            break
        if len(measured_times) >= max_measurements:
            break

    average_time = sum(measured_times) / len(measured_times)
    result = {'time': average_time}
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
