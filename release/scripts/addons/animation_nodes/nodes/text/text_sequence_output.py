import bpy
from ... base_types import AnimationNode

class TextSequenceOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TextSequenceOutputNode"
    bl_label = "Text Sequence Output"
    bl_width_default = 160
    errorHandlingType = "MESSAGE"

    def create(self):
        self.newInput("Sequence", "Sequence", "sequence", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Text", "Text", "text")
        self.newInput("Integer", "Size", "size", value = 200)
        self.newInput("Boolean", "Shadow", "shadow", value = False)
        self.newInput("Text", "X Align", "xAlign", value = "CENTER")
        self.newInput("Text", "Y Align", "yAlign", value = "BOTTOM")
        self.newInput("Float", "X Location", "xLocation", value = 0.5)
        self.newInput("Float", "Y Location", "yLocation", value = 0.0)
        self.newInput("Float", "Wrap Width", "wrapWidth", value = 0.0)
        self.newOutput("Sequence", "Sequence", "sequence")

        for socket in self.inputs[1:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False
        for socket in self.inputs[4:]:
            socket.hide = True

    def getExecutionCode(self, required):
        yield "if getattr(sequence, 'type', '') == 'TEXT':"
        yield "    pass"

        s = self.inputs
        if s["Text"].isUsed:        yield "    sequence.text = text"
        if s["Size"].isUsed:        yield "    sequence.font_size = size"
        if s["Shadow"].isUsed:      yield "    sequence.use_shadow = shadow"
        if s["X Align"].isUsed:     yield "    self.setXAlignment(sequence, xAlign)"
        if s["Y Align"].isUsed:     yield "    self.setYAlignment(sequence, yAlign)"
        if s["X Location"].isUsed:  yield "    sequence.location[0] = xLocation"
        if s["Y Location"].isUsed:  yield "    sequence.location[1] = yLocation"
        if s["Wrap Width"].isUsed:  yield "    sequence.wrap_width = wrapWidth"

    def setXAlignment(self, sequence, align):
        alignTypes = ("LEFT", "CENTER", "RIGHT")
        if align in alignTypes:
            sequence.align_x = align
        else:
            self.setErrorMessage("X Align must be in " + str(alignTypes))

    def setYAlignment(self, sequence, align):
        alignTypes = ("TOP", "CENTER", "BOTTOM")
        if align in alignTypes:
            sequence.align_y = align
        else:
            self.setErrorMessage("Y Align must be in " + str(alignTypes))

    def getBakeCode(self):
        yield "if getattr(sequence, 'type', '') == 'TEXT':"
        yield "    pass"
        s = self.inputs
        if s["Size"].isUsed:        yield "    sequence.keyframe_insert('font_size')"
        if s["Shadow"].isUsed:      yield "    sequence.keyframe_insert('use_shadow')"
        if s["X Align"].isUsed:     yield "    sequence.keyframe_insert('align_x')"
        if s["Y Align"].isUsed:     yield "    sequence.keyframe_insert('align_y')"
        if s["X Location"].isUsed:  yield "    sequence.keyframe_insert('location', index = 0)"
        if s["Y Location"].isUsed:  yield "    sequence.keyframe_insert('location', index = 1)"
        if s["Wrap Width"].isUsed:  yield "    sequence.keyframe_insert('wrap_width')"
