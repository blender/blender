import bpy
from bpy.props import *
from ... events import executionCodeChanged
from . c_utils import vectorsToEulers, eulersToVectors
from ... base_types import AnimationNode, VectorizedSocket

conversionTypeItems = [
    ("VECTOR_TO_EULER", "Vector to Euler", "", "NONE", 0),
    ("EULER_TO_VECTOR", "Euler to Vector", "", "NONE", 1)]

class ConvertVectorAndEulerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConvertVectorAndEulerNode"
    bl_label = "Convert Vector and Euler"
    dynamicLabelType = "ALWAYS"

    onlySearchTags = True
    searchTags = [(name, {"conversionType" : repr(type)}) for type, name, _,_,_ in conversionTypeItems]

    useDegree = BoolProperty(name = "Use Degree", default = False,
        update = executionCodeChanged)

    conversionType = EnumProperty(name = "Conversion Type", default = "EULER_TO_VECTOR",
        update = AnimationNode.refresh, items = conversionTypeItems)

    useList = VectorizedSocket.newProperty()

    def create(self):
        if self.conversionType == "VECTOR_TO_EULER":
            self.newInput(VectorizedSocket("Vector", "useList",
                ("Vector", "vector"), ("Vectors", "vectors")))
            self.newOutput(VectorizedSocket("Euler", "useList",
                ("Euler", "euler"), ("Eulers", "eulers")))
        if self.conversionType == "EULER_TO_VECTOR":
            self.newInput(VectorizedSocket("Euler", "useList",
                ("Euler", "euler"), ("Eulers", "eulers")))
            self.newOutput(VectorizedSocket("Vector", "useList",
                ("Vector", "vector"), ("Vectors", "vectors")))
        self.inputs[0].defaultDrawType = "PROPERTY_ONLY"

    def draw(self, layout):
        layout.prop(self, "conversionType", text = "")
        layout.prop(self, "useDegree")

    def drawLabel(self):
        for item in conversionTypeItems:
            if self.conversionType == item[0]: return item[1]

    def getExecutionCode(self, required):
        if self.useList:
            if self.conversionType == "VECTOR_TO_EULER":
                return "eulers = self.vectorsToEulers(vectors)"
            elif self.conversionType == "EULER_TO_VECTOR":
                return "vectors = self.eulersToVectors(eulers)"
        else:
            if self.conversionType == "VECTOR_TO_EULER":
                if self.useDegree: return "euler = Euler(vector / 180 * math.pi, 'XYZ')"
                else: return "euler = Euler(vector, 'XYZ')"
            elif self.conversionType == "EULER_TO_VECTOR":
                if self.useDegree: return "vector = Vector(euler) * 180 / math.pi"
                else: return "vector = Vector(euler)"

    def vectorsToEulers(self, vectors):
        return vectorsToEulers(vectors, self.useDegree)

    def eulersToVectors(self, eulers):
        return eulersToVectors(eulers, self.useDegree)

    def getUsedModules(self):
        return ["math"]
