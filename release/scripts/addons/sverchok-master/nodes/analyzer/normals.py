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

import math

from mathutils import Vector, Matrix

import bpy
from bpy.props import BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

def calc_mesh_normals(vertices, edges, faces):
    bm = bmesh_from_pydata(vertices, edges, faces, normal_update=True)
    vertex_normals = []
    face_normals = []
    for vertex in bm.verts:
        vertex_normals.append(tuple(vertex.normal))
    for face in bm.faces:
        face_normals.append(tuple(face.normal))
    bm.free()
    return vertex_normals, face_normals

class GetNormalsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Calculate normals of faces and vertices '''
    bl_idname = 'GetNormalsNode'
    bl_label = 'Calculate normals'
    bl_icon = 'SNAP_NORMAL'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Polygons")

        self.outputs.new('VerticesSocket', "FaceNormals")
        self.outputs.new('VerticesSocket', "VertexNormals")

    def process(self):

        if not (self.outputs['VertexNormals'].is_linked or self.outputs['FaceNormals'].is_linked):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])

        result_vertex_normals = []
        result_face_normals = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s])
        for vertices, edges, faces in zip(*meshes):
            vertex_normals, face_normals = calc_mesh_normals(vertices, edges, faces)
            result_vertex_normals.append(vertex_normals)
            result_face_normals.append(face_normals)

        if self.outputs['FaceNormals'].is_linked:
            self.outputs['FaceNormals'].sv_set(result_face_normals)
        if self.outputs['VertexNormals'].is_linked:
            self.outputs['VertexNormals'].sv_set(result_vertex_normals)

def register():
    bpy.utils.register_class(GetNormalsNode)


def unregister():
    bpy.utils.unregister_class(GetNormalsNode)

