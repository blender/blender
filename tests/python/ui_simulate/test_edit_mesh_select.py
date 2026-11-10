# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


# -----------------------------------------------------------------------------
# Utilities


def _view3d_area(window):
    return ui.get_window_area_by_type(window, 'VIEW_3D')


def _view3d_region(window):
    area = _view3d_area(window)
    return next(region for region in area.regions if region.type == 'WINDOW')


def _view3d_context_override(window):
    import bpy
    return bpy.context.temp_override(
        window=window, area=_view3d_area(window), region=_view3d_region(window))


def _get_region_point(window, factor_x, factor_y):
    """A region-space point at a fraction of the region size."""
    region = _view3d_region(window)
    return (int(region.width * factor_x), int(region.height * factor_y))


def _project_to_region(window, co_local):
    """Where a point of the edited object lands in region space."""
    from bpy_extras.view3d_utils import location_3d_to_region_2d

    region = _view3d_region(window)
    ob = window.view_layer.objects.active
    point = location_3d_to_region_2d(region, region.data, ob.matrix_world @ co_local)
    return (int(point[0]), int(point[1]))


def _get_vert_region_point(window, index):
    return _project_to_region(window, window.view_layer.objects.active.data.vertices[index].co)


def _get_front_verts(window, count):
    """The vertices nearest to the viewer, in that order."""
    from mathutils import Vector

    region = _view3d_region(window)
    towards_viewer = region.data.view_rotation @ Vector((0.0, 0.0, 1.0))
    ob = window.view_layer.objects.active
    order = sorted(ob.data.vertices,
                   key=lambda v: (ob.matrix_world @ v.co).dot(towards_viewer),
                   reverse=True)
    return [v.index for v in order[:count]]


def _setup_cube_edit_mode(e, window, select_mode, use_xray):
    """A single cube in edit mode with nothing selected."""
    import bpy

    yield e.shift.f5()                  # 3D Viewport.
    yield e.ctrl.alt.space()            # Full-screen.
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.

    with _view3d_context_override(window):
        bpy.ops.mesh.primitive_cube_add(size=2.0)
        bpy.ops.object.mode_set(mode='EDIT')

    bpy.context.scene.tool_settings.mesh_select_mode = select_mode
    _view3d_area(window).spaces.active.shading.show_xray = use_xray

    with _view3d_context_override(window):
        bpy.ops.mesh.select_all(action='DESELECT')
    yield


def _get_edit_bmesh(window):
    import bmesh
    return bmesh.from_edit_mesh(window.view_layer.objects.active.data)


def _get_selection_counts(window):
    """Selected vertex, edge and face counts."""
    mesh = window.view_layer.objects.active.data
    return (mesh.total_vert_sel, mesh.total_edge_sel, mesh.total_face_sel)


def _get_active_elements(window):
    """The selection history's active element as a (type name, index) pair, and the active face."""
    bm = _get_edit_bmesh(window)
    history = bm.select_history.active
    active_face = bm.faces.active
    return (
        None if history is None else (type(history).__name__, history.index),
        None if active_face is None else active_face.index,
    )


def _box_select(window, min_point, max_point, mode='SET'):
    import bpy
    with _view3d_context_override(window):
        bpy.ops.view3d.select_box(
            xmin=min_point[0], xmax=max_point[0], ymin=min_point[1], ymax=max_point[1],
            wait_for_input=False, mode=mode,
        )
    yield


def _get_whole_region_box(window):
    return (_get_region_point(window, 0.0, 0.0), _get_region_point(window, 1.0, 1.0))


def _select_pick(window, location, **params):
    import bpy
    with _view3d_context_override(window):
        # Picking twice in the same place cycles to the next element underneath.
        bpy.ops.view3d.select(location=(0, 0))
        bpy.ops.view3d.select(location=location, **params)
    yield


def _deselect_all_pick(window):
    yield from _select_pick(window, _get_region_point(window, 0.02, 0.02), deselect_all=True)


