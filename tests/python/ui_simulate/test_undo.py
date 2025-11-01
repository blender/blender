# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import datetime

# FIXME: Since 2.8 or so, there is a problem with simulated events
# where a popup needs the main-loop to cycle once before new events
# are handled. This isn't great but seems not to be a problem for users?
_MENU_CONFIRM_HACK = True


# -----------------------------------------------------------------------------
# Utilities


def _keep_open():
    """
    Only for development, handy so we can quickly keep the window open while testing.
    """
    import bpy
    bpy.app.use_event_simulate = False


def _test_window(windows_exclude=None):
    import bpy
    wm = bpy.data.window_managers[0]
    if windows_exclude is None:
        return wm.windows[0]
    for window in wm.windows:
        if window not in windows_exclude:
            return window
    return None


def _test_vars(window):
    import unittest
    from modules.easy_keys import EventGenerate
    return (
        EventGenerate(window),
        unittest.TestCase(),
    )


def _call_by_name(e, text: str):
    yield e.f3()
    yield e.text(text)
    yield e.ret()


def _call_menu(e, text: str):
    yield e.f3()
    yield e.text_unicode(text.replace(" -> ", " \u25b8 "))
    yield e.ret()


def _window_size_in_pixels(window):
    import sys
    size = window.width, window.height
    # macOS window size is a multiple of the pixel_size.
    if sys.platform == "darwin":
        from bpy import context
        # The value is always rounded to an int, so converting to an int is safe here.
        pixel_size = int(context.preferences.system.pixel_size)
        size = size[0] * pixel_size, size[1] * pixel_size
    return size


