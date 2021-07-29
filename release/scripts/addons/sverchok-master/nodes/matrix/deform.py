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
from sverchok.node_tree import (SverchCustomTreeNode)
from sverchok.data_structure import (Vector_generate, matrixdef, Matrix_listing,
                            Matrix_generate, updateNode)


class MatrixDeformNode(bpy.types.Node, SverchCustomTreeNode):
    ''' MatrixDeform '''
    bl_idname = 'MatrixDeformNode'
    bl_label = 'Matrix Deform'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('MatrixSocket', "Original")
        self.inputs.new('VerticesSocket', "Location")
        self.inputs.new('VerticesSocket', "Scale")
        self.inputs.new('VerticesSocket', "Rotation")
        self.inputs.new('StringsSocket', "Angle")
        self.outputs.new('MatrixSocket', "Matrix")

    def process(self):
        O,L,S,R,A = self.inputs
        Om = self.outputs[0]
        if Om.is_linked:
            if O.is_linked:
                orig = Matrix_generate(O.sv_get())
            else:
                return
            if L.is_linked:
                loc = Vector_generate(L.sv_get())
            else:
                loc = [[]]
            if S.is_linked:
                scale = Vector_generate(S.sv_get())
            else:
                scale = [[]]
            if R.is_linked:
                rot = Vector_generate(R.sv_get())
            else:
                rot = [[]]

            rotA, angle = [[]], [[0.0]]
            # ability to add vector & vector difference instead of only rotation values
            if A.is_linked:
                if A.links[0].from_socket.bl_idname == 'VerticesSocket':
                    rotA = Vector_generate(A.sv_get())
                    angle = [[]]
                else:
                    angle = A.sv_get()
                    rotA = [[]]
            matrixes_ = matrixdef(orig, loc, scale, rot, angle, rotA)
            matrixes = Matrix_listing(matrixes_)
            Om.sv_set(matrixes)

    def update_socket(self, context):
        updateNode(self, context)


def register():
    bpy.utils.register_class(MatrixDeformNode)


def unregister():
    bpy.utils.unregister_class(MatrixDeformNode)

if __name__ == '__main__':
    register()