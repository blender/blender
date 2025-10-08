# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_pyapi_bmesh.py -- --verbose

__all__ = (
    "main",
)

import bmesh
import unittest


# ------------------------------------------------------------------------------
# Internal Utilities

def save_to_blend_file_for_testing(bm):
    """
    Useful for inspecting test data.
    """
    import bpy
    from bpy import context

    bpy.ops.wm.read_factory_settings(use_empty=True)
    me = bpy.data.meshes.new("test output")
    bm.to_mesh(me)
    ob = bpy.data.objects.new("", me)

    view_layer = context.view_layer
    layer_collection = context.layer_collection or view_layer.active_layer_collection
    scene_collection = layer_collection.collection

    scene_collection.objects.link(ob)
    ob.select_set(True)
    view_layer.objects.active = ob

    # Write to the $CWD.
    bpy.ops.wm.save_as_mainfile(filepath="bl_pyapi_bmesh.blend")


# ------------------------------------------------------------------------------
# Basic Tests

class TestBMeshBasic(unittest.TestCase):

    def test_create_uvsphere(self):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(
            bm,
            u_segments=8,
            v_segments=5,
            radius=1.0,
        )

        self.assertEqual(len(bm.verts), 34)
        self.assertEqual(len(bm.edges), 72)
        self.assertEqual(len(bm.faces), 40)

        bm.free()


# ------------------------------------------------------------------------------
# UV Selection

def bm_uv_select_check_or_empty(
        bm,
        sync=False,
        flush=False,
        contiguous=False,
):
    return bmesh.utils.uv_select_check(
        bm,
        sync=sync,
        flush=flush,
        contiguous=contiguous,
    ) or {}


def bm_uv_select_check_non_zero(
        bm, /, *,
        sync=False,
        flush=False,
        contiguous=False,
):
    """
    Remove all zero keys, so it's convenient to isolate failures.
    """
    result = bmesh.utils.uv_select_check(
        bm,
        sync=sync,
        flush=flush,
        contiguous=contiguous,
    )
    if result is not None:
        return {key: value for key, value in result.items() if value != 0}
    return {}


def bm_uv_select_set_all(bm, /, *, select):
    for f in bm.faces:
        f.uv_select = select
        for l in f.loops:
            l.uv_select_vert = select
            l.uv_select_edge = select


def bm_uv_layer_from_coords(bm, uv_layer):
    for face in bm.faces:
        for loop in face.loops:
            loop_uv = loop[uv_layer]
            # Use XY position of the vertex as a uv coordinate.
            loop_uv.uv = loop.vert.co.xy


def bm_loop_select_count_vert_edge_face(bm):
    """
    Return a tuple of UV selection counts (vert, edge, face).
    Use for tests.
    """
    mesh_vert = 0
    mesh_edge = 0
    mesh_face = 0

    uv_vert = 0
    uv_edge = 0
    uv_face = 0

    for v in bm.verts:
        if v.hide:
            continue
        if v.select:
            mesh_vert += 1
    for e in bm.edges:
        if e.hide:
            continue
        if e.select:
            mesh_edge += 1

    for f in bm.faces:
        if f.hide:
            continue
        if f.select:
            mesh_face += 1

        if f.uv_select:
            uv_face += 1
        for l in f.loops:
            if l.uv_select_vert:
                uv_vert += 1
            if l.uv_select_edge:
                uv_edge += 1

    return (uv_vert, uv_edge, uv_face), (mesh_vert, mesh_edge, mesh_face)


def bm_uv_select_reset(bm, /, *, select):
    bm_uv_select_set_all(bm, select=select)
    bm.uv_select_sync_to_mesh()


