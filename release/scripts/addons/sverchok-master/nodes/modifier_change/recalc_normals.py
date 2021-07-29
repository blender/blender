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
from bpy.props import IntProperty, EnumProperty, BoolProperty, FloatProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh


class SvRecalcNormalsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Recalc face normals '''
    bl_idname = 'SvRecalcNormalsNode'
    bl_label = 'Recalc normals'
    bl_icon = 'OUTLINER_OB_EMPTY'

    invert = BoolProperty(name="Inside",
        description="Calculate inside normals",
        default=False,
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Edges', 'Edges')
        self.inputs.new('StringsSocket', 'Polygons', 'Polygons')
        self.inputs.new('StringsSocket', 'Mask')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')

    def draw_buttons(self, context, layout):
        layout.prop(self, "invert")

    def process(self):
        if not (self.inputs['Vertices'].is_linked and self.inputs['Polygons'].is_linked):
            return
        if not (any(self.outputs[name].is_linked for name in ['Vertices', 'Edges', 'Polygons'])):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])
        mask_s = self.inputs['Mask'].sv_get(default=[[True]])

        result_vertices = []
        result_edges = []
        result_faces = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, mask_s])

        for vertices, edges, faces, mask in zip(*meshes):

            bm = bmesh_from_pydata(vertices, edges, faces, normal_update=True)
            fullList(mask, len(faces))

            b_faces = []
            for m, face in zip(mask, bm.faces):
                if m:
                    b_faces.append(face)

            bmesh.ops.recalc_face_normals(bm, faces=b_faces)
            new_vertices, new_edges, new_faces = pydata_from_bmesh(bm)
            bm.free()

            if self.invert:
                new_faces = [list(reversed(face)) for face in new_faces]

            result_vertices.append(new_vertices)
            result_edges.append(new_edges)
            result_faces.append(new_faces)

        if self.outputs['Vertices'].is_linked:
            self.outputs['Vertices'].sv_set(result_vertices)
        if self.outputs['Edges'].is_linked:
            self.outputs['Edges'].sv_set(result_edges)
        if self.outputs['Polygons'].is_linked:
            self.outputs['Polygons'].sv_set(result_faces)

def register():
    bpy.utils.register_class(SvRecalcNormalsNode)


def unregister():
    bpy.utils.unregister_class(SvRecalcNormalsNode)

if __name__ == '__main__':
    register()



