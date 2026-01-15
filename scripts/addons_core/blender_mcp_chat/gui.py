# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""GUI panels for MCP Chat addon."""

import bpy
from bpy.types import Panel, UIList, Menu


# ============================================================================
# Chat Message List
# ============================================================================

class MCP_UL_chat_messages(UIList):
    """UIList for displaying chat messages."""

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)

            # Role icon
            if item.role == "user":
                row.label(text="", icon='USER')
            elif item.role == "assistant":
                row.label(text="", icon='PLUGIN')
            else:
                row.label(text="", icon='INFO')

            # Message content (truncated for display)
            content = item.content
            if len(content) > 50:
                content = content[:47] + "..."
            row.label(text=content)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon='CHAT')


# ============================================================================
# Main Chat Panel
# ============================================================================

class VIEW3D_PT_mcp_chat(Panel):
    """Main MCP Chat panel in the 3D View sidebar."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "MCP Chat"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        settings = scene.mcp_chat

        layout.use_property_split = True
        layout.use_property_decorate = False

        # Server Status
        box = layout.box()
        row = box.row()
        row.label(text="Server Status:", icon='WORLD_DATA')

        if settings.server_running:
            row.label(text="Running", icon='CHECKMARK')
        else:
            row.label(text="Stopped", icon='X')

        # Server controls
        row = box.row(align=True)
        if settings.server_running:
            row.operator("mcp.stop_server", text="Stop Server", icon='PAUSE')
            row.operator("mcp.refresh_status", text="", icon='FILE_REFRESH')
        else:
            row.operator("mcp.start_server", text="Start Server", icon='PLAY')

        # Connection info
        if settings.server_running:
            col = box.column(align=True)
            col.label(text=f"Host: {settings.server_host}:{settings.server_port}")
            col.label(text=f"Clients: {settings.connected_clients}")

            row = col.row(align=True)
            row.operator("mcp.test_connection", text="Test", icon='DRIVER')
            row.operator("mcp.copy_connection_info", text="Copy Info", icon='COPYDOWN')


class VIEW3D_PT_mcp_chat_settings(Panel):
    """Server settings sub-panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Server Settings"
    bl_parent_id = "VIEW3D_PT_mcp_chat"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        settings = scene.mcp_chat

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(align=True)
        col.prop(settings, "server_host", text="Host")
        col.prop(settings, "server_port", text="Port")

        layout.separator()
        layout.prop(settings, "telemetry_consent", text="Allow Telemetry")


