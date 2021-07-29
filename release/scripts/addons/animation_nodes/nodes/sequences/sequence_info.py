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

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        yield "if sequence is not None:"
        if isLinked["opacity"]: yield "    opacity = sequence.blend_alpha"
        if isLinked["blendType"]: yield "    blendType = sequence.blend_type"
        if isLinked["channel"]: yield "    channel = sequence.channel"
        if isLinked["effectFader"]: yield "    effectFader = sequence.effect_fader"

        if isLinked["totalDuration"]: yield "    totalDuration = sequence.frame_duration"
        if isLinked["finalDuration"]: yield "    finalDuration = sequence.frame_final_duration"
        if isLinked["finalStartFrame"]: yield "    finalStartFrame = sequence.frame_final_start"
        if isLinked["finalEndFrame"]: yield "    finalEndFrame = sequence.frame_final_end"
        if isLinked["startOffset"]: yield "    startOffset = sequence.frame_offset_start"
        if isLinked["endOffset"]: yield "    endOffset = sequence.frame_offset_end"
        if isLinked["startFrame"]: yield "    startFrame = sequence.frame_start"
        if isLinked["stillFrameStart"]: yield "    stillFrameStart = sequence.frame_still_start"
        if isLinked["stillFrameEnd"]: yield "    stillFrameEnd = sequence.frame_still_end"

        if isLinked["lock"]: yield "    lock = sequence.lock"
        if isLinked["mute"]: yield "    mute = sequence.mute"
        if isLinked["name"]: yield "    name = sequence.name"
        if isLinked["select"]: yield "    select = sequence.select"
        if isLinked["speedFactor"]: yield "    speedFactor = sequence.speed_factor"
        if isLinked["type"]: yield "    type = sequence.type"
        if isLinked["useDefaultFade"]: yield "    useDefaultFade = sequence.use_default_fade"
        if isLinked["useLinearModifiers"]: yield "    useLinearModifiers = sequence.use_linear_modifiers"

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
