# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.

Select-picking elements in pose & edit mode with the object type selectability toggles
in the 3D viewport header (the "Selectability & Visibility" settings).
"""
import modules.ui_test_utils as ui

# The posed armature, with a single bone.
ARMATURE_LOCATION = (6.0, 0.0, 0.0)
# The middle of the posed armature's bone.
BONE_PICK_CO = (6.0, 0.0, 1.0)

METABALL_LOCATION = (-12.0, 0.0, 0.0)


# -----------------------------------------------------------------------------
# Utilities

def _view3d_region(area):
    return next(region for region in area.regions if region.type == 'WINDOW')


def _view3d_location_from_world(area, co):
    """Region relative pixel location of a world-space coordinate."""
    from bpy_extras.view3d_utils import location_3d_to_region_2d
    region = _view3d_region(area)
    xy = location_3d_to_region_2d(region, region.data, co)
    return int(xy[0]), int(xy[1])


def _view3d_select_at(window, area, co):
    """Select-pick a world-space coordinate by calling ``view3d.select`` directly."""
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.view3d.select(location=_view3d_location_from_world(area, co), deselect_all=True)
    yield


def _show_object_reset(space, *, disable=()):
    """
    Show & allow selecting all object types except ``disable``:
    the ``show_object_*`` attributes without their prefix, e.g. "select_mesh".
    """
    prefix = "show_object_"
    toggles = {
        prop.identifier[len(prefix):]
        for prop in space.bl_rna.properties
        if prop.identifier.startswith((prefix + "viewport_", prefix + "select_"))
    }
    toggles_unknown = set(disable) - toggles
    assert not toggles_unknown, toggles_unknown
    for toggle in toggles:
        setattr(space, prefix + toggle, toggle not in disable)
    yield


def _scene_setup():
    """
    Replace the startup scene with, in a front orthographic view:

    - "Armature": a single bone, with "Weight Paint Cube" parented to it.
    """
    import bpy

    e, t, window = ui.test_window()
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    with bpy.context.temp_override(window=window, area=area):
        bpy.ops.screen.screen_full_area(use_hide_panels=True)
    # Maximizing creates a new screen, the area must be looked up again.
    yield
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    region = _view3d_region(area)

    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob)

    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.armature_add(radius=2.0, location=ARMATURE_LOCATION)
        ob_armature = window.view_layer.objects.active
        ob_armature.name = "Armature"

        bpy.ops.mesh.primitive_cube_add(size=1.5)
        ob_weight_paint_cube = window.view_layer.objects.active
        ob_weight_paint_cube.name = "Weight Paint Cube"
        ob_weight_paint_cube.parent = ob_armature
        ob_weight_paint_cube.parent_type = 'ARMATURE'
        ob_weight_paint_cube.location = (0.0, 0.0, -3.0)

        bpy.ops.object.select_all(action='DESELECT')
        window.view_layer.objects.active = None

        bpy.ops.view3d.view_axis(type='FRONT')
        bpy.ops.view3d.view_all()

    # Reset the toggles & draw once so the view matrices are valid before projecting locations.
    yield from _show_object_reset(area.spaces.active)
    return e, t, window, area


def _metaball_add(window, area):
    """Add "Metaball", a single ball."""
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.metaball_add(type='BALL', radius=2.0, location=METABALL_LOCATION)
        ob_metaball = window.view_layer.objects.active
        ob_metaball.name = "Metaball"

        bpy.ops.object.select_all(action='DESELECT')
        window.view_layer.objects.active = None
        bpy.ops.view3d.view_all()
    yield


def _object_mode_enter(window, area):
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.mode_set(mode='OBJECT')
    yield


def _pose_mode_enter(window, area, ob_armature):
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
        ob_armature.select_set(True)
        window.view_layer.objects.active = ob_armature
        bpy.ops.object.mode_set(mode='POSE')
    yield


def _edit_mode_enter(window, area, ob):
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        window.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
    yield


def _weight_paint_mode_enter(window, area, ob_armature, ob):
    """Enter weight paint with the armature selected, so it enters pose mode too."""
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
        ob_armature.select_set(True)
        ob.select_set(True)
        window.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
    yield


# -----------------------------------------------------------------------------
# Tests

def pose_bone_select_with_armature_unselectable():
    """
    Bones remain selectable in pose mode when the armature can't be selected,
    whether by its type's selection toggle or the outliner's.
    The same applies to the type's toggle when weight painting an object the armature deforms.
    This checks the current behavior, not a requirement,
    see the element select note in ``draw_context.cc``.
    Drawing doesn't depend on selectability, so only picking is checked.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    objects = window.view_layer.objects
    ob_armature = objects["Armature"]

    def pick_bone():
        # Deselect the bone so the pick must select it.
        pose_bone = ob_armature.pose.bones[0]
        pose_bone.select = False
        yield from _view3d_select_at(window, area, BONE_PICK_CO)
        t.assertTrue(pose_bone.select)

    yield from _pose_mode_enter(window, area, ob_armature)
    t.assertEqual(ob_armature.mode, 'POSE')

    # Pose mode, "Armature" selection disabled: the bone is picked.
    yield from _show_object_reset(space, disable={"select_armature"})
    yield from pick_bone()

    # Pose mode, the armature unselectable in the outliner: the bone is picked.
    yield from _show_object_reset(space)
    ob_armature.hide_select = True
    yield
    yield from pick_bone()
    # Restore the outliner toggle.
    ob_armature.hide_select = False
    yield

    # Weight paint on an object the armature deforms, "Armature" selection disabled: the bone is picked.
    yield from _object_mode_enter(window, area)
    yield from _weight_paint_mode_enter(window, area, ob_armature, objects["Weight Paint Cube"])
    t.assertEqual(objects["Weight Paint Cube"].mode, 'WEIGHT_PAINT')
    t.assertEqual(ob_armature.mode, 'POSE')
    yield from _show_object_reset(space, disable={"select_armature"})
    yield from pick_bone()


