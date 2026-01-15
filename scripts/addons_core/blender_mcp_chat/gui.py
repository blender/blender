# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Chat sidebar UI for Blender AI assistant."""

import bpy
from bpy.types import Panel, UIList


class MCP_UL_messages(UIList):
    """Chat message list."""

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)

            if item.role == "user":
                row.alignment = 'RIGHT'
                row.label(text=item.content[:80])
                row.label(text="", icon='USER')
            else:
                row.label(text="", icon='LIGHT')
                row.label(text=item.content[:80])

        elif self.layout_type == 'GRID':
            layout.label(text="", icon='CHAT')


class VIEW3D_PT_ai_chat(Panel):
    """Main AI Chat panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Chat"

    def draw_header(self, context):
        settings = context.scene.mcp_chat
        row = self.layout.row(align=True)
        if settings.server_running:
            row.label(text="", icon='RADIOBUT_ON')
        else:
            row.label(text="", icon='RADIOBUT_OFF')

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        settings = scene.mcp_chat

        # Server not running - show connect button
        if not settings.server_running:
            col = layout.column()
            col.scale_y = 1.5
            col.operator("mcp.start_server", text="Connect", icon='LINKED')

            col = layout.column()
            col.scale_y = 0.8
            col.label(text="Start the server to enable")
            col.label(text="AI assistant connection")
            return

        # Chat messages area
        col = layout.column()

        # Message list
        rows = 12 if len(scene.mcp_chat_messages) > 0 else 4
        col.template_list(
            "MCP_UL_messages", "",
            scene, "mcp_chat_messages",
            settings, "chat_message_index",
            rows=rows,
            type='DEFAULT'
        )

        # Input area
        col = layout.column(align=True)
        col.prop(settings, "chat_input", text="")

        row = col.row(align=True)
        row.scale_y = 1.2
        row.operator("mcp.send_message", text="Send", icon='EXPORT')
        row.operator("mcp.clear_chat", text="", icon='X')


class VIEW3D_PT_ai_context(Panel):
    """Context panel showing what AI can see."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Context"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        col = layout.column(align=True)

        # Scene info
        row = col.row()
        row.label(text="Scene")
        row.label(text=scene.name)

        # Selected object
        row = col.row()
        row.label(text="Selected")
        if context.active_object:
            row.label(text=context.active_object.name)
        else:
            row.label(text="None")

        # Object count
        row = col.row()
        row.label(text="Objects")
        row.label(text=str(len(scene.objects)))

        layout.separator()

        # Quick actions
        col = layout.column(align=True)
        col.operator("mcp.get_scene_info", text="Refresh Context", icon='FILE_REFRESH')
        col.operator("mcp.take_screenshot", text="Share Viewport", icon='IMAGE_DATA')


class VIEW3D_PT_ai_server(Panel):
    """Server status and settings."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Connection"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat

        # Status
        row = layout.row()
        row.label(text="Status")
        if settings.server_running:
            row.label(text="Running", icon='CHECKMARK')
        else:
            row.label(text="Stopped", icon='X')

        if settings.server_running:
            row = layout.row()
            row.label(text="Address")
            row.label(text=f"{settings.server_host}:{settings.server_port}")

            row = layout.row()
            row.label(text="Clients")
            row.label(text=str(settings.connected_clients))

            layout.separator()

            row = layout.row(align=True)
            row.operator("mcp.stop_server", text="Disconnect", icon='UNLINKED')
            row.operator("mcp.refresh_status", text="", icon='FILE_REFRESH')
        else:
            layout.separator()
            col = layout.column()
            col.prop(settings, "server_host", text="Host")
            col.prop(settings, "server_port", text="Port")

            layout.separator()
            layout.operator("mcp.start_server", text="Connect", icon='LINKED')


class VIEW3D_PT_ai_integrations(Panel):
    """Third-party integrations."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Assets"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        mcp = context.scene.mcp_chat

        # PolyHaven
        row = layout.row()
        row.prop(mcp.polyhaven, "enabled", text="PolyHaven")
        if mcp.polyhaven.enabled:
            layout.prop(mcp.polyhaven, "default_resolution", text="Resolution")

        # Sketchfab
        row = layout.row()
        row.prop(mcp.sketchfab, "enabled", text="Sketchfab")
        if mcp.sketchfab.enabled:
            layout.prop(mcp.sketchfab, "api_token", text="Token")

        # Hyper3D
        row = layout.row()
        row.prop(mcp.hyper3d, "enabled", text="Hyper3D")
        if mcp.hyper3d.enabled:
            col = layout.column()
            col.prop(mcp.hyper3d, "mode", text="Mode")
            col.prop(mcp.hyper3d, "api_key", text="Key")

        # Hunyuan3D
        row = layout.row()
        row.prop(mcp.hunyuan3d, "enabled", text="Hunyuan3D")
        if mcp.hunyuan3d.enabled:
            col = layout.column()
            col.prop(mcp.hunyuan3d, "mode", text="Mode")
            if mcp.hunyuan3d.mode == 'TENCENT':
                col.prop(mcp.hunyuan3d, "secret_id", text="ID")
                col.prop(mcp.hunyuan3d, "secret_key", text="Key")
            else:
                col.prop(mcp.hunyuan3d, "local_url", text="URL")


classes = (
    MCP_UL_messages,
    VIEW3D_PT_ai_chat,
    VIEW3D_PT_ai_context,
    VIEW3D_PT_ai_server,
    VIEW3D_PT_ai_integrations,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
