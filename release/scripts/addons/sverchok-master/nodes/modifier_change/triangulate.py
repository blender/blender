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


class SvTriangulateNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Triangulate mesh '''
    bl_idname = 'SvTriangulateNode'
    bl_label = 'Triangulate mesh'
    bl_icon = 'MOD_TRIANGULATE'

    quad_modes = [
        ("0", "Beauty", "Split the quads in nice triangles, slower method", 1),
        ("1", "Fixed", "Split the quads on the 1st and 3rd vertices", 2),
        ("2", "Fixed Alternate", "Split the quads on the 2nd and 4th vertices", 3),
        ("3", "Shortest Diagonal", "Split the quads based on the distance between the vertices", 4)
    ]

    ngon_modes = [
        ("0", "Beauty", "Arrange the new triangles nicely, slower method", 1),
        ("1", "Clip", "Split the ngons using a scanfill algorithm", 2)
    ]

    quad_mode = EnumProperty(
        name='Quads mode',
        description="Quads processing mode",
        items=quad_modes,
        default="0",
        update=updateNode)

    ngon_mode = EnumProperty(
        name="Polygons mode",
        description="Polygons processing mode",
        items=ngon_modes,
        default="0",
        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Edges', 'Edges')
        self.inputs.new('StringsSocket', 'Polygons', 'Polygons')
        self.inputs.new('StringsSocket', 'Mask')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')
        self.outputs.new('StringsSocket', 'NewEdges')
        self.outputs.new('StringsSocket', 'NewPolys')

    def draw_buttons(self, context, layout):
        layout.prop(self, "quad_mode")
        layout.prop(self, "ngon_mode")

    def process(self):
        if not (self.inputs['Vertices'].is_linked and self.inputs['Polygons'].is_linked):
            return
        if not (any(self.outputs[name].is_linked for name in ['Vertices', 'Edges', 'Polygons', 'NewEdges', 'NewPolys'])):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])
        mask_s = self.inputs['Mask'].sv_get(default=[[True]])

        result_vertices = []
        result_edges = []
        result_faces = []
        result_new_edges = []
        result_new_faces = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, mask_s])

        for vertices, edges, faces, mask in zip(*meshes):

            bm = bmesh_from_pydata(vertices, edges, faces)
            fullList(mask, len(faces))

            b_faces = []
            for m, face in zip(mask, bm.faces):
                if m:
                    b_faces.append(face)

            res = bmesh.ops.triangulate(
                bm, faces=b_faces,
                quad_method=int(self.quad_mode),
                ngon_method=int(self.ngon_mode))

            b_new_edges = [tuple(v.index for v in edge.verts) for edge in res['edges']]
            b_new_faces = [[v.index for v in face.verts] for face in res['faces']]

            new_vertices, new_edges, new_faces = pydata_from_bmesh(bm)
            bm.free()

            result_vertices.append(new_vertices)
            result_edges.append(new_edges)
            result_faces.append(new_faces)
            result_new_edges.append(b_new_edges)
            result_new_faces.append(b_new_faces)

        if self.outputs['Vertices'].is_linked:
            self.outputs['Vertices'].sv_set(result_vertices)
        if self.outputs['Edges'].is_linked:
            self.outputs['Edges'].sv_set(result_edges)
        if self.outputs['Polygons'].is_linked:
            self.outputs['Polygons'].sv_set(result_faces)
        if self.outputs['NewEdges'].is_linked:
            self.outputs['NewEdges'].sv_set(result_new_edges)
        if self.outputs['NewPolys'].is_linked:
            self.outputs['NewPolys'].sv_set(result_new_faces)


def register():
    bpy.utils.register_class(SvTriangulateNode)


def unregister():
    bpy.utils.unregister_class(SvTriangulateNode)
