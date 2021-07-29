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
from sverchok.data_structure import changable_sockets, multi_socket, updateNode
from sverchok.utils.listutils import preobrazovatel


class ZipNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Zip node '''
    bl_idname = 'ZipNode'
    bl_label = 'List Zip'
    bl_icon = 'OUTLINER_OB_EMPTY'

    level = IntProperty(name='level', default=1, min=1, update=updateNode)
    typ = StringProperty(name='typ', default='')
    newsock = BoolProperty(name='newsock', default=False)

    unwrap = BoolProperty(
        name='unwrap',
        description='unwrap objects?',
        default=False,
        update=updateNode
    )

    base_name = 'data '
    multi_socket_type = 'StringsSocket'

    def draw_buttons(self, context, layout):
        layout.prop(self, "level", text="Level")
        layout.prop(self, "unwrap", text="UnWrap")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'data')
        self.outputs.new('StringsSocket', 'data')

    def update(self):
        # inputs
        multi_socket(self, min=1)

        if 'data' in self.inputs and self.inputs['data'].links:
            # Adaptive socket
            inputsocketname = 'data'
            outputsocketname = ['data']
            changable_sockets(self, inputsocketname, outputsocketname)


    def process(self):
        if self.outputs['data'].is_linked:
            slots = []
            for socket in self.inputs:
                if socket.is_linked:
                    slots.append(socket.sv_get())
            if len(slots) < 2:
                return
            output = self.myZip(slots, self.level)
            if self.unwrap:
                output = preobrazovatel(output, [2, 3])
            self.outputs[0].sv_set(output)

    def myZip(self, list_all, level, level2=0):
        if level == level2:
            if isinstance(list_all, (list, tuple)):
                list_lens = []
                list_res = []
                for l in list_all:
                    if isinstance(l, (list, tuple)):
                        list_lens.append(len(l))
                    else:
                        list_lens.append(0)

                min_len = min(list_lens)
                for value in range(min_len):
                    lt = []
                    for l in list_all:
                        lt.append(l[value])
                    t = list(lt)
                    list_res.append(t)
                return list_res
            else:
                return False
        elif level > level2:
            if isinstance(list_all, (list, tuple)):
                list_res = []
                list_tr = self.myZip(list_all, level, level2+1)
                if list_tr is False:
                    list_tr = list_all
                t = []
                for tr in list_tr:
                    if isinstance(list_tr, (list, tuple)):
                        list_tl = self.myZip(tr, level, level2+1)
                        if list_tl is False:
                            list_tl = list_tr
                        t.append(list_tl)
                list_res.extend(list(t))
                return list_res
            else:
                return False

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(ZipNode)


def unregister():
    bpy.utils.unregister_class(ZipNode)

# if __name__ == '__main__':
#    register()
