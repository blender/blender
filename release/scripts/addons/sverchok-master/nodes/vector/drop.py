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
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (
    dataCorrect, Matrix_generate, updateNode,
    Vector_generate, Vector_degenerate
)


class VectorDropNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Drop vertices depending on matrix, as on default rotation, drops to zero matrix '''
    bl_idname = 'VectorDropNode'
    bl_label = 'Vector Drop'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vectors", "Vectors")
        self.inputs.new('MatrixSocket', "Matrixes", "Matrixes")
        self.outputs.new('VerticesSocket', "Vectors", "Vectors")

    def process(self):
        # inputs
        if not self.outputs['Vectors'].is_linked:
            return
                        
        vecs_ = self.inputs['Vectors'].sv_get()
        vecs = Vector_generate(vecs_)
        
        mats_ = dataCorrect(self.inputs['Matrixes'].sv_get())
        mats = Matrix_generate(mats_)
        
        vectors = self.vecscorrect(vecs, mats)
        self.outputs['Vectors'].sv_set(vectors)

    @staticmethod
    def vecscorrect(vecs, mats):
        out = []
        lengthve = len(vecs)-1
        for i, m in enumerate(mats):
            out_ = []
            k = i
            if k > lengthve:
                k = lengthve
            vec_c = Vector((0, 0, 0))
            for v in vecs[k]:
                vec = v*m
                out_.append(vec)
                vec_c += vec

            vec_c = vec_c / len(vecs[k])

            v = out_[1]-out_[0]
            w = out_[2]-out_[0]
            A = v.y*w.z - v.z*w.y
            B = -v.x*w.z + v.z*w.x
            C = v.x*w.y - v.y*w.x
            #D = -out_[0].x*A - out_[0].y*B - out_[0].z*C

            norm = Vector((A, B, C)).normalized()
            vec0 = Vector((0, 0, 1))

            mat_rot_norm = vec0.rotation_difference(norm).to_matrix().to_4x4()
            out_pre = []
            for v in out_:
                v_out = (v-vec_c) * mat_rot_norm
                out_pre.append(v_out[:])

            out.append(out_pre)

        return out


def register():
    bpy.utils.register_class(VectorDropNode)


def unregister():
    bpy.utils.unregister_class(VectorDropNode)
