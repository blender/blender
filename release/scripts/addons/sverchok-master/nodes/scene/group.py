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

"""
old file, to for current groups look in monad.py
the nodes are kept and registered to be able to upgrade old
beta nodes
"""

import bpy
from bpy.props import StringProperty, EnumProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import multi_socket, node_id
from sverchok.core.update_system import make_tree_from_nodes, do_update
import ast


socket_types = [("StringsSocket", "s", "Numbers, polygon data, generic"),
                ("VerticesSocket", "v", "Vertices, point and vector data"),
                ("MatrixSocket", "m", "Matrix")]


class SvRemoveSocketOperator(bpy.types.Operator):
    bl_idname = "node.sverchok_remove_socket"
    bl_label = "Remove Socket"

    socket_name = StringProperty()

    def execute(self, context):
        socket = context.socket

        if socket.is_output:
            sockets = socket.node.outputs
        else:
            sockets = socket.node.inputs
        sockets.remove(socket)
        return {"FINISHED"}

class SvMoveSocketOperator(bpy.types.Operator):
    bl_idname = "node.sverchok_move_socket"
    bl_label = "Remove Socket"

    pos = IntProperty()

    def execute(self, context):
        socket = context.socket
        if socket.is_output:
            sockets = socket.node.outputs
        else:
            sockets = socket.node.inputs
        for i, s in enumerate(sockets):
            if s == socket:
                break
        new_pos = i + self.pos
        print(i, new_pos)
        sockets.move(i, new_pos)
        return {"FINISHED"}

class SvEditSocket(bpy.types.NodeSocket):
    '''Edit Socket'''
    bl_idname = "SvEditSocket"
    bl_label = "Edit Socket"


    def change_socket(self, context):
        pass

    socket_type = EnumProperty(items=socket_types, update=change_socket,
                                default="StringsSocket")
    old_name = StringProperty()

    def draw_color(self, context, node):
        colors = {"StringsSocket": (0.6, 1.0, 0.6, 1.0),
                  "VerticesSocket":(0.9, 0.6, 0.2, 1.0) ,
                  "MatrixSocket":  (.2, .8, .8, 1.0)}
        return colors[self.socket_type]

    def draw(self, context, layout, node, text):
        split = layout.split(percentage=.50)
        split.prop(self, "name", text='')
        split = split.split(percentage=.50)
        split.prop(self, "socket_type", text="")
        split.operator("node.sverchok_move_socket", text="",icon='TRIA_UP').pos = -1
        split.operator("node.sverchok_move_socket", text="", icon='TRIA_DOWN').pos = 1
        split.operator("node.sverchok_remove_socket", text="", icon='X').socket_name = self.name



class StoreSockets:

    socket_data = StringProperty()

    def draw_buttons(self, context, layout):
        if self.id_data.bl_idname == "SverchCustomTreeType" and self.parent:
            op = layout.operator("node.sv_node_group_done")
            op.frame_name = self.parent.name

    def collect(self):
        out = {}
        data = []
        for sockets, name in self.get_sockets():
            data = []
            for socket in filter(lambda s:s.is_linked, sockets):
                if socket.bl_idname == "SvEditSocket":
                    data.append((socket.socket_type, socket.name, socket.old_name))
                else:
                    data.append((socket.bl_idname, socket.name))
            out[name] = data
        self.socket_data = str(out)

    def load(self):
        edit_mode = self.id_data.bl_idname == "SverchCustomTreeType"
        print("Edit mode",edit_mode)
        data = ast.literal_eval(self.socket_data)
        for k,values in data.items():
            sockets = getattr(self, k)
            sockets.clear()
            for val in values:
                s_type = val[0]
                s_name = val[1]
                if not s_name in sockets:
                    if edit_mode:
                        s = sockets.new("SvEditSocket", s_name)
                        s.socket_type = s_type
                        s.old_name = s_name
                    else:
                        sockets.new(s_type, s_name)

    def get_stype(self, socket):
        if socket.is_output:
            return socket.links[0].to_node.bl_idname
        else:
            return socket.links[0].from_node.bl_idname

    def update(self):
        if self.id_data.bl_idname == "SverchCustomTreeType":
            sockets, name = next(self.get_sockets())
            if self.socket_data and sockets and sockets[-1].links:
                sockets.new("SvEditSocket", str(len(sockets)))


