# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_not_empty,
    get_nodes_links,
    node_at_pos,
)
from ..utils.draw import (
    draw_callback_nodeoutline,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_lazy_mix(Operator, NWBase):
    """Add a Mix RGB/Shader node by interactively drawing lines between nodes"""
    bl_idname = "node.nw_lazy_mix"
    bl_label = "Mix Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_not_empty(cls, context)

    def modal(self, context, event):
        context.area.tag_redraw()
        nodes, links = get_nodes_links(context)
        cont = True

        node1 = None
        if not context.scene.NWBusyDrawing:
            node1 = node_at_pos(nodes, context, event)
            if node1:
                context.scene.NWBusyDrawing = node1.name
        else:
            if context.scene.NWBusyDrawing != 'STOP':
                node1 = nodes[context.scene.NWBusyDrawing]

        context.scene.NWLazySource = node1.name
        context.scene.NWLazyTarget = node_at_pos(nodes, context, event).name

        if event.type == 'MOUSEMOVE':
            self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))

        elif event.type == 'RIGHTMOUSE' and event.value == 'RELEASE':
            bpy.types.SpaceNodeEditor.draw_handler_remove(self._handle, 'WINDOW')

            node2 = None
            node2 = node_at_pos(nodes, context, event)
            if node2:
                context.scene.NWBusyDrawing = node2.name

            if node1 == node2:
                cont = False

            if cont:
                if node1 and node2:
                    for node in nodes:
                        node.select = False
                    node1.select = True
                    node2.select = True

                    bpy.ops.node.nw_merge_nodes(mode="MIX", merge_type="AUTO")

            context.scene.NWBusyDrawing = ""
            return {'FINISHED'}

        elif event.type == 'ESC':
            bpy.types.SpaceNodeEditor.draw_handler_remove(self._handle, 'WINDOW')
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type != 'NODE_EDITOR':
            self.report({'WARNING'}, "Active editor should be a node editor for the operator to run")
            return {'CANCELLED'}

        # the arguments we pass the callback
        args = (self, context, 'MIX')
        # Add the region OpenGL drawing callback
        # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
        self._handle = bpy.types.SpaceNodeEditor.draw_handler_add(
            draw_callback_nodeoutline, args, 'WINDOW', 'POST_PIXEL')

        self.mouse_path = []

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}
