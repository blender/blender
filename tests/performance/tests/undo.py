# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api


# Validate performances when one heavy geometry is in the scene:
#  - Writing and loading memfile undo steps of changes in the heavy geometry itself.
#  - Writing and loading memfile undo steps of changes to the object using the heavy geometry.
def _run_heavy_geometry(dummy_):
    import bpy
    import mathutils
    import time

    ob = bpy.data.objects["Cube"]
    assert (bpy.context.object == ob)
    ob.modifiers.new(name="Subsurf", type='SUBSURF').levels = 9
    bpy.ops.object.modifier_apply(modifier="Subsurf")

    start_time = time.time()
    # NOTE: The first undo push is necessary to be able to undo, since it creates the
    # initial state for memfile undo (it is not initialized by default in background mode).
    bpy.ops.ed.undo_push()

    # Empty undo step.
    bpy.ops.ed.undo_push()
    bpy.ops.ed.undo()
    bpy.ops.ed.redo()

    # Object-modified undo step.
    ob = bpy.data.objects["Cube"]
    assert (bpy.context.object == ob)
    ob.location.x += 1.0
    bpy.ops.ed.undo_push()
    bpy.ops.ed.undo()
    bpy.ops.ed.undo()
    bpy.ops.ed.redo()
    bpy.ops.ed.redo()

    # Mesh-modified undo step.
    ob = bpy.data.objects["Cube"]
    assert (bpy.context.object == ob)
    ob.data.transform(mathutils.Matrix.Translation((1, 0, 0)))
    bpy.ops.ed.undo_push()
    bpy.ops.ed.undo()
    bpy.ops.ed.undo()
    bpy.ops.ed.undo()
    bpy.ops.ed.redo()
    bpy.ops.ed.redo()
    bpy.ops.ed.redo()
    elapsed_time = time.time() - start_time

    result = {'time': elapsed_time, 'undo_stack_memory': getattr(bpy.app, "memory_usage_undo", lambda: 0)()}
    return result


class BlendUndoMemfileHeavyGeometryTest(api.Test):
    def name(self):
        return "undo_memfile_heavy_mesh_geometry"

    def category(self):
        return "undo"

    def run(self, env, device_id, gpu_backend):
        result, _ = env.run_in_blender(_run_heavy_geometry, {}, ["--factory-startup"])
        return result


# Validate performances when an extremely large amount of small independant blocks of data are present.
# This is generating many IDProperties in an ID.
def _run_many_bheads_and_pointers(args):
    import bpy
    import mathutils
    import time

    num_props_per_level = args["num_props_per_level"]
    num_levels = args["num_levels"]

    # Recursively generate idproperties containing other idproperties.
    def gen_idprops(id_prop_owner, num_props_per_level, num_levels, curr_level):
        if curr_level == num_levels:
            for i in range(num_props_per_level):
                id_prop_owner[str(i)] = i
        else:
            for i in range(num_props_per_level):
                id_prop_owner[str(i)] = {}
                gen_idprops(id_prop_owner[str(i)], num_props_per_level, num_levels, curr_level + 1)

    ob = bpy.data.objects["Cube"]

    # Generate many IDProps in the object.
    ob['test_idproperties'] = {}
    gen_idprops(ob['test_idproperties'], num_props_per_level, num_levels, 1)

    start_time = time.time()
    # NOTE: The first undo push is necessary to be able to undo, since it creates the
    # initial state for memfile undo (it is not initialized by default in background mode).
    bpy.ops.ed.undo_push()

    # Empty undo step.
    bpy.ops.ed.undo_push()
    bpy.ops.ed.undo()
    bpy.ops.ed.redo()

    # Object-modified undo step.
    ob = bpy.data.objects["Cube"]
    ob['test_idproperties_empty'] = {}
    bpy.ops.ed.undo_push()
    bpy.ops.ed.undo()
    bpy.ops.ed.undo()
    bpy.ops.ed.redo()
    bpy.ops.ed.redo()
    elapsed_time = time.time() - start_time

    result = {'time': elapsed_time, 'undo_stack_memory': getattr(bpy.app, "memory_usage_undo", lambda: 0)()}
    return result


class BlendUndoMemfileManyPointersTest(api.Test):
    def name(self):
        return "undo_memfile_1M_bheads_and_pointers"

    def category(self):
        return "undo"

    def run(self, env, device_id, gpu_backend):
        result, _ = env.run_in_blender(
            _run_many_bheads_and_pointers,
            # Will generate 100^3, i.e. 1M idprops.
            {"num_props_per_level": 100, "num_levels": 3},
            ["--factory-startup"]
        )
        return result


def generate(env):
    return [BlendUndoMemfileHeavyGeometryTest(), BlendUndoMemfileManyPointersTest()]
