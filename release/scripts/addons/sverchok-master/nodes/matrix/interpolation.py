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
from mathutils import Matrix

from sverchok.node_tree import SverchCustomTreeNode, MatrixSocket, StringsSocket
from sverchok.data_structure import (updateNode, fullList,
                                     Matrix_listing, Matrix_generate)


# Matrix are assumed to be in format
# [M1 M2 Mn ...] per Matrix_generate and Matrix_listing
# Instead of empty matrix input identity matrix is used.
# So only one matrix input is needed for useful result
# Factor a list of value float values between 0.0 and 1.0,


class MatrixInterpolationNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Interpolate between two matrices '''
    bl_idname = 'MatrixInterpolationNode'
    bl_label = 'Matrix Interpolation'
    bl_icon = 'OUTLINER_OB_EMPTY'

    factor_ = bpy.props.FloatProperty(name='Factor', description='Interpolation',
                                      default=0.5, min=0.0, max=1.0,
                                      options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Factor", "Factor").prop_name = 'factor_'
        self.inputs.new('MatrixSocket', "A", "A")
        self.inputs.new('MatrixSocket', "B", "B")
        self.outputs.new('MatrixSocket', "C", "C")

    def process(self):
        # inputs
        if not self.outputs['C'].is_linked:
            return
        id_mat = Matrix_listing([Matrix.Identity(4)])
        A = Matrix_generate(self.inputs['A'].sv_get(default=id_mat))
        B = Matrix_generate(self.inputs['B'].sv_get(default=id_mat))
        factor = self.inputs['Factor'].sv_get()


        matrixes_ = []
        # match inputs, first matrix A and B using fullList
        # then extend the factor list if necessary,
        # A and B should control length of list, not interpolation lists
        max_l = max(len(A), len(B))
        fullList(A, max_l)
        fullList(B, max_l)
        if len(factor) < max_l:
            fullList(factor, max_l)
        for i in range(max_l):
            for k in range(len(factor[i])):
                matrixes_.append(A[i].lerp(B[i], factor[i][k]))

        matrixes = Matrix_listing(matrixes_)
        self.outputs['C'].sv_set(matrixes)


def register():
    bpy.utils.register_class(MatrixInterpolationNode)


def unregister():
    bpy.utils.unregister_class(MatrixInterpolationNode)
