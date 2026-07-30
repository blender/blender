# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.

Pie menu interaction tests driven by event simulation.

For example, these pie menus are covered:
  - Object Mode pie -> Edit Mode
  - Snap pie -> Cursor to World Origin
  - Pivot pie -> Cursor
  - Transform Orientation pie -> Cursor
  - Shading pie -> Solid
  - View pie -> Camera
  - Weight Paint lock pie -> Lock All
"""

import os

import modules.ui_test_utils as ui

# Tracking editor pie menu tests require a clip with valid tracking data.
# The synthetic tripod motion test file provides stable marker/solver state.
TRACKING_BLEND_FILE = os.path.join(
    os.path.dirname(__file__),
    "..", "..", "files", "tracking", "synthetic-tripod-motion.blend",
)


PIE_DIRECTION_OFFSETS = {
    "W": (-180, 0),
    "E": (180, 0),
    "S": (0, -180),
    "N": (0, 180),
    "NW": (-150, 150),
    "NE": (150, 150),
    "SW": (-150, -150),
    "SE": (150, -150),
}


def _setup_area(area_type):
    import bpy

    bpy.ops.wm.read_homefile(use_empty=True)
    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    area.type = area_type
    yield  # Let the event loop process the area type change.
    center = ui.get_area_center(area)
    return e, t, window, area, center


def _setup_view3d():
    import bpy

    e, t, window, area, center = yield from _setup_area("VIEW_3D")
    bpy.ops.mesh.primitive_cube_add()
    yield
    return e, t, area, center


def _setup_clip_tracking_area():
    import bpy

    assert os.path.isfile(TRACKING_BLEND_FILE), TRACKING_BLEND_FILE
    bpy.ops.wm.open_mainfile(filepath=TRACKING_BLEND_FILE)
    yield

    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    area.type = "CLIP_EDITOR"
    area.spaces.active.mode = "TRACKING"
    area.spaces.active.view = "CLIP"
    yield
    center = ui.get_area_center(area)
    return e, t, window, area, center


def _create_animation_object():
    import bpy

    anim_object = bpy.data.objects.new("PieMenuAnimObject", None)
    bpy.context.scene.collection.objects.link(anim_object)
    bpy.context.view_layer.objects.active = anim_object
    anim_object.select_set(True)
    return anim_object


def _create_animation_object_with_keyframes():
    import bpy

    anim_object = _create_animation_object()
    anim_object.location = (0.0, 0.0, 0.0)
    anim_object.keyframe_insert(data_path="location", frame=1)
    anim_object.location = (10.0, 0.0, 0.0)
    anim_object.keyframe_insert(data_path="location", frame=120)
    bpy.context.scene.frame_set(1)
    yield
    return anim_object


def _action_channelbag(animated_id):
    action = animated_id.animation_data.action
    action_slot = animated_id.animation_data.action_slot
    return action.layers[0].strips[0].channelbag(action_slot)


def _create_nla_object():
    import bpy

    nla_object = _create_animation_object()
    nla_object.location = (0.0, 0.0, 0.0)
    nla_object.keyframe_insert(data_path="location", frame=1)
    nla_object.location = (1.0, 0.0, 0.0)
    nla_object.keyframe_insert(data_path="location", frame=20)

    action = nla_object.animation_data.action
    nla_track = nla_object.animation_data.nla_tracks.new()
    nla_strip = nla_track.strips.new("PieMenuStrip", 1, action)
    nla_strip.select = True
    nla_track.is_solo = True
    nla_object.animation_data.action = None
    bpy.context.scene.frame_set(1)
    yield
    return nla_object, nla_strip


def _view2d_rect(region):
    v2d = region.view2d

    bottom_left = v2d.region_to_view(0, 0)
    top_right = v2d.region_to_view(region.width, region.height)

    return (
        bottom_left[0],
        top_right[0],
        bottom_left[1],
        top_right[1],
    )


def _interpolate_to_direction(start, offset, *, steps=6):
    start_x, start_y = start
    offset_x, offset_y = offset
    for step in range(1, steps + 1):
        factor = step / steps
        yield (
            int(start_x + (offset_x * factor)),
            int(start_y + (offset_y * factor)),
        )


def _press_and_drag_to_direction(e, center, spawn_key, direction, *, steps=6):
    # Move from the area center towards a fixed offset so the cursor
    # lands inside the requested pie menu slice.
    yield e.cursor_position_set(*center, move=True)
    yield
    spawn_key.press()
    yield
    for coords in _interpolate_to_direction(center, PIE_DIRECTION_OFFSETS[direction], steps=steps):
        yield e.cursor_position_set(*coords, move=True)
        yield
    spawn_key.release()
    yield


def test_object_mode_pie_edit():
    import bpy

    e, t, area, center = yield from _setup_view3d()
    yield from _press_and_drag_to_direction(e, center, e.ctrl.tab, "E")

    t.assertEqual(bpy.context.active_object.mode, "EDIT")


def test_snap_pie_cursor_to_world_origin():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    bpy.context.scene.cursor.location = (1.0, 2.0, 3.0)
    yield

    yield from _press_and_drag_to_direction(e, center, e.shift.s, "SW")

    t.assertEqual(tuple(bpy.context.scene.cursor.location), (0.0, 0.0, 0.0))


def test_pivot_pie_cursor():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    bpy.context.scene.tool_settings.transform_pivot_point = "MEDIAN_POINT"
    yield

    yield from _press_and_drag_to_direction(e, center, e.period, "E")

    t.assertEqual(bpy.context.scene.tool_settings.transform_pivot_point, "CURSOR")


def test_orientation_pie_cursor():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    bpy.context.scene.transform_orientation_slots[0].type = "GLOBAL"
    yield

    yield from _press_and_drag_to_direction(e, center, e.comma, "NE")

    t.assertEqual(bpy.context.scene.transform_orientation_slots[0].type, "CURSOR")


def test_shading_pie_solid():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    area.spaces.active.shading.type = "WIREFRAME"
    yield

    yield from _press_and_drag_to_direction(e, center, e.z, "E")

    t.assertEqual(area.spaces.active.shading.type, "SOLID")


def test_view_pie_camera():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    bpy.ops.object.camera_add(location=(0.0, -6.0, 0.0), rotation=(3.14 / 2, 0.0, 0.0),)
    bpy.context.scene.camera = bpy.context.active_object
    yield

    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "SW")

    t.assertEqual(area.spaces.active.region_3d.view_perspective, "CAMERA")


def test_transform_gizmo_pie_show_gizmos():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    prefs = bpy.context.window_manager.keyconfigs.active.preferences
    original_action = prefs.v3d_tilde_action
    prefs.v3d_tilde_action = "GIZMO"
    try:
        area.spaces.active.show_gizmo = False
        yield

        yield from _press_and_drag_to_direction(e, center, e.accent_grave, "N")

        t.assertTrue(area.spaces.active.show_gizmo)
    finally:
        prefs.v3d_tilde_action = original_action


def test_shading_ex_pie_toggle_overlays():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    prefs = bpy.context.window_manager.keyconfigs.active.preferences
    original_use_ex_pie = prefs.use_v3d_shade_ex_pie
    prefs.use_v3d_shade_ex_pie = True
    try:
        area.spaces.active.overlay.show_overlays = False
        yield

        yield from _press_and_drag_to_direction(e, center, e.z, "N")

        t.assertTrue(area.spaces.active.overlay.show_overlays)
    finally:
        prefs.use_v3d_shade_ex_pie = original_use_ex_pie


def test_proportional_editing_falloff_pie_sphere():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    bpy.context.scene.tool_settings.proportional_edit_falloff = "SMOOTH"
    yield

    yield from _press_and_drag_to_direction(e, center, e.shift.o, "E")

    t.assertEqual(bpy.context.scene.tool_settings.proportional_edit_falloff, "SPHERE")


def test_sculpt_automasking_pie_topology():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    yield from _press_and_drag_to_direction(e, center, e.ctrl.tab, "S")
    t.assertEqual(bpy.context.active_object.mode, "SCULPT")

    bpy.context.tool_settings.sculpt.mesh_automasking_settings.use_automasking_topology = False
    yield

    yield from _press_and_drag_to_direction(e, center, e.alt.a, "W")

    t.assertTrue(bpy.context.tool_settings.sculpt.mesh_automasking_settings.use_automasking_topology)


def test_anim_keyframe_insert_pie_location():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    prefs = bpy.context.window_manager.keyconfigs.active.preferences

    # The 'I' key defaults to a standard popup menu.
    # We must explicitly enable click-drag pies to access the pie menu variant.
    original_use_pie_click_drag = prefs.use_pie_click_drag
    prefs.use_pie_click_drag = True
    try:
        anim_object = _create_animation_object()
        anim_object.location = (1.0, 2.0, 3.0)
        bpy.context.scene.frame_set(1)
        yield

        yield from _press_and_drag_to_direction(e, center, e.i, "W")

        channelbag = _action_channelbag(anim_object)
        t.assertGreater(len(channelbag.fcurves), 0)
        t.assertEqual(channelbag.fcurves[0].data_path, "location")
    finally:
        prefs.use_pie_click_drag = original_use_pie_click_drag


def test_weight_paint_vgroup_lock_pie():
    import bpy

    e, t, area, center = yield from _setup_view3d()

    obj = bpy.context.active_object
    obj.vertex_groups.new(name="Group A")
    obj.vertex_groups.new(name="Group B")
    yield

    yield from _press_and_drag_to_direction(e, center, e.ctrl.tab, "NW")
    t.assertEqual(obj.mode, "WEIGHT_PAINT")

    yield from _press_and_drag_to_direction(e, center, e.k, "W")

    t.assertTrue(all(group.lock_weight for group in obj.vertex_groups))


def test_graph_editor_pivot_pie_cursor():
    import bpy

    e, t, window, area, center = yield from _setup_area("GRAPH_EDITOR")
    area.spaces.active.pivot_point = "BOUNDING_BOX_CENTER"
    yield

    yield from _press_and_drag_to_direction(e, center, e.period, "E")

    t.assertEqual(area.spaces.active.pivot_point, "CURSOR")


def test_graph_editor_view_pie_view_all():
    import bpy

    e, t, window, area, center = yield from _setup_area("GRAPH_EDITOR")

    bpy.ops.mesh.primitive_cube_add()
    obj = bpy.context.active_object
    obj.location = (0.0, 0.0, 0.0)
    obj.keyframe_insert(data_path="location", frame=1)
    obj.location = (10.0, 0.0, 0.0)
    obj.keyframe_insert(data_path="location", frame=120)
    yield

    region = next(region for region in area.regions if region.type == "WINDOW")
    before = _view2d_rect(region)
    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "W")
    after = _view2d_rect(region)

    t.assertNotEqual(before, after)


def test_dopesheet_snap_pie_current_frame():
    import bpy

    e, t, window, area, center = yield from _setup_area("DOPESHEET_EDITOR")

    obj = yield from _create_animation_object_with_keyframes()
    area.spaces.active.mode = "ACTION"
    bpy.context.scene.frame_set(42)
    yield

    channelbag = _action_channelbag(obj)
    channelbag.fcurves[0].keyframe_points[0].select_control_point = True
    yield

    yield from _press_and_drag_to_direction(e, center, e.shift.s, "W")

    t.assertEqual(round(channelbag.fcurves[0].keyframe_points[0].co.x), 42)


def test_dopesheet_view_pie_view_all():
    import bpy

    e, t, window, area, center = yield from _setup_area("DOPESHEET_EDITOR")

    obj = yield from _create_animation_object_with_keyframes()
    area.spaces.active.mode = "ACTION"
    yield

    region = next(region for region in area.regions if region.type == "WINDOW")
    before = _view2d_rect(region)
    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "W")
    after = _view2d_rect(region)

    t.assertNotEqual(before, after)


def test_sequencer_pivot_pie_cursor():
    import bpy

    e, t, window, area, center = yield from _setup_area("SEQUENCE_EDITOR")

    bpy.context.workspace.sequencer_scene = bpy.context.scene
    yield

    # SEQUENCER_MT_pivot_pie only fires in the Preview region's keymap.
    area.spaces.active.view_type = "PREVIEW"
    bpy.context.scene.tool_settings.sequencer_tool_settings.pivot_point = "CENTER"
    yield

    yield from _press_and_drag_to_direction(e, center, e.period, "E")

    t.assertEqual(bpy.context.scene.tool_settings.sequencer_tool_settings.pivot_point, "CURSOR")


def test_nla_snap_pie_current_frame():
    import bpy

    e, t, window, area, center = yield from _setup_area("NLA_EDITOR")

    nla_object, nla_strip = yield from _create_nla_object()
    bpy.context.view_layer.objects.active = nla_object
    bpy.context.scene.frame_set(42)
    yield

    nla_strip.select = True
    yield

    yield from _press_and_drag_to_direction(e, center, e.shift.s, "W")

    t.assertEqual(nla_strip.frame_start, 42)


def test_nla_view_pie_view_all():
    import bpy

    e, t, window, area, center = yield from _setup_area("NLA_EDITOR")

    nla_object, nla_strip = yield from _create_nla_object()
    nla_strip.frame_start = 100
    yield

    region = next(region for region in area.regions if region.type == "WINDOW")
    before = _view2d_rect(region)
    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "W")
    after = _view2d_rect(region)

    t.assertNotEqual(before, after)


def test_image_pivot_pie_cursor():
    import bpy

    e, t, window, area, center = yield from _setup_area("IMAGE_EDITOR")
    area.spaces.active.pivot_point = "BOUNDING_BOX_CENTER"
    yield

    yield from _press_and_drag_to_direction(e, center, e.period, "E")

    t.assertEqual(area.spaces.active.pivot_point, "CURSOR")


def test_image_view_pie_view_all():
    import bpy

    e, t, window, area, center = yield from _setup_area("IMAGE_EDITOR")

    image = bpy.data.images.new("PieMenuImageView", width=128, height=128)
    area.spaces.active.image = image
    area.spaces.active.zoom_percentage = 500.0
    yield

    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "W")

    t.assertLess(area.spaces.active.zoom_percentage, 500.0)


def test_filebrowser_view_pie_list_horizontal():
    import bpy

    e, t, window, area, center = yield from _setup_area("FILE_BROWSER")

    params = area.spaces.active.params
    params.display_type = "LIST_VERTICAL"
    yield

    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "E")

    t.assertEqual(params.display_type, "LIST_HORIZONTAL")


def test_clip_pivot_pie_cursor():
    import bpy

    e, t, window, area, center = yield from _setup_clip_tracking_area()

    area.spaces.active.pivot_point = "BOUNDING_BOX_CENTER"
    yield

    yield from _press_and_drag_to_direction(e, center, e.period, "E")

    t.assertEqual(area.spaces.active.pivot_point, "CURSOR")


def test_clip_marker_pie_affine():
    import bpy

    e, t, window, area, center = yield from _setup_clip_tracking_area()

    clip = area.spaces.active.clip
    track = clip.tracking.tracks.active
    t.assertIsNotNone(track)
    track.motion_model = "Loc"
    yield

    yield from _press_and_drag_to_direction(e, center, e.shift.e, "E")

    t.assertEqual(track.motion_model, "Affine")


def test_clip_solving_pie_tripod_solver():
    import bpy

    e, t, window, area, center = yield from _setup_clip_tracking_area()

    settings = area.spaces.active.clip.tracking.settings
    settings.use_tripod_solver = False
    yield

    yield from _press_and_drag_to_direction(e, center, e.shift.s, "S")

    t.assertTrue(settings.use_tripod_solver)


def test_clip_view_pie_view_all():
    import bpy

    e, t, window, area, center = yield from _setup_clip_tracking_area()

    region = next(region for region in area.regions if region.type == "WINDOW")
    before = _view2d_rect(region)
    yield from _press_and_drag_to_direction(e, center, e.accent_grave, "W")
    after = _view2d_rect(region)

    t.assertNotEqual(before, after)
