# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api


MODIFIER_ENABLE_MARKER = "benchmark_enable_modifier"


def _find_marked_modifiers(bpy):
    marked_modifiers = []

    for ob in bpy.data.objects:
        if MODIFIER_ENABLE_MARKER not in ob:
            continue

        marker = ob[MODIFIER_ENABLE_MARKER]
        if type(marker) is not bool or not marker:
            raise RuntimeError(
                f"Invalid {MODIFIER_ENABLE_MARKER!r} marker on object {ob.name!r}: "
                "expected the Boolean value True"
            )

        if bpy.context.view_layer.objects.get(ob.name) is None:
            raise RuntimeError(
                f"Object {ob.name!r} marked with {MODIFIER_ENABLE_MARKER!r} "
                "is not in the active view layer"
            )

        nodes_modifiers = [modifier for modifier in ob.modifiers if modifier.type == 'NODES']
        if not nodes_modifiers:
            raise RuntimeError(
                f"Object {ob.name!r} marked with {MODIFIER_ENABLE_MARKER!r} "
                "has no Geometry Nodes modifier; a marked object must have exactly one"
            )
        if len(nodes_modifiers) > 1:
            raise RuntimeError(
                f"Object {ob.name!r} marked with {MODIFIER_ENABLE_MARKER!r} "
                f"has {len(nodes_modifiers)} Geometry Nodes modifiers; "
                "a marked object must have exactly one"
            )

        modifier = nodes_modifiers[0]
        if modifier.show_viewport:
            raise RuntimeError(
                f"Geometry Nodes modifier {modifier.name!r} on object {ob.name!r} "
                f"marked with {MODIFIER_ENABLE_MARKER!r} is already enabled"
            )

        marked_modifiers.append((ob, modifier))

    return marked_modifiers


def _run(args):
    import bpy
    import time

    marked_modifiers = _find_marked_modifiers(bpy)

    # Evaluate objects once first, to avoid any possible lazy evaluation later.
    bpy.context.view_layer.update()

    if marked_modifiers:
        # Enable the marked modifiers outside the timed section. The following update is the
        # dependency-graph evaluation caused by enabling them.
        for ob, modifier in marked_modifiers:
            modifier.show_viewport = True
            ob.update_tag()

        start_time = time.time()
        bpy.context.view_layer.update()
        elapsed_time = time.time() - start_time
        return {'time': elapsed_time}

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

    def run(self, env, device_id, gpu_backend):
        args = {}

        result, _ = env.run_in_blender(_run, args, [self.filepath])

        return result


def generate(env):
    filepaths = env.find_blend_files('geometry_nodes/*')
    return [GeometryNodesTest(filepath) for filepath in filepaths]
