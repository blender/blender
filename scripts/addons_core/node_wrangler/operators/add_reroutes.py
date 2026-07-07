# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import EnumProperty
from bpy_extras.node_utils import connect_sockets

from ..utils.constants import (
    rl_outputs,
)
from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
    get_nodes_links,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_add_reroutes(Operator, NWBase):
    """Add Reroute nodes and link them to outputs of selected nodes"""
    bl_idname = "node.nw_add_reroutes"
    bl_label = "Add Reroutes"
    bl_description = "Add reroutes to outputs"
    bl_options = {'REGISTER', 'UNDO'}

    option: EnumProperty(
        name="Option",
        items=[
            ('ALL', 'To All', 'Add to all outputs'),
            ('LOOSE', 'To Loose', 'Add only to loose outputs'),
            ('LINKED', 'To Linked', 'Add only to linked outputs'),
        ]
    )

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context)

    def execute(self, context):
        nodes, _links = get_nodes_links(context)
        post_select = []  # Nodes to be selected after execution.
        y_offset = -22.0

        # Create reroutes and recreate links.
        for node in [n for n in nodes if n.select]:
            if not node.outputs:
                continue
            x = node.location.x + node.width + 20.0
            y = node.location.y
            new_node_reroutes = []

            # Unhide 'REROUTE' nodes to avoid issues with location.y
            if node.type == 'REROUTE':
                node.hide = False
            else:
                y -= 35.0

            reroutes_count = 0  # Will be used when aligning reroutes added to hidden nodes.
            for out_i, output in enumerate(node.outputs):
                if output.is_unavailable or isinstance(output, bpy.types.NodeSocketVirtual):
                    continue
                if node.type == 'R_LAYERS' and output.name != 'Alpha':
                    # If 'R_LAYERS' check if output is used in render pass.
                    # If output is "Alpha", assume it's used. Not available in passes.
                    node_scene = node.scene
                    node_layer = node.layer
                    for rlo in rl_outputs:
                        # Check entries in global 'rl_outputs' variable.
                        if output.name in {rlo.output_name, rlo.exr_output_name}:
                            if not getattr(node_scene.view_layers[node_layer], rlo.render_pass):
                                continue
                # Output is valid when option is 'all' or when 'loose' output has no links.
                valid = ((self.option == 'ALL') or
                         (self.option == 'LOOSE' and not output.links) or
                         (self.option == 'LINKED' and output.links))
                if valid:
                    # Add reroutes only if valid.
                    n = nodes.new('NodeReroute')
                    nodes.active = n
                    for link in output.links:
                        connect_sockets(n.outputs[0], link.to_socket)
                    connect_sockets(output, n.inputs[0])
                    n.location = x, y
                    new_node_reroutes.append(n)
                    post_select.append(n)
                if valid or not output.hide:
                    # Offset reroutes for all outputs, except hidden ones.
                    reroutes_count += 1
                    y += y_offset

            # Nicer reroutes distribution along y when node.hide.
            if node.hide:
                y_translate = reroutes_count * y_offset / 2.0 - y_offset - 35.0
                for reroute in new_node_reroutes:
                    reroute.location.y -= y_translate

        if post_select:
            for node in nodes:
                # Select only newly created nodes.
                node.select = node in post_select
        else:
            # No new nodes were created.
            return {'CANCELLED'}

        return {'FINISHED'}
