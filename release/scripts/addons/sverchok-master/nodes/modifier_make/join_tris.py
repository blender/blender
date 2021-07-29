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

from bpy.props import FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata


def join_tris(verts, faces, self):
    if not verts:
        return False

    bm = bmesh_from_pydata(verts, [], faces)

    bmesh.ops.join_triangles(
        bm, faces=bm.faces,
        angle_face_threshold=self.face_threshold,
        angle_shape_threshold=self.shape_threshold
    )
    bm.verts.index_update()
    bm.faces.index_update()

    faces_out = []
    verts_out = [vert.co[:] for vert in bm.verts]
    _ = [faces_out.append([v.index for v in face.verts]) for face in bm.faces]

    bm.clear()
    bm.free()
    return (verts_out, faces_out)


class SvJoinTrianglesNode(bpy.types.Node, SverchCustomTreeNode):
    '''Join coplanar Triangles'''
    bl_idname = 'SvJoinTrianglesNode'
    bl_label = 'Join Triangles'
    bl_icon = 'OUTLINER_OB_EMPTY'

    face_threshold = FloatProperty(
        min=0.0, step=0.1,
        name='angle face threshold', description='not sure',
        update=updateNode)

    shape_threshold = FloatProperty(
        min=0.0, step=0.1,
        name='angle shape threshold', description='not sure',
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')
        self.inputs.new('StringsSocket', 'Polygons')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Polygons')

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.prop(self, 'face_threshold', text='face')
        col.prop(self, 'shape_threshold', text='shape')

    def process(self):
        if not self.outputs['Polygons'].is_linked:
            return

        verts = self.inputs['Vertices'].sv_get()
        faces = self.inputs['Polygons'].sv_get()
        if not len(verts) == len(faces):
            return

        verts_out = []
        polys_out = []

        for v_obj, f_obj in zip(verts, faces):
            res = join_tris(v_obj, f_obj, self)
            if not res:
                return
            verts_out.append(res[0])
            polys_out.append(res[1])

        self.outputs['Vertices'].sv_set(verts_out)
        self.outputs['Polygons'].sv_set(polys_out)



def register():
    bpy.utils.register_class(SvJoinTrianglesNode)


def unregister():
    bpy.utils.unregister_class(SvJoinTrianglesNode)