def edit_mode_element_select_with_type_unselectable():
    """
    Edit bones & metaball elements remain selectable in edit mode when their object can't be selected,
    whether by its type's selection toggle or the outliner's.
    This checks the current behavior, not a requirement,
    see the element select note in ``draw_context.cc``.
    Drawing doesn't depend on selectability, so only picking is checked.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    yield from _metaball_add(window, area)
    objects = window.view_layer.objects

    def pick_edit_bone(ob):
        # Deselect the bone so the pick must select it.
        edit_bone = ob.data.edit_bones[0]
        edit_bone.select = False
        edit_bone.select_head = False
        edit_bone.select_tail = False
        yield from _view3d_select_at(window, area, BONE_PICK_CO)
        t.assertTrue(ob.data.edit_bones[0].select)

    def pick_metaball_element(ob):
        from mathutils import Vector
        # Deselect the element so the pick must select it.
        element = ob.data.elements[0]
        element.select = False
        # On the element's radius circle, which is drawn for selection.
        co = ob.matrix_world @ (element.co + Vector((element.radius, 0.0, 0.0)))
        yield from _view3d_select_at(window, area, co)
        t.assertTrue(ob.data.elements[0].select)

    for ob, toggle, pick in (
            (objects["Armature"], "select_armature", pick_edit_bone),
            (objects["Metaball"], "select_meta", pick_metaball_element),
    ):
        yield from _edit_mode_enter(window, area, ob)
        t.assertEqual(ob.mode, 'EDIT')

        # Everything selectable: the element is picked.
        yield from pick(ob)

        # The object's type selection disabled: the element is picked.
        yield from _show_object_reset(space, disable={toggle})
        yield from pick(ob)

        # The object unselectable in the outliner: the element is picked.
        yield from _show_object_reset(space)
        ob.hide_select = True
        yield
        yield from pick(ob)
        # Restore the outliner toggle.
        ob.hide_select = False
        yield

        yield from _object_mode_enter(window, area)
