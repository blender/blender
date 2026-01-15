# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""GUI for Blender AI Assistant."""

import bpy
from bpy.types import Panel, UIList
import textwrap


class AI_UL_messages(UIList):
    """Chat message list."""

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            col = layout.column(align=True)

            # Role indicator
            if item.role == "user":
                col.label(text="You:", icon='USER')
            else:
                col.label(text="AI:", icon='LIGHT')

            # Wrap long messages
            wrapped = textwrap.wrap(item.content, width=40)
            for line in wrapped[:8]:  # Max 8 lines preview
                col.label(text=line)
            if len(wrapped) > 8:
                col.label(text="...")


class VIEW3D_PT_ai_assistant(Panel):
    """Main AI Assistant panel."""
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Assistant"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        settings = scene.ai_assistant

        # Check for API key
        if not settings.api_key:
            box = layout.box()
            col = box.column(align=True)
            col.label(text="API Key Required", icon='ERROR')
            col.prop(settings, "api_key", text="")
            col.label(text="Get key from console.anthropic.com")
            return

        # Messages
        if len(scene.ai_messages) > 0:
            layout.template_list(
                "AI_UL_messages", "",
                scene, "ai_messages",
                settings, "chat_message_index",
                rows=8
            )
        else:
            box = layout.box()
            col = box.column(align=True)
            col.scale_y = 0.9
            col.label(text="Ask me anything about Blender!")
            col.label(text="I can create objects, modify scenes,")
            col.label(text="write scripts, and more.")

        # Processing indicator
        if settings.is_processing:
            row = layout.row()
            row.alignment = 'CENTER'
            row.label(text="Thinking...", icon='SORTTIME')

        # Input
        layout.separator()
        col = layout.column(align=True)
        col.prop(settings, "chat_input", text="")

        row = col.row(align=True)
        row.scale_y = 1.3
        row.operator("ai.send_message", text="Send", icon='EXPORT')
        row.operator("ai.clear_chat", text="", icon='X')


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

        col = layout.column()
        col.prop(settings, "api_key", text="API Key")
        col.prop(settings, "model", text="Model")


classes = (
    AI_UL_messages,
    VIEW3D_PT_ai_assistant,
    VIEW3D_PT_ai_settings,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
