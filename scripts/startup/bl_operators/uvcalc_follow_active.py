# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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


def extend(obj, EXTEND_MODE, use_uv_selection):
    import bmesh
    from .uvcalc_transform import is_face_uv_selected

    me = obj.data

    bm = bmesh.from_edit_mesh(me)

    f_act = bm.faces.active

    if f_act is None:
        return STATUS_ERR_ACTIVE_FACE  # Active face cannot be none.
    if not f_act.select:
        return STATUS_ERR_NOT_SELECTED  # Active face is not selected.
    if len(f_act.verts) != 4:
        return STATUS_ERR_NOT_QUAD  # Active face is not a quad
    if not me.uv_layers:
        return STATUS_ERR_MISSING_UV_LAYER  # Object's mesh doesn't have any UV layers.

    uv_act = bm.loops.layers.uv.active  # Always use the active UV layer.

    # Construct a set of selected quads.
    faces = {f for f in bm.faces if len(f.verts) == 4 and f.select}
    if use_uv_selection:
        # Filter `faces` to extract only UV selected quads.
        faces = {f for f in faces if is_face_uv_selected(f, uv_act, False)}

    if not faces:
        return STATUS_ERR_NO_FACES_SELECTED

    def walk_face():
        from collections import deque

        for f in bm.faces:
            f.tag = f not in faces

        faces_deque = deque()
        faces_deque.append(f_act)
        f_act.tag = True  # Queued.

        while faces_deque:  # Breadth first search.
            f = faces_deque.popleft()
            for l in f.loops:
                l_edge = l.edge
                if l_edge.seam:
                    continue  # Don't walk across seams.
                if not l_edge.is_manifold:
                    continue  # Don't walk across non-manifold.
                l_other = l.link_loop_radial_next  # Manifold implies uniqueness.
                f_other = l_other.face
                if f_other.tag:
                    continue  # Either queued, visited, not selected, or not quad.
                yield (f, l, f_other)
                faces_deque.append(f_other)
                f_other.tag = True  # Queued.

    def walk_edgeloop(l):
        """
        Could make this a generic function
        """
        e_first = l.edge
        e = None
        while True:
            e = l.edge
            yield e

            # Don't step past non-manifold edges.
            if e.is_manifold:
                # Walk around the quad and then onto the next face.
                l = l.link_loop_radial_next
                if len(l.face.verts) == 4:
                    l = l.link_loop_next.link_loop_next
                    if l.edge is e_first:
                        break
                else:
                    break
            else:
                break

    uv_updates = []

    def record_and_assign_uv(dest, source):
        from mathutils import Vector

        if dest[uv_act].uv == source:
            return  # Already placed correctly, probably a nearby quad.
        dest_uv_copy = Vector(dest[uv_act].uv)  # Make a copy to prevent aliasing.
        uv_updates.append([dest.vert, dest_uv_copy, source])  # Record changes.
        dest[uv_act].uv = source  # Assign updated UV.

    def extrapolate_uv(fac, l_a_outer, l_a_inner, l_b_outer, l_b_inner):
        l_a_inner_uv = l_a_inner[uv_act].uv
        l_a_outer_uv = l_a_outer[uv_act].uv
        record_and_assign_uv(l_b_inner, l_a_inner_uv)
        record_and_assign_uv(l_b_outer, l_a_inner_uv * (1 + fac) - l_a_outer_uv * fac)

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

        extrapolate_uv(fac, l_a[3], l_a[0], l_b[3], l_b[0])
        extrapolate_uv(fac, l_a[2], l_a[1], l_b[2], l_b[1])

    # -------------------------------------------
    # Calculate average length per loop if needed.

    if EXTEND_MODE == 'LENGTH_AVERAGE':
        bm.edges.index_update()
        edge_lengths = [None] * len(bm.edges)

        for f in faces:
            # We know it's a quad.
            l_quad = f.loops[:]
            l_pair_a = (l_quad[0], l_quad[2])
            l_pair_b = (l_quad[1], l_quad[3])

            for l_pair in (l_pair_a, l_pair_b):
                if edge_lengths[l_pair[0].edge.index] is None:

                    edge_length_store = [-1.0]
                    edge_length_accum = 0.0
                    edge_length_total = 0

                    for l in l_pair:
                        if edge_lengths[l.edge.index] is None:
                            for e in walk_edgeloop(l):
                                if edge_lengths[e.index] is None:
                                    edge_lengths[e.index] = edge_length_store
                                    edge_length_accum += e.calc_length()
                                    edge_length_total += 1

                    edge_length_store[0] = edge_length_accum / edge_length_total

    for f_triple in walk_face():
        apply_uv(*f_triple)

    # Propagate UV changes across boundary of selection.
    for (v, original_uv, source) in uv_updates:
        # Visit all loops associated with our vertex.
        for loop in v.link_loops:
            # If the loop's UV matches the original, assign the new UV.
            if loop[uv_act].uv == original_uv:
                loop[uv_act].uv = source

    bmesh.update_edit_mesh(me, loop_triangles=False)
    return STATUS_OK


def main(context, operator):
    use_uv_selection = True
    view = context.space_data
    if context.space_data and context.space_data.type == 'VIEW_3D':
        use_uv_selection = False  # When called from the 3D editor, UV selection is ignored.

    num_meshes = 0
    num_errors = 0
    status = 0

    ob_list = context.objects_in_mode_unique_data
    for ob in ob_list:
        num_meshes += 1

        ret = extend(ob, operator.properties.mode, use_uv_selection)
        if ret != STATUS_OK:
            num_errors += 1
            status |= ret

    if num_errors == num_meshes:
        if status & STATUS_ERR_NOT_QUAD:
            operator.report({'ERROR'}, "Active face must be a quad")
        elif status & STATUS_ERR_NOT_SELECTED:
            operator.report({'ERROR'}, "Active face not selected")
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