# -----------------------------------------------------------------------------
# Box Select Tests


def view3d_mesh_edit_box_select_vertex_mode():
    """Selecting every vertex of a cube flushes up to its edges and faces."""
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (True, False, False), use_xray=True)
    t.assertEqual(_get_selection_counts(window), (0, 0, 0), "setup left something selected")

    yield from _box_select(window, *_get_whole_region_box(window))
    verts, edges, faces = _get_selection_counts(window)
    t.assertEqual(verts, 8, "expected the whole cube")
    t.assertEqual(edges, 12, "selection did not flush to edges")
    t.assertEqual(faces, 6, "selection did not flush to faces")

    yield from _box_select(window, _get_region_point(window, 0.0, 0.0),
                           _get_region_point(window, 0.02, 0.02))
    t.assertEqual(_get_selection_counts(window), (0, 0, 0), "an empty box did not clear the selection")


def view3d_mesh_edit_box_select_occlusion():
    """Without X-ray, box select skips occluded vertices."""
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (True, False, False), use_xray=False)

    yield from _box_select(window, *_get_whole_region_box(window))
    verts = _get_selection_counts(window)[0]
    t.assertGreater(verts, 0)
    t.assertLess(verts, 8, "expected the occluded vertices to be skipped")

    _view3d_area(window).spaces.active.shading.show_xray = True
    yield from _box_select(window, *_get_whole_region_box(window))
    t.assertEqual(_get_selection_counts(window)[0], 8, "X-ray should reach the occluded vertices")


def view3d_mesh_edit_box_select_extend():
    """Extending and subtracting build on the existing selection instead of replacing it."""
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (True, False, False), use_xray=True)

    left = (_get_region_point(window, 0.0, 0.0), _get_region_point(window, 0.5, 1.0))
    right = (_get_region_point(window, 0.5, 0.0), _get_region_point(window, 1.0, 1.0))

    yield from _box_select(window, *left)
    after_left = _get_selection_counts(window)[0]
    t.assertGreater(after_left, 0)
    t.assertLess(after_left, 8, "half the region should be a partial selection")

    yield from _box_select(window, *right, mode='ADD')
    t.assertEqual(_get_selection_counts(window)[0], 8, "extending both halves should select everything")

    yield from _box_select(window, *left, mode='SUB')
    t.assertEqual(_get_selection_counts(window)[0], 8 - after_left,
                  "subtracting did not remove the left half")


def view3d_mesh_edit_box_select_face_mode():
    """Face mode selects faces, and flushes down to the vertices and edges they use."""
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (False, False, True), use_xray=True)

    yield from _box_select(window, *_get_whole_region_box(window))
    verts, edges, faces = _get_selection_counts(window)
    t.assertEqual(faces, 6, "expected every face")
    t.assertEqual(verts, 8, "face selection did not flush down to vertices")
    t.assertEqual(edges, 12, "face selection did not flush down to edges")


def view3d_mesh_edit_box_select_mixed_mode():
    """
    With vertex and edge select modes, a box crossing an edge but containing none of its vertices
    selects the edge and its vertices.
    """
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (True, True, False), use_xray=True)

    mesh = window.view_layer.objects.active.data
    vert_a, vert_b = mesh.edges[0].vertices
    x, y = _project_to_region(window, (mesh.vertices[vert_a].co + mesh.vertices[vert_b].co) / 2.0)
    yield from _box_select(window, (x - 3, y - 3), (x + 3, y + 3))
    t.assertEqual(_get_selection_counts(window), (2, 1, 0),
                  "expected the crossed edge and its vertices")


# -----------------------------------------------------------------------------
# Pick Select Tests


