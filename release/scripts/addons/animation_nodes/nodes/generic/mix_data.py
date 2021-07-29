import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

nodeTypes = {
    "Matrix" : "Mix Matrices",
    "Vector" : "Mix Vectors",
    "Float" : "Mix Floats",
    "Color" : "Mix Colors",
    "Euler" : "Mix Eulers",
    "Quaternion" : "Mix Quaternions" }

class MixDataNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MixDataNode"
    bl_label = "Mix"
    dynamicLabelType = "ALWAYS"

    onlySearchTags = True
    searchTags = [(tag, {"dataType" : repr(type)}) for type, tag in nodeTypes.items()]

    dataType = StringProperty(default = "Float", update = AnimationNode.refresh)
    
    clampFactor = BoolProperty(name = "Clamp Factor",
        description = "Clamp factor between 0 and 1",
        default = False, update = executionCodeChanged)

    def create(self):
        self.newInput("Float", "Factor", "factor")
        self.newInput(self.dataType, "A", "a")
        self.newInput(self.dataType, "B", "b")
        self.newOutput(self.dataType, "Result", "result")

    def draw(self, layout):
        layout.prop(self, "clampFactor")

    def drawLabel(self):
        return nodeTypes[self.outputs[0].dataType]

    def getExecutionCode(self):
        if self.clampFactor:
            yield "f = min(max(factor, 0.0), 1.0)"
        else:
            yield "f = factor"
        yield getMixCode(self.dataType, "a", "b", "f", "result")

def getMixCode(dataType, mix1 = "a", mix2 = "b", factor = "f", result = "result"):
    if dataType in ("Float", "Vector", "Quaternion"): return "{} = {} * (1 - {}) + {} * {}".format(result, mix1, factor, mix2, factor)
    if dataType == "Matrix": return "{} = {}.lerp({}, {})".format(result, mix1, mix2, factor)
    if dataType == "Color": return "{} = [v1 * (1 - {}) + v2 * {} for v1, v2 in zip({}, {})]".format(result, factor, factor, mix1, mix2)
    if dataType == "Euler": return "{} = animation_nodes.utils.math.mixEulers({}, {}, {})".format(result, mix1, mix2, factor)
