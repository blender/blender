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

OLD_OP = "node.sverchok_generic_callback_old"

# Warning, changing this node without modifying the update system might break functionlaity
# bl_idname and var_name is used by the update system


class WifiOutNode(bpy.types.Node, SverchCustomTreeNode):
    ''' WifiOutNode '''
    bl_idname = 'WifiOutNode'
    bl_label = 'Wifi out'
    bl_icon = 'OUTLINER_OB_EMPTY'

    var_name = StringProperty(name='var_name',
                              default='')

    def avail_var_name(self, context):
        ng = self.id_data
        out = [(n.var_name, n.var_name, "") for n in ng.nodes
               if n.bl_idname == 'WifiInNode']
        if out:
            out.sort(key=lambda n: n[0])
        return out

    var_names = EnumProperty(items=avail_var_name, name="var names")

    def set_var_name(self):
        self.var_name = self.var_names
        ng = self.id_data
        wifi_dict = {node.var_name: node
                     for node in ng.nodes
                     if node.bl_idname == 'WifiInNode'}
        self.outputs.clear()
        if self.var_name in wifi_dict:
            self.outputs.clear()
            node = wifi_dict[self.var_name]
            self.update()
        else:
            self.outputs.clear()

    def reset_var_name(self):
        self.var_name = ""
        self.outputs.clear()

    def draw_buttons(self, context, layout):
        op_name = OLD_OP
        if self.var_name:
            row = layout.row()
            row.label(text="Var:")
            row.label(text=self.var_name)
            op = layout.operator(op_name, text='Unlink')
            op.fn_name = "reset_var_name"
        else:
            layout.prop(self, "var_names")
            op = layout.operator(op_name, text='Link')
            op.fn_name = "set_var_name"

    def sv_init(self, context):
        pass

    def gen_var_name(self):
        #from socket
        if self.outputs:
            n = self.outputs[0].name.rstrip("[0]")
            self.var_name = n

    def update(self):
        if not self.var_name and self.outputs:
            self.gen_var_name()
        ng = self.id_data
        wifi_dict = {node.var_name: node
                     for name, node in ng.nodes.items()
                     if node.bl_idname == 'WifiInNode'}

        node = wifi_dict.get(self.var_name)
        if node:
            inputs = node.inputs
            outputs = self.outputs

            # match socket type
            for idx, i_o in enumerate(zip(inputs, outputs)):
                i_socket, o_socket = i_o
                if i_socket.links:
                    f_socket = i_socket.links[0].from_socket
                    if f_socket.bl_idname != o_socket.bl_idname:
                        outputs.remove(o_socket)
                        outputs.new(f_socket.bl_idname, i_socket.name)
                        outputs.move(len(self.outputs)-1, idx)

            # adjust number of inputs
            while len(outputs) != len(inputs)-1:
                if len(outputs) > len(inputs)-1:
                    outputs.remove(outputs[-1])
                else:
                    n = len(outputs)
                    socket = inputs[n]
                    if socket.links:
                        s_type = socket.links[0].from_socket.bl_idname
                    else:
                        s_type = 'StringsSocket'
                    s_name = socket.name
                    outputs.new(s_type, s_name)

    def process(self):
        ng = self.id_data
        wifi_dict = {node.var_name: node
                     for name, node in ng.nodes.items()
                     if node.bl_idname == 'WifiInNode'}

        node = wifi_dict.get(self.var_name)
        # transfer data
        for in_socket, out_socket in zip(node.inputs, self.outputs):
            if in_socket.is_linked and out_socket.is_linked:
                data = in_socket.sv_get(deepcopy=False)
                out_socket.sv_set(data)


def register():
    bpy.utils.register_class(WifiOutNode)


def unregister():
    bpy.utils.unregister_class(WifiOutNode)
