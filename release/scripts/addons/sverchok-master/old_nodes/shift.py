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

from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import (updateNode, changable_sockets,
                            SvSetSocketAnyType, SvGetSocketAnyType)


class ShiftNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Shift node '''
    bl_idname = 'ShiftNode'
    bl_label = 'List Shift'
    bl_icon = 'OUTLINER_OB_EMPTY'

    shift_c = IntProperty(name='Shift',
                          default=0,
                          update=updateNode)
    enclose = BoolProperty(name='check_tail',
                           default=True,
                           update=updateNode)
    level = IntProperty(name='level',
                        default=0, min=0,
                        update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="level")
        layout.prop(self, "enclose", text="enclose")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data", "data")
        self.inputs.new('StringsSocket', "shift", "shift").prop_name = 'shift_c'
        self.outputs.new('StringsSocket', 'data', 'data')

    def update(self):
        if 'data' in self.inputs and self.inputs['data'].links:
            # адаптивный сокет
            inputsocketname = 'data'
            outputsocketname = ['data']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if not self.outputs["data"].is_linked:
            return
            
        data = self.inputs['data'].sv_get()
        number = self.inputs["shift"].sv_get()
        output = self.shift(data, number, self.enclose, self.level)

        self.outputs['data'].sv_set(output)

    def shift(self, list_a, shift, check_enclose, level, cou=0):
        if level:
            list_all = []
            for idx, obj in enumerate(list_a):
                list_all.append(self.shift(obj, shift, check_enclose, level-1, idx))

        else:
            list_all = []
            if type(list_a) == list:
                indx = min(cou, len(shift)-1)
                for i, l in enumerate(list_a):
                    if type(l) == tuple:
                        l = list(l[:])
                    k = min(len(shift[indx])-1, i)
                    n = shift[indx][k]
                    n_ = min(abs(n), len(l))
                    if n < 0:
                        list_out = l[:-n_]
                        if check_enclose:
                            list_out = l[-n_:]+list_out
                    else:
                        list_out = l[n_:]
                        if check_enclose:
                            list_out.extend(l[:n_])
                    #print('\nn list_out', n,list_out)
                    list_all.append(list_out)
            if list_all == []:
                list_all = [[]]
        return list_all


def register():
    bpy.utils.register_class(ShiftNode)


def unregister():
    bpy.utils.unregister_class(ShiftNode)