def _cursor_motion_data_x(window):
    size = _window_size_in_pixels(window)
    return [
        (x, size[1] // 2) for x in
        range(int(size[0] * 0.2), int(size[0] * 0.8), 80)
    ]


def _cursor_motion_data_y(window):
    size = _window_size_in_pixels(window)
    return [
        (size[0] // 2, y) for y in
        range(int(size[1] * 0.2), int(size[1] * 0.8), 80)
    ]


def _cursor_motion_data_xy(window):
    size = _window_size_in_pixels(window)
    return [
        (p, p) for p in
        range(int(size[0] * 0.2), int(size[0] * 0.8), 80)
    ]


def _window_area_get_by_type(window, space_type):
    for area in window.screen.areas:
        if area.type == space_type:
            return area


def _cursor_position_from_area(area):
    return (
        area.x + area.width // 2,
        area.y + area.height // 2,
    )


def _cursor_position_from_spacetype(window, space_type):
    area = _window_area_get_by_type(window, space_type)
    if area is None:
        raise Exception("Space Type {!r} not found".format(space_type))
    return _cursor_position_from_area(area)


def _view3d_object_calc_screen_space_location(window, name: str):
    from bpy_extras.view3d_utils import location_3d_to_region_2d

    area = _window_area_get_by_type(window, 'VIEW_3D')
    region = next((region for region in area.regions if region.type == 'WINDOW'))
    rv3d = region.data

    ob = window.view_layer.objects[name]
    co = location_3d_to_region_2d(region, rv3d, ob.matrix_world.translation)
    return int(co[0]), int(co[1])


def _view3d_object_select_by_name(e, name: str):
    location = _view3d_object_calc_screen_space_location(e.window, name)
    e.cursor_position_set(*location, move=True)
    # e.shift.rightmouse.tap()  # Set the cursor so it's possible to see what was selected.
    yield
    e.ctrl.leftmouse.tap()
    yield


def _setup_window_areas_from_ui_types(e, ui_types):
    assert len(e.window.screen.areas) == 1
    total_areas = len(ui_types)
    i = 0
    while len(e.window.screen.areas) < total_areas:
        areas = list(e.window.screen.areas)
        for area in areas:
            event_xy = _cursor_position_from_area(area)
            e.cursor_position_set(x=event_xy[0], y=event_xy[1], move=True)
            # areas_len_prev = len(e.window.screen.areas)
            if (i % 2) == 0:
                yield from _call_menu(e, "View -> Area -> Horizontal Split")
            else:
                yield from _call_menu(e, "View -> Area -> Vertical Split")
            e.leftmouse.tap()
            yield
            # areas_len_curr = len(e.window.screen.areas)
            # assert areas_len_curr != areas_len_prev
            if len(e.window.screen.areas) >= total_areas:
                break
        i += 1

    # Use direct assignment, it's possible to use shortcuts for most area types, it's tedious.
    for ty, area in zip(ui_types, e.window.screen.areas, strict=True):
        area.ui_type = ty
    yield


def _print_undo_steps_and_line():
    """
    Keep even when unused, handy for tracking down problems.
    """
    from inspect import currentframe
    cf = currentframe()
    line = cf.f_back.f_lineno

    import bpy
    wm = bpy.data.window_managers[0]
    print(__file__ + ":" + str(line))
    wm.print_undo_steps()


def _bmesh_from_object(ob):
    import bmesh
    return bmesh.from_edit_mesh(ob.data)


# -----------------------------------------------------------------------------
# Text Editor

def _text_editor_startup(e):
    yield e.shift.f11()                 # Text editor.
    yield e.ctrl.alt.space()            # Full-screen.
    yield e.alt.n()                     # New text.


def _text_editor_and_3dview_startup(e, window):
    # Add text block in properties editors.
    pos_text = _cursor_position_from_spacetype(window, 'PROPERTIES')
    e.cursor_position_set(*pos_text, move=True)
    yield e.shift.f11()                 # Text editor.
    yield e.alt.n()                     # New text.


def text_editor_simple():
    e, t = _test_vars(_test_window())

    import bpy
    yield from _text_editor_startup(e)
    text = bpy.data.texts[0]

    yield e.text("Hello\nWorld")
    t.assertEqual(text.as_string(), "Hello\nWorld")
    yield e.shift.home().ctrl.x().back_space()
    yield e.home().ctrl.v().ret()
    t.assertEqual(text.as_string(), "World\nHello")
    yield e.ctrl.a().tab()
    t.assertEqual(text.as_string(), "    World\n    Hello")
    yield e.ctrl.z(5)
    t.assertEqual(text.as_string(), "Hello\nWorld")


def text_editor_edit_mode_mix():
    # Ensure text edits and mesh edits can co-exist properly (see: T66658).
    e, t = _test_vars(window := _test_window())

    import bpy
    yield from _text_editor_and_3dview_startup(e, window)
    text = bpy.data.texts[0]

    pos_text = _cursor_position_from_spacetype(window, 'TEXT_EDITOR')
    pos_v3d = _cursor_position_from_spacetype(window, 'VIEW_3D')

    # View 3D: edit-mode
    e.cursor_position_set(*pos_v3d, move=True)
    yield from _call_menu(e, "Add -> Mesh -> Cube")

    yield e.numpad_period()             # View all.
    yield e.tab()                       # Edit mode.
    yield e.a()                         # Select all.

    # Text: add text 'AA'.
    e.cursor_position_set(*pos_text, move=True)
    yield e.text("AA")
    t.assertEqual(text.as_string(), "AA")

    # View 3D: duplicate & move.
    e.cursor_position_set(*pos_v3d, move=True)
    yield e.shift.d().x().text("3").ret()
    yield e.g().z().text("1").ret()
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8 * 2)
    e.home()

    # Text: add text 'BB'
    e.cursor_position_set(*pos_text, move=True)
    yield e.text("BB")
    t.assertEqual(text.as_string(), "AABB")

    # View 3D: duplicate & move.
    e.cursor_position_set(*pos_v3d, move=True)
    yield e.shift.d().x().text("3").ret()
    yield e.g().z().text("1").ret()
    e.home()
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8 * 3)

    # Text: add text 'CC'
    e.cursor_position_set(*pos_text, move=True)
    yield e.text("CC")
    t.assertEqual(text.as_string(), "AABBCC")

    # View 3D: duplicate & move.
    e.cursor_position_set(*pos_v3d, move=True)
    yield e.shift.d().x().text("3").ret()
    yield e.g().z().text("1").ret()
    e.home()
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8 * 4)

    # Undo and check the state is valid.
    yield e.ctrl.z(4)
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8 * 3)
    t.assertEqual(text.as_string(), "AABB")

    yield e.ctrl.z(4)
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8 * 2)
    t.assertEqual(text.as_string(), "AA")

    yield e.ctrl.z(4)
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8)
    t.assertEqual(text.as_string(), "")

    # Finally redo all.
    yield e.ctrl.shift.z(4 * 3)
    t.assertEqual(len(_bmesh_from_object(window.view_layer.objects.active).verts), 8 * 4)
    t.assertEqual(text.as_string(), "AABBCC")

# -----------------------------------------------------------------------------
# Node Editor


def _compositor_startup_area(e):
    """
    Set up the compositor node editor
    """
    yield e.shift.f3(2)                # Compositor
#    yield e.ctrl.alt.space()           # Full-screen.


def compositor_make_group():
    import bpy
    e, t = _test_vars(window := _test_window())
    yield from _compositor_startup_area(e)

    # Create a node tree with multiple nodes and select all nodes.
    # TODO: Node tree should be created through the UI
    node_group = bpy.data.node_groups.new(name="comp ntree", type="CompositorNodeTree")
    window.scene.compositing_node_group = node_group
    yield from _call_menu(e, "Add -> Color -> Alpha Convert")
    yield e.ret()  # Confirm adding node.
    yield from _call_menu(e, "Add -> Filter -> Filter")
    yield e.ret()
    yield e.a()  # Select all.
    t.assertEqual(len(window.scene.compositing_node_group.nodes), 2)
    yield e.ctrl.g()  # Make group.
    t.assertEqual(len(window.scene.compositing_node_group.nodes), 1)
    yield e.ctrl.z()
    t.assertEqual(len(window.scene.compositing_node_group.nodes), 2)
    yield e.ctrl.z(5)  # Revert to original state


# -----------------------------------------------------------------------------
# 3D View

