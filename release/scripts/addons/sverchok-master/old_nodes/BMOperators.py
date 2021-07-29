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
import bmesh
from bpy.props import EnumProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, enum_item as e, second_as_first_cycle as safc)


class SvBMOpsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BMesh Ops '''
    bl_idname = 'SvBMOpsNode'
    bl_label = 'BMesh Ops'
    bl_icon = 'OUTLINER_OB_EMPTY'

    PV = ['remove_doubles(bm,verts=e,dist=v[0])',
          'collapse(bm,edges=e,uvs=v[0])',
          'unsubdivide(bm,verts=e,iterations=v[0])',
          'holes_fill(bm,edges=e,sides=v[0])',
          'dissolve_faces(bm,faces=e,use_verts=v[0])',
          'connect_verts_concave(bm,faces=e)',
          'recalc_face_normals(bm,faces=e)',
          'rotate_edges(bm, edges=e, use_ccw=v[0])',
          'connect_verts_nonplanar(bm,angle_limit=v[0],faces=e)',
          'triangulate(bm,faces=e,quad_method=v[0],ngon_method=v[1])',
          'dissolve_edges(bm,edges=e,use_verts=v[0],use_face_split=v[1])',
          'dissolve_verts(bm,verts=e,use_face_split=v[0],use_boundary_tear=v[1])',
          'grid_fill(bm,edges=e,mat_nr=v[0],use_smooth=v[1],use_interp_simple=v[2])',
          'poke(bm,faces=e,offset=v[0],center_mode=v[1],use_relative_offset=v[2])',
          'bridge_loops(bm,edges=e,use_pairs=v[0],use_cyclic=v[1],use_merge=v[2],merge_factor=v[3],twist_offset=v[4])',
          'smooth_vert(bm,verts=e,factor=v[0],mirror_clip_x=v[1],mirror_clip_y=v[2],mirror_clip_z=v[3],clip_dist=v[4],use_axis_x=v[5],use_axis_y=v[6],use_axis_z=v[7])',
          'join_triangles(bm,faces=e,cmp_seam=v[0],cmp_sharp=v[1],cmp_uvs=v[2],cmp_vcols=v[3],cmp_materials=v[4],angle_face_threshold=v[5],angle_shape_threshold=v[6])',
          'subdivide_edgering(bm,edges=e,interp_mode=v[0],smooth=v[1],cuts=v[2],profile_shape=v[3],profile_shape_factor=v[4])',
          'inset_individual(bm,faces=e,thickness=v[0],depth=v[1],use_even_offset=v[2],use_interpolate=v[3],use_relative_offset=v[4])',
          'inset_region(bm,faces=e,use_boundary=v[0],use_even_offset=v[1],use_interpolate=v[2],use_relative_offset=v[3],use_edge_rail=v[4],thickness=v[5],depth=v[6],use_outset=v[7])',
          ]

    oper = EnumProperty(name="BMop", default=PV[0], items=e(PV), update=updateNode)

    def sv_init(self, context):
        si = self.inputs.new
        si('StringsSocket', 'bmesh_list')
        si('StringsSocket', 'Value(v)')
        si('StringsSocket', 'BM_element(e)')
        self.outputs.new('StringsSocket', 'bmesh_list')

    def draw_buttons(self, context, layout):
        layout.prop(self, "oper", "Get")

    def process(self):
        if not self.outputs['bmesh_list'].is_linked:
            return
        bml, val, e = self.inputs
        obj = bml.sv_get()
        v = safc(obj, val.sv_get([[1]*12]))
        outp = []
        op = "bmesh.ops."+self.oper
        if e.is_linked:
            element = e.sv_get()
            for bm, v, e in zip(obj,v, element):
                exec(op)
                outp.append(bm.copy())
                bm.free()
        else:
            if "verts=e" in op:
                cur = "verts"
            elif "edges=e" in op:
                cur = "edges"
            elif "faces=e" in op:
                cur = "faces"
            for bm, v in zip(obj,v):
                e = getattr(bm, cur)
                exec(op)
                outp.append(bm.copy())
                bm.free()
        self.outputs['bmesh_list'].sv_set(outp)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBMOpsNode)


def unregister():
    bpy.utils.unregister_class(SvBMOpsNode)
