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

import bpy
from bpy.props import BoolProperty
import bmesh
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, Matrix_generate, Vector_generate


# based on CrossSectionNode
# but using python bmesh code for driving
# by Linus Yng / edits+upgrades Dealga McArdle


def bisect(cut_me_vertices, cut_me_edges, pp, pno, outer, inner, fill):

    if not cut_me_edges or not cut_me_vertices:
        return False

    cut_me_polygons = []
    if len(cut_me_edges[0]) > 2:
        cut_me_polygons = cut_me_edges.copy()
        cut_me_edges = []

    bm = bmesh.new()
    bm_verts = [bm.verts.new(v) for v in cut_me_vertices]
    if cut_me_edges:
        for edge in cut_me_edges:
            bm.edges.new((bm_verts[edge[0]], bm_verts[edge[1]]))
    else:
        for face in cut_me_polygons:
            bm.faces.new([bm_verts[i] for i in face])

    geom_in = bm.verts[:] + bm.edges[:] + bm.faces[:]
    res = bmesh.ops.bisect_plane(
        bm, geom=geom_in, dist=0.00001,
        plane_co=pp, plane_no=pno, use_snap_center=False,
        clear_outer=outer, clear_inner=inner)

    # this needs work function with solid geometry
    if fill:
        fres = bmesh.ops.edgenet_prepare(
            bm, edges=[e for e in res['geom_cut'] if isinstance(e, bmesh.types.BMEdge)]
        )
        bmesh.ops.edgeloop_fill(bm, edges=fres['edges'])
    
    edges = []
    faces = []
    bm.verts.index_update()
    bm.edges.index_update()
    bm.faces.index_update()

    for edge in bm.edges[:]:
        edges.append([v.index for v in edge.verts[:]])
    verts = [vert.co[:] for vert in bm.verts[:]]
    for face in bm.faces:
        faces.append([v.index for v in face.verts[:]])

    bm.clear()
    bm.free()

    return (verts, edges, faces)


class SvBisectNode(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvBisectNode'
    bl_label = 'Bisect'
    bl_icon = 'OUTLINER_OB_EMPTY'

    inner = BoolProperty(
        name='inner', description='clear inner',
        default=False, update=updateNode)

    outer = BoolProperty(
        name='outer', description='clear outer',
        default=False, update=updateNode)

    fill = BoolProperty(
        name='fill', description='Fill cuts', 
        default=False, update=updateNode)

    slice_mode = BoolProperty(
        name="Per Object", update=updateNode, default=False,
        description="slice each object with all matrices, or match object and matrices individually"
    )

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('StringsSocket', 'edg_pol')
        self.inputs.new('MatrixSocket', 'cut_matrix')

        self.outputs.new('VerticesSocket', 'vertices')
        self.outputs.new('StringsSocket', 'edges')
        self.outputs.new('StringsSocket', 'polygons')

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, 'inner', text="Inner", toggle=True)
        row.prop(self, 'outer', text="Outer", toggle=True)
        row = layout.row(align=True)
        row.prop(self, 'fill', text="Fill", toggle=True)
        if hasattr(self, 'slice_mode'):
            row.prop(self, 'slice_mode', toggle=True)

    def process(self):

        if not all([s.is_linked for s in self.inputs]):
            return

        if not self.outputs['vertices'].is_linked:
            return

        verts_ob = Vector_generate(self.inputs['vertices'].sv_get())
        edg_pols = self.inputs['edg_pol'].sv_get()
        cut_mats_ = self.inputs['cut_matrix'].sv_get()
        cut_mats = Matrix_generate(cut_mats_)
        verts_out = []
        edges_out = []
        polys_out = []

        if not hasattr(self, 'slice_mode') or not self.slice_mode:

            for cut_mat in cut_mats:
                pp = cut_mat.to_translation()
                pno = Vector((0.0, 0.0, 1.0)) * cut_mat.to_3x3().transposed()
                for obj in zip(verts_ob, edg_pols):
                    res = bisect(obj[0], obj[1], pp, pno, self.outer, self.inner, self.fill)
                    if not res:
                        return
                    verts_out.append(res[0])
                    edges_out.append(res[1])
                    polys_out.append(res[2])
        
        else:

            for idx, (obj) in enumerate(zip(verts_ob, edg_pols)):

                cut_mat = cut_mats[idx if idx < len(cut_mats) else -1]
                pp = cut_mat.to_translation()
                pno = Vector((0.0, 0.0, 1.0)) * cut_mat.to_3x3().transposed()
        
                res = bisect(obj[0], obj[1], pp, pno, self.outer, self.inner, self.fill)
                if not res:
                    return
                verts_out.append(res[0])
                edges_out.append(res[1])
                polys_out.append(res[2])            


        self.outputs['vertices'].sv_set(verts_out)
        self.outputs['edges'].sv_set(edges_out)
        self.outputs['polygons'].sv_set(polys_out)


def register():
    bpy.utils.register_class(SvBisectNode)


def unregister():
    bpy.utils.unregister_class(SvBisectNode)
