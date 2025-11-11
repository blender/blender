# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
    abs_node_location,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_center_selected(Operator, NWBase):
    """Move selected nodes to the center of the node editor"""
    bl_idname = "node.nw_center_nodes"
    bl_label = "Center Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context)

    def execute(self, context):
        selection = context.selected_nodes

        # Pick outermost selected nodes
        nodes = []
        for node in selection:
            if node.parent and node.parent.select:
                continue
            nodes.append(node)

        # Get bound center of picked nodes
        nodes_x = []
        nodes_y = []
        nodes_right = []
        nodes_bottom = []
        for n in nodes:
            loc_abs = abs_node_location(n)
            nodes_x.append(loc_abs.x)
            nodes_y.append(loc_abs.y)
            if n.type == 'FRAME':
                nodes_right.append(loc_abs.x + n.width)
                nodes_bottom.append(loc_abs.y - n.height)
            elif n.type == 'REROUTE':
                nodes_right.append(loc_abs.x)
                nodes_bottom.append(loc_abs.y)
            else:
                nodes_right.append(loc_abs.x + n.width)
                nodes_bottom.append(loc_abs.y - n.dimensions.y)
        mid_x = (min(nodes_x) + max(nodes_right)) / 2
        mid_y = (max(nodes_y) + min(nodes_bottom)) / 2

        for node in nodes:
            node.location.x -= mid_x
            node.location.y -= mid_y

        return {'FINISHED'}
