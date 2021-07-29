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
import mathutils
from mathutils.geometry import tessellate_polygon as tessellate

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh


"""

This node is a heavier implementation of Triangulate until someone finds time to figure
out the bug in the original node.

"""

class SvHeavyTriangulateNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Triangulate mesh (Heavy)'''
    bl_idname = 'SvHeavyTriangulateNode'
    bl_label = 'Triangulate mesh (heavy)'
    bl_icon = 'MOD_TRIANGULATE'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Polygons', 'Polygons')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')

    def process(self):

        inputs = self.inputs
        outputs = self.outputs

        if not (inputs['Vertices'].is_linked and inputs['Polygons'].is_linked):
            return

        named = ['Vertices', 'Edges', 'Polygons']
        if not (any(outputs[name].is_linked for name in named)):
            return

        vertices_s = inputs['Vertices'].sv_get(default=[[]])
        faces_s = inputs['Polygons'].sv_get(default=[[]])

        result_vertices = []
        result_edges = []
        result_faces = []

        meshes = match_long_repeat([vertices_s, faces_s])

        for vertices, faces in zip(*meshes):

            bm = bmesh_from_pydata(vertices, [], faces)

            new_edges = []
            new_faces = []

            for f in bm.faces:
                coords = [v.co for v in f.verts]
                indices = [v.index for v in f.verts]

                if len(coords) > 3:
                    for pol in tessellate([coords]):
                        new_faces.append([indices[i] for i in pol])
                else:
                    new_faces.append([v.index for v in f.verts])

            result_vertices.append([v.co[:] for v in bm.verts])
            result_edges.append(new_edges)
            result_faces.append(new_faces)

        output_list = [
            ['Vertices', result_vertices],
            ['Edges', result_edges],
            ['Polygons', result_faces]
        ]

        for output_name, output_data in output_list:
            if outputs[output_name].is_linked:
                outputs[output_name].sv_set(output_data)


def register():
    bpy.utils.register_class(SvHeavyTriangulateNode)


def unregister():
    bpy.utils.unregister_class(SvHeavyTriangulateNode)
