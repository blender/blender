# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

__all__ = (
    "BackendType",
    "COLOR_BACKEND_TYPES",
    "MASK_BACKEND_TYPES",
    "AttributeType",
    "get_attribute_data",
    "set_view3d_context_override",
    "generate_stroke",
    "generate_monkey"
)

from enum import Enum, unique


@unique
class BackendType(Enum):
    MESH = 0
    MULTIRES = 1


@unique
class AttributeType(Enum):
    POSITION = 0
    MASK = 1
    FACE_SET = 2
    COLOR = 3
    COLOR_CORNER = 4


COLOR_BACKEND_TYPES = [BackendType.MESH]

# Applying a multires mesh does not transfer mask values.
# See #153743 for a tracking issue to enable these tests
MASK_BACKEND_TYPES = [BackendType.MESH]


def _get_mesh(backend_type):
    import bpy
    if backend_type == BackendType.MESH:
        return bpy.context.active_object.data
    else:
        # Duplicate and apply the modifier
        bpy.ops.sculpt.sculptmode_toggle()

        original_object = bpy.data.objects['Suzanne']
        bpy.ops.object.select_all(action='DESELECT')
        original_object.select_set(True)

        bpy.ops.object.duplicate()

        duplicate_object = bpy.context.selected_objects[0]
        bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]

        bpy.context.active_object.modifiers['Multires'].levels = 1

        bpy.ops.object.modifier_apply(modifier='Multires')

        # Restore initial object as "active" and return to sculpt mode
        bpy.context.view_layer.objects.active = original_object
        bpy.ops.sculpt.sculptmode_toggle()

        return duplicate_object.data


def get_attribute_data(backend_type, attribute_type):
    if attribute_type in {AttributeType.COLOR, AttributeType.COLOR_CORNER} and backend_type == BackendType.MULTIRES:
        raise Exception("Multires does not support color attributes")

    import numpy as np
    mesh = _get_mesh(backend_type)

    match attribute_type:
        case AttributeType.POSITION:
            attribute_name = 'position'
            attribute_domain = 'POINT'
            attribute_size = 3
            attribute_data_type = np.float32
            is_color = False
        case AttributeType.MASK:
            attribute_name = '.sculpt_mask'
            attribute_domain = 'POINT'
            attribute_size = 1
            attribute_data_type = np.float32
            is_color = False
        case AttributeType.FACE_SET:
            attribute_name = '.sculpt_face_set'
            attribute_domain = 'FACE'
            attribute_size = 1
            attribute_data_type = np.int32
            is_color = False
        case AttributeType.COLOR:
            attribute_name = 'Color'
            attribute_domain = 'POINT'
            attribute_size = 4
            attribute_data_type = np.float32
            is_color = True
        case AttributeType.COLOR_CORNER:
            attribute_name = 'Color'
            attribute_domain = 'CORNER'
            attribute_size = 4
            attribute_data_type = np.float32
            is_color = True
        case _:
            raise Exception("Invalid type specified")

    num_elements = mesh.attributes.domain_size(attribute_domain)
    attribute_data = np.zeros((num_elements * attribute_size), dtype=attribute_data_type)

    attribute = mesh.attributes.get(attribute_name)
    if is_color:
        meta_attribute = 'color'
    else:
        if attribute_size > 1:
            meta_attribute = 'vector'
        else:
            meta_attribute = 'value'

    if attribute:
        attribute.data.foreach_get(meta_attribute, np.ravel(attribute_data))

    return attribute_data


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


def generate_monkey(backend):
    """
    Create a dense enough mesh to use for testing.
    """
    import bpy
    bpy.ops.mesh.primitive_monkey_add()

    context_override = bpy.context.copy()
    set_view3d_context_override(context_override)
    with bpy.context.temp_override(**context_override):
        bpy.ops.view3d.view_axis(type='FRONT')
        bpy.ops.view3d.view_selected()

    if backend == BackendType.MESH:
        bpy.ops.object.subdivision_set(level=2, relative=False, ensure_modifier=True)
        bpy.ops.object.modifier_apply(modifier="Subdivision")

    bpy.ops.ed.undo_push()
    bpy.ops.sculpt.sculptmode_toggle()

    if backend == BackendType.MULTIRES:
        bpy.ops.object.subdivision_set(level=2, relative=False, ensure_modifier=True)


def generate_stroke(context, start_percent=(0.0, 0.0), end_percent=(1.0, 1.0)):
    """
    Generate stroke for any of the paint mode operators (e.g. bpy.ops.sculpt.brush_stroke_

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

    num_steps = 50

    start = Vector((0 * start_percent[0], 0 * start_percent[1]))
    end = Vector((context['area'].width * end_percent[0], context['area'].height * end_percent[1]))
    delta = (end - start) / (num_steps - 1)

    stroke = []
    for i in range(num_steps):
        step = template.copy()
        step["mouse_event"] = start + delta * i
        stroke.append(step)

    return stroke
