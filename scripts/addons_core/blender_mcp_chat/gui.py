# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""GUI panels for MCP Chat addon."""

import bpy
from bpy.types import Panel, UIList


class MCP_UL_chat_messages(UIList):
    """UIList for displaying chat messages."""

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            icon = 'USER' if item.role == "user" else 'SCRIPTPLUGINS'
            content = item.content[:60] + "..." if len(item.content) > 60 else item.content
            layout.label(text=content, icon=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon='CHAT')


class VIEW3D_PT_mcp_chat(Panel):
    """Main MCP Chat panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "MCP Server"

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat

        # Status row
        row = layout.row()
        row.scale_y = 1.2
        if settings.server_running:
            row.operator("mcp.stop_server", text="Stop", icon='PAUSE', depress=True)
            sub = row.row(align=True)
            sub.alignment = 'RIGHT'
            sub.label(text=f"{settings.server_host}:{settings.server_port}")
        else:
            row.operator("mcp.start_server", text="Start Server", icon='PLAY')

        if settings.server_running:
            # Connection status
            box = layout.box()
            col = box.column(align=True)

            row = col.row()
            row.label(text="Status")
            sub = row.row()
            sub.alignment = 'RIGHT'
            sub.label(text="Connected", icon='CHECKMARK')

            row = col.row()
            row.label(text="Clients")
            sub = row.row()
            sub.alignment = 'RIGHT'
            sub.label(text=str(settings.connected_clients))

            # Actions
            row = layout.row(align=True)
            row.operator("mcp.test_connection", text="Test")
            row.operator("mcp.copy_connection_info", text="Copy")
            row.operator("mcp.refresh_status", text="", icon='FILE_REFRESH')


class VIEW3D_PT_mcp_settings(Panel):
    """Server settings panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Settings"
    bl_parent_id = "VIEW3D_PT_mcp_chat"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat

        col = layout.column()
        col.prop(settings, "server_host", text="Host")
        col.prop(settings, "server_port", text="Port")


class VIEW3D_PT_mcp_integrations(Panel):
    """Integrations panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Integrations"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        mcp = context.scene.mcp_chat

        # PolyHaven
        box = layout.box()
        row = box.row()
        row.prop(mcp.polyhaven, "enabled", text="")
        row.label(text="PolyHaven")
        if mcp.polyhaven.enabled:
            row.label(text="", icon='CHECKMARK')
            box.prop(mcp.polyhaven, "default_resolution", text="Resolution")

        # Sketchfab
        box = layout.box()
        row = box.row()
        row.prop(mcp.sketchfab, "enabled", text="")
        row.label(text="Sketchfab")
        if mcp.sketchfab.enabled:
            if mcp.sketchfab.api_token:
                row.label(text="", icon='CHECKMARK')
            else:
                row.label(text="", icon='ERROR')
            box.prop(mcp.sketchfab, "api_token", text="Token")

        # Hyper3D
        box = layout.box()
        row = box.row()
        row.prop(mcp.hyper3d, "enabled", text="")
        row.label(text="Hyper3D Rodin")
        if mcp.hyper3d.enabled:
            if mcp.hyper3d.api_key:
                row.label(text="", icon='CHECKMARK')
            else:
                row.label(text="", icon='ERROR')
            col = box.column()
            col.prop(mcp.hyper3d, "mode", text="Mode")
            col.prop(mcp.hyper3d, "api_key", text="Key")

        # Hunyuan3D
        box = layout.box()
        row = box.row()
        row.prop(mcp.hunyuan3d, "enabled", text="")
        row.label(text="Hunyuan3D")
        if mcp.hunyuan3d.enabled:
            has_creds = (mcp.hunyuan3d.mode == 'LOCAL' or
                         (mcp.hunyuan3d.secret_id and mcp.hunyuan3d.secret_key))
            row.label(text="", icon='CHECKMARK' if has_creds else 'ERROR')
            col = box.column()
            col.prop(mcp.hunyuan3d, "mode", text="Mode")
            if mcp.hunyuan3d.mode == 'TENCENT':
                col.prop(mcp.hunyuan3d, "secret_id", text="ID")
                col.prop(mcp.hunyuan3d, "secret_key", text="Key")
            else:
                col.prop(mcp.hunyuan3d, "local_url", text="URL")


class VIEW3D_PT_mcp_hunyuan_settings(Panel):
    """Hunyuan3D generation settings."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Generation"
    bl_parent_id = "VIEW3D_PT_mcp_integrations"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.mcp_chat.hunyuan3d.enabled

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat.hunyuan3d

        col = layout.column()
        col.prop(settings, "octree_resolution", text="Resolution")
        col.prop(settings, "num_inference_steps", text="Steps")
        col.prop(settings, "guidance_scale", text="Guidance")


class VIEW3D_PT_mcp_tools(Panel):
    """Quick tools panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Tools"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.scale_y = 1.1
        col.operator("mcp.get_scene_info", text="Scene Info", icon='SCENE_DATA')
        col.operator("mcp.take_screenshot", text="Screenshot", icon='IMAGE_DATA')


classes = (
    MCP_UL_chat_messages,
    VIEW3D_PT_mcp_chat,
    VIEW3D_PT_mcp_settings,
    VIEW3D_PT_mcp_integrations,
    VIEW3D_PT_mcp_hunyuan_settings,
    VIEW3D_PT_mcp_tools,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
