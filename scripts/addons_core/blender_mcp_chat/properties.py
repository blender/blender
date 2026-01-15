# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Properties for Blender AI Assistant."""

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
    """A single chat message."""
    role: StringProperty(name="Role", default="user")
    content: StringProperty(name="Content", default="")
    timestamp: FloatProperty(name="Timestamp", default=0.0)


class AISettings(PropertyGroup):
    """Settings for the AI Assistant."""

    # API settings
    api_key: StringProperty(
        name="API Key",
        description="Anthropic API key for Claude",
        subtype='PASSWORD',
        default=""
    )
    model: EnumProperty(
        name="Model",
        description="AI model to use",
        items=[
            ('claude-sonnet-4-20250514', 'Claude Sonnet 4', 'Fast and capable'),
            ('claude-3-5-sonnet-20241022', 'Claude 3.5 Sonnet', 'Previous generation'),
        ],
        default='claude-sonnet-4-20250514'
    )

    # Chat state
    chat_input: StringProperty(
        name="Message",
        description="Type your message here",
        default=""
    )
    chat_message_index: IntProperty(default=0)
    is_processing: BoolProperty(
        name="Processing",
        description="Whether the AI is currently processing",
        default=False
    )

    # System prompt
    system_prompt: StringProperty(
        name="System Prompt",
        description="Custom instructions for the AI",
        default="You are an AI assistant integrated into Blender. Help users with 3D modeling, animation, and scripting tasks. You can execute Python code to control Blender directly."
    )


classes = (
    ChatMessage,
    AISettings,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.ai_assistant = PointerProperty(type=AISettings)
    bpy.types.Scene.ai_messages = CollectionProperty(type=ChatMessage)


def unregister():
    del bpy.types.Scene.ai_messages
    del bpy.types.Scene.ai_assistant

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
