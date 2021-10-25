import bpy
from ... base_types import AnimationNode, VectorizedSocket

class FloatClampNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FloatClampNode"
    bl_label = "Clamp"
    dynamicLabelType = "HIDDEN_ONLY"

    useValueList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Float", "useValueList",
            ("Value", "value"),
            ("Values", "values", dict(dataIsModified = True))))

        self.newInput("Float", "Min", "minValue", value = 0.0)
        self.newInput("Float", "Max", "maxValue", value = 1.0)

        self.newOutput(VectorizedSocket("Float", "useValueList",
            ("Value", "outValue"), ("Values", "outValues")))

    def getExecutionCode(self, required):
        if self.useValueList:
            yield "outValues = values"
            yield "outValues.clamp(minValue, maxValue)"
        else:
            yield "outValue = min(max(value, minValue), maxValue)"

    def drawLabel(self):
        label = "clamp(min, max)"
        if self.inputs["Min"].isUnlinked:
            label = label.replace("min", str(round(self.inputs["Min"].value, 4)))
        if self.inputs["Max"].isUnlinked:
            label = label.replace("max", str(round(self.inputs["Max"].value, 4)))
        return label
