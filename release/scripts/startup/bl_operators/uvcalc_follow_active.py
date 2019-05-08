# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# for full docs see...
# https://docs.blender.org/manual/en/dev/editors/uv_image/uv/editing/unwrapping/mapping_types.html#follow-active-quads

import bpy
from bpy.types import Operator


STATUS_OK = (1 << 0)
STATUS_ERR_ACTIVE_FACE = (1 << 1)
STATUS_ERR_NOT_SELECTED = (1 << 2)
STATUS_ERR_NOT_QUAD = (1 << 3)


def extend(obj, EXTEND_MODE):
    import bmesh
    me = obj.data

    bm = bmesh.from_edit_mesh(me)

    faces = [f for f in bm.faces if f.select and len(f.verts) == 4]
    if not faces:
        return 0

    f_act = bm.faces.active

    if f_act is None:
        return STATUS_ERR_ACTIVE_FACE
    if not f_act.select:
        return STATUS_ERR_NOT_SELECTED
    elif len(f_act.verts) != 4:
        return STATUS_ERR_NOT_QUAD

    # Script will fail without UVs.
    if not me.uv_layers:
        me.uv_layers.new()

    uv_act = bm.loops.layers.uv.active

    # our own local walker
    def walk_face_init(faces, f_act):
        # first tag all faces True (so we don't uvmap them)
        for f in bm.faces:
            f.tag = True
        # then tag faces arg False
        for f in faces:
            f.tag = False
        # tag the active face True since we begin there
        f_act.tag = True

    def walk_face(f):
        # all faces in this list must be tagged
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
            # swap
            faces_a, faces_b = faces_b, faces_a
            faces_b.clear()

    def walk_edgeloop(l):
        """
        Could make this a generic function
        """
        e_first = l.edge
        e = None
        while True:
            e = l.edge
            yield e

            # don't step past non-manifold edges
            if e.is_manifold:
                # welk around the quad and then onto the next face
                l = l.link_loop_radial_next
                if len(l.face.verts) == 4:
                    l = l.link_loop_next.link_loop_next
                    if l.edge is e_first:
                        break
                else:
                    break
            else:
                break

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
        #  copy from this face to the one above.

        # get the other loops
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
            fac = edge_lengths[l_b[2].edge.index][0] / edge_lengths[l_a[1].edge.index][0]
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

        extrapolate_uv(fac,
                       l_a_uv[3], l_a_uv[0],
                       l_b_uv[3], l_b_uv[0])

        extrapolate_uv(fac,
                       l_a_uv[2], l_a_uv[1],
                       l_b_uv[2], l_b_uv[1])

    # -------------------------------------------
    # Calculate average length per loop if needed

    if EXTEND_MODE == 'LENGTH_AVERAGE':
        bm.edges.index_update()
        edge_lengths = [None] * len(bm.edges)

        for f in faces:
            # we know its a quad
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

    # done with average length
    # ------------------------

    walk_face_init(faces, f_act)
    for f_triple in walk_face(f_act):
        apply_uv(*f_triple)

    bmesh.update_edit_mesh(me, False)
    return STATUS_OK


def main(context, operator):
    num_meshes = 0
    num_errors = 0
    status = 0

    ob_list = context.objects_in_mode_unique_data
    for ob in ob_list:
        num_meshes += 1

        ret = extend(ob, operator.properties.mode)
        if ret != STATUS_OK:
            num_errors += 1
            status |= ret

    if num_errors == num_meshes:
        if status & STATUS_ERR_NOT_QUAD:
            operator.report({'ERROR'}, "Active face must be a quad")
        elif status & STATUS_ERR_NOT_SELECTED:
            operator.report({'ERROR'}, "Active face not selected")
        else:
            assert((status & STATUS_ERR_ACTIVE_FACE) != 0)
            operator.report({'ERROR'}, "No active face")


class FollowActiveQuads(Operator):
    """Follow UVs from active quads along continuous face loops"""
    bl_idname = "uv.follow_active_quads"
    bl_label = "Follow Active Quads"
    bl_options = {'REGISTER', 'UNDO'}

    mode: bpy.props.EnumProperty(
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
