import bpy
from bpy.props import *
from . c_utils import createEdges
from ... base_types import AnimationNode
from ... data_structures import Vector3DList, EdgeIndicesList

class CreateEdgesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CreateEdgesNode"
    bl_label = "Create Edges"
    bl_width_default = 150

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Edge Starts", "edgeStarts")
        self.newInput("Vector List", "Edge Ends", "edgeEnds")

        self.newOutput("Vector List", "Vectors", "vectors")
        self.newOutput("Edge Indices List", "Edge Indices List", "edgeIndicesList")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, edgeStarts, edgeEnds):
        self.errorMessage = ""
        if len(edgeStarts) != len(edgeEnds):
            self.errorMessage = "List lengths not equal"
            return Vector3DList(), EdgeIndicesList()
        return createEdges(edgeStarts, edgeEnds)
