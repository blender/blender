# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Operators for Blender AI Assistant."""

import bpy
import time
from bpy.types import Operator

from . import ai_backend


class AI_OT_send_message(Operator):
    """Send message to AI"""
    bl_idname = "ai.send_message"
    bl_label = "Send"

    def execute(self, context):
        scene = context.scene
        settings = scene.ai_assistant

        msg = settings.chat_input.strip()
        if not msg:
            return {'CANCELLED'}

        if not settings.api_key:
            self.report({'ERROR'}, "Set API key first")
            return {'CANCELLED'}

        if settings.is_processing:
            return {'CANCELLED'}

        # Add user message
        m = scene.ai_messages.add()
        m.role = "user"
        m.content = msg
        m.timestamp = time.time()

        settings.chat_input = ""
        settings.is_processing = True

        # Build history
        messages = [{"role": x.role, "content": x.content} for x in scene.ai_messages]

        # Send
        ai_backend.send_message(
            settings.api_key,
            settings.model,
            messages,
            self._on_response
        )

        return {'FINISHED'}

    def _on_response(self, result):
        scene = bpy.context.scene
        settings = scene.ai_assistant

        m = scene.ai_messages.add()
        m.role = "assistant"
        m.content = result if isinstance(result, str) else str(result)
        m.timestamp = time.time()

        settings.is_processing = False

        # Redraw
        for win in bpy.context.window_manager.windows:
            for area in win.screen.areas:
                area.tag_redraw()


class AI_OT_clear_chat(Operator):
    """Clear chat"""
    bl_idname = "ai.clear_chat"
    bl_label = "Clear"

    def execute(self, context):
        context.scene.ai_messages.clear()
        return {'FINISHED'}


classes = (
    AI_OT_send_message,
    AI_OT_clear_chat,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
