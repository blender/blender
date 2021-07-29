import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures import PolygonIndicesList
from . c_utils import polygonIndicesListFromVertexAmounts

modeItems = [
    ("INDICES", "Indices", "", "NONE", 0),
    ("VERTEX_AMOUNT", "Vertex Amount", "NONE", 1)
]

class CreatePolygonIndicesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CreatePolygonIndicesNode"
    bl_label = "Create Polygon Indices"
    bl_width_default = 150

    mode = EnumProperty(name = "Mode", default = "VERTEX_AMOUNT",
        items = modeItems, update = AnimationNode.refresh)

    useList = BoolProperty(name = "Use List", default = False,
        update = AnimationNode.refresh)

    errorMessage = StringProperty()

    def create(self):
        if self.mode == "INDICES":
            self.newInput("Integer List", "Indices", "indices")
            self.newOutput("Polygon Indices", "Polygon Indices", "polygonIndices")
        elif self.mode == "VERTEX_AMOUNT":
            if self.useList:
                self.newInput("Integer List", "Vertex Amounts", "vertexAmounts")
                self.newOutput("Polygon Indices List", "Polygon Indices List", "polygonIndicesList")
            else:
                self.newInput("Integer", "Vertex Amount", "vertexAmount", value = 3, minValue = 3)
                self.newOutput("Polygon Indices", "Polygon Indices", "polygonIndices")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "mode", text = "")
        if self.mode == "VERTEX_AMOUNT":
            row.prop(self, "useList", text = "", icon = "LINENUMBERS_ON")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def getExecutionFunctionName(self):
        if self.mode == "INDICES":
            return "execute_Indices"
        elif self.mode == "VERTEX_AMOUNT":
            if self.useList:
                return "execute_VertexAmount_List"
            else:
                return "execute_VertexAmount_Single"

    def execute_Indices(self, indices):
        if len(indices) < 3:
            self.errorMessage = "less than 3 indices"
            return (0, 1, 2)
        elif any(index < 0 for index in indices):
            self.errorMessage = "index < 0"
            return (0, 1, 2)
        else:
            self.errorMessage = ""
            return tuple(indices)

    def execute_VertexAmount_Single(self, vertexAmount):
        if vertexAmount < 3:
            self.errorMessage = "less than 3 indices"
            return (0, 1, 2)
        else:
            self.errorMessage = ""
            return tuple(range(vertexAmount))

    def execute_VertexAmount_List(self, lengths):
        try:
            self.errorMessage = ""
            return polygonIndicesListFromVertexAmounts(lengths)
        except Exception as e:
            self.errorMessage = str(e)
            return PolygonIndicesList()
