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

from mathutils import Vector, Matrix
from operator import iadd
from functools import reduce

import bpy
from bpy.props import IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, Matrix_generate, Vector_generate, Vector_degenerate


def concat(lists):
    return reduce(iadd, lists)


def Matrix_degenerate(ms):
    return [[j[:] for j in M] for M in ms]


def iterate_matrices(matrices, vertices, edges, faces, count, offset, r=0):
    result_vertices = []
    result_edges = []
    result_faces = []

    x = 0
    for matrix in matrices:
        new_vertices, new_edges, new_faces = iterate(matrices, matrix, vertices, edges, faces, count, offset+x, r)
        x += len(new_vertices)
        result_vertices.extend(new_vertices)
        result_edges.extend(new_edges)
        result_faces.extend(new_faces)

    return result_vertices, result_edges, result_faces


def iterate(matrices, matrix, vertices, edges, faces, count, offset, r=0):
    result_vertices = []
    result_edges = []
    result_faces = []
    if count == 0:
        return result_vertices, result_edges, result_faces

    new_vertices = [matrix*v for v in vertices]
    new_edges = [(v1+offset+r, v2+offset+r) for v1, v2 in edges]
    new_faces = [[v+offset+r for v in face] for face in faces]

    result_vertices.extend(new_vertices)
    result_edges.extend(new_edges)
    result_faces.extend(new_faces)

    n = len(new_vertices)
    rest_vertices, rest_edges, rest_faces = iterate_matrices(matrices, new_vertices, edges, faces, count-1, offset, r+n)

    result_vertices.extend(rest_vertices)
    result_edges.extend(rest_edges)
    result_faces.extend(rest_faces)

    return result_vertices, result_edges, result_faces


def shift_edges(edges, offset):
    return [(v1+offset, v2+offset) for (v1, v2) in edges]


def shift_faces(faces, offset):
    return [[v+offset for v in face] for face in faces]


def calc_matrix_powers(matrices, count):
    if count == 0:
        return []
    if count == 1:
        return matrices

    result = []
    result.extend(matrices)
    for m in matrices:
        result.extend([m*n for n in calc_matrix_powers(matrices, count-1)])

    return result


class SvIterateNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Iterate matrix transformation '''
    bl_idname = 'SvIterateNode'
    bl_label = 'Iterate matrix transformation'
    bl_icon = 'OUTLINER_OB_EMPTY'

    count_ = IntProperty(
        name='Iterations', description='Number of iterations',
        default=1, min=0, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('MatrixSocket', "Matrix", "Matrix")
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', 'Edges', 'Edges')
        self.inputs.new('StringsSocket', 'Polygons', 'Polygons')
        self.inputs.new('StringsSocket', "Iterations", "Iterations").prop_name = "count_"

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')
        self.outputs.new('MatrixSocket', 'Matrices')

    def process(self):
        # inputs
        if not self.inputs['Matrix'].is_linked:
            return

        matrices = self.inputs['Matrix'].sv_get()
        matrices = Matrix_generate(matrices)
        counts = self.inputs['Iterations'].sv_get()[0]
        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        vertices_s = Vector_generate(vertices_s)
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])

        if self.outputs['Vertices'].is_linked or self.outputs['Matrices'].is_linked:

            result_vertices = []
            result_edges = []
            result_faces = []
            result_matrices = []

            if edges_s[0]:
                if len(edges_s) != len(vertices_s):
                    raise Exception(
                        "Invalid number of edges: {} != {}".format(len(edges_s), len(vertices_s))
                    )

            if faces_s[0]:
                if len(faces_s) != len(vertices_s):
                    raise Exception(
                        "Invalid number of polygons: {} != {}".format(len(faces_s), len(vertices_s))
                    )

            meshes = match_long_repeat([vertices_s, edges_s, faces_s, counts])

            offset = 0
            for vertices, edges, faces, count in zip(*meshes):
                result_vertices.extend(vertices)

                result_edges.extend(shift_edges(edges, offset))
                result_faces.extend(shift_faces(faces, offset))
                #result_matrices.extend([Matrix()] * len(matrices))
                result_matrices.append(Matrix())
                offset += len(vertices)

                new_vertices, new_edges, new_faces = iterate_matrices(matrices, vertices, edges, faces, count, offset)
                offset += len(new_vertices)
                new_matrices = calc_matrix_powers(matrices, count)

                result_vertices.extend(new_vertices)
                result_edges.extend(new_edges)
                result_faces.extend(new_faces)
                result_matrices.extend(new_matrices)

            result_vertices = Vector_degenerate([result_vertices])
            if self.outputs['Vertices'].is_linked:
                self.outputs['Vertices'].sv_set(result_vertices)
            if self.outputs['Edges'].is_linked:
                self.outputs['Edges'].sv_set([result_edges])
            if self.outputs['Polygons'].is_linked:
                self.outputs['Polygons'].sv_set([result_faces])
            if self.outputs['Matrices'].is_linked:
                self.outputs['Matrices'].sv_set(Matrix_degenerate(result_matrices))


def register():
    bpy.utils.register_class(SvIterateNode)


def unregister():
    bpy.utils.unregister_class(SvIterateNode)


if __name__ == '__main__':
    register()
