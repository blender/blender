# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "classes",
)

from bpy.types import Operator

from bpy.props import (
    EnumProperty,
)

STATUS_OK = (1 << 0)
STATUS_ERR_ACTIVE_FACE = (1 << 1)
STATUS_ERR_NOT_SELECTED = (1 << 2)
STATUS_ERR_NOT_QUAD = (1 << 3)
STATUS_ERR_MISSING_UV_LAYER = (1 << 4)
STATUS_ERR_NO_FACES_SELECTED = (1 << 5)


def extend(scene, obj, EXTEND_MODE, use_uv_selection):
    import bmesh
    from .uvcalc_transform import is_face_uv_selected_fn_from_context

    me = obj.data

    bm = bmesh.from_edit_mesh(me)

    f_act = bm.faces.active

    if f_act is None:
        return STATUS_ERR_ACTIVE_FACE  # Active face cannot be none.
    if not f_act.select:
        return STATUS_ERR_NOT_SELECTED  # Active face is not selected.
    if len(f_act.verts) != 4:
        return STATUS_ERR_NOT_QUAD  # Active face is not a quad
    uv_act = bm.loops.layers.uv.active  # Always use the active UV layer.
    if uv_act is None:
        return STATUS_ERR_MISSING_UV_LAYER  # Object's mesh doesn't have any UV layers.

    if use_uv_selection:
        face_select_test_fn = is_face_uv_selected_fn_from_context(scene, bm)
        faces = [
            f for f in bm.faces
            if f.select and len(f.verts) == 4 and face_select_test_fn(f, False)
        ]
    else:
        faces = [
            f for f in bm.faces
            if f.select and len(f.verts) == 4
        ]

    if not faces:
        return STATUS_ERR_NO_FACES_SELECTED

    # Our own local walker.

    def walk_face_init(faces, f_act):
        # First tag all faces True (so we don't UV-map them).
        for f in bm.faces:
            f.tag = True
        # Then tag faces argument False.
        for f in faces:
            f.tag = False
        # Tag the active face True since we begin there.
        f_act.tag = True

    def walk_face(f):
        # All faces in this list must be tagged.
        f.tag = True
        faces_a = [f]
        faces_b = []

        while faces_a:
            for f in faces_a:
                for l in f.loops:
                    l_edge = l.edge
                    if (l_edge.is_manifold is True) and (l_edge.seam is False):
                        l_other = l.link_loop_radial_next
                        f_other = l_other.face
                        if not f_other.tag:
                            yield (f, l, f_other)
                            f_other.tag = True
                            faces_b.append(f_other)
            # Swap.
            faces_a, faces_b = faces_b, faces_a
            faces_b.clear()

    # Utility, only for `walk_edgeloop_all`.
    def walk_edgeloop_all_impl_loop(loop_stack, edges_visited, l):
        l_other = l.link_loop_next.link_loop_next
        l_other_edge = l_other.edge
        if l_other_edge not in edges_visited:
            edges_visited.add(l_other_edge)
            yield l_other_edge
            if not l_other_edge.is_boundary:
                loop_stack.append(l_other)

    def walk_edgeloop_all(e):
        # Walks over all edge loops connected by quads (even edges with 3+ users).
        # Could make this a generic function.

        loop_stack = []
        edges_visited = {e}

        yield e

        # This initial iteration is needed because the loops never walk back over the face they come from.
        for l in e.link_loops:
            if len(l.face.verts) != 4:
                continue
            yield from walk_edgeloop_all_impl_loop(loop_stack, edges_visited, l)

        while loop_stack and (l_test := loop_stack.pop()):
            # Walk around the quad and then onto the next face.
            l = l_test
            while (l := l.link_loop_radial_next) is not l_test:
                if len(l.face.verts) != 4:
                    continue
                yield from walk_edgeloop_all_impl_loop(loop_stack, edges_visited, l)

    def extrapolate_uv(
            fac,
            l_a_outer, l_a_inner,
            l_b_outer, l_b_inner,
    ):
        l_b_inner[:] = l_a_inner
        l_b_outer[:] = l_a_inner + ((l_a_inner - l_a_outer) * fac)

    def apply_uv(_f_prev, l_prev, _f_next):
        l_a = [None, None, None, None]
        l_b = [None, None, None, None]

        l_a[0] = l_prev
        l_a[1] = l_a[0].link_loop_next
        l_a[2] = l_a[1].link_loop_next
        l_a[3] = l_a[2].link_loop_next

        #  l_b
        #  +-----------+
        #  |(3)        |(2)
        #  |           |
        #  |l_next(0)  |(1)
        #  +-----------+
        #        ^
        #  l_a   |
        #  +-----------+
        #  |l_prev(0)  |(1)
        #  |    (f)    |
        #  |(3)        |(2)
        #  +-----------+
        #  Copy from this face to the one above.

        # Get the other loops.
        l_next = l_prev.link_loop_radial_next
        if l_next.vert != l_prev.vert:
            l_b[1] = l_next
            l_b[0] = l_b[1].link_loop_next
            l_b[3] = l_b[0].link_loop_next
            l_b[2] = l_b[3].link_loop_next
        else:
            l_b[0] = l_next
            l_b[1] = l_b[0].link_loop_next
            l_b[2] = l_b[1].link_loop_next
            l_b[3] = l_b[2].link_loop_next

        l_a_uv = [l[uv_act].uv for l in l_a]
        l_b_uv = [l[uv_act].uv for l in l_b]

        if EXTEND_MODE == 'LENGTH_AVERAGE':
            d1 = edge_lengths[l_a[1].edge.index][0]
            d2 = edge_lengths[l_b[2].edge.index][0]
            try:
                fac = d2 / d1
            except ZeroDivisionError:
                fac = 1.0
        elif EXTEND_MODE == 'LENGTH':
            a0, b0, c0 = l_a[3].vert.co, l_a[0].vert.co, l_b[3].vert.co
            a1, b1, c1 = l_a[2].vert.co, l_a[1].vert.co, l_b[2].vert.co

            d1 = (a0 - b0).length + (a1 - b1).length
            d2 = (b0 - c0).length + (b1 - c1).length
            try:
                fac = d2 / d1
            except ZeroDivisionError:
                fac = 1.0
        else:
            fac = 1.0

        extrapolate_uv(
            fac,
            l_a_uv[3], l_a_uv[0],
            l_b_uv[3], l_b_uv[0],
        )

        extrapolate_uv(
            fac,
            l_a_uv[2], l_a_uv[1],
            l_b_uv[2], l_b_uv[1],
        )

    # -------------------------------------------
    # Calculate average length per loop if needed.

    if EXTEND_MODE == 'LENGTH_AVERAGE':
        bm.edges.index_update()
        edge_lengths = [None] * len(bm.edges)

        for f in faces:
            # We know it's a quad.
            l_quad = f.loops[:]

            # The opposite loops `l_quad[2]` & `l_quad[3]` are implicit (walking will handle).
            for l_init in (l_quad[0], l_quad[1]):
                # No need to check both because the initializing
                # one side of the pair will have initialized the second.
                l_init_edge = l_init.edge
                if edge_lengths[l_init_edge.index] is not None:
                    continue

                edge_length_store = [-1.0]
                edge_length_accum = 0.0
                edge_length_total = 0

                for e in walk_edgeloop_all(l_init_edge):
                    # Any previously met edges should have expanded into `l_init_edge`
                    # (which has no length).
                    assert edge_lengths[e.index] is None

                    edge_lengths[e.index] = edge_length_store
                    edge_length_accum += e.calc_length()
                    edge_length_total += 1

                edge_length_store[0] = edge_length_accum / edge_length_total

    # done with average length
    # ------------------------

    walk_face_init(faces, f_act)
    for f_triple in walk_face(f_act):
        apply_uv(*f_triple)

    bmesh.update_edit_mesh(me, loop_triangles=False)
    return STATUS_OK


