# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import BoolProperty
from bpy.app.translations import pgettext_rpt as rpt_

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_not_empty,
    nw_check_space_type,
    get_nodes_links,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_delete_unused(Operator, NWBase):
    """Delete all nodes with unused outputs"""
    bl_idname = 'node.nw_del_unused'
    bl_label = 'Delete Unused Nodes'
    bl_options = {'REGISTER', 'UNDO'}

    delete_muted: BoolProperty(
        name="Delete Muted",
        description="Dissolve all muted nodes with reconnect",
        default=True)
    delete_frames: BoolProperty(
        name="Delete Empty Frames",
        description="Delete all frames that have no nodes inside them",
        default=True)

    def is_unused_node(self, node):
        end_types = ['OUTPUT_MATERIAL', 'OUTPUT', 'VIEWER', 'COMPOSITE',
                     'SPLITVIEWER', 'OUTPUT_FILE', 'LEVELS', 'OUTPUT_LIGHT',
                     'OUTPUT_WORLD', 'GROUP_INPUT', 'GROUP_OUTPUT', 'FRAME',
                     'WARNING']
        if node.type in end_types:
            return False

        for output in node.outputs:
            if output.links:
                return False
        return True

    @classmethod
    def poll(cls, context):
        """Disabled for custom nodes as we do not know which nodes are supported."""
        return (nw_check(cls, context)
                and nw_check_not_empty(cls, context)
                and nw_check_space_type(cls, context, {'ShaderNodeTree', 'CompositorNodeTree',
                                        'TextureNodeTree', 'GeometryNodeTree'}))

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        # Store selection
        selection = []
        for node in nodes:
            if node.select:
                selection.append(node.name)

        for node in nodes:
            node.select = False

        deleted_nodes = []
        temp_deleted_nodes = []
        del_unused_iterations = len(nodes)
        for it in range(0, del_unused_iterations):
            temp_deleted_nodes = list(deleted_nodes)  # keep record of last iteration
            for node in nodes:
                if self.is_unused_node(node):
                    node.select = True
                    deleted_nodes.append(node.name)
                    bpy.ops.node.delete()

            if temp_deleted_nodes == deleted_nodes:  # stop iterations when there are no more nodes to be deleted
                break

        if self.delete_frames:
            repeat = True
            while repeat:
                frames_in_use = []
                frames = []
                repeat = False
                for node in nodes:
                    if node.parent:
                        frames_in_use.append(node.parent)
                for node in nodes:
                    if node.type == 'FRAME' and node not in frames_in_use:
                        frames.append(node)
                        if node.parent:
                            repeat = True  # repeat for nested frames
                for node in frames:
                    if node not in frames_in_use:
                        node.select = True
                        deleted_nodes.append(node.name)
                bpy.ops.node.delete()

        if self.delete_muted:
            for node in nodes:
                if node.mute:
                    node.select = True
                    deleted_nodes.append(node.name)
            bpy.ops.node.delete_reconnect()

        # get unique list of deleted nodes (iterations would count the same node more than once)
        deleted_nodes = list(set(deleted_nodes))
        for n in deleted_nodes:
            self.report({'INFO'}, rpt_("Node {} deleted").format(n))
        num_deleted = len(deleted_nodes)
        if num_deleted == 0:
            self.report({'INFO'}, "Nothing to delete")
        else:
            if num_deleted > 1:
                message = rpt_("Deleted {} nodes".format(num_deleted))
            else:
                message = rpt_("Deleted 1 node")
            self.report({'INFO'}, message)

        # Restore selection
        nodes, links = get_nodes_links(context)
        for node in nodes:
            if node.name in selection:
                node.select = True
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)