def _view3d_startup_area_maximized(e):
    """
    Set the 3D viewport and set the area full-screen so no other regions.
    """
    yield e.shift.f5()                  # 3D Viewport.
    yield e.ctrl.alt.space()            # Full-screen.
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.


def _view3d_startup_area_single(e):
    """
    Create a single area (not full screen)
    this has the advantage that the window can be duplicated (not the case with a full-screened area).
    """
    yield e.shift.f5()                  # 3D Viewport.
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.

    for _ in range(len(e.window.screen.areas)):
        # 3D Viewport.
        event_xy = _cursor_position_from_spacetype(e.window, e.window.screen.areas[0].type)
        e.cursor_position_set(x=event_xy[0], y=event_xy[1], move=True)
        yield e.shift.f5()
        yield from _call_menu(e, "View -> Area -> Close Area")
    assert len(e.window.screen.areas) == 1


def view3d_simple():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    # NOTE: it should be possible to consider "Add -> Mesh -> Plane" an exact match.
    # However, shortcuts are now included so without them this ends up fuzzy-matching to "Add -> Image -> Mesh Plane".
    # To resolve that it's necessary to match the entire shortcut which... changes based on the platform (sign!).
    use_menu_search_workaround = True
    if use_menu_search_workaround:
        import sys
        yield from _call_menu(e, "Add ({:s} A) -> Mesh -> Plane".format(
            "\u21e7" if sys.platform == "darwin" else "Shift"
        ))
        del sys
    else:
        # It would be nice if this could be restored.
        yield from _call_menu(e, "Add -> Mesh -> Plane")

    # Duplicate and rotate.
    for _ in range(3):
        yield e.shift.d().x().text("3").ret()
        yield e.r.z().text("15").ret()
    t.assertEqual(len(window.view_layer.objects), 4)
    yield e.a()                         # Select all.
    yield e.numpad_7().numpad_period()  # View top.
    yield e.ctrl.j()                    # Join.
    t.assertEqual(len(window.view_layer.objects), 1)
    yield e.tab()                       # Edit mode.
    yield from _call_menu(e, "Edge -> Subdivide")
    yield e.tab()                       # Object mode.
    t.assertEqual(len(window.view_layer.objects.active.data.polygons), 16)
    yield e.ctrl.z(12)                  # Undo until start.
    t.assertEqual(len(window.view_layer.objects), 0)
    yield e.ctrl.shift.z(12)            # Redo until end.
    t.assertEqual(len(window.view_layer.objects.active.data.polygons), 16)


def view3d_sculpt_with_memfile_step():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Torus")

    # Note: this could also be replaced by adding the multires modifier (see comment below).
    yield e.tab()                       # Enter Edit mode.
    yield e.ctrl.e().d()                # Subdivide.
    yield e.ctrl.e().d()                # Subdivide.
    yield e.tab()                       # Leave Edit mode.

    yield e.numpad_period()             # View all.
    yield e.ctrl.tab().s()              # Sculpt via pie menu.

    # Add a 'memfile' undo step without leaving Sculpt mode.
    yield e.f3().text("add const").ret().d()  # Add 'Limit Distance' constraint.
    # Note: Multires modifier exhibits even more issues with undo/redo in sculpt mode, but unfortunately geometry is not
    # available from python anymore while in sculpt mode, so we cannot test/check if undo/redo steps apply properly.
    # yield e.ctrl.two()                  # Add multires modifier.

    # Utility to extract current mesh coordinates (used to ensure undo/redo steps are applied properly).
    def extract_mesh_cos(window):
        # TODO: Find/add a way to get that info when there is a multires active in Sculpt mode.
        window.view_layer.update()
        tmp_mesh = window.view_layer.objects.active.to_mesh(preserve_all_data_layers=True)
        tmp_cos = [0.0] * len(tmp_mesh.vertices) * 3
        tmp_mesh.vertices.foreach_get("co", tmp_cos)
        window.view_layer.objects.active.to_mesh_clear()
        return tmp_cos

    mesh_verts_cos_before_sculpt = extract_mesh_cos(window)

    # Add a first sculpt stroke.
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    mesh_verts_cos_sculpt_stroke1 = extract_mesh_cos(window)
    t.assertNotEqual(mesh_verts_cos_before_sculpt, mesh_verts_cos_sculpt_stroke1)

    # Add a second sculpt stroke.
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))
    mesh_verts_cos_sculpt_stroke2 = extract_mesh_cos(window)
    t.assertNotEqual(mesh_verts_cos_sculpt_stroke1, mesh_verts_cos_sculpt_stroke2)

    # Undo to first sculpt stroke.
    yield e.ctrl.z()
    mesh_verts_cos = extract_mesh_cos(window)
    t.assertEqual(mesh_verts_cos, mesh_verts_cos_sculpt_stroke1)

    # Undo to memfile step (add constraint), fine here (T82532),
    # but would fail if we had added a Multires modifier instead (T82851).
    yield e.ctrl.z()
    mesh_verts_cos = extract_mesh_cos(window)
    t.assertEqual(mesh_verts_cos, mesh_verts_cos_before_sculpt)

    # Redo first sculpt stroke, would now be undone (in Multires case, T82851),
    # or not redone (in constraint case, T82532).
    yield e.ctrl.shift.z()
    mesh_verts_cos = extract_mesh_cos(window)
    t.assertEqual(mesh_verts_cos, mesh_verts_cos_sculpt_stroke1)

    # Redo second sculpt stroke, would redo properly,
    # as well as part of the first one that affects the same nodes (T82851, T82532).
    yield e.ctrl.shift.z()
    mesh_verts_cos = extract_mesh_cos(window)
    t.assertEqual(mesh_verts_cos, mesh_verts_cos_sculpt_stroke2)


