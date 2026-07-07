# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import IntProperty

from copy import copy

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_not_empty,
    get_nodes_links,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_align_selected(Operator, NWBase):
    '''Align selected nodes in a grid pattern'''
    bl_idname = "node.nw_align_nodes"
    bl_label = "Align Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    margin: IntProperty(
        name='Margin',
        description='The amount of space between nodes',
        default=50,
    )

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_not_empty(cls, context)

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        margin = self.margin

        selection = []
        for node in nodes:
            if node.select and node.type != 'FRAME':
                selection.append(node)

        # If no nodes are selected, align all nodes
        active_loc = None
        if not selection:
            selection = nodes
        elif nodes.active in selection:
            active_loc = copy(nodes.active.location)  # make a copy, not a reference

        # Check if nodes should be laid out horizontally or vertically
        # use dimension to get center of node, not corner
        x_locs = [n.location.x + (n.dimensions.x / 2) for n in selection]
        y_locs = [n.location.y - (n.dimensions.y / 2) for n in selection]
        x_range = max(x_locs) - min(x_locs)
        y_range = max(y_locs) - min(y_locs)
        mid_x = (max(x_locs) + min(x_locs)) / 2
        mid_y = (max(y_locs) + min(y_locs)) / 2
        horizontal = x_range > y_range

        # Sort selection by location of node mid-point
        if horizontal:
            selection = sorted(selection, key=lambda n: n.location.x + (n.dimensions.x / 2))
        else:
            selection = sorted(selection, key=lambda n: n.location.y - (n.dimensions.y / 2), reverse=True)

        # Alignment
        current_pos = 0
        for node in selection:
            current_margin = margin

            # Use a smaller margin for hidden nodes.
            current_margin = current_margin * 0.5 if node.hide else current_margin

            if horizontal:
                node.location.x = current_pos
                current_pos += current_margin + node.dimensions.x
                node.location.y = mid_y + (node.dimensions.y / 2)
            else:
                # `node.bl_height_min` is the min size of a collapsed node, +6 for the outlines and margins.
                hide_offset = (node.dimensions.y - (node.bl_height_min + 6)) / 2 if node.hide else 0

                # Hidden nodes center their sockets around the label instead of below.
                node.location.y = current_pos - hide_offset

                # Use half-margin for vertical alignment.
                current_pos -= (current_margin * 0.3) + node.dimensions.y

                node.location.x = mid_x - (node.dimensions.x / 2)

        # If active node is selected, center nodes around it
        if active_loc is not None:
            active_loc_diff = active_loc - nodes.active.location
            for node in selection:
                node.location += active_loc_diff
        else:  # Position nodes centered around where they used to be
            locs = ([n.location.x + (n.dimensions.x / 2) for n in selection]
                    ) if horizontal else ([n.location.y - (n.dimensions.y / 2) for n in selection])
            new_mid = (max(locs) + min(locs)) / 2
            for node in selection:
                if horizontal:
                    node.location.x += (mid_x - new_mid)
                else:
                    node.location.y += (mid_y - new_mid)

        return {'FINISHED'}
