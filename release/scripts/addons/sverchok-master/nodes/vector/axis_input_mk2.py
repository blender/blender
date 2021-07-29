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
from bpy.props import StringProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


class SvAxisInputNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' axis input '''

    bl_idname = 'SvAxisInputNodeMK2'
    bl_label = 'Vector X/Y/Z'
    bl_icon = 'MANIPUL'

    m = [("-1", "-1", "", 0), ("0", "0", "", 1), ("1", "1", "", 2)]
    axis_x = EnumProperty(items=m, update=updateNode, name='X', default='1')
    axis_y = EnumProperty(items=m, update=updateNode, name='Y', default='1')
    axis_z = EnumProperty(items=m, update=updateNode, name='Z', default='1')

    def sv_init(self, context):
        self.width = 100
        self.outputs.new('VerticesSocket', "Vector")

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, 'axis_x', text='')
        row.prop(self, 'axis_y', text='')
        row.prop(self, 'axis_z', text='')

    def get_axis(self):
        return int(self.axis_x), int(self.axis_y), int(self.axis_z)

    def draw_label(self):
        return str('[{0}, {1}, {2}]'.format(*self.get_axis()))

    def process(self):
        vec_out = self.outputs[0]
        if vec_out.is_linked:
            vec_out.sv_set([[list(self.get_axis())]])


def register():
    bpy.utils.register_class(SvAxisInputNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvAxisInputNodeMK2)
