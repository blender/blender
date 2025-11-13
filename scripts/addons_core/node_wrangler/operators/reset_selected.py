# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy_extras.node_utils import connect_sockets
from bpy.app.translations import pgettext_rpt as rpt_

from itertools import chain

from ..utils.nodes import (
    nw_check,
    nw_check_active,
    nw_check_selected,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_reset_selected(Operator):
    """Revert nodes back to the default state, but keep connections"""
    bl_idname = "node.nw_reset_nodes"
    bl_label = "Reset Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (nw_check(cls, context)
                and nw_check_selected(cls, context)
                and nw_check_active(cls, context))

    @staticmethod
    def is_frame_node(node):
        return node.bl_idname == "NodeFrame"

    group_node_types = {"CompositorNodeGroup", "GeometryNodeGroup", "ShaderNodeGroup"}
    # TODO All zone nodes are ignored here for now, because replacing one of the input/output pair breaks the zone.
    # It's possible to handle zones by using the `paired_output` function of an input node
    # and reconstruct the zone using the `pair_with_output` function.
    zone_node_types = {"GeometryNodeRepeatInput", "GeometryNodeRepeatOutput", "NodeClosureInput",
                        "NodeClosureOutput", "GeometryNodeSimulationInput", "GeometryNodeSimulationOutput",
                        "GeometryNodeForeachGeometryElementInput", "GeometryNodeForeachGeometryElementOutput"}
    node_ignore = group_node_types | zone_node_types | {"NodeFrame", "NodeReroute"}

    @classmethod
    def ignore_node(cls, node):
        return node.bl_idname in cls.node_ignore

    def execute(self, context):
        node_active = context.active_node
        node_selected = context.selected_nodes
        active_node_name = node_active.name if node_active.select else None
        valid_nodes = [n for n in node_selected if not self.ignore_node(n)]

        # Create output lists
        selected_node_names = [n.name for n in node_selected]
        success_names = []

        # Reset all valid children in a frame
        node_active_is_frame = False
        if len(node_selected) == 1 and self.is_frame_node(node_active):
            node_tree = node_active.id_data
            children = [n for n in node_tree.nodes if n.parent == node_active]
            if children:
                valid_nodes = [n for n in children if not self.ignore_node(n)]
                selected_node_names = [n.name for n in children if not self.ignore_node(n)]
                node_active_is_frame = True

        # Check if valid nodes in selection
        if not (len(valid_nodes) > 0):
            # Check for frames only
            frames_selected = [n for n in node_selected if self.is_frame_node(n)]
            if (len(frames_selected) > 1 and len(frames_selected) == len(node_selected)):
                self.report({'ERROR'}, "Please select only 1 frame to reset")
            else:
                self.report({'ERROR'}, "No valid node(s) in selection")
            return {'CANCELLED'}

        # Report nodes that are not valid
        if len(valid_nodes) != len(node_selected) and node_active_is_frame is False:
            valid_node_names = [n.name for n in valid_nodes]
            not_valid_names = list(set(selected_node_names) - set(valid_node_names))
            message = rpt_("Ignored {}").format(", ".join(not_valid_names))
            self.report({'INFO'}, message)

        # Deselect all nodes
        for i in node_selected:
            i.select = False

        # Run through all valid nodes
        for node in valid_nodes:
            parent = node.parent if node.parent else None
            node_loc = [node.location.x, node.location.y]

            node_tree = node.id_data
            props_to_copy = 'bl_idname name location height width'.split(' ')

            reconnections = []
            mappings = chain.from_iterable([node.inputs, node.outputs])
            for i in (i for i in mappings if i.is_linked):
                for L in i.links:
                    reconnections.append([L.from_socket.path_from_id(), L.to_socket.path_from_id()])

            props = {j: getattr(node, j) for j in props_to_copy}

            new_node = node_tree.nodes.new(props['bl_idname'])
            props_to_copy.pop(0)

            for prop in props_to_copy:
                setattr(new_node, prop, props[prop])

            nodes = node_tree.nodes
            nodes.remove(node)
            new_node.name = props['name']

            if parent:
                new_node.parent = parent
                new_node.location = node_loc

            for str_from, str_to in reconnections:
                connect_sockets(eval(str_from), eval(str_to))

            new_node.select = False
            success_names.append(new_node.name)

        # Reselect all nodes
        if selected_node_names and node_active_is_frame is False:
            for i in selected_node_names:
                node_tree.nodes[i].select = True

        if active_node_name is not None:
            node_tree.nodes[active_node_name].select = True
            node_tree.nodes.active = node_tree.nodes[active_node_name]

        message = rpt_("Successfully reset {}").format(", ".join(success_names))
        self.report({'INFO'}, message)
        return {'FINISHED'}
