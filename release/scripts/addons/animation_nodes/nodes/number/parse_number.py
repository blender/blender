import bpy
from bpy.props import *
from ... base_types import VectorizedNode

class ParseNumberNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ParseNumberNode"
    bl_label = "Parse Number"

    parsingSuccessfull = BoolProperty()
    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Text", "useList",
            ("Text", "text", dict(value = "0")), ("Texts", "texts"))
        self.newVectorizedOutput("Float", "useList",
            ("Number", "number"), ("Numbers", "numbers"))

    def draw(self, layout):
        if not self.parsingSuccessfull:
            layout.label("Parsing Error", icon = "ERROR")

    def getExecutionCode(self):
        if self.useList:
            yield "try:"
            yield "    numbers = DoubleList.fromValues(float(text) for text in texts)"
            yield "    self.parsingSuccessfull = True"
            yield "except:"
            yield "    numbers = DoubleList()"
            yield "    self.parsingSuccessfull = False"
        else:
            yield "try:"
            yield "    number = float(text)"
            yield "    self.parsingSuccessfull = True"
            yield "except:"
            yield "    number = 0"
            yield "    self.parsingSuccessfull = False"
