import bpy
from ... base_types import AnimationNode

class TextBlockWriterNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TextBlockWriterNode"
    bl_label = "Text Block Writer"

    def create(self):
        self.newInput("Text Block", "Text Block", "textBlock", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Text", "Text", "text")
        self.newInput("Boolean", "Enabled", "enabled", hide = True)
        self.newOutput("Text Block", "Text Block", "textBlock")

    def execute(self, textBlock, text, enabled):
        if not enabled or textBlock is None: return textBlock
        textBlock.from_string(text)
        return textBlock
