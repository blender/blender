import bpy
from bpy.props import *
from ... base_types import AnimationNode

class CharacterPropertiesOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CharacterPropertiesOutputNode"
    bl_label = "Character Properties Output"

    allowNegativeIndex = BoolProperty(default = True)

    def create(self):
        self.newInput("Object", "Text Object", "object", defaultDrawType = "PROPERTY_ONLY")

        self.newInput("Integer", "Start", "start", value = 0)
        self.newInput("Integer", "End", "end", value = -1)

        self.newInput("Integer", "Material Index", "materialIndex", value = 0)
        self.newInput("Boolean", "Bold", "bold", value = False)
        self.newInput("Boolean", "Italic", "italic", value = False)
        self.newInput("Boolean", "Underline", "underline", value = False)
        self.newInput("Boolean", "Small Caps", "smallCaps", value = False)

        for socket in self.inputs[3:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False
        for socket in self.inputs[4:]:
            socket.hide = True

        self.newOutput("Object", "Object", "object")

    def drawAdvanced(self, layout):
        layout.prop(self, "allowNegativeIndex")

    def getExecutionCode(self):
        lines = []

        if any([socket.isUsed for socket in self.inputs[3:]]):
            lines.append("if getattr(object, 'type', '') == 'FONT':")
            lines.append("    textObject = object.data")

            if self.allowNegativeIndex: lines.append("    s, e = start, end")
            else: lines.append("    s, e = max(0, start), max(0, end)")

            lines.append("    for char in textObject.body_format[s:e]:")
        if self.inputs["Material Index"].isUsed: lines.append(" "*8 + "char.material_index = materialIndex")
        if self.inputs["Bold"].isUsed: lines.append(" "*8 + "char.use_bold = bold")
        if self.inputs["Italic"].isUsed: lines.append(" "*8 + "char.use_italic = italic")
        if self.inputs["Underline"].isUsed: lines.append(" "*8 + "char.use_underline = underline")
        if self.inputs["Small Caps"].isUsed: lines.append(" "*8 + "char.use_small_caps = smallCaps")

        return lines
