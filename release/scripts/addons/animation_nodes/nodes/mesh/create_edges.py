import bpy
from . c_utils import createEdges
from ... base_types import AnimationNode

class CreateEdgesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CreateEdgesNode"
    bl_label = "Create Edges"
    bl_width_default = 140
    errorHandlingType = "EXCEPTION"

    def create(self):
        self.newInput("Vector List", "Edge Starts", "edgeStarts")
        self.newInput("Vector List", "Edge Ends", "edgeEnds")

        self.newOutput("Vector List", "Vectors", "vectors")
        self.newOutput("Edge Indices List", "Edge Indices List", "edgeIndicesList")

    def execute(self, edgeStarts, edgeEnds):
        if len(edgeStarts) != len(edgeEnds):
            self.raiseErrorMessage("List lengths not equal")
        return createEdges(edgeStarts, edgeEnds)
