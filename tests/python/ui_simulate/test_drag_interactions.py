# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything; its methods are accessed by ``run_blender_setup.py``.

These tests use real mouse motion in Blender's event, but not native
OS drag-and-drop.
"""

import modules.ui_test_utils as ui


def _window_area(window, area_type):
    area = ui.largest_area(window.screen)
    area.type = area_type
    return area


def _mouse_move_line(e, start_xy, end_xy, steps=8):
    start_x, start_y = start_xy
    end_x, end_y = end_xy

    for step in range(steps + 1):
        factor = step / steps
        x = round(start_x + (end_x - start_x) * factor)
        y = round(start_y + (end_y - start_y) * factor)
        e.cursor_position_set(x, y, move=True)
        yield


def _modal_translate(e, start_xy, delta_xy, steps=8):
    """
    Start a standard Blender grab/translate and move the cursor along a line.
    """
    end_xy = (start_xy[0] + delta_xy[0], start_xy[1] + delta_xy[1])
    e.cursor_position_set(*start_xy, move=True)
    yield
    yield e.g()
    yield from _mouse_move_line(e, start_xy, end_xy, steps=steps)
    yield e.ret()
    yield


def _setup_compositor_tree(window):
    import bpy

    scene = window.scene
    tree = bpy.data.node_groups.new(name="ui_simulate_single_drag", type="CompositorNodeTree")
    scene.compositing_node_group = tree
    return tree


def _setup_node_editor(window, tree):
    area = _window_area(window, "NODE_EDITOR")
    space = area.spaces.active
    space.tree_type = tree.bl_idname
    space.node_tree = tree
    return area


def _setup_dopesheet(window):
    area = _window_area(window, "DOPESHEET_EDITOR")
    space = area.spaces.active
    space.ui_mode = "DOPESHEET"
    space.show_markers = True
    return area, space


def _setup_graph_editor(window):
    area = _window_area(window, "GRAPH_EDITOR")
    space = area.spaces.active
    return area, space


def _setup_image_editor(window):
    area = _window_area(window, "IMAGE_EDITOR")
    space = area.spaces.active
    return area, space


def _setup_uv_editor(window):
    area = _window_area(window, "IMAGE_EDITOR")
    space = area.spaces.active
    space.mode = "UV"
    return area


def _animated_cube():
    import bpy
    cube = bpy.data.objects["Cube"]
    scene = bpy.context.scene
    scene.frame_set(1)
    cube.location = (0.0, 0.0, 0.0)
    cube.keyframe_insert(data_path="location", frame=1)
    scene.frame_set(20)
    cube.location = (4.0, 1.5, 2.0)
    cube.keyframe_insert(data_path="location", frame=20)
    scene.frame_set(1)
    return cube


def _select_keyframe(action, fcurve, index=0):
    """
    Select only the requested keyframe on an F-Curve.
    """
    action.deselect_keys()

    keyframe = fcurve.keyframe_points[index]
    keyframe.select_control_point = True
    return keyframe


def _prepare_uv_vertex_selection(mesh):
    import bmesh
    import bpy

    # use_uv_select_sync lives on Scene.tool_settings, not on Mesh
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    bm = bmesh.from_edit_mesh(mesh)

    for vert in bm.verts:
        vert.select = False

    # Select vertex 0 for the drag test.
    bm.verts[0].select = True
    bm.select_history.clear()
    bm.select_history.add(bm.verts[0])
    bmesh.update_edit_mesh(mesh, loop_triangles=False, destructive=False)


def _get_fcurve(obj, data_path, index=0):
    """
    Return (action, fcurve) for the requested animation channel.
    """
    action = obj.animation_data.action
    slot = obj.animation_data.action_slot
    channelbag = action.layers[0].strips[0].channelbag(slot)
    return action, channelbag.fcurves.find(data_path, index=index)


def view3d_object_drag():
    import bpy
    e, t, window = ui.test_window()
    area = _window_area(window, "VIEW_3D")
    cube = bpy.data.objects["Cube"]
    cube.select_set(True)
    window.view_layer.objects.active = cube
    yield

    before_location = tuple(cube.location)
    before_active = window.view_layer.objects.active

    yield from _modal_translate(e, ui.get_area_center(area), (120, 70))

    after_location = tuple(cube.location)
    t.assertNotEqual(after_location, before_location, "Object location should change after dragging")
    t.assertIs(window.view_layer.objects.active, before_active, "Active object should remain the dragged object")
    t.assertTrue(cube.select_get(), "Selection should be preserved")

    yield e.ctrl.z()
    yield
    cube = bpy.data.objects["Cube"]
    t.assertEqual(tuple(cube.location), before_location, "Undo should restore the exact object location")

    yield e.ctrl.shift.z()
    yield
    cube = bpy.data.objects["Cube"]
    t.assertEqual(tuple(cube.location), after_location, "Redo should restore the moved object location")


def viewport_navigation_drags():

    e, t, window = ui.test_window()
    area = _window_area(window, "VIEW_3D")
    region = next(region for region in area.regions if region.type == "WINDOW")
    rv3d = region.data

    before_rotation = tuple(rv3d.view_rotation)
    before_location = tuple(rv3d.view_location)
    before_distance = rv3d.view_distance

    center = ui.get_area_center(area)
    yield from e.middlemouse.cursor_motion(ui.cursor_motion_data_circle(center, 80))
    t.assertNotEqual(tuple(rv3d.view_rotation), before_rotation, "Rotate drag should change the view rotation")

    yield from e.shift.middlemouse.cursor_motion(ui.cursor_motion_data_x(window))
    t.assertNotEqual(tuple(rv3d.view_location), before_location, "Pan drag should change the view location")

    yield from e.ctrl.middlemouse.cursor_motion(ui.cursor_motion_data_y(window))
    t.assertNotEqual(rv3d.view_distance, before_distance, "Zoom drag should change the view distance")


def node_single_drag():
    """
    Translate a single node in the compositor node editor.
    """

    e, t, window = ui.test_window()
    tree = _setup_compositor_tree(window)
    area = _setup_node_editor(window, tree)

    node = tree.nodes.new("CompositorNodeRGB")
    node.name = "TestRGBNode"
    node.location = (0.0, 0.0)
    node.select = True
    tree.nodes.active = node
    yield

    before_location = tuple(node.location)

    yield from _modal_translate(e, ui.get_area_center(area), (140, 90))

    after_location = tuple(node.location)

    t.assertNotEqual(
        after_location,
        before_location,
        "Node location should change after translate",
    )


def node_multiple_drag():
    """
    Drag multiple selected nodes together and verify the relative offsets stay intact.
    """
    e, t, window = ui.test_window()
    tree = _setup_compositor_tree(window)
    area = _setup_node_editor(window, tree)

    nodes = [
        tree.nodes.new("CompositorNodeRGB"),
        tree.nodes.new("CompositorNodeRGB"),
        tree.nodes.new("CompositorNodeRGB"),
    ]
    locations = [(0.0, 0.0), (160.0, 40.0), (-120.0, -80.0)]
    for node, location in zip(nodes, locations, strict=True):
        node.location = location
        node.select = True
    tree.nodes.active = nodes[0]
    yield

    before = [tuple(node.location) for node in nodes]
    yield from _modal_translate(e, ui.get_area_center(area), (110, 65))
    after = [tuple(node.location) for node in nodes]

    delta_0 = (after[0][0] - before[0][0], after[0][1] - before[0][1])
    t.assertNotEqual(delta_0, (0.0, 0.0), "Selected nodes should move")
    for idx, (before_loc, after_loc) in enumerate(zip(before, after, strict=True)):
        delta = (after_loc[0] - before_loc[0], after_loc[1] - before_loc[1])
        t.assertAlmostEqual(
            delta[0],
            delta_0[0],
            places=4,
            msg=f"Selected node {idx} X delta differs",
        )
        t.assertAlmostEqual(
            delta[1],
            delta_0[1],
            places=4,
            msg=f"Selected node {idx} Y delta differs",
        )


def dopesheet_keyframe_drag():
    """
    Drag a selected keyframe in the Dope Sheet.
    """

    e, t, window = ui.test_window()
    area, _ = _setup_dopesheet(window)

    cube = _animated_cube()
    action, fcurve = _get_fcurve(cube, "location", index=0)
    keyframe = _select_keyframe(action, fcurve, index=1)
    yield

    before = tuple(keyframe.co)

    yield from _modal_translate(
        e,
        ui.get_area_center(area),
        (120, 0),
    )

    after = tuple(keyframe.co)

    t.assertNotEqual(
        after,
        before,
        "Dope Sheet keyframe coordinates should change after dragging",
    )

    t.assertTrue(
        keyframe.select_control_point,
        "Dragged keyframe should remain selected",
    )


def graph_editor_drag():
    """
    Drag a selected Graph Editor keyframe.
    """

    e, t, window = ui.test_window()
    area, _ = _setup_graph_editor(window)

    cube = _animated_cube()
    action, fcurve = _get_fcurve(cube, "location", index=0)
    keyframe = _select_keyframe(action, fcurve, index=1)
    yield

    before = tuple(keyframe.co)

    yield from _modal_translate(
        e,
        ui.get_area_center(area),
        (120, 60),
    )

    after = tuple(keyframe.co)

    t.assertNotEqual(
        after,
        before,
        "Graph keyframe coordinates should change after dragging",
    )

    # Verify the dragged keyframe remains selected after the interaction.
    t.assertTrue(
        keyframe.select_control_point,
        "Dragged keyframe should remain selected",
    )


def uv_editor_drag():
    """
    Drag a UV vertex in the UV editor.
    """
    import bmesh

    e, t, window = ui.test_window()
    area = _setup_uv_editor(window)

    # Create a plane and enter Edit Mode.
    yield from ui.call_menu(e, "Add -> Mesh -> Plane")
    plane = window.view_layer.objects.active
    plane.select_set(True)
    window.view_layer.objects.active = plane

    yield e.tab()
    t.assertEqual(plane.mode, 'EDIT', "Object should be in Edit Mode after pressing Tab")

    mesh = plane.data

    # Ensure a UV map exists.
    if mesh.uv_layers.active is None:
        mesh.uv_layers.new(name="UVMap")

    _prepare_uv_vertex_selection(mesh)
    yield

    bm = bmesh.from_edit_mesh(mesh)
    uv_layer = bm.loops.layers.uv.active

    t.assertIsNotNone(uv_layer, "Mesh should have an active UV layer")
    t.assertGreater(len(bm.faces), 0, "Mesh should contain at least one face")

    loop = bm.faces[0].loops[0]
    before = tuple(loop[uv_layer].uv)

    yield from _modal_translate(e, ui.get_area_center(area), (100, 50))

    bmesh.update_edit_mesh(mesh)

    bm = bmesh.from_edit_mesh(mesh)
    uv_layer = bm.loops.layers.uv.active
    loop = bm.faces[0].loops[0]
    after = tuple(loop[uv_layer].uv)

    t.assertNotEqual(after, before, "UV coordinates should change after dragging")

    yield e.ctrl.z()

    # Undo may re-allocate the object and Mesh datablocks, so reacquire
    # both from the current view layer.
    plane = window.view_layer.objects.active
    mesh = plane.data

    bm = bmesh.from_edit_mesh(mesh)
    uv_layer = bm.loops.layers.uv.active
    loop = bm.faces[0].loops[0]

    t.assertEqual(tuple(loop[uv_layer].uv), before, "Undo should restore the exact UV coordinates")


def image_editor_pan():
    """
    Pan the image editor with the middle mouse button.
    """
    e, t, window = ui.test_window()
    area, space = _setup_image_editor(window)
    region = next(region for region in area.regions if region.type == "WINDOW")

    space.zoom_percentage = 400.0
    yield

    probe_x, probe_y = ui.get_area_center(area)
    before_cur = tuple(region.view2d.region_to_view(probe_x - region.x, probe_y - region.y))

    yield from e.middlemouse.cursor_motion(ui.cursor_motion_data_x(window))
    after_cur = tuple(region.view2d.region_to_view(probe_x - region.x, probe_y - region.y))

    t.assertNotEqual(after_cur, before_cur, "Image editor view should pan")
