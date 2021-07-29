import bpy
from bpy.props import *
from . mix_data import getMixCode
from ... sockets.info import toListDataType
from ... events import executionCodeChanged
from ... base_types import AnimationNode

nodeTypes = {
    "Matrix" : "Mix Matrix List",
    "Vector" : "Mix Vector List",
    "Float" : "Mix Float List",
    "Color" : "Mix Color List",
    "Euler" : "Mix Euler List",
    "Quaternion" : "Mix Quaternion List" }

class MixDataListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MixDataListNode"
    bl_label = "Mix Data List"
    dynamicLabelType = "ALWAYS"

    onlySearchTags = True
    searchTags = [(tag, {"dataType" : repr(type)}) for type, tag in nodeTypes.items()]

    dataType = StringProperty(update = AnimationNode.refresh, default = "Float")

    repeat = BoolProperty(name = "Repeat", default = False,
        description = "Repeat the factor for values above and below 0-1", update = executionCodeChanged)

    def create(self):
        listDataType = toListDataType(self.dataType)
        self.newInput("Float", "Factor", "factor")
        self.newInput(listDataType, listDataType, "dataList")
        self.newInput("Interpolation", "Interpolation", "interpolation").defaultDrawType = "PROPERTY_ONLY"
        self.newOutput(self.dataType, "Result", "result")

    def draw(self, layout):
        layout.prop(self, "repeat")

    def drawLabel(self):
        return nodeTypes[self.dataType]

    def getExecutionCode(self):
        yield "length = len(dataList)"
        yield "if length > 0:"
        yield "    f = (factor{}) * (length - 1)".format(" % 1" if self.repeat else "")
        yield "    before = dataList[max(min(math.floor(f), length - 1), 0)]"
        yield "    after = dataList[max(min(math.ceil(f), length - 1), 0)]"
        yield "    influence = interpolation(f % 1)"
        yield "    " + getMixCode(self.dataType, "before", "after", "influence", "result")
        yield "else: result = self.outputs[0].getDefaultValue()"

    def getUsedModules(self):
        return ["math"]
