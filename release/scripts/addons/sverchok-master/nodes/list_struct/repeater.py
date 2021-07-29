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


class ListRepeaterNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List repeater '''
    bl_idname = 'ListRepeaterNode'
    bl_label = 'List Repeater'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level',
                        default=1, min=0,
                        update=updateNode)
    number = IntProperty(name='number',
                         default=1, min=1,
                         update=updateNode)
    unwrap = BoolProperty(name='unwrap',
                          default=False,
                          update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")
        layout.prop(self, "unwrap", text="unwrap")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data", "Data")
        self.inputs.new('StringsSocket', "Number", "Number").prop_name = 'number'
        self.outputs.new('StringsSocket', "Data", "Data")

    def update(self):
        if 'Data' in self.inputs and self.inputs['Data'].links:
            inputsocketname = 'Data'
            outputsocketname = ['Data', ]
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if self.inputs['Data'].is_linked:
            data = self.inputs['Data'].sv_get()

            if self.inputs['Number'].is_linked:
                tmp = self.inputs['Number'].sv_get()
                Number = tmp[0]
            else:
                Number = [self.number]

            if self.outputs['Data'].is_linked:
                out_ = self.count(data, self.level, Number)
                if self.unwrap:
                    if len(out_) > 0:
                        out = []
                        for o in out_:
                            out.extend(o)
                else:
                    out = out_

                self.outputs['Data'].sv_set(out)

    def count(self, data, level, number, cou=0):
        if level:
            out = []
            for idx, obj in enumerate(data):
                out.append(self.count(obj, level - 1, number, idx))

        else:
            out = []
            indx = min(cou, len(number) - 1)
            for i in range(int(number[indx])):
                out.append(data)
        return out


def register():
    bpy.utils.register_class(ListRepeaterNode)


def unregister():
    bpy.utils.unregister_class(ListRepeaterNode)
