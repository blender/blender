# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""GUI for Blender AI Assistant."""

import bpy
from bpy.types import Panel, UIList
import textwrap


def draw_message(layout, role, content):
    """Draw a single chat message."""
    box = layout.box()

    # Header with role
    row = box.row()
    if role == "user":
        row.label(text="You", icon='USER')
    else:
        row.label(text="Claude", icon='LIGHT')

    # Content
    col = box.column(align=True)
    col.scale_y = 0.85

    # Word wrap
    for line in content.split('\n'):
        wrapped = textwrap.wrap(line, width=45) or ['']
        for wrap_line in wrapped:
            col.label(text=wrap_line)


class VIEW3D_PT_ai_chat(Panel):
    """AI Chat panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Chat"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        settings = scene.ai_assistant

        # API key setup
        if not settings.api_key:
            box = layout.box()
            col = box.column(align=True)
            col.label(text="Enter API Key to start", icon='KEY_HLT')
            col.prop(settings, "api_key", text="")
            col.separator()
            col.scale_y = 0.8
            col.label(text="Get yours at:")
            col.label(text="console.anthropic.com")
            return

        # Chat messages
        col = layout.column(align=True)

        if len(scene.ai_messages) == 0:
            box = col.box()
            box_col = box.column(align=True)
            box_col.scale_y = 0.9
            box_col.label(text="Hi! I'm your Blender assistant.")
            box_col.label(text="")
            box_col.label(text="Try asking me to:")
            box_col.label(text="  Create a red cube")
            box_col.label(text="  Add a sun light")
            box_col.label(text="  Make a simple scene")
        else:
            # Show last few messages
            messages = list(scene.ai_messages)[-6:]
            for msg in messages:
                draw_message(col, msg.role, msg.content)

        # Thinking indicator
        if settings.is_processing:
            box = col.box()
            row = box.row()
            row.alignment = 'CENTER'
            row.label(text="Thinking...", icon='SORTTIME')

        # Input area
        layout.separator()

        col = layout.column(align=True)
        col.prop(settings, "chat_input", text="")

        row = col.row(align=True)
        row.scale_y = 1.4
        sub = row.row(align=True)
        sub.enabled = not settings.is_processing
        sub.operator("ai.send_message", text="Send", icon='PLAY')
        row.operator("ai.clear_chat", text="", icon='TRASH')


class VIEW3D_PT_ai_settings(Panel):
    """Settings panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Settings"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        settings = context.scene.ai_assistant

        layout.prop(settings, "api_key", text="API Key")
        layout.prop(settings, "model", text="Model")


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
