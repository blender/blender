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

# the concrete monad classes plus common base
import ast
import pprint

import bpy
from bpy.types import Node
from bpy.props import StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.core import monad as monad_def
reverse_lookup = {'outputs': 'inputs', 'inputs': 'outputs'}



class SvSocketAquisition:

    socket_map = {'outputs': 'to_socket', 'inputs': 'from_socket'}
    node_kind = StringProperty()

    @property
    def get_outputs_info(self):
        if self.node_kind == 'outputs':
            return [{
                    'socket_name': s.name,
                    'socket_identifier': s.identifier,
                    'socket_prop_name': s.prop_name
                    } for s in self.outputs]


    def update(self):
        kind = self.node_kind
        if not kind:
            return

        monad = self.id_data
        if monad.bl_idname == "SverchCustomTreeType":
            return

        socket_list = getattr(self, kind)
        _socket = self.socket_map.get(kind) # from_socket, to_socket

        if len(socket_list) == 0:
            print('sockets wiped, skipped update')
            return

        if socket_list[-1].is_linked:

            # gather socket data
            socket = socket_list[-1]
            if kind == "outputs":
                prop_name = monad.add_prop_from(socket)
                cls = monad.update_cls()
                new_name, new_type, prop_data = cls.input_template[-1]
            else:
                prop_name = ""
                cls = monad.update_cls()
                prop_data = {}
                new_name, new_type = cls.output_template[-1]

            # transform socket type from dummy to new type
            new_socket = socket.replace_socket(new_type, new_name=new_name)
            if prop_name:
                new_socket.prop_name = prop_name

            # update all monad nodes (front facing)
            for instance in monad.instances:
                sockets = getattr(instance, reverse_lookup[kind])
                new_socket = sockets.new(new_type, new_name)
                for name, value in prop_data.items():
                    if not name == 'prop_name':
                        setattr(new_socket, name, value)
                    else:
                        new_socket.prop_name = prop_name or ''

            # print('------')
            # print(prop_data)
            # pprint.pprint(self.get_outputs_info)

            # add new dangling dummy
            socket_list.new('SvDummySocket', 'connect me')

    # stashing and repopulate are used for iojson

    def stash(self):
        socket_kinds = []
        for s in getattr(self, self.node_kind):
            if not s.bl_idname == 'SvDummySocket':
                socket_kinds.append([s.name, s.bl_idname])
        return socket_kinds

    def repopulate(self, socket_kinds):
        sockets = getattr(self, self.node_kind)
        sockets.remove(sockets[0])
        for idx, (s, stype) in enumerate(socket_kinds):
            # print('add', s, stype, 'to', self.name)
            sockets.new(stype, s)




class SvGroupInputsNodeExp(Node, SverchCustomTreeNode, SvSocketAquisition):
    bl_idname = 'SvGroupInputsNodeExp'
    bl_label = 'Group Inputs Exp'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        si = self.outputs.new
        si('SvDummySocket', 'connect me')
        self.node_kind = 'outputs'

        self.use_custom_color = True
        self.color = monad_def.MONAD_COLOR

class SvGroupOutputsNodeExp(Node, SverchCustomTreeNode, SvSocketAquisition):
    bl_idname = 'SvGroupOutputsNodeExp'
    bl_label = 'Group Outputs Exp'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        si = self.inputs.new
        si('SvDummySocket', 'connect me')
        self.node_kind = 'inputs'

        self.use_custom_color = True
        self.color = monad_def.MONAD_COLOR


def call_init(self, context):
    monad = self.monad
    if self.monad:
        self.input_template = monad.generate_inputs()
        self.output_template = monad.generate_outputs()


class SvMonadGenericNode(Node, SverchCustomTreeNode,  monad_def.SvGroupNodeExp):
    bl_idname = 'SvMonadGenericNode'
    bl_label = 'Group'
    bl_icon = 'OUTLINER_OB_EMPTY'

    data_storage = StringProperty()
    cls_bl_idname = StringProperty(update=call_init)

    @property
    def input_template(self):
        if not self.data_storage:
            return []
        data = ast.literal_eval(self.data_storage)
        return data.get("input_template", {})

    @input_template.setter
    def input_template(self, value):
        if self.data_storage:
            data = ast.literal_eval(self.data_storage)
        else:
            data = {}
        data["input_template"] = value
        self.data_storage = str(data)
        self.inputs.clear()
        for socket_name, socket_bl_idname, _ in value:
            self.inputs.new(socket_bl_idname, socket_name)

    @property
    def output_template(self):
        if not self.data_storage:
            return []
        data = ast.literal_eval(self.data_storage)
        return data.get("output_template", [])

    @output_template.setter
    def output_template(self, value):
        if self.data_storage:
            data = ast.literal_eval(self.data_storage)
        else:
            data = {}
        data["output_template"] = value
        self.data_storage = str(data)
        self.outputs.clear()
        for socket_name, socket_bl_idname in value:
            self.outputs.new(socket_bl_idname, socket_name)

    @property
    def monad(self):
        if not self.cls_bl_idname:
            return None

        for monad in bpy.data.node_groups:
            if hasattr(monad, "cls_bl_idname"):
                if monad.cls_bl_idname == self.cls_bl_idname:
                    return monad

        return None

    def sv_init(self, context):
        self.use_custom_color = True
        self.color = monad_def.MONAD_COLOR



class SvMonadInfoNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Monad Info '''
    bl_idname = 'SvMonadInfoNode'
    bl_label = 'Monad Info'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.outputs.new('StringsSocket', "Loop Idx")
        self.outputs.new('StringsSocket', "Loop Total")

        self.use_custom_color = True
        self.color = monad_def.MONAD_COLOR
        if self.id_data.bl_idname == "SverchCustomTreeType":
            self.color = (0.9, 0, 0)

    def process(self):
        # outputs
        monad = self.id_data
        try:
            idx = monad["current_index"]
            total = monad["current_total"]
        except Exception as err:
            print(repr(err))
            idx, total = 0 , 0
            print("couldn't find monad info")

        for socket, data in zip(self.outputs, [idx, total]):
            if socket.is_linked:
                socket.sv_set([[data]])

classes = [
    SvGroupInputsNodeExp,
    SvGroupOutputsNodeExp,
    SvMonadGenericNode,
    SvMonadInfoNode
]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
