# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator, Menu
from bpy.props import BoolProperty, IntProperty
from bpy_extras.node_utils import connect_sockets
from bpy.app.translations import contexts as i18n_contexts

from ..interface import (
    socket_to_icon,
)
from ..utils.nodes import (
    NWBase,
    NWBaseMenu,
    nw_check,
    nw_check_not_empty,
    get_nodes_links,
    node_at_pos,
    autolink,
    force_update,
)
from ..utils.draw import (
    draw_callback_nodeoutline,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_lazy_connect(Operator, NWBase):
    """Connect two nodes without clicking a specific socket (automatically determined)"""
    bl_idname = "node.nw_lazy_connect"
    bl_label = "Lazy Connect"
    bl_options = {'REGISTER', 'UNDO'}
    with_menu: BoolProperty()

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

            link_success = False
            if cont:
                if node1 and node2:
                    original_sel = []
                    original_unsel = []
                    for node in nodes:
                        if node.select:
                            node.select = False
                            original_sel.append(node)
                        else:
                            original_unsel.append(node)
                    node1.select = True
                    node2.select = True

                    # link_success = autolink(node1, node2, links)
                    if self.with_menu:
                        available_outps = [outp for outp in node1.outputs if outp.enabled]
                        if len(available_outps) > 1 and node2.inputs:
                            bpy.ops.wm.call_menu("INVOKE_DEFAULT", name=NODE_MT_lazy_connect_outputs.bl_idname)
                        elif len(available_outps) == 1:
                            bpy.ops.node.lazy_connect_call_inputs_menu(from_socket=0)
                    else:
                        link_success = autolink(node1, node2, links)

                    for node in original_sel:
                        node.select = True
                    for node in original_unsel:
                        node.select = False

            if link_success:
                force_update(context)
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

        nodes, links = get_nodes_links(context)
        node = node_at_pos(nodes, context, event)
        if node:
            context.scene.NWBusyDrawing = node.name

        # The arguments we pass the callback.
        mode = "LINK"
        if self.with_menu:
            mode = "LINKMENU"
        args = (self, context, mode)
        # Add the region OpenGL drawing callback
        # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
        self._handle = bpy.types.SpaceNodeEditor.draw_handler_add(
            draw_callback_nodeoutline, args, 'WINDOW', 'POST_PIXEL')

        self.mouse_path = []

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


# Operator for calling the pop-up menu of inputs
class NODE_OT_lazy_connect_call_inputs_menu(Operator, NWBase):
    """Link from this output"""
    bl_idname = 'node.lazy_connect_call_inputs_menu'
    bl_label = 'Make Link'
    bl_options = {'REGISTER', 'UNDO'}
    from_socket: IntProperty()

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        context.scene.NWSourceSocket = self.from_socket

        n1 = nodes[context.scene.NWLazySource]
        n2 = nodes[context.scene.NWLazyTarget]
        if len(n2.inputs) > 1:
            bpy.ops.wm.call_menu("INVOKE_DEFAULT", name=NODE_MT_lazy_connect_inputs.bl_idname)
        elif len(n2.inputs) == 1:
            connect_sockets(n1.outputs[self.from_socket], n2.inputs[0])
        return {'FINISHED'}


# Operator for making the link between two sockets
class NODE_OT_lazy_connect_make_link(Operator, NWBase):
    """Make a link from one socket to another"""
    bl_idname = 'node.lazy_connect_make_link'
    bl_label = 'Make Link'
    bl_options = {'REGISTER', 'UNDO'}

    from_socket: IntProperty()
    to_socket: IntProperty()

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        n1 = nodes[context.scene.NWLazySource]
        n2 = nodes[context.scene.NWLazyTarget]

        connect_sockets(n1.outputs[self.from_socket], n2.inputs[self.to_socket])

        force_update(context)

        return {'FINISHED'}


#### ------------------------------ MENUS ------------------------------ ####

class NODE_MT_lazy_connect_outputs(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_lazy_connect_outputs"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        nodes, links = get_nodes_links(context)

        layout.label(text="From Socket", icon='RADIOBUT_OFF')
        layout.separator()

        n1 = nodes[context.scene.NWLazySource]
        for index, output in enumerate(n1.outputs):
            # Only show sockets that are exposed.
            if output.enabled:
                layout.operator(
                    "node.lazy_connect_call_inputs_menu",
                    text=output.name,
                    text_ctxt=i18n_contexts.default,
                    icon=socket_to_icon(output),
                ).from_socket = index


class NODE_MT_lazy_connect_inputs(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_lazy_connect_inputs"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        nodes, links = get_nodes_links(context)

        layout.label(text="To Socket", icon='FORWARD')
        layout.separator()

        n2 = nodes[context.scene.NWLazyTarget]

        for index, input in enumerate(n2.inputs):
            # Only show sockets that are exposed.
            # This prevents, for example, the scale value socket
            # of the vector math node being added to the list when
            # the mode is not 'SCALE'.
            if input.enabled:
                op = layout.operator(
                    "node.lazy_connect_make_link", text=input.name,
                    text_ctxt=i18n_contexts.default,
                    icon=socket_to_icon(input),
                )
                op.from_socket = context.scene.NWSourceSocket
                op.to_socket = index
