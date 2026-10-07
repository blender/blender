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

UDIM_OBJECT_NAME = "body_GEO_NEW"
UDIM_UV_NAME = "godzilUV"
UDIM_BRUSH_SIZE = 500
UDIM_SUBDIVIDE_LEVEL = 3
UDIM_CANVAS_IMAGE = {
    DataType.BYTE: "overlayColor",
    DataType.FLOAT: "scalesScale_bump_roughness",
}


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

    bpy.context.preferences.experimental.use_3d_texture_paint = True

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
    bpy.ops.paint.texture_paint_toggle()

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
        relative_asset_identifier="brushes/essentials_brushes-mesh_texture.blend/Brush/Paint Hard")


def generate_stroke(context):
    """
    Generate stroke for the bpy.ops.paint.image_paint operator

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


def _measure_brush_strokes(context, test_setup: bool, total_time_start, timeout):
    import bpy
    import time

    context_override = context.copy()
    set_view3d_context_override(context_override)

    paint_object = context.view_layer.objects.active

    min_measurements = 5
    max_measurements = 100
    measurements = []
    with context.temp_override(**context_override):
        stroke = generate_stroke(context_override)

        # Once for warmup, to do initial setup.
        bpy.ops.paint.image_paint(stroke=stroke, override_location=True)
        bpy.ops.ed.undo_push()

        while True:
            if test_setup:
                # Invalidate PBVH pixel data to measure setup.
                paint_object.update_tag()
                context.view_layer.update()

                # Perform a stroke first to get the setup and stroke timing
                start = time.time()
                bpy.ops.paint.image_paint(stroke=stroke, override_location=True)
                setup_plus_stroke = time.time() - start
                bpy.ops.ed.undo_push()

                # Perform a second stroke to get the stroke timing
                start = time.time()
                bpy.ops.paint.image_paint(stroke=stroke, override_location=True)
                stroke_only = time.time() - start
                measurements.append(setup_plus_stroke - stroke_only)
            else:
                # Time stroke only.
                start = time.time()
                bpy.ops.paint.image_paint(stroke=stroke, override_location=True)
                measurements.append(time.time() - start)

            bpy.ops.ed.undo_push()
            if len(measurements) >= min_measurements and (time.time() - total_time_start) > timeout:
                break
            if len(measurements) >= max_measurements:
                break

    return {"time": sum(measurements) / len(measurements)}


def _run_brush_test(args: dict):
    import bpy
    import time

    # This test can only run in alpha, for now, due to the texture paint mode being an experimental feature
    if bpy.app.version_cycle != 'alpha':
        return {"time": 0.0}

    context = bpy.context

    timeout = 10
    total_time_start = time.time()

    # Keep only a few undo steps to limit memory usage.
    context.preferences.edit.undo_steps = 2

    # Create an undo stack explicitly. This isn't created by default in background mode.
    bpy.ops.ed.undo_push()

    prepare_scene(context, args["object_type"], args["dimension"], args["data_type"])
    context_override = context.copy()
    set_view3d_context_override(context_override)
    with context.temp_override(**context_override):
        # This needs to run in a View3D context to work correctly for texture paint
        prepare_brush()

    return _measure_brush_strokes(context, args.get("test_setup", False), total_time_start, timeout)


def prepare_udim_scene(context: any, data_type: DataType, subdivide_level: int = 0):
    import bpy

    bpy.context.preferences.experimental.use_3d_texture_paint = True

    # Ensure the current mode is object, as the file may be saved in another mode.
    if context.object:
        bpy.ops.object.mode_set(mode='OBJECT')

    body = bpy.data.objects[UDIM_OBJECT_NAME]

    # Bake into static mesh without modifiers.
    depsgraph = context.evaluated_depsgraph_get()
    baked_mesh = bpy.data.meshes.new_from_object(
        body.evaluated_get(depsgraph), preserve_all_data_layers=True, depsgraph=depsgraph)
    paint_object = bpy.data.objects.new("udim_paint_target", baked_mesh)
    context.scene.collection.objects.link(paint_object)
    paint_object.matrix_world = body.matrix_world
    context.view_layer.objects.active = paint_object

    # Remove other objects.
    for other in list(bpy.data.objects):
        if other is not paint_object:
            bpy.data.objects.remove(other, do_unlink=True)

    if subdivide_level > 0:
        # Apply subdivision.
        modifier = paint_object.modifiers.new("Subdivision", 'SUBSURF')
        modifier.levels = subdivide_level
        with context.temp_override(object=paint_object):
            bpy.ops.object.modifier_apply(modifier=modifier.name)

    # Select object.
    for ob in context.view_layer.objects:
        ob.select_set(False)
    paint_object.select_set(True)
    context.view_layer.objects.active = paint_object

    # Set active UV map and paint canvas.
    mesh = paint_object.data
    mesh.uv_layers.active = mesh.uv_layers[UDIM_UV_NAME]

    tool_settings = context.scene.tool_settings
    tool_settings.image_paint.mode = 'IMAGE'
    tool_settings.image_paint.canvas = bpy.data.images[UDIM_CANVAS_IMAGE[data_type]]

    # Set viewport.
    context_override = context.copy()
    set_view3d_context_override(context_override)
    with context.temp_override(**context_override):
        bpy.ops.view3d.view_axis(type='FRONT')
        bpy.ops.view3d.view_selected()

    bpy.ops.paint.texture_paint_toggle()

    tool_settings.image_paint.unified_paint_settings.size = UDIM_BRUSH_SIZE


def _run_udim_brush_test(args: dict):
    import bpy
    import time

    # This test can only run in alpha, for now, due to the texture paint mode being an experimental feature
    if bpy.app.version_cycle != 'alpha':
        return {"time": 0.0}

    context = bpy.context

    timeout = 10
    total_time_start = time.time()

    # Keep only a few undo steps to limit memory usage, and create an undo stack.
    context.preferences.edit.undo_steps = 2

    # Create an undo stack explicitly. This isn't created by default in background mode.
    bpy.ops.ed.undo_push()

    prepare_udim_scene(context, args["data_type"], args["subdivide_level"])
    context_override = context.copy()
    set_view3d_context_override(context_override)
    with context.temp_override(**context_override):
        # This needs to run in a View3D context to work correctly for texture paint
        prepare_brush()

    return _measure_brush_strokes(context, args.get("test_setup", False), total_time_start, timeout)


class TexturePaintBrushTest(api.Test):
    def __init__(
            self,
            filepath: pathlib.Path,
            object_type: MeshType,
            dimension: int,
            data_type: DataType,
            test_setup: bool = False):
        self.filepath = filepath
        self.object_type = object_type
        self.dimension = dimension
        self.data_type = data_type
        self.test_setup = test_setup

    def name(self):
        suffix = "_setup" if self.test_setup else "_stroke"
        return "{}_{}_{}{}".format(
            self.object_type.name.lower(), self.data_type.name.lower(), self.dimension, suffix)

    def category(self):
        return "texture_paint"

    def run(self, env, _device_id, _gpu_backend):
        args = {
            'object_type': self.object_type,
            'dimension': self.dimension,
            'data_type': self.data_type,
            'test_setup': self.test_setup,
        }

        result, _ = env.run_in_blender(_run_brush_test, args, [self.filepath])

        return result


class UDIMMonsterPaintTest(api.Test):
    def __init__(self, filepath: pathlib.Path, data_type: DataType, subdivide_level: int = 0,
                 test_setup: bool = False):
        self.filepath = filepath
        self.data_type = data_type
        self.subdivide_level = subdivide_level
        self.test_setup = test_setup

    def name(self):
        suffix = "_setup" if self.test_setup else "_stroke"
        if self.subdivide_level > 0:
            return "udim_monster_subdiv{}_{}{}".format(
                self.subdivide_level, self.data_type.name.lower(), suffix)
        return "udim_monster_{}{}".format(self.data_type.name.lower(), suffix)

    def category(self):
        return "texture_paint"

    def run(self, env, _device_id, _gpu_backend):
        args = {'data_type': self.data_type, 'subdivide_level': self.subdivide_level,
                'test_setup': self.test_setup}

        result, _ = env.run_in_blender(_run_udim_brush_test, args, [self.filepath])

        return result


def generate(env):
    filepaths = env.find_blend_files('texture_paint/*')
    blends_by_dir = {filepath.parent.name: filepath for filepath in filepaths}

    brush_tests = []

    base_filepath = blends_by_dir.get('brushes')
    if base_filepath is not None:
        for object_type in MeshType:
            for dimension in DIMENSIONS:
                for data_type in DataType:
                    brush_tests.append(TexturePaintBrushTest(base_filepath, object_type, dimension, data_type))
                    brush_tests.append(
                        TexturePaintBrushTest(base_filepath, object_type, dimension, data_type,
                                              test_setup=True))

    udim_filepath = blends_by_dir.get('udim_monster')
    if udim_filepath is not None:
        for data_type in DataType:
            for subdivide_level in (0, UDIM_SUBDIVIDE_LEVEL):
                brush_tests.append(UDIMMonsterPaintTest(udim_filepath, data_type, subdivide_level))
                brush_tests.append(
                    UDIMMonsterPaintTest(udim_filepath, data_type, subdivide_level,
                                         test_setup=True))

    return brush_tests
