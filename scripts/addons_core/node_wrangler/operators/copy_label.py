# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import EnumProperty

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
    get_nodes_links,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_copy_label(Operator, NWBase):
    bl_idname = "node.nw_copy_label"
    bl_label = "Copy Label"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = "Copy label from active to selected nodes"

    option: EnumProperty(
        name="Option",
        description="Source of name of label",
        items=(
            ('FROM_ACTIVE', 'From Active', 'From active node',),
            ('FROM_NODE', 'From Node', 'From node linked to selected node'),
            ('FROM_SOCKET', 'From Socket', 'From socket linked to selected node'),
        )
    )

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context, min=2)

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        option = self.option
        active = nodes.active
        if option == 'FROM_ACTIVE':
            if active:
                src_label = active.label
                for node in [n for n in nodes if n.select and nodes.active != n]:
                    node.label = src_label
        elif option == 'FROM_NODE':
            selected = [n for n in nodes if n.select]
            for node in selected:
                for input in node.inputs:
                    if input.links:
                        src = input.links[0].from_node
                        node.label = src.label
                        break
        elif option == 'FROM_SOCKET':
            selected = [n for n in nodes if n.select]
            for node in selected:
                for input in node.inputs:
                    if input.links:
                        src = input.links[0].from_socket
                        node.label = src.name
                        break

        return {'FINISHED'}
