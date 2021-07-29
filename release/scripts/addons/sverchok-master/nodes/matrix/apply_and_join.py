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
from bpy.props import BoolProperty
from mathutils import Matrix, Vector
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (Matrix_generate, updateNode)
from sverchok.utils.sv_mesh_utils import mesh_join


class SvMatrixApplyJoinNode(bpy.types.Node, SverchCustomTreeNode):
    '''
    M * verts (optional join)  ///

    Multiply vectors on matrices with several objects in output,
    and process edges & faces too 
    '''
    bl_idname = 'SvMatrixApplyJoinNode'
    bl_label = 'Matrix Apply'
    bl_icon = 'OUTLINER_OB_EMPTY'

    do_join = BoolProperty(name='Join', default=True, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Faces")
        self.inputs.new('MatrixSocket', "Matrices")
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Faces")

    def draw_buttons(self, context, layout):
        layout.prop(self, "do_join")

    def process(self):
        if not self.inputs['Matrices'].is_linked:
            return
        vertices = self.inputs['Vertices'].sv_get()
        matrices = self.inputs['Matrices'].sv_get()
        matrices = Matrix_generate(matrices)
        n = len(matrices)
        result_vertices = (vertices*n)[:n]
        outV = []
        for i, i2 in zip(matrices, result_vertices):
            outV.append([(i*Vector(v))[:] for v in i2])
        edges = self.inputs['Edges'].sv_get(default=[[]])
        faces = self.inputs['Faces'].sv_get(default=[[]])
        result_edges = (edges * n)[:n]
        result_faces = (faces * n)[:n]
        if self.do_join:
            outV, result_edges, result_faces = mesh_join(outV, result_edges, result_faces)
            outV, result_edges, result_faces = [outV], [result_edges], [result_faces]
        self.outputs['Edges'].sv_set(result_edges)
        self.outputs['Faces'].sv_set(result_faces)
        self.outputs['Vertices'].sv_set(outV)


def register():
    bpy.utils.register_class(SvMatrixApplyJoinNode)


def unregister():
    bpy.utils.unregister_class(SvMatrixApplyJoinNode)
