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

from itertools import product

import bpy
from mathutils import Matrix

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (dataCorrect, Matrix_listing)


class SvBBoxNode(bpy.types.Node, SverchCustomTreeNode):
    '''Bounding box'''
    bl_idname = 'SvBBoxNode'
    bl_label = 'Bounding box'
    bl_icon = 'BBOX'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges', 'Edges')
        self.outputs.new('VerticesSocket', 'Mean')
        self.outputs.new('MatrixSocket', 'Center', 'Center')

    def process(self):
        if not self.inputs['Vertices'].is_linked:
            return
        if not any(s.is_linked for s in self.outputs):
            return
        has_mat_out = bool(self.outputs['Center'].is_linked)
        has_mean = bool(self.outputs['Mean'].is_linked)
        has_vert_out = bool(self.outputs['Vertices'].is_linked)
        vert = self.inputs['Vertices'].sv_get(deepcopy=False)
        vert = dataCorrect(vert, nominal_dept=2)

        if vert:
            verts_out = []
            edges_out = []
            edges = [
                (0, 1), (1, 3), (3, 2), (2, 0),  # bottom edges
                (4, 5), (5, 7), (7, 6), (6, 4),  # top edges
                (0, 4), (1, 5), (2, 6), (3, 7)  # sides
            ]
            mat_out = []
            mean_out = []

            for v in vert:
                if has_mat_out or has_vert_out:
                    maxmin = list(zip(map(max, *v), map(min, *v)))
                    out = list(product(*reversed(maxmin)))
                    verts_out.append([l[::-1] for l in out[::-1]])
                edges_out.append(edges)
                if has_mat_out:
                    center = [(u+v)*.5 for u, v in maxmin]
                    mat = Matrix.Translation(center)
                    scale = [(u-v)*.5 for u, v in maxmin]
                    for i, s in enumerate(scale):
                        mat[i][i] = s
                    mat_out.append(mat)
                if has_mean:
                    avr = list(map(sum, zip(*v)))
                    avr = [n/len(v) for n in avr]
                    mean_out.append([avr])

            if has_vert_out:
                self.outputs['Vertices'].sv_set(verts_out)

            if self.outputs['Edges'].is_linked:
                self.outputs['Edges'].sv_set(edges_out)

            if has_mean:
                self.outputs['Mean'].sv_set(mean_out)

            if self.outputs['Center'].is_linked:
                self.outputs['Center'].sv_set(Matrix_listing(mat_out))



def register():
    bpy.utils.register_class(SvBBoxNode)


def unregister():
    bpy.utils.unregister_class(SvBBoxNode)
