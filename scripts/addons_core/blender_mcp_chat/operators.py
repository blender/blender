# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Operators for Blender AI Assistant."""

import bpy
import time
from bpy.types import Operator
from bpy.props import StringProperty

from . import ai_backend


class AI_OT_send_message(Operator):
    """Send message to AI assistant"""
    bl_idname = "ai.send_message"
    bl_label = "Send"
    bl_description = "Send your message to the AI assistant"

    def execute(self, context):
        scene = context.scene
        settings = scene.ai_assistant

        message = settings.chat_input.strip()
        if not message:
            return {'CANCELLED'}

        if not settings.api_key:
            self.report({'ERROR'}, "Please set your API key in Settings")
            return {'CANCELLED'}

        if settings.is_processing:
            self.report({'WARNING'}, "Please wait for the current response")
            return {'CANCELLED'}

        # Add user message
        msg = scene.ai_messages.add()
        msg.role = "user"
        msg.content = message
        msg.timestamp = time.time()

        # Clear input and set processing
        settings.chat_input = ""
        settings.is_processing = True

        # Build message history for API
        messages = []
        for m in scene.ai_messages:
            messages.append({"role": m.role, "content": m.content})

        # Send to AI
        ai_backend.send_message_async(
            api_key=settings.api_key,
            model=settings.model,
            messages=messages,
            system_prompt=settings.system_prompt,
            scene_name=scene.name,
            callback=lambda result: self._handle_response(context, result)
        )

        # Force UI redraw
        for area in context.screen.areas:
            area.tag_redraw()

        return {'FINISHED'}

    def _handle_response(self, context, result):
        """Handle AI response (called from main thread)."""
        scene = bpy.context.scene
        settings = scene.ai_assistant

        # Add assistant message
        msg = scene.ai_messages.add()
        msg.role = "assistant"
        msg.content = result.get("content", "No response")
        msg.timestamp = time.time()

        settings.is_processing = False
        settings.chat_message_index = len(scene.ai_messages) - 1

        # Force UI redraw
        for window in bpy.context.window_manager.windows:
            for area in window.screen.areas:
                area.tag_redraw()


class AI_OT_clear_chat(Operator):
    """Clear chat history"""
    bl_idname = "ai.clear_chat"
    bl_label = "Clear"
    bl_description = "Clear all messages"

    def execute(self, context):
        context.scene.ai_messages.clear()
        context.scene.ai_assistant.chat_message_index = 0
        return {'FINISHED'}


class AI_OT_run_code(Operator):
    """Run Python code in Blender"""
    bl_idname = "ai.run_code"
    bl_label = "Run Code"
    bl_description = "Execute Python code"
    bl_options = {'REGISTER', 'UNDO'}

    code: StringProperty(name="Code", default="")

    def execute(self, context):
        if not self.code:
            return {'CANCELLED'}

        result = ai_backend.execute_python(self.code)

        if result.get("success"):
            self.report({'INFO'}, "Code executed")
        else:
            self.report({'ERROR'}, result.get("error", "Unknown error"))

        return {'FINISHED'}


classes = (
    AI_OT_send_message,
    AI_OT_clear_chat,
    AI_OT_run_code,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
