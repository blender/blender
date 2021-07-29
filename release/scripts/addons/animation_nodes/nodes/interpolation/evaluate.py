import bpy
from ... base_types import VectorizedNode

class EvaluateInterpolationNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_EvaluateInterpolationNode"
    bl_label = "Evaluate Interpolation"
    bl_width_default = 150

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Float", "useList",
            ("Position", "position", dict(minValue = 0, maxValue = 1)),
            ("Positions", "positions"))

        self.newInput("Interpolation", "Interpolation", "interpolation", defaultDrawType = "PROPERTY_ONLY")

        self.newVectorizedOutput("Float", "useList",
            ("Value", "value"), ("Values", "values"))

    def getExecutionCode(self):
        if self.useList:
            return "values = interpolation.evaluateList(positions)"
        else:
            return "value = interpolation(max(min(position, 1.0), 0.0))"