class TestBMeshUVSelectSimple(unittest.TestCase):

    def test_uv_grid(self):
        bm = bmesh.new()
        bmesh.ops.create_grid(
            bm,
            x_segments=3,
            y_segments=4,
            size=1.0,
        )
        self.assertEqual(len(bm.verts), 20)
        self.assertEqual(len(bm.edges), 31)
        self.assertEqual(len(bm.faces), 12)

        # Nothing selected.
        bm.uv_select_sync_valid = True
        self.assertEqual(bm_uv_select_check_or_empty(bm, sync=True), {})

        # All verts selected, no UV's selected.
        for v in bm.verts:
            v.select = True

        bm.uv_select_sync_valid = True
        self.assertEqual(bm_uv_select_check_or_empty(bm, sync=True).get(
            "count_uv_vert_none_selected_with_vert_selected", 0), 20)

        # No verts selected, all UV's selected.
        for v in bm.verts:
            v.select = False
        for f in bm.faces:
            for l in f.loops:
                l.uv_select_vert = True

        bm.uv_select_sync_valid = True
        self.assertTrue(
            bm_uv_select_check_or_empty(bm, sync=True).get("count_uv_vert_any_selected_with_vert_unselected", 0),
            48)

        bm.free()

    def test_uv_contiguous_verts(self):
        from mathutils import Vector
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=2, y_segments=2, size=1.0)
        self.assertEqual((len(bm.verts), len(bm.edges), len(bm.faces)), (9, 12, 4))

        faces = list(bm.faces)

        # Sort faces so the order is always predictable.
        vector_dot = Vector((0.95, 0.05, 0.0))
        faces.sort(key=lambda f: vector_dot.dot(f.calc_center_median()))

        # Checker de-select UV's, each face has an isolated selection.
        for i, f in enumerate(faces):
            do_select = bool(i % 2)
            for l in f.loops:
                l.uv_select_vert = do_select
                l.uv_select_edge = do_select

        bm.uv_select_sync_valid = True
        result = bm_uv_select_check_or_empty(bm, sync=True, flush=True, contiguous=False)
        self.assertTrue(result.get("count_uv_vert_any_selected_with_vert_unselected", 0), 48)

        bm.free()

    def test_uv_select_flush_mode(self):
        bm = bmesh.new()

        # Do a NOP empty mesh check.
        bm.uv_select_sync_valid = True
        bm.uv_select_flush_mode()
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        bmesh.ops.create_grid(bm, x_segments=3, y_segments=3, size=1.0)
        # Needed for methods that act on UV select.
        bm.uv_select_sync_valid = True

        # Do a NOP.
        bm.uv_select_flush_mode()
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Simple tests that selects all elements in a mode: `VERT`.
        bm.select_mode = {'VERT'}
        bm_uv_select_set_all(bm, select=False)
        # Select only verts.
        for f in bm.faces:
            for l in f.loops:
                l.uv_select_vert = True
        bm.uv_select_flush_mode()
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((36, 36, 9), (16, 24, 9)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Simple tests that selects all elements in a mode: `EDGE`.
        bm.select_mode = {'EDGE'}
        bm_uv_select_set_all(bm, select=False)
        # Select only edges..
        for f in bm.faces:
            for l in f.loops:
                l.uv_select_edge_set(True)
        bm.uv_select_flush_mode()
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((36, 36, 9), (16, 24, 9)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Simple tests that selects all elements in a mode: `FACE`.
        bm.select_mode = {'FACE'}
        bm_uv_select_set_all(bm, select=False)
        # Select only faces.
        for f in bm.faces:
            f.uv_select_set(True)
        bm.uv_select_flush_mode()
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((36, 36, 9), (16, 24, 9)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # TODO: Complex mixed selection.

    def test_uv_select_flush(self):
        from mathutils import Vector
        bm = bmesh.new()

        # Do a NOP empty mesh check.
        bm.uv_select_sync_valid = True
        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        bmesh.ops.create_grid(bm, x_segments=3, y_segments=3, size=1.0)
        # Needed for methods that act on UV select.
        bm.uv_select_sync_valid = True
        self.assertEqual((len(bm.verts), len(bm.edges), len(bm.faces)), (16, 24, 9))

        # Do a NOP check.
        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        faces = list(bm.faces)

        # Sort faces so the order is always predictable.
        vector_dot = Vector((0.95, 0.05, 0.0))
        faces.sort(key=lambda f: vector_dot.dot(f.calc_center_median()))

        f_center = faces[len(faces) // 2]
        self.assertEqual(f_center.calc_center_median().to_tuple(6), (0.0, 0.0, 0.0))

        uv_layer = bm.loops.layers.uv.new()
        bm_uv_layer_from_coords(bm, uv_layer)

        # Select 4 vertices.
        for l in f_center.loops:
            l.uv_select_vert = True
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((4, 0, 0), (0, 0, 0)))
        # Check.
        self.assertEqual(
            bm_uv_select_check_non_zero(bm, sync=True, flush=True),
            {
                "count_uv_edge_unselected_with_all_verts_selected": 4,
                "count_uv_face_unselected_with_all_verts_selected": 1,
                "count_uv_vert_any_selected_with_vert_unselected": 4,
            },
        )

        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((4, 4, 1), (4, 4, 1)))
        self.assertEqual(
            bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True),
            # Not actually an error as the UV's have intentionally been selected in isolation.
            {
                "count_uv_vert_non_contiguous_selected": 5,
            },
        )

        # De-select those 4, ensure the selection remains false afterwards.
        for l in f_center.loops:
            l.uv_select_vert = False

        bm.uv_select_flush(False)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # Select a single faces UV's bottom left hand corner (as well as adjacent UV's).
        for f in faces:
            for l in f.loops:
                xy = l.vert.co.xy[:]
                if xy[0] > 0.0 or xy[1] > 0.0:
                    continue
                l.uv_select_vert = True

        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((9, 6, 1), (4, 4, 1)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # Ensure flushing de-selection does nothing when there is nothing to do.
        bm.uv_select_flush(False)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((9, 6, 1), (4, 4, 1)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        self.assertTrue(bm.uv_select_sync_valid)

        bm.free()

    def test_uv_select_sync_from_mesh(self):
        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.new()
        del uv_layer

        # Do a NOP empty mesh check.
        bm.select_flush(True)
        bm.uv_select_sync_from_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        bmesh.ops.create_grid(bm, x_segments=4, y_segments=4, size=2.0)
        # Needed for methods that act on UV select.
        bm.uv_select_sync_valid = True

        # Deselect all verts and flush back to the mesh.
        for v in bm.verts:
            v.select = False
        bm.select_flush(True)
        bm.uv_select_sync_from_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Select all verts and flush back to the mesh.
        for v in bm.verts:
            v.select = True
        bm.select_flush(True)
        bm.uv_select_sync_from_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((64, 64, 16), (25, 40, 16)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # TODO: Complex mixed selection.

    def test_uv_select_sync_to_mesh(self):
        # Even though this is called in other tests,
        # perform some additional checks here such as checking hide is respected.

        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.new()
        del uv_layer

        # Do a NOP empty mesh check.
        bm.select_flush(True)
        bm.uv_select_sync_from_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        bmesh.ops.create_grid(bm, x_segments=4, y_segments=4, size=2.0)
        # Needed for methods that act on UV select.
        bm.uv_select_sync_valid = True

        # Select a single faces UV's bottom left hand corner (as well as adjacent UV's).
        for f in bm.faces:
            for l in f.loops:
                l.uv_select_vert = True

        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()

        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((64, 64, 16), (25, 40, 16)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Hide all geometry, then check syncing doesn't select them.
        for v in bm.verts:
            v.hide = True
        for e in bm.edges:
            e.hide = True
        for f in bm.faces:
            f.hide = True

        bm.uv_select_flush(True)
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        bm.uv_select_sync_to_mesh()
        # Nothing should be selected because the mesh is hidden.
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))

    def test_uv_select_foreach_set(self):
        # Select UV's directly, similar to selecting in the UV editor.
        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.new()
        bm.uv_select_sync_valid = True

        # Do a NOP empty mesh check.
        bm.uv_select_foreach_set(True)

        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Do a NOP empty mesh check with empty arguments.
        bm.uv_select_foreach_set(True, loop_verts=[], loop_edges=[], faces=[])

        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True), {})

        # Use 3 segments to avoid central vertices (simplifies above/below tests when picking half the mesh).
        bmesh.ops.create_grid(bm, x_segments=3, y_segments=3, size=2.0)
        bm_uv_layer_from_coords(bm, uv_layer)

        # Select all vertices with X below 0.0.
        verts_x_pos = []
        verts_x_neg = []
        for v in bm.verts:
            (verts_x_pos if v.co.x > 0.0 else verts_x_neg).append(v)
        self.assertEqual((len(verts_x_neg), len(verts_x_pos)), (8, 8))

        verts_x_pos_as_set = set(verts_x_pos)

        # Other elements from the verts (to pass to selection).
        faces_x_pos = [
            f for f in bm.faces
            # Find the loop that spans positive edges.
            if len(set(l.vert for l in f.loops) & verts_x_pos_as_set) == 4
        ]
        self.assertEqual(len(faces_x_pos), 3)

        loop_edges_x_pos = [
            l for f in faces_x_pos
            for l in f.loops
        ]
        self.assertEqual(len(loop_edges_x_pos), 12)

        loop_verts_x_pos = [next(iter(v.link_loops)) for v in verts_x_pos]
        self.assertEqual(len(loop_verts_x_pos), 8)

        # NOTE: regarding allowing `count_uv_vert_non_contiguous_selected` when de-selecting edges & faces.
        # This occurs because of `uv_select_flush_mode` which doesn't take `contiguous` UV's into account.

        # ---------------
        # Select by Verts
        bm_uv_select_reset(bm, select=False)
        bm.uv_select_foreach_set(True, loop_verts=loop_verts_x_pos)
        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((18, 15, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})
        # ------------------
        # De-Select by Verts
        bm_uv_select_reset(bm, select=True)
        bm.uv_select_foreach_set(False, loop_verts=loop_verts_x_pos)
        bm.uv_select_flush(False)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((18, 15, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # ---------------
        # Select by Edges
        bm.select_mode = {'EDGE'}
        bm_uv_select_reset(bm, select=False)
        bm.uv_select_foreach_set(True, loop_edges=loop_edges_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((18, 15, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # ------------------
        # De-Select by Edges
        bm_uv_select_reset(bm, select=True)
        bm.uv_select_foreach_set(False, loop_edges=loop_edges_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((24, 21, 3), (12, 14, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {
            "count_uv_vert_non_contiguous_selected": 4,  # Not an error, to be expected.
        })

        # ---------------
        # Select by Faces
        bm.select_mode = {'FACE'}
        bm_uv_select_reset(bm, select=False)
        bm.uv_select_foreach_set(True, faces=faces_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((12, 12, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {
            "count_uv_vert_non_contiguous_selected": 4,  # Not an error, to be expected.
        })

        # ------------------
        # De-Select by Faces
        bm_uv_select_reset(bm, select=True)
        bm.uv_select_foreach_set(False, faces=faces_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((24, 24, 6), (12, 17, 6)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {
            "count_uv_vert_non_contiguous_selected": 4,  # Not an error, to be expected.
        })

        # save_to_blend_file_for_testing(bm)

    def test_uv_select_foreach_set_from_mesh(self):
        """
        Select mesh elements, similar to selecting in the viewport,
        which is then flushed to the UV editor.
        """
        # Select geometry directly, similar to selecting in the 3D viewport.
        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.new()
        bm.uv_select_sync_valid = True

        # Do a NOP empty mesh check.
        bm.uv_select_foreach_set_from_mesh(True)

        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # Do a NOP empty mesh check with empty arguments.
        bm.uv_select_foreach_set_from_mesh(True, verts=[], edges=[], faces=[])

        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((0, 0, 0), (0, 0, 0)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # Use 3 segments to avoid central vertices (simplifies above/below tests when picking half the mesh).
        bmesh.ops.create_grid(bm, x_segments=3, y_segments=3, size=2.0)
        bm_uv_layer_from_coords(bm, uv_layer)

        # Select all vertices with X below 0.0.
        verts_x_pos = []
        verts_x_neg = []
        for v in bm.verts:
            (verts_x_pos if v.co.x > 0.0 else verts_x_neg).append(v)
        self.assertEqual((len(verts_x_neg), len(verts_x_pos)), (8, 8))

        verts_x_pos_as_set = set(verts_x_pos)

        # Other elements from the verts (to pass to selection).
        faces_x_pos = [
            f for f in bm.faces
            # Find the loop that spans positive edges.
            if len(set(l.vert for l in f.loops) & verts_x_pos_as_set) == 4
        ]
        self.assertEqual(len(faces_x_pos), 3)

        edges_x_pos = [
            e for e in bm.edges
            if len(set(e.verts) & verts_x_pos_as_set) == 2
        ]
        self.assertEqual(len(edges_x_pos), 10)

        loop_verts_x_pos = [next(iter(v.link_loops)) for v in verts_x_pos]
        self.assertEqual(len(loop_verts_x_pos), 8)

        # NOTE: regarding allowing `count_uv_vert_non_contiguous_selected` when de-selecting edges & faces.
        # This occurs because of `uv_select_flush_mode` which doesn't take `contiguous` UV's into account.

        # ---------------
        # Select by Verts
        bm_uv_select_reset(bm, select=False)
        bm.uv_select_foreach_set_from_mesh(True, verts=verts_x_pos)
        bm.uv_select_flush(True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((18, 15, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})
        # ------------------
        # De-Select by Verts
        bm_uv_select_reset(bm, select=True)
        bm.uv_select_foreach_set_from_mesh(False, verts=verts_x_pos)
        bm.uv_select_flush(False)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((18, 15, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # ---------------
        # Select by Edges
        bm.select_mode = {'EDGE'}
        bm_uv_select_reset(bm, select=False)
        bm.uv_select_foreach_set_from_mesh(True, edges=edges_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((18, 15, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {})

        # ------------------
        # De-Select by Edges
        bm_uv_select_reset(bm, select=True)
        bm.uv_select_foreach_set_from_mesh(False, edges=edges_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((24, 21, 3), (12, 14, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {
            "count_uv_vert_non_contiguous_selected": 4,  # Not an error, to be expected.
        })

        # ---------------
        # Select by Faces
        bm.select_mode = {'FACE'}
        bm_uv_select_reset(bm, select=False)
        bm.uv_select_foreach_set_from_mesh(True, faces=faces_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((12, 12, 3), (8, 10, 3)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {
            "count_uv_vert_non_contiguous_selected": 4,  # Not an error, to be expected.
        })

        # ------------------
        # De-Select by Faces
        bm_uv_select_reset(bm, select=True)
        bm.uv_select_foreach_set_from_mesh(False, faces=faces_x_pos)
        bm.uv_select_flush_mode(flush_down=True)
        bm.uv_select_sync_to_mesh()
        self.assertEqual(bm_loop_select_count_vert_edge_face(bm), ((24, 24, 6), (12, 17, 6)))
        self.assertEqual(bm_uv_select_check_non_zero(bm, sync=True, flush=True, contiguous=True), {
            "count_uv_vert_non_contiguous_selected": 4,  # Not an error, to be expected.
        })

        # save_to_blend_file_for_testing(bm)


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == "__main__":
    main()
