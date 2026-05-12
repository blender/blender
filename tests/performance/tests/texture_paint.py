# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import api
import enum
import pathlib


class MeshType(enum.IntEnum):
    CUBE = 0
    MONKEY = 1
    SUBDIV_3_MONKEY = 2


class DataType(enum.IntEnum):
    BYTE = 0
    FLOAT = 1


DIMENSIONS = [1024, 4096]


def set_view3d_context_override(context_override):
    """
    Set context override to become the first viewport in the active workspace

    The ``context_override`` is expected to be a copy of an actual current context
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


def prepare_scene(context: any, object: MeshType, image_dimension: int, data_type: DataType):
    """
    Prepare a clean state of the scene suitable for benchmarking
    """
    import bpy

    bpy.context.preferences.experimental.use_sculpt_texture_paint = True

    # Ensure the current mode is object, as it might not be always the case
    # if the benchmark script is run from a non-clean state of the .blend file.
    if context.object:
        bpy.ops.object.mode_set(mode='OBJECT')

    # Delete all current objects from the scene.
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.outliner.orphans_purge()

    if object == MeshType.MONKEY:
        bpy.ops.mesh.primitive_monkey_add(size=2, align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
    elif object == MeshType.CUBE:
        bpy.ops.mesh.primitive_cube_add(size=2, align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
    elif object == MeshType.SUBDIV_3_MONKEY:
        bpy.ops.mesh.primitive_monkey_add(size=2, align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.ops.object.subdivision_set(level=3, relative=False, ensure_modifier=True)
        bpy.ops.object.modifier_apply(modifier="Subdivision")
    else:
        raise NotImplementedError

    context_override = context.copy()
    set_view3d_context_override(context_override)
    with context.temp_override(**context_override):
        bpy.ops.view3d.view_axis(type='FRONT')
        bpy.ops.view3d.view_selected()
    bpy.ops.object.mode_set(mode='SCULPT')

    is_float_image = data_type == DataType.FLOAT

    bpy.ops.paint.add_texture_paint_slot(
        type='BASE_COLOR',
        slot_type='IMAGE',
        name="Untitled",
        color=(
            1.0,
            1.0,
            1.0,
            1.0),
        width=image_dimension,
        height=image_dimension,
        alpha=True,
        generated_type='BLANK',
        float=is_float_image)


def prepare_brush():
    import bpy
    bpy.ops.brush.asset_activate(
        asset_library_type='ESSENTIALS',
        relative_asset_identifier="brushes/essentials_brushes-mesh_sculpt.blend/Brush/Paint Hard")


def generate_stroke(context):
    """
    Generate stroke for the bpy.ops.sculpt.brush_stroke operator

    The generated stroke coves the full plane diagonal.
    """
    import bpy
    from mathutils import Vector

    template = {
        "name": "stroke",
        "mouse": (0.0, 0.0),
        "mouse_event": (0, 0),
        "is_start": True,
        "location": (0, 0, 0),
        "pressure": 1.0,
        "time": 1.0,
        "size": 1.0,
        "x_tilt": 0,
        "y_tilt": 0
    }

    version = bpy.app.version
    if version[0] <= 4 and version[1] <= 3:
        template["pen_flip"] = False

    num_steps = 100
    start = Vector((context["area"].width, context["area"].height))
    end = Vector((0, 0))
    delta = (end - start) / (num_steps - 1)

    stroke = []
    for i in range(num_steps):
        step = template.copy()
        step["mouse_event"] = start + delta * i
        stroke.append(step)

    return stroke


def _run_brush_test(args: dict):
    import bpy
    import time

    # This test can only run in alpha, for now, due to the texture paint mode being an experimental feature
    if bpy.app.version_cycle != 'alpha':
        return {"time": 0.0}

    context = bpy.context

    timeout = 10
    total_time_start = time.time()

    # Create an undo stack explicitly. This isn't created by default in background mode.
    bpy.ops.ed.undo_push()

    prepare_brush()

    min_measurements = 5
    max_measurements = 100
    measurements = []
    while True:
        prepare_scene(context, args["object_type"], args["dimension"], args["data_type"])
        context_override = context.copy()
        set_view3d_context_override(context_override)
        with context.temp_override(**context_override):
            start = time.time()
            bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)
            bpy.ops.ed.undo_push()
            measurements.append(time.time() - start)
        if len(measurements) >= min_measurements and (time.time() - total_time_start) > timeout:
            break
        if len(measurements) >= max_measurements:
            break

    return {"time": sum(measurements) / len(measurements)}


class TexturePaintBrushTest(api.Test):
    def __init__(self, filepath: pathlib.Path, object_type: MeshType, dimension: int, data_type: DataType):
        self.filepath = filepath
        self.object_type = object_type
        self.dimension = dimension
        self.data_type = data_type

    def name(self):
        return "{}_{}_{}".format(self.object_type.name.lower(), self.data_type.name.lower(), self.dimension)

    def category(self):
        return "texture_paint"

    def run(self, env, _device_id, _gpu_backend):
        args = {
            'object_type': self.object_type,
            'dimension': self.dimension,
            'data_type': self.data_type
        }

        result, _ = env.run_in_blender(_run_brush_test, args, [self.filepath])

        return result


def generate(env):
    filepaths = env.find_blend_files('texture_paint/*')
    # For now, we only expect there to ever be a single file to use as the basis for generating other brush tests
    assert len(filepaths) == 1

    brush_tests = [TexturePaintBrushTest(filepaths[0], object_type, dimension, data_type)
                   for object_type in MeshType for dimension in DIMENSIONS for data_type in DataType]
    return brush_tests
