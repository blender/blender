# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Operator
from bpy.props import IntProperty

from copy import copy

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_not_empty,
    nw_check_selected,
    get_width,
    get_height,
    get_center,
    get_middle,
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
        return nw_check(cls, context) and nw_check_not_empty(cls, context) and nw_check_selected(cls, context)

    def execute(self, context):
        tree = context.space_data.edit_tree
        nodes = tree.nodes
        margin = self.margin

        selection = []
        for node in nodes:
            if node.select and node.type != 'FRAME':
                selection.append(node)

        if not selection:
            self.report({'WARNING'}, "No nodes to arrange in selection.")
            return {'CANCELLED'}

        active_loc = None
        if nodes.active in selection:
            active_loc = copy(nodes.active.location_absolute)  # make a copy, not a reference

        # Check if nodes should be laid out horizontally or vertically
        # use dimension to get center of node, not corner
        x_locs = list(map(get_center, selection))
        y_locs = list(map(get_middle, selection))

        x_range = max(x_locs) - min(x_locs)
        y_range = max(y_locs) - min(y_locs)
        mid_x = (max(x_locs) + min(x_locs)) / 2
        mid_y = (max(y_locs) + min(y_locs)) / 2
        horizontal = x_range > y_range

        # Sort selection by location of node mid-point
        if horizontal:
            selection = sorted(selection, key=get_center)
        else:
            selection = sorted(selection, key=get_middle, reverse=True)

        # Alignment
        current_pos = 0
        for node in selection:
            width = get_width(node)
            height = get_height(node)
            current_margin = margin

            # Use a smaller margin for hidden nodes.
            current_margin = current_margin * 0.5 if node.hide else current_margin

            if horizontal:
                node.location_absolute.x = current_pos
                current_pos += current_margin + width
                node.location_absolute.y = mid_y + (height / 2)
            else:
                # `node.bl_height_min` is the min size of a collapsed node, +6 for the outlines and margins.
                hide_offset = (height - (node.bl_height_min + 6)) / 2 if node.hide else 0

                # Hidden nodes center their sockets around the label instead of below.
                node.location_absolute.y = current_pos - hide_offset

                # Use half-margin for vertical alignment.
                current_pos -= (current_margin * 0.3) + height

                node.location_absolute.x = mid_x - (width / 2)

        # If active node is selected, center nodes around it
        if active_loc is not None:
            active_loc_diff = active_loc - nodes.active.location_absolute
            for node in selection:
                node.location_absolute += active_loc_diff
        else:  # Position nodes centered around where they used to be
            func = get_center if horizontal else get_middle
            locs = list(map(func, selection))

            new_mid = (max(locs) + min(locs)) / 2
            for node in selection:
                if horizontal:
                    node.location_absolute.x += (mid_x - new_mid)
                else:
                    node.location_absolute.y += (mid_y - new_mid)

        return {'FINISHED'}
