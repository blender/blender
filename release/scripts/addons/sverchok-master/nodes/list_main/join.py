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
from sverchok.data_structure import (changable_sockets, multi_socket, updateNode)

from sverchok.utils.listutils import joiner, myZip_2, wrapper_2

class ListJoinNode(bpy.types.Node, SverchCustomTreeNode):
    ''' ListJoin node '''
    bl_idname = 'ListJoinNode'
    bl_label = 'List Join'
    bl_icon = 'OUTLINER_OB_EMPTY'

    JoinLevel = IntProperty(name='JoinLevel', description='Choose join level of data (see help)',
                            default=1, min=1,
                            update=updateNode)
    mix_check = BoolProperty(name='mix', description='Grouping similar to zip()',
                             default=False,
                             update=updateNode)
    wrap_check = BoolProperty(name='wrap', description='Grouping similar to append(list)',
                              default=False,
                              update=updateNode)
    typ = StringProperty(name='typ',
                         default='')
    newsock = BoolProperty(name='newsock',
                           default=False)

    base_name = 'data '
    multi_socket_type = 'StringsSocket'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data", "data")
        self.outputs.new('StringsSocket', 'data', 'data')

    def draw_buttons(self, context, layout):
        layout.prop(self, "mix_check", text="mix")
        layout.prop(self, "wrap_check", text="wrap")
        layout.prop(self, "JoinLevel", text="JoinLevel lists")

    def update(self):
        # inputs
        multi_socket(self, min=1)

        if 'data' in self.inputs and len(self.inputs['data'].links) > 0:
            inputsocketname = 'data'
            outputsocketname = ['data']
            changable_sockets(self, inputsocketname, outputsocketname)

    def process(self):
        if 'data' in self.outputs and self.outputs['data'].is_linked:
            slots = []
            for socket in self.inputs:
                if socket.is_linked:
                    slots.append(socket.sv_get())
            if len(slots) == 0:
                return

            list_result = joiner(slots, self.JoinLevel)
            result = list_result.copy()
            if self.mix_check:
                list_mix = myZip_2(slots, self.JoinLevel)
                result = list_mix.copy()

            if self.wrap_check:
                list_wrap = wrapper_2(slots, list_result, self.JoinLevel)
                result = list_wrap.copy()

                if self.mix_check:
                    list_wrap_mix = wrapper_2(slots, list_mix, self.JoinLevel)
                    result = list_wrap_mix.copy()

            self.outputs[0].sv_set(result)

def register():
    bpy.utils.register_class(ListJoinNode)


def unregister():
    bpy.utils.unregister_class(ListJoinNode)
