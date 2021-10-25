import bpy
from ... base_types import AnimationNode, VectorizedSocket

class EvaluateInterpolationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EvaluateInterpolationNode"
    bl_label = "Evaluate Interpolation"
    bl_width_default = 160

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Float", "useList",
            ("Position", "position", dict(minValue = 0, maxValue = 1)),
            ("Positions", "positions")))

        self.newInput("Interpolation", "Interpolation", "interpolation",
            defaultDrawType = "PROPERTY_ONLY")

        self.newOutput(VectorizedSocket("Float", "useList",
            ("Value", "value"), ("Values", "values")))

    def getExecutionCode(self, required):
        if self.useList:
            return "values = interpolation.evaluateList(positions)"
        else:
            return "value = interpolation(max(min(position, 1.0), 0.0))"