class SvGroupNode(bpy.types.Node, SverchCustomTreeNode, StoreSockets):
    '''
    Sverchok Group node
    '''
    bl_idname = 'SvGroupNode'
    bl_label = 'Group'
    bl_icon = 'OUTLINER_OB_EMPTY'

    group_name = StringProperty()

    def update(self):
        '''
        Override inherited
        '''
        pass

    def draw_buttons(self, context, layout):
        if self.id_data.bl_idname == "SverchCustomTreeType":
            op = layout.operator("node.sv_node_group_edit")
            op.group_name = self.group_name

    def adjust_sockets(self, nodes):
        swap = {"inputs":"outputs",
                "outputs": "inputs"}
        for n in nodes:
            data = ast.literal_eval(n.socket_data)
            """
            This below is somewhat broken and needs
            to be reworked.
            """

            print("adjusting sockets",data)
            for k, values in data.items():
                sockets = getattr(self, swap[k])
                for i, socket_data in enumerate(values):
                    if len(socket_data) == 3 and socket_data[2]:
                        print(socket_data)
                        s_type, s_name, s_old_name = socket_data
                        curr_socket = sockets[s_old_name]
                        print(curr_socket.name)
                        s = curr_socket.replace_socket(s_type, s_name, i)
                        print(s.name)
                    else:
                        sockets.new(*socket_data)

    def process(self):
        group_ng = bpy.data.node_groups[self.group_name]
        in_node = find_node("SvGroupInputsNode", group_ng)
        out_node = find_node('SvGroupOutputsNode', group_ng)
        for socket in self.inputs:
            if socket.is_linked:
                data = socket.sv_get(deepcopy=False)
                in_node.outputs[socket.name].sv_set(data)
        #  get update list
        #  could be cached
        ul = make_tree_from_nodes([out_node.name], group_ng, down=False)
        do_update(ul, group_ng.nodes)
        # set output sockets correctly
        for socket in self.outputs:
            if socket.is_linked:
                data = out_node.inputs[socket.name].sv_get(deepcopy=False)
                socket.sv_set(data)

    def load(self):
        data = ast.literal_eval(self.socket_data)
        for k,values in data.items():
            sockets = getattr(self, k)
            sockets.clear()
            for s in values:
                if not s[1] in sockets:
                    sockets.new(*s)

    def get_sockets(self):
        yield self.inputs, "inputs"
        yield self.outputs, "outputs"


def find_node(id_name, ng):
    for n in ng.nodes:
        if n.bl_idname == id_name:
            return n
    raise LookupError

class SvIterationNode(bpy.types.Node, SverchCustomTreeNode, StoreSockets):
    bl_idname = 'SvIterationNode'
    bl_label = 'Group Inputs'
    bl_icon = 'OUTLINER_OB_EMPTY'

    iter_count = IntProperty(name="Count")
    group_name = StringProperty()

    def update(self):
        '''
        Override inherited
        '''
        pass

    def draw_buttons(self, context, layout):
        if self.id_data.bl_idname == "SverchCustomTreeType":
            op = layout.operator("node.sv_node_group_edit")
            op.group_name = self.group_name
            layout.prop(self, "iter_count")

    def adjust_sockets(self, nodes):
        swap = {"inputs":"outputs",
                "outputs": "inputs"}
        for n in nodes:
            data = ast.literal_eval(n.socket_data)
            for k, values in data.items():
                sockets = getattr(self, swap[k])
                for i,s in enumerate(values):
                    if i < len(sockets):
                        sockets[i].name = s[1]
                    else:
                        sockets.new(*s)

    def process(self):
        group_ng = bpy.data.node_groups[self.group_name]
        in_node = find_node("SvGroupInputsNode", group_ng)
        out_node = find_node('SvGroupOutputsNode', group_ng)
        ul = make_tree_from_nodes([out_node.name], group_ng, down=False)

        for socket in self.inputs:
            if socket.is_linked:
                data = socket.sv_get(deepcopy=False)
                in_node.outputs[socket.name].sv_set(data)
        #  get update list
        #  could be cached
        for i in range(self.iter_count):
            do_update(ul, group_ng.nodes)
            for socket in out_node.inputs:
                if socket.is_linked:
                    data = out_node.inputs[socket.name].sv_get(deepcopy=False)
                    socket.sv_set(data)
                    in_node.outputs[socket.name].sv_set(data)

        # set output sockets correctly
        for socket in self.outputs:
            if socket.is_linked:
                data = out_node.inputs[socket.name].sv_get(deepcopy=False)
                socket.sv_set(data)

    def get_sockets(self):
        yield self.inputs, "inputs"
        yield self.outputs, "outputs"


class SvGroupInputsNode(bpy.types.Node, SverchCustomTreeNode, StoreSockets):
    bl_idname = 'SvGroupInputsNode'
    bl_label = 'Group Inputs'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def get_sockets(self):
        yield self.outputs, "outputs"


class SvGroupOutputsNode(bpy.types.Node, SverchCustomTreeNode, StoreSockets):
    bl_idname = 'SvGroupOutputsNode'
    bl_label = 'Group outputs'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def get_sockets(self):
        yield self.inputs, "inputs"

classes = [
    #SvEditSocket,
    SvGroupNode,
    SvGroupInputsNode,
    SvGroupOutputsNode,
    #SvMoveSocketOperator,
    #SvRemoveSocketOperator,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
