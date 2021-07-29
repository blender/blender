import bpy
from bpy.utils import smpte_from_frame
from ... base_types import AnimationNode

class TimecodeGeneratorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TimecodeGeneratorNode"
    bl_label = "Timecode Generator"

    def create(self):
        self.newInput("Float", "Frame", "frame")
        socket = self.newInput("Float", "Frame Rate", "frameRate", minValue = 0)
        socket.value = bpy.context.scene.render.fps
        self.newOutput("Text", "Timecode", "timecode")

    def execute(self, frame, frameRate):
        if frameRate > 0:
            return smpte_from_frame(frame, fps = frameRate)
        return smpte_from_frame(0)
