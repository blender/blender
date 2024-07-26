# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api


def set_view3d_context_override(context_override):
    """
    Set context override to become the first viewport in the active workspace

    The `context_override` is expected to be a copy of an actual current context
    obtained by `context.copy()`
    """

    for area in context_override["screen"].areas:
        if area.type != 'VIEW_3D':
            continue
        for space in area.spaces:
            if space.type != 'VIEW_3D':
                continue
            for region in area.regions:
                if region.type != 'WINDOW':
                    continue
                context_override["area"] = area
                context_override["region"] = region


def prepare_sculpt_scene(context):
    import bpy
    """
    Prepare a clean state of the scene suitable for benchmarking

    It creates a high-res object and moves it to a sculpt mode.
    """

    # Ensure the current mode is object, as it might not be the always the case
    # if the benchmark script is run from a non-clean state of the .blend file.
    if context.object:
        bpy.ops.object.mode_set(mode='OBJECT')

    # Delete all current objects from the scene.
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.outliner.orphans_purge()

    group = bpy.data.node_groups.new("Test", 'GeometryNodeTree')
    group.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')
    group_output_node = group.nodes.new('NodeGroupOutput')

    size = 1500

    grid_node = group.nodes.new('GeometryNodeMeshGrid')
    grid_node.inputs["Size X"].default_value = 2.0
    grid_node.inputs["Size Y"].default_value = 2.0
    grid_node.inputs["Vertices X"].default_value = size
    grid_node.inputs["Vertices Y"].default_value = size

    group.links.new(grid_node.outputs["Mesh"], group_output_node.inputs[0])

    bpy.ops.mesh.primitive_plane_add(size=2, align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))

    ob = context.object
    md = ob.modifiers.new("Test", 'NODES')
    md.node_group = group

    bpy.ops.object.modifier_apply(modifier="Test")

    bpy.ops.object.select_all(action='SELECT')
    # Move the plane to the sculpt mode.
    bpy.ops.object.mode_set(mode='SCULPT')


def generate_stroke(context):
    """
    Generate stroke for the bpy.ops.sculpt.brush_stroke operator

    The generated stroke coves the full plane diagonal.
    """
    from mathutils import Vector

    template = {
        "name": "stroke",
        "mouse": (0.0, 0.0),
        "mouse_event": (0, 0),
        "pen_flip": False,
        "is_start": True,
        "location": (0, 0, 0),
        "pressure": 1.0,
        "time": 1.0,
        "size": 1.0,
        "x_tilt": 0,
        "y_tilt": 0
    }

    num_steps = 100
    start = Vector((-1, -1, 0))
    end = Vector((1, 1, 0))
    delta = (end - start) / (num_steps - 1)

    stroke = []
    for i in range(num_steps):
        step = template.copy()
        step["location"] = start + delta * i
        # TODO: mouse and mouse_event?
        stroke.append(step)

    return stroke


def _run(args):
    import bpy
    import time
    context = bpy.context

    # Create an undo stack explicitly. This isn't created by default in background mode.
    bpy.ops.ed.undo_push()

    prepare_sculpt_scene(context)

    context_override = context.copy()
    set_view3d_context_override(context_override)

    with context.temp_override(**context_override):
        start = time.time()
        bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override))
        end = time.time()

    result = {'time': end - start}
    # bpy.ops.wm.save_mainfile(filepath="/home/hans/Documents/test.blend")
    return result


class SculptBrushTest(api.Test):
    def __init__(self, filepath):
        self.filepath = filepath

    def name(self):
        return self.filepath.stem

    def category(self):
        return "sculpt"

    def run(self, env, device_id):
        args = {}

        result, _ = env.run_in_blender(_run, args, [self.filepath])

        return result


def generate(env):
    filepaths = env.find_blend_files('sculpt/*')
    return [SculptBrushTest(filepath) for filepath in filepaths]
