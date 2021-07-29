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
from sverchok.data_structure import (changable_sockets, repeat_last, updateNode,
                            SvSetSocketAnyType, SvGetSocketAnyType)

class ListItemNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List item '''
    bl_idname = 'ListItemNode'
    bl_label = 'List item old'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level_to_count',
                        default=2, min=0,
                        update=updateNode)
    item = IntProperty(name='item',
                       default=0,
                       update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")
        layout.prop(self, "item", text="item")

    def init(self, context):
        self.inputs.new('StringsSocket', "Data", "Data")
        self.outputs.new('StringsSocket', "Item", "Item")
        self.outputs.new('StringsSocket', "Other", "Other")

    def process(self):
        if 'Data' in self.inputs and len(self.inputs['Data'].links) > 0:
            # адаптивный сокет
            inputsocketname = 'Data'
            outputsocketname = ['Item', 'Other']
            changable_sockets(self, inputsocketname, outputsocketname)

        if 'Item' in self.outputs and self.outputs['Item'].links or \
                'Other' in self.outputs and self.outputs['Other'].links:

            if 'Data' in self.inputs and self.inputs['Data'].links:
                data = SvGetSocketAnyType(self, self.inputs['Data'])

                if 'Item' in self.outputs and self.outputs['Item'].links:
                    out = self.count(data, self.level-1, self.item, True)
                    SvSetSocketAnyType(self, 'Item', out)
                if 'Other' in self.outputs and self.outputs['Other'].links:
                    out = self.count(data, self.level-1, self.item, False)
                    SvSetSocketAnyType(self, 'Other', out)

    def count(self, data, level, item, itself):
        if level:
            out = []
            for obj in data:
                out.append(self.count(obj, level-1, item, itself))

        elif type(data) == tuple:
            if item > len(data)-1:
                item = len(data)-1
            if itself:
                out = [data[item]]
            else:
                out = [data[:item]+data[item+1:]]
        elif type(data) == list:
            if item > len(data)-1:
                item = len(data)-1
            if itself:
                out = [data[item]]
            else:
                data.pop(item)
                out = [data]
        else:
            out = None
        return out

    def update_socket(self, context):
        self.update()
        
def register():
    bpy.utils.register_class(ListItemNode)

def unregister():
    bpy.utils.unregister_class(ListItemNode)