def main(context, operator):
    scene = context.scene
    use_uv_selection = True
    if context.space_data and context.space_data.type == 'VIEW_3D':
        use_uv_selection = False  # When called from the 3D editor, UV selection is ignored.

    num_meshes = 0
    num_errors = 0
    status = 0

    ob_list = context.objects_in_mode_unique_data
    for ob in ob_list:
        num_meshes += 1
        ret = extend(scene, ob, operator.properties.mode, use_uv_selection)
        if ret != STATUS_OK:
            num_errors += 1
            status |= ret

    if num_errors == num_meshes:
        if status & STATUS_ERR_NOT_QUAD:
            operator.report({'ERROR'}, "Active face must be a quad")
        elif status & STATUS_ERR_NOT_SELECTED:
            operator.report({'ERROR'}, "Active face not selected")
        elif status & STATUS_ERR_NO_FACES_SELECTED:
            operator.report({'ERROR'}, "No selected faces")
        elif status & STATUS_ERR_MISSING_UV_LAYER:
            operator.report({'ERROR'}, "No UV layers")
        else:
            assert status & STATUS_ERR_ACTIVE_FACE != 0
            operator.report({'ERROR'}, "No active face")


class FollowActiveQuads(Operator):
    """Follow UVs from active quads along continuous face loops"""
    bl_idname = "uv.follow_active_quads"
    bl_label = "Follow Active Quads"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(
        name="Edge Length Mode",
        description="Method to space UV edge loops",
        items=(
            ('EVEN', "Even", "Space all UVs evenly"),
            ('LENGTH', "Length", "Average space UVs edge length of each loop"),
            ('LENGTH_AVERAGE', "Length Average", "Average space UVs edge length of each loop"),
        ),
        default='LENGTH_AVERAGE',
    )

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def execute(self, context):
        main(context, self)
        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


classes = (
    FollowActiveQuads,
)
