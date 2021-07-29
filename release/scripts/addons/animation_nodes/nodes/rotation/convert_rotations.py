import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

conversionTypeItems = [
    ("QUATERNION_TO_EULER", "Quaternion to Euler", "", "NONE", 0),
    ("EULER_TO_QUATERNION", "Euler to Quaternion", "", "NONE", 1),
    ("QUATERNION_TO_MATRIX", "Quaternion to Matrix", "", "NONE", 2),
    ("MATRIX_TO_QUATERNION", "Matrix to Quaternion", "", "NONE", 3),
    ("EULER_TO_MATRIX", "Euler to Matrix", "", "NONE", 4),
    ("MATRIX_TO_EULER", "Matrix to Euler", "", "NONE", 5),
    ("QUATERNION_TO_AXIS_ANGLE", "Quaternion to Axis Angle", "", "NONE", 6),
    ("AXIS_ANGLE_TO_QUATERNION", "Axis Angle to Quaternion", "", "NONE", 7) ]

class ConvertRotationsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConvertRotationsNode"
    bl_label = "Convert Rotations"
    bl_width_default = 160
    dynamicLabelType = "ALWAYS"

    onlySearchTags = True
    searchTags = [(name, {"conversionType" : repr(type)}) for type, name, _,_,_ in conversionTypeItems]

    conversionType = EnumProperty(name = "Conversion Type", default = "QUATERNION_TO_EULER",
        items = conversionTypeItems, update = AnimationNode.refresh)

    useDegree = BoolProperty(name = "Use Degree", default = False, update = executionCodeChanged)

    def create(self):
        if self.conversionType == "QUATERNION_TO_EULER":
            self.newInput("Quaternion", "Quaternion", "quaternion")
            self.newOutput("Euler", "Euler", "euler")
        if self.conversionType == "EULER_TO_QUATERNION":
            self.newInput("Euler", "Euler", "euler")
            self.newOutput("Quaternion", "Quaternion", "quaternion")

        if self.conversionType == "QUATERNION_TO_MATRIX":
            self.newInput("Quaternion", "Quaternion", "quaternion")
            self.newOutput("Matrix", "Matrix", "matrix")
        if self.conversionType == "MATRIX_TO_QUATERNION":
            self.newInput("Matrix", "Matrix", "matrix")
            self.newOutput("Quaternion", "Quaternion", "quaternion")

        if self.conversionType == "EULER_TO_MATRIX":
            self.newInput("Euler", "Euler", "euler")
            self.newOutput("Matrix", "Matrix", "matrix")
        if self.conversionType == "MATRIX_TO_EULER":
            self.newInput("Matrix", "Matrix", "matrix")
            self.newOutput("Euler", "Euler", "euler")

        if self.conversionType == "QUATERNION_TO_AXIS_ANGLE":
            self.newInput("Quaternion", "Quaternion", "quaternion")
            self.newOutput("Vector", "Axis", "axis")
            self.newOutput("Float", "Angle", "angle")
        if self.conversionType == "AXIS_ANGLE_TO_QUATERNION":
            self.newInput("Vector", "Axis", "axis")
            self.newInput("Float", "Angle", "angle")
            self.newOutput("Quaternion", "Quaternion", "quaternion")

        self.inputs[0].defaultDrawType = "PREFER_PROPERTY"

    def draw(self, layout):
        layout.prop(self, "conversionType", text = "")
        if "ANGLE" in self.conversionType: layout.prop(self, "useDegree")

    def drawLabel(self):
        for item in conversionTypeItems:
            if self.conversionType == item[0]: return item[1]

    def getExecutionCode(self):
        if self.conversionType == "QUATERNION_TO_EULER":
            return "euler = quaternion.to_euler('XYZ')"
        if self.conversionType == "EULER_TO_QUATERNION":
            return "quaternion = euler.to_quaternion()"

        if self.conversionType == "QUATERNION_TO_MATRIX":
            return "matrix = quaternion.to_matrix().to_4x4()"
        if self.conversionType == "MATRIX_TO_QUATERNION":
            return "quaternion = matrix.to_quaternion()"

        if self.conversionType == "EULER_TO_MATRIX":
            return "matrix = euler.to_matrix().to_4x4()"
        if self.conversionType == "MATRIX_TO_EULER":
            return "euler = matrix.to_euler('XYZ')"

        if self.conversionType == "QUATERNION_TO_AXIS_ANGLE":
            if self.useDegree: return "axis, angle = quaternion.axis, math.degrees(quaternion.angle)"
            else: return "axis, angle = quaternion.to_axis_angle()"
        if self.conversionType == "AXIS_ANGLE_TO_QUATERNION":
            if self.useDegree: return "quaternion = Quaternion(axis, math.radians(angle))"
            else: return "quaternion = Quaternion(axis, angle)"

    def getUsedModules(self):
        return ["math"]