class VIEW3D_PT_mcp_chat_tools(Panel):
    """Quick tools sub-panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Quick Tools"
    bl_parent_id = "VIEW3D_PT_mcp_chat"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("mcp.get_scene_info", text="Get Scene Info", icon='SCENE_DATA')
        col.operator("mcp.take_screenshot", text="Take Screenshot", icon='RENDER_STILL')


# ============================================================================
# PolyHaven Integration Panel
# ============================================================================

class VIEW3D_PT_mcp_polyhaven(Panel):
    """PolyHaven integration panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "PolyHaven"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        settings = context.scene.mcp_chat.polyhaven
        self.layout.prop(settings, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat.polyhaven

        layout.use_property_split = True
        layout.use_property_decorate = False

        if not settings.enabled:
            layout.label(text="Enable to use PolyHaven assets", icon='INFO')
            return

        col = layout.column(align=True)
        col.prop(settings, "default_resolution", text="Resolution")

        layout.separator()
        layout.label(text="Asset Types:", icon='ASSET_MANAGER')

        row = layout.row(align=True)
        op = row.operator("mcp.search_polyhaven", text="HDRIs")
        op.asset_type = "hdris"
        op = row.operator("mcp.search_polyhaven", text="Textures")
        op.asset_type = "textures"
        op = row.operator("mcp.search_polyhaven", text="Models")
        op.asset_type = "models"


# ============================================================================
# Sketchfab Integration Panel
# ============================================================================

class VIEW3D_PT_mcp_sketchfab(Panel):
    """Sketchfab integration panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Sketchfab"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        settings = context.scene.mcp_chat.sketchfab
        self.layout.prop(settings, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat.sketchfab

        layout.use_property_split = True
        layout.use_property_decorate = False

        if not settings.enabled:
            layout.label(text="Enable to use Sketchfab models", icon='INFO')
            return

        col = layout.column(align=True)
        col.prop(settings, "api_token", text="API Token")

        if not settings.api_token:
            layout.label(text="API token required for downloads", icon='ERROR')
        else:
            layout.label(text="Token configured", icon='CHECKMARK')


# ============================================================================
# Hyper3D Integration Panel
# ============================================================================

class VIEW3D_PT_mcp_hyper3d(Panel):
    """Hyper3D Rodin integration panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Hyper3D Rodin"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        settings = context.scene.mcp_chat.hyper3d
        self.layout.prop(settings, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat.hyper3d

        layout.use_property_split = True
        layout.use_property_decorate = False

        if not settings.enabled:
            layout.label(text="Enable for AI 3D generation", icon='INFO')
            return

        col = layout.column(align=True)
        col.prop(settings, "mode", text="Mode")
        col.prop(settings, "api_key", text="API Key")

        if not settings.api_key:
            layout.label(text="API key required", icon='ERROR')
        else:
            layout.label(text="API key configured", icon='CHECKMARK')

        layout.separator()
        layout.label(text="Generate 3D models from text or images", icon='MESH_MONKEY')


# ============================================================================
# Hunyuan3D Integration Panel
# ============================================================================

class VIEW3D_PT_mcp_hunyuan3d(Panel):
    """Hunyuan3D integration panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "Hunyuan3D"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        settings = context.scene.mcp_chat.hunyuan3d
        self.layout.prop(settings, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        settings = context.scene.mcp_chat.hunyuan3d

        layout.use_property_split = True
        layout.use_property_decorate = False

        if not settings.enabled:
            layout.label(text="Enable for Hunyuan3D generation", icon='INFO')
            return

        col = layout.column(align=True)
        col.prop(settings, "mode", text="Mode")

        if settings.mode == 'TENCENT':
            col.prop(settings, "secret_id", text="Secret ID")
            col.prop(settings, "secret_key", text="Secret Key")

            if not settings.secret_id or not settings.secret_key:
                layout.label(text="Tencent Cloud credentials required", icon='ERROR')
            else:
                layout.label(text="Credentials configured", icon='CHECKMARK')
        else:
            col.prop(settings, "local_url", text="API URL")

        layout.separator()

        # Generation parameters
        box = layout.box()
        box.label(text="Generation Settings:", icon='SETTINGS')
        col = box.column(align=True)
        col.prop(settings, "octree_resolution", text="Resolution")
        col.prop(settings, "num_inference_steps", text="Steps")
        col.prop(settings, "guidance_scale", text="Guidance")


# ============================================================================
# About/Help Panel
# ============================================================================

class VIEW3D_PT_mcp_about(Panel):
    """About and help panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Chat"
    bl_label = "About"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        layout.label(text="Blender MCP Chat v1.0.0")
        layout.label(text="Connect Blender to AI assistants")

        layout.separator()

        box = layout.box()
        box.label(text="Available Commands:", icon='INFO')
        col = box.column(align=True)
        col.scale_y = 0.8
        col.label(text="get_scene_info")
        col.label(text="get_object_info")
        col.label(text="get_viewport_screenshot")
        col.label(text="execute_code")

        layout.separator()

        layout.label(text="Integrations:", icon='PLUGIN')
        col = layout.column(align=True)
        col.scale_y = 0.8
        col.label(text="PolyHaven - Free PBR assets")
        col.label(text="Sketchfab - 3D model library")
        col.label(text="Hyper3D - Text/image to 3D")
        col.label(text="Hunyuan3D - Tencent 3D gen")


# ============================================================================
# Registration
# ============================================================================

classes = (
    MCP_UL_chat_messages,
    VIEW3D_PT_mcp_chat,
    VIEW3D_PT_mcp_chat_settings,
    VIEW3D_PT_mcp_chat_tools,
    VIEW3D_PT_mcp_polyhaven,
    VIEW3D_PT_mcp_sketchfab,
    VIEW3D_PT_mcp_hyper3d,
    VIEW3D_PT_mcp_hunyuan3d,
    VIEW3D_PT_mcp_about,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
