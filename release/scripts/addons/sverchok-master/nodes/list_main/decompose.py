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
from sverchok.data_structure import (levelsOflist, multi_socket, changable_sockets,
                                     updateNode, get_other_socket)

from sverchok.core import update_system

OLD_OP = "node.sverchok_generic_callback_old"

class SvListDecomposeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List devided to multiple sockets in some level '''
    bl_idname = 'SvListDecomposeNode'
    bl_label = 'List Decompose'
    bl_icon = 'OUTLINER_OB_EMPTY'

    # two veriables for multi socket input
    base_name = StringProperty(default='data')
    multi_socket_type = StringProperty(default='StringsSocket')

    def auto_count(self):
        data = self.inputs['data'].sv_get(default="not found")
        other = get_other_socket(self.inputs['data'])
        if other and data == "not found":
            update_system.process_to_node(other.node)
            data = self.inputs['data'].sv_get()

        leve = levelsOflist(data)
        if leve+1 < self.level:
            self.level = leve+1
        result = self.beat(data, self.level)
        self.count = min(len(result), 16)

    def set_count(self, context):
        other = get_other_socket(self.inputs[0])
        if not other:
            return
        self.multi_socket_type = other.bl_idname
        multi_socket(self, min=1, start=0, breck=True, out_count=self.count)

    level = IntProperty(
        name='level', default=1, min=1, update=updateNode)

    count = IntProperty(
        name='Count', default=1, min=1, max=16, update=set_count)

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, 'level')
        row = col.row()
        row.prop(self, 'count')
        op = row.operator(OLD_OP, text="Auto set")
        op.fn_name = "auto_count"

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data")
        self.outputs.new('StringsSocket', "data[0]")


    def update(self):
        other = get_other_socket(self.inputs[0])
        if not other:
            return
        self.multi_socket_type = other.bl_idname
        multi_socket(self, min=1, start=0, breck=True, out_count=self.count)
        outputsocketname = [name.name for name in self.outputs]
        changable_sockets(self, 'data', outputsocketname)


    def process(self):
        data = self.inputs['data'].sv_get()
        result = self.beat(data, self.level)
        for out, socket in zip(result, self.outputs[:30]):
            if socket.is_linked:
                socket.sv_set(out)

    def beat(self, data, level, count=1):
        out = []
        if level > 1:
            for objects in data:
                out.extend(self.beat(objects, level-1, count+1))
        else:
            for i in range(count):
                data = [data]
            return data
        return out


def register():
    bpy.utils.register_class(SvListDecomposeNode)


def unregister():
    bpy.utils.unregister_class(SvListDecomposeNode)


# if __name__ == '__main__':
#    register()
