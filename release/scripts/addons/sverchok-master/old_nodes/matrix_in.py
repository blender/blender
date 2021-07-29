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
import mathutils
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (matrixdef, Matrix_listing, Vector_generate)


class MatrixGenNode(bpy.types.Node, SverchCustomTreeNode):
    ''' MatrixGenerator '''
    bl_idname = 'MatrixGenNode'
    bl_label = 'Matrix in'
    bl_icon = 'OUTLINER_OB_EMPTY'

    replacement_nodes = [('SvMatrixGenNodeMK2', None, None)]

    def sv_init(self, context):
        s = self.inputs.new('VerticesSocket', "Location")
        s.use_prop = True
        s = self.inputs.new('VerticesSocket', "Scale")
        s.use_prop = True
        s.prop = (1, 1 , 1)
        s = self.inputs.new('VerticesSocket', "Rotation")
        s.use_prop = True
        s.prop = (0, 0, 1)
        self.inputs.new('StringsSocket', "Angle")
        self.outputs.new('MatrixSocket', "Matrix")

    def process(self):
        L,S,R,A = self.inputs
        Ma = self.outputs[0]
        if not Ma.is_linked:
            return
        loc = Vector_generate(L.sv_get())
        scale = Vector_generate(S.sv_get())
        rot = Vector_generate(R.sv_get())
        rotA, angle = [[]], [[0.0]]
        # ability to add vector & vector difference instead of only rotation values
        if A.is_linked:
            if A.links[0].from_socket.bl_idname == 'VerticesSocket':
                rotA = Vector_generate(A.sv_get())
                angle = [[]]
            else:
                angle = A.sv_get()
                rotA = [[]]
        max_l = max(len(loc[0]), len(scale[0]), len(rot[0]), len(angle[0]), len(rotA[0]))
        orig = []
        for l in range(max_l):
            M = mathutils.Matrix()
            orig.append(M)
        matrixes_ = matrixdef(orig, loc, scale, rot, angle, rotA)
        matrixes = Matrix_listing(matrixes_)
        Ma.sv_set(matrixes)


def register():
    bpy.utils.register_class(MatrixGenNode)


def unregister():
    bpy.utils.unregister_class(MatrixGenNode)

if __name__ == '__main__':
    register()
