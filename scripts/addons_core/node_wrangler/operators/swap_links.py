# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy_extras.node_utils import connect_sockets

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
    get_nodes_links,
    force_update,
)

#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_swap_links(Operator, NWBase):
    """Swap the output connections of the two selected nodes, or two similar inputs of a single node"""
    bl_idname = 'node.nw_swap_links'
    bl_label = 'Swap Links'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context, max=2)

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        selected_nodes = context.selected_nodes
        n1 = selected_nodes[0]

        # Swap outputs
        if len(selected_nodes) == 2:
            n2 = selected_nodes[1]
            if n1.outputs and n2.outputs:
                n1_outputs = []
                n2_outputs = []

                out_index = 0
                for output in n1.outputs:
                    if output.links:
                        for link in output.links:
                            n1_outputs.append([out_index, link.to_socket])
                            links.remove(link)
                    out_index += 1

                out_index = 0
                for output in n2.outputs:
                    if output.links:
                        for link in output.links:
                            n2_outputs.append([out_index, link.to_socket])
                            links.remove(link)
                    out_index += 1

                for connection in n1_outputs:
                    try:
                        connect_sockets(n2.outputs[connection[0]], connection[1])
                    except:
                        self.report({'WARNING'},
                                    "Some connections have been lost due to differing numbers of output sockets")
                for connection in n2_outputs:
                    try:
                        connect_sockets(n1.outputs[connection[0]], connection[1])
                    except:
                        self.report({'WARNING'},
                                    "Some connections have been lost due to differing numbers of output sockets")
            else:
                if n1.outputs or n2.outputs:
                    self.report({'WARNING'}, "One of the nodes has no outputs")
                else:
                    self.report({'WARNING'}, "Neither of the nodes have outputs")

        # Swap Inputs
        elif len(selected_nodes) == 1:
            if n1.inputs and n1.inputs[0].is_multi_input:
                self.report({'WARNING'}, "Cannot swap inputs of a multi-input socket")
                return {'FINISHED'}
            if n1.inputs:
                types = []
                i = 0
                for i1 in n1.inputs:
                    if i1.is_linked and not i1.is_multi_input:
                        similar_types = 0
                        for i2 in n1.inputs:
                            if i1.type == i2.type and i2.is_linked and not i2.is_multi_input:
                                similar_types += 1
                        types.append([i1, similar_types, i])
                    i += 1
                types.sort(key=lambda k: k[1], reverse=True)

                if types:
                    t = types[0]
                    if t[1] == 2:
                        for i2 in n1.inputs:
                            if t[0].type == i2.type == t[0].type and t[0] != i2 and i2.is_linked:
                                pair = [t[0], i2]
                        i1f = pair[0].links[0].from_socket
                        i1t = pair[0].links[0].to_socket
                        i2f = pair[1].links[0].from_socket
                        i2t = pair[1].links[0].to_socket
                        connect_sockets(i1f, i2t)
                        connect_sockets(i2f, i1t)
                    if t[1] == 1:
                        if len(types) == 1:
                            fs = t[0].links[0].from_socket
                            i = t[2]
                            links.remove(t[0].links[0])
                            if i + 1 == len(n1.inputs):
                                i = -1
                            i += 1
                            while n1.inputs[i].is_linked:
                                i += 1
                            connect_sockets(fs, n1.inputs[i])
                        elif len(types) == 2:
                            i1f = types[0][0].links[0].from_socket
                            i1t = types[0][0].links[0].to_socket
                            i2f = types[1][0].links[0].from_socket
                            i2t = types[1][0].links[0].to_socket
                            connect_sockets(i1f, i2t)
                            connect_sockets(i2f, i1t)

                else:
                    self.report({'WARNING'}, "This node has no input connections to swap")
            else:
                self.report({'WARNING'}, "This node has no inputs to swap")

        force_update(context)
        return {'FINISHED'}
