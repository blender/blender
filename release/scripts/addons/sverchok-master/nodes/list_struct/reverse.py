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
from bpy.props import BoolProperty, IntProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, changable_sockets)


class ListReverseNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List Reverse Node '''
    bl_idname = 'ListReverseNode'
    bl_label = 'List Reverse'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level_to_Reverse',
                        default=2, min=1,
                        update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data", "data")
        self.outputs.new('StringsSocket', 'data', 'data')

    def update(self):
        if 'data' in self.inputs and self.inputs['data'].links:
            # адаптивный сокет
            inputsocketname = 'data'
            outputsocketname = ['data']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if self.outputs[0].is_linked:
            data = self.inputs['data'].sv_get(deepcopy=False)
            output = self.revers(data, self.level)
            self.outputs[0].sv_set(output)

    def revers(self, data, level):
        level -= 1
        if level:
            out = []
            for l in data:
                out.append(self.revers(l, level))
            return out
        elif isinstance(data, (list, tuple)):
            return data[::-1]
        else:
            return data[::-1]


def register():
    bpy.utils.register_class(ListReverseNode)


def unregister():
    bpy.utils.unregister_class(ListReverseNode)