def view3d_sculpt_dyntopo_simple():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Torus")
    # Avoid dynamic topology prompt.
    yield from _call_by_name(e, "Remove UV Map")
    if _MENU_CONFIRM_HACK:
        yield
    yield e.r().y().text("45").ret()    # Rotate Y 45.
    yield e.ctrl.a().r()                # Apply rotation.
    yield e.numpad_period()             # View all.
    yield e.ctrl.tab().s()              # Sculpt via pie menu.
    yield from _call_menu(e, "Sculpt -> Dynamic Topology Toggle")
    # TODO: should be accessible from menu.
    yield from _call_by_name(e, "Symmetrize")
    yield e.ctrl.tab().o()              # Object mode.
    t.assertEqual(len(window.view_layer.objects.active.data.polygons), 1258)
    yield e.delete()                   # Delete the object.
    yield e.ctrl.z()                    # Undo...
    yield e.ctrl.z()                    # Undo used to crash here: T60974
    t.assertEqual(len(window.view_layer.objects.active.data.polygons), 1258)
    t.assertEqual(window.view_layer.objects.active.mode, 'SCULPT')


def view3d_sculpt_dyntopo_and_edit():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Torus")
    yield e.numpad_period()             # View all.
    yield from _call_by_name(e, "Remove UV Map")
    yield e.ctrl.tab().s()              # Sculpt via pie menu.
    yield e.ctrl.d().ret()              # Dynamic topology.
    # TODO: should be accessible from menu.
    yield from _call_by_name(e, "Symmetrize")
    # Some painting (demo it works, not needed for the crash)
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    yield e.tab()                       # Edit mode.
    yield e.tab()                       # Object mode.
    yield e.ctrl.z(3)                   # Undo
    # yield e.ctrl.z()                    # Undo asserts (nested undo call from dyntopo)


def view3d_sculpt_trim():
    """
    Test that trim functionality can be undone and redone correctly.
    Operations that work on the entire mesh exercise a different code path from normal sculpt undo.
    """

    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)
    yield from _call_menu(e, "Add -> Mesh -> Torus")
    yield e.numpad_period()             # View all.
    yield from _call_by_name(e, "Remove UV Map")
    yield e.ctrl.tab().s()              # Sculpt via pie menu.

    # Utility to extract current mesh coordinates (used to ensure undo/redo steps are applied properly).
    def extract_mesh_positions(window):
        # TODO: Find/add a way to get that info when there is a multires active in Sculpt mode.
        window.view_layer.update()
        tmp_mesh = window.view_layer.objects.active.to_mesh(preserve_all_data_layers=True)
        tmp_cos = [0.0] * len(tmp_mesh.vertices) * 3
        tmp_mesh.vertices.foreach_get("co", tmp_cos)
        window.view_layer.objects.active.to_mesh_clear()
        return tmp_cos

    beginning_positions = extract_mesh_positions(window)
    yield from _call_by_name(e, "Box Trim")
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_xy(window))    # Perform the trim
    after_trim_positions = extract_mesh_positions(window)
    t.assertNotEqual(beginning_positions, after_trim_positions)

    yield e.ctrl.z()                                                        # Undo Trim
    after_undo_positions = extract_mesh_positions(window)
    t.assertEqual(beginning_positions, after_undo_positions)

    yield e.ctrl.shift.z()                                                  # Redo Trim
    after_redo_positions = extract_mesh_positions(window)
    t.assertEqual(after_trim_positions, after_redo_positions)


