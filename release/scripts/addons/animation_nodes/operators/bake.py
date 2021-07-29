import bpy
from bpy.props import *
from .. preferences import getPreferences

class BakeAnimation(bpy.types.Operator):
    bl_idname = "an.bake_to_keyframes"
    bl_label = "Bake to Keyframes"
    bl_description = "Playback animation and make keyframes (only supported nodes)"

    startFrame = IntProperty(default = 1)
    endFrame = IntProperty(default = 250)

    def invoke(self, context, event):
        context.window_manager.modal_handler_add(self)
        self.timer = context.window_manager.event_timer_add(0.001, context.window)

        getPreferences().executionCode.type = "BAKE"

        self.scene = context.scene
        self.scene.frame_set(self.startFrame)
        return {"RUNNING_MODAL"}

    def modal(self, context, event):
        if event.type in ("RIGHTMOUSE", "ESC"):
            return self.finish()

        currentFrame = self.scene.frame_current

        if event.type == "TIMER":
            self.scene.frame_set(currentFrame + 1)
            self.scene.update()

        if self.scene.frame_current == self.endFrame:
            return self.finish()

        return {"RUNNING_MODAL"}

    def finish(self):
        getPreferences().executionCode.type = "DEFAULT"
        bpy.context.window_manager.event_timer_remove(self.timer)
        return {"FINISHED"}
