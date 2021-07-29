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
from bpy.props import FloatVectorProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, Matrix_listing


class SvMatrixValueIn(bpy.types.Node, SverchCustomTreeNode):
    ''' MatrixValueIn '''
    bl_idname = 'SvMatrixValueIn'
    bl_label = 'Matrix Input'
    bl_icon = 'OUTLINER_OB_EMPTY'

    id_matrix = (1.0, 0.0, 0.0, 0.0,
                 0.0, 1.0, 0.0, 0.0,
                 0.0, 0.0, 1.0, 0.0,
                 0.0, 0.0, 0.0, 1.0)

    matrix = FloatVectorProperty(name="matrix", description="matrix",
                                 default=id_matrix, subtype='MATRIX',
                                 size=16, precision=3, update=updateNode)

    def sv_init(self, context):
        self.outputs.new('MatrixSocket', "Matrix", "Matrix")
        self.width = 300

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        for i in range(4):
            row = col.row(align=True)
            for j in range(i, 16, 4):
                row.prop(self, 'matrix', text='', index=j)

    def draw_buttons_ext(self, context, layout):
        pass

    def process(self):
        if 'Matrix' in self.outputs and self.outputs['Matrix'].is_linked:
            m_out = Matrix_listing([self.matrix])
            self.outputs[0].sv_set(m_out)


def register():
    bpy.utils.register_class(SvMatrixValueIn)


def unregister():
    bpy.utils.unregister_class(SvMatrixValueIn)
