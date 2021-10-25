import bpy
from ... base_types import AnimationNode

class SequenceInfoNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SequenceInfoNode"
    bl_label = "Sequence Info"

    def create(self):
        self.newInput("Sequence", "Sequence", "sequence", defaultDrawType = "PROPERTY_ONLY")

        self.newOutput("Text", "Name", "name")
        self.newOutput("Text", "Type", "type")
        self.newOutput("Integer", "Channel", "channel")
        self.newOutput("Integer", "Final Duration", "finalDuration")
        self.newOutput("Integer", "Final Start Frame", "finalStartFrame")
        self.newOutput("Integer", "Final End Frame", "finalEndFrame")

        self.newOutput("Float", "Opacity", "opacity")
        self.newOutput("Text", "Blend Type", "blendType")
        self.newOutput("Float", "Effect Fader", "effectFader")

        self.newOutput("Integer", "Start Frame", "startFrame")
        self.newOutput("Integer", "Start Offset", "startOffset")
        self.newOutput("Integer", "End Offset", "endOffset")
        self.newOutput("Integer", "Total Duration", "totalDuration")
        self.newOutput("Integer", "Still Frame Start", "stillFrameStart")
        self.newOutput("Integer", "Still Frame End", "stillFrameEnd")

        self.newOutput("Boolean", "Lock", "lock")
        self.newOutput("Boolean", "Mute", "mute")
        self.newOutput("Boolean", "Select", "select")
        self.newOutput("Float", "Speed Factor", "speedFactor")
        self.newOutput("Boolean", "Use Default Fade", "useDefaultFade")
        self.newOutput("Boolean", "Use Linear Modifiers", "useLinearModifiers")

        for socket in self.outputs[6:]:
            socket.hide = True

    def getExecutionCode(self, required):
        if len(required) == 0:
            return

        yield "if sequence is not None:"
        if "opacity" in required: yield "    opacity = sequence.blend_alpha"
        if "blendType" in required: yield "    blendType = sequence.blend_type"
        if "channel" in required: yield "    channel = sequence.channel"
        if "effectFader" in required: yield "    effectFader = sequence.effect_fader"

        if "totalDuration" in required: yield "    totalDuration = sequence.frame_duration"
        if "finalDuration" in required: yield "    finalDuration = sequence.frame_final_duration"
        if "finalStartFrame" in required: yield "    finalStartFrame = sequence.frame_final_start"
        if "finalEndFrame" in required: yield "    finalEndFrame = sequence.frame_final_end"
        if "startOffset" in required: yield "    startOffset = sequence.frame_offset_start"
        if "endOffset" in required: yield "    endOffset = sequence.frame_offset_end"
        if "startFrame" in required: yield "    startFrame = sequence.frame_start"
        if "stillFrameStart" in required: yield "    stillFrameStart = sequence.frame_still_start"
        if "stillFrameEnd" in required: yield "    stillFrameEnd = sequence.frame_still_end"

        if "lock" in required: yield "    lock = sequence.lock"
        if "mute" in required: yield "    mute = sequence.mute"
        if "name" in required: yield "    name = sequence.name"
        if "select" in required: yield "    select = sequence.select"
        if "speedFactor" in required: yield "    speedFactor = sequence.speed_factor"
        if "type" in required: yield "    type = sequence.type"
        if "useDefaultFade" in required: yield "    useDefaultFade = sequence.use_default_fade"
        if "useLinearModifiers" in required: yield "    useLinearModifiers = sequence.use_linear_modifiers"

        yield "else:"
        yield "    opacity = 0.0"
        yield "    blendType = ''"
        yield "    channel = 1"
        yield "    effectFader = 0"
        yield "    totalDuration = finalDuration = finalStartFrame = finalEndFrame = 0"
        yield "    startOffset = endOffset = startFrame = 0"
        yield "    stillFrameStart = stillFrameEnd = 0"
        yield "    lock = False"
        yield "    mute = False"
        yield "    name = ''"
        yield "    select = False"
        yield "    speedFactor = 0.0"
        yield "    type = ''"
        yield "    useDefaultFade = False"
        yield "    useLinearModifiers = False"
