import bpy
from bpy.props import *
from ... base_types import AnimationNode

conversionTypeItems = [
    ("DEGREE_TO_RADIAN", "Degree to Radian", "", "NONE", 0),
    ("RADIAN_TO_DEGREE", "Radian to Degree", "", "NONE", 1)]

class ConvertAngleNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConvertAngleNode"
    bl_label = "Convert Angle"

    searchTags = [(name, {"conversionType" : repr(type)}) for type, name, *_ in conversionTypeItems]

    conversionType = EnumProperty(name = "Conversion Type", default = "DEGREE_TO_RADIAN",
        items = conversionTypeItems, update = AnimationNode.refresh)

    def create(self):
        if self.conversionType == "DEGREE_TO_RADIAN":
            self.newInput("Float", "Degree", "inAngle")
            self.newOutput("Float", "Radian", "outAngle")
        elif self.conversionType == "RADIAN_TO_DEGREE":
            self.newInput("Float", "Radian", "inAngle")
            self.newOutput("Float", "Degree", "outAngle")

    def draw(self, layout):
        layout.prop(self, "conversionType", text = "")

    def getExecutionCode(self):
        if self.conversionType == "DEGREE_TO_RADIAN": return "outAngle = inAngle / 180 * math.pi"
        if self.conversionType == "RADIAN_TO_DEGREE": return "outAngle = inAngle * 180 / math.pi"

    def getUsedModules(self):
        return ["math"]