def view3d_sculpt_dyntopo_stroke_toggle():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Torus")
    yield e.numpad_period()             # View all.
    yield from _call_by_name(e, "Remove UV Map")
    yield e.ctrl.tab().s()              # Sculpt via pie menu.

    # Utility to extract current mesh coordinates (used to ensure undo/redo steps are applied properly).
    def extract_mesh_positions(window):
        # TODO: Find/add a way to get that info when there is a multires active in Sculpt mode.
        window.view_layer.update()
        tmp_mesh = window.view_layer.objects.active.to_mesh(preserve_all_data_layers=True)
        tmp_cos = [0.0] * len(tmp_mesh.vertices) * 3
        tmp_mesh.vertices.foreach_get("co", tmp_cos)
        window.view_layer.objects.active.to_mesh_clear()
        return tmp_cos

    original_positions = extract_mesh_positions(window)
    yield from _call_by_name(e, "Dynamic Topology")  # On

    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))

    yield from _call_by_name(e, "Dynamic Topology")  # Off
    after_toggle_off = extract_mesh_positions(window)
    t.assertNotEqual(original_positions, after_toggle_off)

    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))
    after_normal_stroke = extract_mesh_positions(window)
    t.assertNotEqual(after_toggle_off, after_normal_stroke)

    yield e.ctrl.z()                          # Undo Stroke
    after_first_undo = extract_mesh_positions(window)
    t.assertEqual(after_first_undo, after_toggle_off)

    yield e.ctrl.z()                          # Undo Toggle Off
    yield e.ctrl.z()                          # Undo Dyntopo Stroke
    yield e.ctrl.z()                          # Undo Toggle On
    after_full_undo = extract_mesh_positions(window)
    t.assertEqual(after_full_undo, original_positions)

    yield e.ctrl.shift.z()                    # Redo Toggle On
    yield e.ctrl.shift.z()                    # Redo Dyntopo Stroke
    yield e.ctrl.shift.z()                    # Redo Toggle Off
    after_toggle_off_redo = extract_mesh_positions(window)
    t.assertEqual(after_toggle_off_redo, after_toggle_off)

    yield e.ctrl.shift.z()                    # Redo Normal Stroke
    after_normal_stroke_redo = extract_mesh_positions(window)
    t.assertEqual(after_normal_stroke, after_normal_stroke_redo)


def view3d_texture_paint_simple():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Monkey")
    yield e.numpad_period()             # View monkey
    yield e.ctrl.tab().t()              # Paint via pie menu.
    yield from _call_by_name(e, "Add Texture Paint Slot")
    yield e.ret()                       # Accept popup.

    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    yield e.ctrl.z(2)                   # Undo: initial texture paint.
    t.assertEqual(window.view_layer.objects.active.mode, 'TEXTURE_PAINT')
    yield e.ctrl.z()                    # Undo: object mode.
    t.assertEqual(window.view_layer.objects.active.mode, 'OBJECT')
    yield e.ctrl.shift.z(2)             # Redo: initial blank canvas.
    t.assertEqual(window.view_layer.objects.active.mode, 'TEXTURE_PAINT')
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    yield e.ctrl.z()                    # Used to crash T61172.


def view3d_texture_paint_complex():
    # More complex test than `view3d_texture_paint_simple`,
    # including interleaved memfile steps,
    # and a call to history to undo several steps at once.
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Monkey")
    yield e.numpad_period()             # View monkey
    yield e.ctrl.tab().t()              # Paint via pie menu.

    yield from _call_by_name(e, "Add Texture Paint Slot")
    yield e.ret()                       # Accept popup.

    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))

    yield from _call_by_name(e, "Add Texture Paint Slot")
    yield e.ret()                       # Accept popup.

    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))

    yield e.ctrl.z(6)                   # Undo: initial texture paint.
    t.assertEqual(window.view_layer.objects.active.mode, 'TEXTURE_PAINT')
    yield e.ctrl.z()                    # Undo: object mode.
    t.assertEqual(window.view_layer.objects.active.mode, 'OBJECT')

    yield e.ctrl.shift.z(2)             # Redo: initial blank canvas.
    t.assertEqual(window.view_layer.objects.active.mode, 'TEXTURE_PAINT')

    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))

    yield from _call_by_name(e, "Undo History")
    yield e.o()                         # Undo everything to Original step.
    t.assertEqual(window.view_layer.objects.active.mode, 'OBJECT')


def view3d_mesh_edit_separate():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Cube")
    yield e.numpad_period()             # View all.
    yield e.tab()                       # Edit mode.
    yield e.shift.d()                   # Duplicate...
    yield e.x().text("3").ret()         # Move X-3.
    yield e.p().s()                     # Separate selection.
    t.assertEqual(len(window.view_layer.objects), 2)
    yield e.ctrl.z()                    # Undo.
    t.assertEqual(len(window.view_layer.objects), 1)
    yield e.tab()                       # Object mode.
    t.assertEqual(len(window.view_layer.objects.active.data.polygons), 12)
    yield e.tab()                       # Edit mode.
    yield e.ctrl.i()                    # Invert selection.
    yield e.p().s()                     # Separate selection.
    yield e.tab()                       # Object mode.
    t.assertEqual([len(ob.data.polygons) for ob in window.view_layer.objects], [6, 6])
    yield e.ctrl.z(8)                   # Undo until start.
    t.assertEqual(len(window.view_layer.objects), 0)
    yield e.ctrl.shift.z(8)             # Redo until end.
    t.assertEqual([len(ob.data.polygons) for ob in window.view_layer.objects], [6, 6])


