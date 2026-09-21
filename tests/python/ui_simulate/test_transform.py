# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


# -----------------------------------------------------------------------------
# Utilities


def _view3d_region(window):
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    return next(region for region in area.regions if region.type == 'WINDOW')


def _view3d_calc_screen_space_location(window, co_world):
    """Window coordinates of a world-space location, for positioning the cursor."""
    from bpy_extras.view3d_utils import location_3d_to_region_2d

    region = _view3d_region(window)
    co = location_3d_to_region_2d(region, region.data, co_world)
    return int(region.x + co[0]), int(region.y + co[1])


def _view3d_context_override(window):
    import bpy
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    return bpy.context.temp_override(window=window, area=area, region=_view3d_region(window))


def _snapping_setup(e, window):
    """Empty scene holding a single plane, with vertex snapping to edited geometry enabled."""
    import bpy

    yield e.shift.f5()                  # 3D Viewport.
    yield e.ctrl.alt.space()            # Full-screen.
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.

    with _view3d_context_override(window):
        bpy.ops.mesh.primitive_plane_add(size=2.0)

    tool_settings = bpy.context.scene.tool_settings
    tool_settings.use_snap = True
    tool_settings.snap_elements = {'VERTEX'}
    tool_settings.snap_target = 'CLOSEST'
    # Snapping to the mesh being edited is what exercises the edit-mode snap path.
    tool_settings.use_snap_edit = True
    yield


def _mesh_edit_verts(window):
    import bmesh
    ob = window.view_layer.objects.active
    bm = bmesh.from_edit_mesh(ob.data)
    bm.verts.ensure_lookup_table()
    return ob, bm


def _mesh_edit_select_single_vert(window, index):
    """Leave only the vertex at \\a index selected, returning its world-space location."""
    import bmesh

    ob, bm = _mesh_edit_verts(window)
    for vert in bm.verts:
        vert.select = False
    bm.verts[index].select = True
    bm.select_flush(False)
    bmesh.update_edit_mesh(ob.data)
    return ob.matrix_world @ bm.verts[index].co


def _snap_vert_onto_location(e, window, vert_index, co_world_target):
    """Drag \\a vert_index with the mouse over \\a co_world_target, letting snapping take over."""
    location_src = _mesh_edit_select_single_vert(window, vert_index)
    yield

    src_co = _view3d_calc_screen_space_location(window, location_src)
    dst_co = _view3d_calc_screen_space_location(window, co_world_target)

    e.cursor_position_set(*src_co, move=True)
    yield
    yield e.g()
    # Move in steps. A single jump doesn't give the modal operator a chance to find a snap target,
    # and the transform would then complete unsnapped.
    steps = 8
    for i in range(1, steps + 1):
        factor = i / steps
        e.cursor_position_set(
            int(src_co[0] + (dst_co[0] - src_co[0]) * factor),
            int(src_co[1] + (dst_co[1] - src_co[1]) * factor),
            move=True,
        )
        yield
    yield e.ret()


def _assert_snapped_onto_other_vert(t, window, vert_index):
    """
    The moved vertex must sit exactly on top of another vertex of the same mesh.

    Which one it picks depends on the path the cursor took, so this doesn't assume a particular
    target. Without snapping the vertex moves in the view plane and lands on none of them.
    """
    _ob, bm = _mesh_edit_verts(window)
    moved = bm.verts[vert_index].co.copy()
    others = [vert.co.copy() for i, vert in enumerate(bm.verts) if i != vert_index]
    t.assertTrue(
        any((moved - co).length < 1e-4 for co in others),
        "vertex {:d} at {!r} snapped onto none of {!r}".format(
            vert_index, moved[:], [co[:] for co in others],
        ),
    )
    return moved


# -----------------------------------------------------------------------------
# Tests


def view3d_mesh_edit_snap_to_self():
    """Snap a vertex onto another vertex of the same mesh being edited."""
    from mathutils import Vector
    e, t, window = ui.test_window()

    yield from _snapping_setup(e, window)
    yield e.tab()                       # Edit mode.
    yield

    # The plane's corners are at (+/-1, +/-1, 0), aim at the opposite one.
    yield from _snap_vert_onto_location(e, window, 0, Vector((1.0, 1.0, 0.0)))
    co = _assert_snapped_onto_other_vert(t, window, 0)
    t.assertAlmostEqual(co.z, 0.0, places=4)

    yield e.tab()                       # Object mode.

    # Snapping is currently implemented by building its own copy of the mesh and hiding
    # what must not be snapped to. None of that may reach the mesh being edited.
    mesh = window.view_layer.objects.active.data
    t.assertFalse(any(vert.hide for vert in mesh.vertices))
    t.assertFalse(any(edge.hide for edge in mesh.edges))
    t.assertFalse(any(face.hide for face in mesh.polygons))


def view3d_mesh_edit_snap_to_self_shape_key():
    """As above, but snapping must use the shape being edited rather than the basis."""
    import bpy
    from mathutils import Vector
    e, t, window = ui.test_window()

    yield from _snapping_setup(e, window)

    ob = window.view_layer.objects.active
    with _view3d_context_override(window):
        bpy.ops.object.shape_key_add(from_mix=False)   # Basis.
        bpy.ops.object.shape_key_add(from_mix=False)   # Key 1.
    key_block = ob.data.shape_keys.key_blocks[1]
    # Raise the active shape key values.
    for point in key_block.data:
        point.co = point.co + Vector((0.0, 0.0, 2.0))
    key_block.value = 1.0
    ob.active_shape_key_index = 1
    yield

    yield e.tab()                       # Edit mode.
    yield

    yield from _snap_vert_onto_location(e, window, 0, Vector((1.0, 1.0, 2.0)))
    co = _assert_snapped_onto_other_vert(t, window, 0)
    # The basis is at zero, so this is what tells the two shapes apart.
    t.assertAlmostEqual(co.z, 2.0, places=4)

    yield e.tab()                       # Object mode.