def view3d_mesh_edit_select_pick_extend():
    """Extending, subtracting and toggling build on the existing selection instead of replacing."""
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (True, False, False), use_xray=False)

    first, second = _get_front_verts(window, 2)
    point_a, point_b = _get_vert_region_point(window, first), _get_vert_region_point(window, second)

    yield from _select_pick(window, point_a)
    t.assertEqual(_get_selection_counts(window)[0], 1, "expected one vertex")

    yield from _select_pick(window, point_b, extend=True)
    t.assertEqual(_get_selection_counts(window)[0], 2, "extending should keep the first vertex selected")

    yield from _select_pick(window, point_a, deselect=True)
    t.assertEqual(_get_selection_counts(window)[0], 1, "subtracting should remove only the first vertex")
    t.assertNotIn(first, {v.index for v in _get_edit_bmesh(window).verts if v.select},
                  "the wrong vertex was de-selected")

    yield from _select_pick(window, point_a, toggle=True)
    t.assertEqual(_get_selection_counts(window)[0], 2, "toggling an unselected vertex should select it")
    yield from _select_pick(window, point_a, toggle=True)
    t.assertEqual(_get_selection_counts(window)[0], 1, "toggling a selected vertex should de-select it")

    yield from _deselect_all_pick(window)
    t.assertEqual(_get_selection_counts(window), (0, 0, 0), "an empty pick did not clear the selection")


def view3d_mesh_edit_select_pick_flushes():
    """
    Picking a face selects its vertices and edges, and picking those vertices in vertex mode
    selects the face again.
    """
    import bpy
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (False, False, True), use_xray=False)

    yield from _select_pick(window, _get_region_point(window, 0.5, 0.5))
    verts, edges, faces = _get_selection_counts(window)
    t.assertEqual(faces, 1, "expected one face")
    t.assertEqual(verts, 4, "face selection did not flush down to vertices")
    t.assertEqual(edges, 4, "face selection did not flush down to edges")

    face_verts = [v.index for face in _get_edit_bmesh(window).faces if face.select
                  for v in face.verts]

    bpy.context.scene.tool_settings.mesh_select_mode = (True, False, False)
    yield from _deselect_all_pick(window)
    for i, vert in enumerate(face_verts):
        yield from _select_pick(window, _get_vert_region_point(window, vert), extend=(i > 0))
    verts, edges, faces = _get_selection_counts(window)
    t.assertEqual(verts, 4, "expected the face's four vertices")
    t.assertEqual(faces, 1, "vertex selection did not flush up to the face")


def view3d_mesh_edit_select_pick_active_element():
    """A pick makes the picked element active, in the selection history and as the active face."""
    import bpy
    e, t, window = ui.test_window()

    yield from _setup_cube_edit_mode(e, window, (True, False, False), use_xray=False)

    first, second = _get_front_verts(window, 2)

    yield from _select_pick(window, _get_vert_region_point(window, first))
    t.assertEqual(_get_active_elements(window)[0], ("BMVert", first),
                  "the picked vertex did not become active")

    yield from _select_pick(window, _get_vert_region_point(window, second), extend=True)
    t.assertEqual(_get_active_elements(window)[0], ("BMVert", second),
                  "extending did not move the active element to the newly picked vertex")

    yield from _select_pick(window, _get_vert_region_point(window, first), extend=True)
    t.assertEqual(_get_active_elements(window)[0], ("BMVert", first),
                  "re-picking a selected vertex did not make it active")
    t.assertEqual(_get_selection_counts(window)[0], 2, "re-picking a selected vertex changed the selection")

    yield from _select_pick(window, _get_vert_region_point(window, first), deselect=True)
    t.assertNotEqual(_get_active_elements(window)[0], ("BMVert", first),
                     "a de-selected vertex stayed in the selection history")

    bpy.context.scene.tool_settings.mesh_select_mode = (False, False, True)
    yield from _select_pick(window, _get_region_point(window, 0.5, 0.5))
    history, active_face = _get_active_elements(window)
    t.assertIsNotNone(active_face, "picking a face did not set the active face")
    t.assertEqual(history, ("BMFace", active_face), "the active face and the selection history disagree")