def view3d_mesh_particle_edit_mode_simple():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Mesh -> Cube")
    yield e.r.z().text("15").ret()      # Single object-mode action (to test mixing different kinds of undo steps).
    yield from _call_menu(e, "Object -> Quick Effects -> Quick Fur")

    yield e.ctrl.tab().s()              # Particle sculpt mode.
    t.assertEqual(window.view_layer.objects.active.mode, 'SCULPT_CURVES')

    # Brush strokes.
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))

    # Undo and redo.
    yield e.ctrl.z(5)
    t.assertEqual(window.view_layer.objects.active.mode, 'OBJECT')
    yield e.shift.ctrl.z(5)

    t.assertEqual(window.view_layer.objects.active.mode, 'SCULPT_CURVES')

    # Brush strokes.
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_y(window))
    yield from e.leftmouse.cursor_motion(_cursor_motion_data_x(window))

    yield e.ctrl.z(7)
    t.assertEqual(window.view_layer.objects.active.mode, 'OBJECT')
    yield e.shift.ctrl.z(7)


def view3d_font_edit_mode_simple():
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    yield from _call_menu(e, "Add -> Text")
    yield e.numpad_period()             # View all.
    yield e.tab()                       # Edit mode.
    yield e.ctrl.back_space()
    yield e.text("Hello\nWorld")
    yield e.tab()                       # Object mode.
    t.assertEqual(window.view_layer.objects.active.data.body, 'Hello\nWorld')
    yield e.r.x().text("90").ret()      # Rotate 90, face the view.
    yield e.tab()                       # Edit mode.
    yield e.end()                       # Edit mode.
    yield e.ctrl.back_space()
    yield e.back_space()
    yield e.tab()                       # Object mode.
    t.assertEqual(window.view_layer.objects.active.data.body, 'Hello')

    yield e.ctrl.z(3)
    t.assertEqual(window.view_layer.objects.active.data.body, 'Hello\nWorld')
    yield e.shift.ctrl.z(3)
    t.assertEqual(window.view_layer.objects.active.data.body, 'Hello')


def view3d_multi_mode_select():
    # Note, this test should be extended to change modes for each object type.
    e, t = _test_vars(window := _test_window())
    yield from _view3d_startup_area_maximized(e)

    object_names = []

    for i, (menu_search, ob_name) in enumerate((
            ("Add -> Armature", "Armature"),
            ("Add -> Text", "Text"),
            ("Add -> Mesh -> Cube", "Cube"),
            ("Add -> Curve -> Bézier", "Curve"),
            ("Add -> Volume -> Empty", "Volume Empty"),
            ("Add -> Metaball -> Ball", "Metaball"),
            ("Add -> Lattice", "Lattice"),
            ("Add -> Light -> Point", "Point Light"),
            ("Add -> Camera", "Camera"),
            ("Add -> Empty -> Plain Axis", "Empty"),
    )):
        yield from _call_menu(e, menu_search)
        # Single object-mode action (to test mixing different kinds of undo steps).
        yield e.g.z().text(str(i * 2)).ret()
        # Rename.
        yield e.f2().text(ob_name).ret()

        object_names.append(window.view_layer.objects.active.name)

    yield from _call_menu(e, "View -> Frame All")
    # print(object_names)

    for ob_name in object_names:
        yield from _view3d_object_select_by_name(e, ob_name)
        yield
        # print()
        # print('=' * 40)
        # print(window.view_layer.objects.active.name, ob_name)

    for ob_name in reversed(object_names):
        t.assertEqual(ob_name, window.view_layer.objects.active.name)
        yield e.ctrl.z()


def _ui_hack_idle_until(until, idle=1 / 60, timeout=1.0):
    """
    Idle while the internal event loop runs until a specified condition is true.

    This should be used sparingly as it likely represents some other failure condition inside Blender. Currently, the
    only known needed usecase is for multi window undo tests which need separate view layers. See #148903 for further
    information on this issue.

    Note: In practice, the timeout value of 1.0 seconds should be more than enough for all cases. In testing with a
    fixed, constant delay, the tests succeeded with a timeout of 1/6th of a second.
    :param until: lambda to check the condition of after each sleep
    :param idle: how long to idle between checks of the `until` lambda.
        Defaults to 60Hz due to common refresh rates.
    :param timeout: the max time in seconds that this busy wait will execute.
    :return:
    """
    import time
    start_time = time.time()
    current_time = time.time()
    while current_time - start_time < timeout or not until():
        yield datetime.timedelta(seconds=idle)
        current_time = time.time()


