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
from sverchok.data_structure import (Vector_generate, Matrix_generate, updateNode)


class SvMatrixTubeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' takes a list of vertices and a list of matrices
        the vertices are to be joined in a ring, copied and transformed by the 1st matrix
        and this ring joined to the previous ring.
        The ring dosen't have to be planar.
        outputs lists of vertices, edges and faces
        ends are capped
    '''
    bl_idname = 'SvMatrixTubeNode'
    bl_label = 'Matrix Tube'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('MatrixSocket', "Matrices")
        self.inputs.new('VerticesSocket', "Vertices")
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Faces")

    def process(self):
        if not self.outputs['Vertices'].is_linked:
            return
        vertices = Vector_generate(self.inputs['Vertices'].sv_get())
        matrices = Matrix_generate(self.inputs['Matrices'].sv_get())
        verts_out, edges_out, faces_out = self.make_tube(matrices, vertices)
        self.outputs['Vertices'].sv_set([verts_out])
        self.outputs['Edges'].sv_set([edges_out])
        self.outputs['Faces'].sv_set([faces_out])

    def make_tube(self, mats, verts):
        edges_out = []
        verts_out = []
        faces_out = []
        vID = 0
        nring = len(verts[0])
        # end face
        faces_out.append(list(range(nring)))
        for i,m in enumerate(mats):
            for j,v in enumerate(verts[0]):
                vout = Matrix(m) * Vector(v)
                verts_out.append(vout.to_tuple())
                vID = j + i*nring
                # rings
                if j != 0:
                    edges_out.append([vID, vID - 1])
                else:
                    edges_out.append([vID, vID + nring-1])
                # lines
                if i != 0:
                    edges_out.append([vID, vID - nring])
                    # faces
                    if j != 0:
                        faces_out.append([vID, vID - nring, vID - nring - 1, vID-1,])
                    else:
                        faces_out.append([vID, vID - nring,  vID-1, vID + nring-1])
        # end face
        # reversing list fixes face normal direction keeps mesh manifold
        f = list(range(vID, vID-nring, -1))
        faces_out.append(f)
        return verts_out, edges_out, faces_out


def register():
    bpy.utils.register_class(SvMatrixTubeNode)


def unregister():
    bpy.utils.unregister_class(SvMatrixTubeNode)
