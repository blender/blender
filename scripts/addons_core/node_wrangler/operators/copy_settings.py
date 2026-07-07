# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy_extras.node_utils import connect_sockets
from bpy.app.translations import pgettext_rpt as rpt_

from itertools import chain

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_active,
    nw_check_node_type,
    nw_check_selected,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_copy_settings(Operator, NWBase):
    bl_idname = "node.nw_copy_settings"
    bl_label = "Copy Settings"
    bl_description = "Copy settings from active to selected nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (nw_check(cls, context)
                and nw_check_active(cls, context)
                and nw_check_selected(cls, context, min=2)
                and nw_check_node_type(cls, context, 'FRAME', invert=True))

    def execute(self, context):
        node_active = context.active_node
        node_selected = context.selected_nodes
        selected_node_names = [n.name for n in node_selected]

        # Get nodes in selection by type
        valid_nodes = [n for n in node_selected if n.type == node_active.type]

        if not (len(valid_nodes) > 1) and node_active:
            self.report({'ERROR'}, rpt_("Selected nodes are not of the same type as {}").format(node_active.name))
            return {'CANCELLED'}

        if len(valid_nodes) != len(node_selected):
            # Report nodes that are not valid
            valid_node_names = [n.name for n in valid_nodes]
            invalid_names = set(selected_node_names) - set(valid_node_names)
            message = rpt_("Ignored {} (not of the same type as {})").format(
                ", ".join(sorted(invalid_names)),
                node_active.name,
            )
            self.report({'INFO'}, message)

        # Reference original
        orig = node_active
        # node_selected_names = [n.name for n in node_selected]

        # Output list
        success_names = []

        # Deselect all nodes
        for i in node_selected:
            i.select = False

        # Code by zeffii from http://blender.stackexchange.com/a/42338/3710
        # Run through all other nodes
        for node in valid_nodes[1:]:

            # Check for frame node
            parent = node.parent if node.parent else None
            node_loc = [node.location.x, node.location.y]

            # Select original to duplicate
            orig.select = True

            # Duplicate selected node
            bpy.ops.node.duplicate()
            new_node = context.selected_nodes[0]

            # Deselect copy
            new_node.select = False

            # Properties to copy
            node_tree = node.id_data
            props_to_copy = 'bl_idname name location height width'.split(' ')

            # Input and outputs
            reconnections = []
            mappings = chain.from_iterable([node.inputs, node.outputs])
            for i in (i for i in mappings if i.is_linked):
                for L in i.links:
                    reconnections.append([L.from_socket.path_from_id(), L.to_socket.path_from_id()])

            # Properties
            props = {j: getattr(node, j) for j in props_to_copy}
            props_to_copy.pop(0)

            for prop in props_to_copy:
                setattr(new_node, prop, props[prop])

            # Get the node tree to remove the old node
            nodes = node_tree.nodes
            nodes.remove(node)
            new_node.name = props['name']

            if parent:
                new_node.parent = parent
                new_node.location = node_loc

            for str_from, str_to in reconnections:
                connect_sockets(eval(str_from), eval(str_to))

            success_names.append(new_node.name)

        orig.select = True
        node_tree.nodes.active = orig
        message = rpt_("Successfully copied attributes from {} to {}").format(
            orig.name,
            ", ".join(success_names)
        )
        self.report({'INFO'}, message)
        return {'FINISHED'}