def view3d_multi_mode_multi_window():
    e_a, t = _test_vars(window_a := _test_window())
    yield from _call_menu(e_a, "Window -> New Main Window")

    e_b, _ = _test_vars(window_b := _test_window(windows_exclude={window_a}))
    del _
    yield from _call_menu(e_b, "New Scene")
    yield e_b.ret()
    if _MENU_CONFIRM_HACK:
        yield from _ui_hack_idle_until(lambda: window_a.view_layer != window_b.view_layer)

    t.assertNotEqual(window_a.view_layer, window_b.view_layer, "Windows should have different view layers")

    for e in (e_a, e_b):
        pos_v3d = _cursor_position_from_spacetype(e.window, 'VIEW_3D')
        e.cursor_position_set(x=pos_v3d[0], y=pos_v3d[1], move=True)
        del pos_v3d

    yield from _view3d_startup_area_maximized(e_a)
    yield from _view3d_startup_area_maximized(e_b)

    undo_current = 0
    undo_state_empty = undo_current

    yield from _call_menu(e_a, "Add -> Torus")
    yield from _call_menu(e_b, "Add -> Monkey")
    undo_current += 2

    # Weight paint via pie menu.
    yield e_a.ctrl.tab().w()
    yield e_b.ctrl.tab().w()
    undo_current += 2
    undo_state_wpaint = undo_current

    t.assertEqual(window_a.view_layer.objects.active.mode, 'WEIGHT_PAINT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'WEIGHT_PAINT')

    # Object mode via pie menu.
    yield e_a.ctrl.tab().o()
    yield e_b.ctrl.tab().o()
    undo_current += 2

    undo_state_non_empty_start = undo_current

    # Edit mode.
    yield e_a.tab()
    yield e_b.tab()
    undo_current += 2

    vert_count_a_start = len(_bmesh_from_object(window_a.view_layer.objects.active).verts)
    vert_count_b_start = len(_bmesh_from_object(window_b.view_layer.objects.active).verts)

    yield from _call_menu(e_a, "Edge -> Subdivide")
    yield from _call_menu(e_b, "Edge -> Subdivide")
    undo_current += 2

    yield e_a.r().y().text("45").ret()  # Rotate Y 45.
    yield e_b.r().z().text("45").ret()  # Rotate Z 45.
    undo_current += 2

    # Object mode.
    yield e_a.tab()
    yield e_b.tab()
    undo_current += 2

    # Object mode via pie menu.
    yield e_a.ctrl.tab().s()
    yield e_b.ctrl.tab().s()
    undo_current += 2

    t.assertEqual(window_a.view_layer.objects.active.mode, 'SCULPT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'SCULPT')

    # Rotate 90.
    yield from _call_menu(e_a, "Sculpt -> Rotate")
    yield e_a.text("90").ret()
    yield from _call_menu(e_b, "Sculpt -> Rotate")
    yield e_b.text("90").ret()
    undo_current += 2

    # Object mode.
    yield e_a.ctrl.tab().o()
    yield e_b.ctrl.tab().o()
    undo_current += 2

    # Edit mode.
    yield e_a.tab()
    yield e_b.tab()
    undo_current += 2

    yield from _call_menu(e_a, "Edge -> Subdivide")
    yield from _call_menu(e_b, "Edge -> Subdivide")
    undo_current += 2

    vert_count_a_end = len(_bmesh_from_object(window_a.view_layer.objects.active).verts)
    vert_count_b_end = len(_bmesh_from_object(window_b.view_layer.objects.active).verts)

    t.assertEqual(vert_count_a_end, 9216)
    t.assertEqual(vert_count_b_end, 7830)

    yield e_a.r().y().text("45").ret()  # Rotate Y 45.
    yield e_b.r().z().text("45").ret()  # Rotate Z 45.
    undo_current += 2

    yield e_a.tab()
    yield e_b.tab()
    undo_current += 2

    undo_state_final = undo_current

    undo_delta = undo_state_final - undo_state_empty

    yield e_a.ctrl.z(undo_delta)
    undo_current -= undo_delta

    # Ensure scene is empty.
    t.assertEqual(len(window_a.view_layer.objects), 0)
    t.assertEqual(len(window_b.view_layer.objects), 0)

    undo_delta = undo_state_final - undo_state_empty
    yield e_a.ctrl.shift.z(undo_delta)
    undo_current += undo_delta

    t.assertEqual(window_a.view_layer.objects.active.mode, 'OBJECT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'OBJECT')

    t.assertEqual(len(window_a.view_layer.objects.active.data.vertices), vert_count_a_end)
    t.assertEqual(len(window_b.view_layer.objects.active.data.vertices), vert_count_b_end)

    undo_delta = undo_state_final - undo_state_wpaint
    yield e_a.ctrl.z(undo_delta)
    undo_current -= undo_delta

    t.assertEqual(window_a.view_layer.objects.active.mode, 'WEIGHT_PAINT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'WEIGHT_PAINT')

    undo_delta = undo_state_non_empty_start - undo_state_wpaint
    yield e_a.ctrl.shift.z(undo_delta)
    undo_current += undo_delta

    t.assertEqual(len(window_a.view_layer.objects.active.data.vertices), vert_count_a_start)
    t.assertEqual(len(window_b.view_layer.objects.active.data.vertices), vert_count_b_start)

    # Further checks could be added but this seems enough.


def view3d_edit_mode_multi_window():
    """
    Use undo and redo with multiple windows in edit-mode,
    this test caused a crash with #110022.
    """
    e_a, t = _test_vars(window_a := _test_window())

    # Nice but slower.
    use_all_area_ui_types = False

    # Use a large, single area so the window can be duplicated & split.
    yield from _view3d_startup_area_single(e_a)

    yield from _call_menu(e_a, "Window -> New Main Window")

    e_b, _ = _test_vars(window_b := _test_window(windows_exclude={window_a}))
    del _

    yield from _call_menu(e_b, "New Scene")
    yield e_b.ret()
    if _MENU_CONFIRM_HACK:
        yield from _ui_hack_idle_until(lambda: window_a.view_layer != window_b.view_layer)

    t.assertNotEqual(window_a.view_layer, window_b.view_layer, "Windows should have different view layers")

    for e in (e_a, e_b):
        pos_v3d = _cursor_position_from_spacetype(e.window, 'VIEW_3D')
        e.cursor_position_set(x=pos_v3d[0], y=pos_v3d[1], move=True)
        del pos_v3d

    undo_current = 0

    yield from _call_menu(e_a, "Add -> Cone")
    yield from _call_menu(e_b, "Add -> Cylinder")
    undo_current += 2

    # Edit mode.
    yield e_a.tab()
    yield e_b.tab()
    undo_current += 2

    undo_state_edit_mode = undo_current

    vert_count_a_start = len(_bmesh_from_object(window_a.view_layer.objects.active).verts)
    vert_count_b_start = len(_bmesh_from_object(window_b.view_layer.objects.active).verts)

    yield e_a.r().y().text("45").ret()  # Rotate Y 45.
    yield e_b.r().z().text("45").ret()  # Rotate Z 45.
    undo_current += 2

    yield from _call_menu(e_a, "Face -> Poke Faces")
    yield from _call_menu(e_b, "Face -> Poke Faces")
    undo_current += 2

    yield from _call_menu(e_a, "Face -> Beautify Faces")
    yield from _call_menu(e_b, "Face -> Beautify Faces")
    undo_current += 2

    yield from _call_menu(e_a, "Face -> Wireframe")
    yield from _call_menu(e_b, "Face -> Wireframe")
    undo_current += 2

    vert_count_a_end = len(_bmesh_from_object(window_a.view_layer.objects.active).verts)
    vert_count_b_end = len(_bmesh_from_object(window_b.view_layer.objects.active).verts)

    # Object mode.
    yield e_a.tab()
    yield e_b.tab()
    undo_current += 2

    # Finished with edits, assert undo is working as expected.

    yield e_a.ctrl.z(undo_current - undo_state_edit_mode)

    t.assertEqual(len(_bmesh_from_object(window_a.view_layer.objects.active).verts), vert_count_a_start)
    t.assertEqual(len(_bmesh_from_object(window_b.view_layer.objects.active).verts), vert_count_b_start)
    t.assertEqual(window_a.view_layer.objects.active.mode, 'EDIT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'EDIT')

    yield e_a.ctrl.shift.z(undo_current - undo_state_edit_mode)

    t.assertEqual(len(window_a.view_layer.objects.active.data.vertices), vert_count_a_end)
    t.assertEqual(len(window_b.view_layer.objects.active.data.vertices), vert_count_b_end)
    t.assertEqual(window_a.view_layer.objects.active.mode, 'OBJECT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'OBJECT')

    # Delete objects.
    yield e_a.delete()
    yield e_b.delete()
    undo_current += 2

    yield e_b.ctrl.z(undo_current)

    # Ensure scene is empty.
    t.assertEqual(len(window_a.view_layer.objects), 0)
    t.assertEqual(len(window_b.view_layer.objects), 0)

    yield e_b.ctrl.shift.z(undo_current - 2)
    undo_current -= 2

    t.assertEqual(len(window_a.view_layer.objects.active.data.vertices), vert_count_a_end)
    t.assertEqual(len(window_b.view_layer.objects.active.data.vertices), vert_count_b_end)
    t.assertEqual(window_a.view_layer.objects.active.mode, 'OBJECT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'OBJECT')

    # Second phase!
    # Split windows & show space types (could be a utility function).
    # Test undo / redo doesn't cause issues when showing different space types.
    if use_all_area_ui_types:
        # TODO: extracting the enum from an exception is not good.
        # As it's a dynamic enum it can't be accessed from `bl_rna.properties`.
        try:
            e_a.window.screen.areas[0].ui_type = '__INVALID__'
        except TypeError as ex:
            ui_types = ex.args[0]
            ui_types = eval(ui_types[ui_types.rfind("("):])
    else:
        ui_types = ('VIEW_3D', 'PROPERTIES')

    for e in (e_a, e_b):
        yield from _setup_window_areas_from_ui_types(e, ui_types)

    # Ensure each undo step redraws.
    for _ in range(undo_current - undo_state_edit_mode):
        yield e_b.ctrl.z()

    t.assertEqual(len(_bmesh_from_object(window_a.view_layer.objects.active).verts), vert_count_a_start)
    t.assertEqual(len(_bmesh_from_object(window_b.view_layer.objects.active).verts), vert_count_b_start)
    t.assertEqual(window_a.view_layer.objects.active.mode, 'EDIT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'EDIT')

    # Ensure each undo step redraws.
    for _ in range(undo_current - undo_state_edit_mode):
        yield e_b.ctrl.shift.z()

    t.assertEqual(len(window_a.view_layer.objects.active.data.vertices), vert_count_a_end)
    t.assertEqual(len(window_b.view_layer.objects.active.data.vertices), vert_count_b_end)
    t.assertEqual(window_a.view_layer.objects.active.mode, 'OBJECT')
    t.assertEqual(window_b.view_layer.objects.active.mode, 'OBJECT')
