# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.props import (
    StringProperty,
    BoolProperty,
    IntProperty,
    FloatProperty,
    EnumProperty,
    CollectionProperty,
    PointerProperty,
)
from bpy.types import PropertyGroup


class ChatMessage(PropertyGroup):
    """A single chat message in the conversation."""
    role: StringProperty(
        name="Role",
        description="Message sender role (user/assistant/system)",
        default="user"
    )
    content: StringProperty(
        name="Content",
        description="Message content",
        default=""
    )
    timestamp: FloatProperty(
        name="Timestamp",
        description="Unix timestamp of message",
        default=0.0
    )


class PolyHavenSettings(PropertyGroup):
    """Settings for PolyHaven integration."""
    enabled: BoolProperty(
        name="Enable PolyHaven",
        description="Enable PolyHaven asset integration",
        default=False
    )
    default_resolution: EnumProperty(
        name="Default Resolution",
        description="Default resolution for downloaded assets",
        items=[
            ('1k', '1K', '1024px resolution'),
            ('2k', '2K', '2048px resolution'),
            ('4k', '4K', '4096px resolution'),
            ('8k', '8K', '8192px resolution'),
        ],
        default='2k'
    )


class SketchfabSettings(PropertyGroup):
    """Settings for Sketchfab integration."""
    enabled: BoolProperty(
        name="Enable Sketchfab",
        description="Enable Sketchfab model integration",
        default=False
    )
    api_token: StringProperty(
        name="API Token",
        description="Sketchfab API token for downloading models",
        default="",
        subtype='PASSWORD'
    )


class Hyper3DSettings(PropertyGroup):
    """Settings for Hyper3D Rodin integration."""
    enabled: BoolProperty(
        name="Enable Hyper3D",
        description="Enable Hyper3D Rodin text/image-to-3D generation",
        default=False
    )
    api_key: StringProperty(
        name="API Key",
        description="Hyper3D Rodin API key",
        default="",
        subtype='PASSWORD'
    )
    mode: EnumProperty(
        name="Mode",
        description="Hyper3D API mode",
        items=[
            ('MAIN_SITE', 'Main Site', 'Use official Hyper3D Rodin API'),
            ('FAL_AI', 'FAL.ai', 'Use FAL.ai hosted Rodin API'),
        ],
        default='MAIN_SITE'
    )


class Hunyuan3DSettings(PropertyGroup):
    """Settings for Hunyuan3D integration."""
    enabled: BoolProperty(
        name="Enable Hunyuan3D",
        description="Enable Tencent Hunyuan3D text/image-to-3D generation",
        default=False
    )
    mode: EnumProperty(
        name="Mode",
        description="Hunyuan3D API mode",
        items=[
            ('TENCENT', 'Tencent Cloud', 'Use official Tencent Cloud API'),
            ('LOCAL', 'Local API', 'Use local Hunyuan3D API server'),
        ],
        default='TENCENT'
    )
    secret_id: StringProperty(
        name="Secret ID",
        description="Tencent Cloud Secret ID",
        default="",
        subtype='PASSWORD'
    )
    secret_key: StringProperty(
        name="Secret Key",
        description="Tencent Cloud Secret Key",
        default="",
        subtype='PASSWORD'
    )
    local_url: StringProperty(
        name="Local API URL",
        description="Local Hunyuan3D API server URL",
        default="http://localhost:8080"
    )
    octree_resolution: IntProperty(
        name="Octree Resolution",
        description="Resolution for mesh generation",
        default=256,
        min=64,
        max=512
    )
    num_inference_steps: IntProperty(
        name="Inference Steps",
        description="Number of inference steps",
        default=25,
        min=1,
        max=100
    )
    guidance_scale: FloatProperty(
        name="Guidance Scale",
        description="Guidance scale for generation",
        default=5.5,
        min=0.0,
        max=20.0
    )


class MCPChatSettings(PropertyGroup):
    """Main settings for the MCP Chat addon."""
    server_running: BoolProperty(
        name="Server Running",
        description="Whether the MCP server is currently running",
        default=False
    )
    server_port: IntProperty(
        name="Server Port",
        description="Port for the MCP server to listen on",
        default=9876,
        min=1024,
        max=65535
    )
    server_host: StringProperty(
        name="Server Host",
        description="Host address for the MCP server",
        default="127.0.0.1"
    )
    connected_clients: IntProperty(
        name="Connected Clients",
        description="Number of currently connected clients",
        default=0
    )
    telemetry_consent: BoolProperty(
        name="Telemetry Consent",
        description="Allow anonymous usage telemetry",
        default=False
    )
    last_command: StringProperty(
        name="Last Command",
        description="Last executed command",
        default=""
    )
    last_result: StringProperty(
        name="Last Result",
        description="Result of the last command",
        default=""
    )
    # Chat interface
    chat_input: StringProperty(
        name="Chat Input",
        description="Enter your message",
        default=""
    )
    chat_message_index: IntProperty(
        name="Chat Message Index",
        description="Index of selected chat message",
        default=0
    )
    # PolyHaven settings
    polyhaven: PointerProperty(type=PolyHavenSettings)
    # Sketchfab settings
    sketchfab: PointerProperty(type=SketchfabSettings)
    # Hyper3D settings
    hyper3d: PointerProperty(type=Hyper3DSettings)
    # Hunyuan3D settings
    hunyuan3d: PointerProperty(type=Hunyuan3DSettings)


classes = (
    ChatMessage,
    PolyHavenSettings,
    SketchfabSettings,
    Hyper3DSettings,
    Hunyuan3DSettings,
    MCPChatSettings,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.mcp_chat = PointerProperty(type=MCPChatSettings)
    bpy.types.Scene.mcp_chat_messages = CollectionProperty(type=ChatMessage)


def unregister():
    del bpy.types.Scene.mcp_chat_messages
    del bpy.types.Scene.mcp_chat

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
