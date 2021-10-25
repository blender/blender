import bpy
from ... base_types import AnimationNode, VectorizedSocket

class InvertBooleanNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InvertBooleanNode"
    bl_label = "Invert Boolean"

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Boolean", "useList",
            ("Input", "input"),
            ("Input", "input", dict(dataIsModified = True))))

        self.newOutput(VectorizedSocket("Boolean", "useList",
            ("Output", "output"),
            ("Output", "output")))

    def getExecutionCode(self, required):
        if self.useList:
            yield "input.invertAll()"
            yield "output = input"
        else:
            yield "output = not input"
