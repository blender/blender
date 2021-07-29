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

from mathutils import Matrix, Vector
#from math import copysign

import bpy
from bpy.props import IntProperty, FloatProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList, Matrix_generate
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

def is_matrix(lst):
    return len(lst) == 4 and len(lst[0]) == 4

class SvExtrudeEdgesNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Extrude edges '''
    bl_idname = 'SvExtrudeEdgesNode'
    bl_label = 'Extrude Edges'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Edg_Pol', 'Edg_Pol')
        #self.inputs.new('StringsSocket', 'Polygons', 'Polygons')
        #self.inputs.new('StringsSocket', 'ExtrudeEdges')
        self.inputs.new('MatrixSocket', "Matrices")

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')
        self.outputs.new('VerticesSocket', 'NewVertices')
        self.outputs.new('StringsSocket', 'NewEdges')
        self.outputs.new('StringsSocket', 'NewFaces')
  
    def process(self):
        # inputs
        if not (self.inputs['Vertices'].is_linked):
            return

        vertices_s = self.inputs['Vertices'].sv_get()
        edges_s = self.inputs['Edg_Pol'].sv_get(default=[[]])
        #faces_s = self.inputs['Polygons'].sv_get(default=[[]])
        matrices_s = self.inputs['Matrices'].sv_get(default=[[]])
        if is_matrix(matrices_s[0]):
            matrices_s = [Matrix_generate(matrices_s)]
        else:
            matrices_s = [Matrix_generate(matrices) for matrices in matrices_s]
        #extrude_edges_s = self.inputs['ExtrudeEdges'].sv_get(default=[[]])

        result_vertices = []
        result_edges = []
        result_faces = []
        result_ext_vertices = []
        result_ext_edges = []
        result_ext_faces = []


        meshes = match_long_repeat([vertices_s, edges_s, matrices_s]) #, extrude_edges_s])
        
        for vertices, edges, matrices in zip(*meshes):
            if len(edges[0]) == 2:
                faces = []
            else:
                faces = edges.copy()
                edges = []
            if not matrices:
                matrices = [Matrix()]
            
            bm = bmesh_from_pydata(vertices, edges, faces)
            # better to do it in separate node, not integrate by default.
            #if extrude_edges:
            #    b_edges = []
            #    for edge in extrude_edges:
            #        b_edge = [e for e in bm.edges if set([v.index for v in e.verts]) == set(edge)]
            #        b_edges.append(b_edge[0])
            #else:
            b_edges = bm.edges

            new_geom = bmesh.ops.extrude_edge_only(bm, edges=b_edges, use_select_history=False)['geom']

            extruded_verts = [v for v in new_geom if isinstance(v, bmesh.types.BMVert)]

            for vertex, matrix in zip(*match_long_repeat([extruded_verts, matrices])):
                bmesh.ops.transform(bm, verts=[vertex], matrix=matrix, space=Matrix())

            extruded_verts = [tuple(v.co) for v in extruded_verts]

            extruded_edges = [e for e in new_geom if isinstance(e, bmesh.types.BMEdge)]
            extruded_edges = [tuple(v.index for v in edge.verts) for edge in extruded_edges]

            extruded_faces = [f for f in new_geom if isinstance(f, bmesh.types.BMFace)]
            extruded_faces = [[v.index for v in edge.verts] for edge in extruded_faces]

            new_vertices, new_edges, new_faces = pydata_from_bmesh(bm)
            bm.free()

            result_vertices.append(new_vertices)
            result_edges.append(new_edges)
            result_faces.append(new_faces)
            result_ext_vertices.append(extruded_verts)
            result_ext_edges.append(extruded_edges)
            result_ext_faces.append(extruded_faces)

        if self.outputs['Vertices'].is_linked:
            self.outputs['Vertices'].sv_set(result_vertices)
        if self.outputs['Edges'].is_linked:
            self.outputs['Edges'].sv_set(result_edges)
        if self.outputs['Polygons'].is_linked:
            self.outputs['Polygons'].sv_set(result_faces)
        if self.outputs['NewVertices'].is_linked:
            self.outputs['NewVertices'].sv_set(result_ext_vertices)
        if self.outputs['NewEdges'].is_linked:
            self.outputs['NewEdges'].sv_set(result_ext_edges)
        if self.outputs['NewFaces'].is_linked:
            self.outputs['NewFaces'].sv_set(result_ext_faces)


def register():
    bpy.utils.register_class(SvExtrudeEdgesNode)


def unregister():
    bpy.utils.unregister_class(SvExtrudeEdgesNode)

if __name__ == '__main__':
    register()


