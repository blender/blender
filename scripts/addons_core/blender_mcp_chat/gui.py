# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""GUI for Blender AI Assistant."""

import bpy
from bpy.types import Panel
import textwrap


def draw_message(layout, role, content):
    """Draw a chat message bubble."""
    box = layout.box()
    row = box.row()

    if role == "user":
        row.label(text="You", icon='USER')
    else:
        row.label(text="Claude", icon='LIGHT')

    col = box.column(align=True)
    col.scale_y = 0.85

    for line in content.split('\n'):
        for wrap in textwrap.wrap(line, width=42) or ['']:
            col.label(text=wrap)


class VIEW3D_PT_ai_chat(Panel):
    """AI Chat panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Assistant"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        ai = scene.ai_assistant

        # Setup screen
        if not ai.api_key:
            box = layout.box()
            col = box.column(align=True)
            col.scale_y = 1.1
            col.label(text="Welcome!", icon='LIGHT')
            col.separator()
            col.label(text="Enter your Anthropic API key:")
            col.prop(ai, "api_key", text="")
            col.separator()
            col.scale_y = 0.8
            col.label(text="console.anthropic.com/settings/keys")
            return

        # Messages
        col = layout.column(align=True)

        if not scene.ai_messages:
            box = col.box()
            c = box.column(align=True)
            c.scale_y = 0.9
            c.label(text="What would you like to create?")
            c.separator()
            c.label(text="Try:")
            c.label(text="  'Add a red cube'")
            c.label(text="  'Create a donut'")
            c.label(text="  'Make a simple room'")
        else:
            for msg in list(scene.ai_messages)[-6:]:
                draw_message(col, msg.role, msg.content)

        # Processing
        if ai.is_processing:
            box = col.box()
            box.label(text="Thinking...", icon='SORTTIME')

        layout.separator()

        # Input
        row = layout.row(align=True)
        row.prop(ai, "chat_input", text="")

        row = layout.row(align=True)
        row.scale_y = 1.3

        sub = row.row(align=True)
        sub.enabled = not ai.is_processing and bool(ai.chat_input.strip())
        sub.operator("ai.send_message", text="Send", icon='PLAY')

        row.operator("ai.clear_chat", text="", icon='TRASH')


class VIEW3D_PT_ai_settings(Panel):
    """Settings."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Settings"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        ai = context.scene.ai_assistant
        layout.prop(ai, "api_key")
        layout.prop(ai, "model")


classes = (
    VIEW3D_PT_ai_chat,
    VIEW3D_PT_ai_settings,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
