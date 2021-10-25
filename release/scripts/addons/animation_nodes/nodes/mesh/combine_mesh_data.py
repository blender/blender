import bpy
from bpy.props import *
from ... data_structures import Mesh
from ... events import propertyChanged
from ... base_types import AnimationNode

class CombineMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CombineMeshNode"
    bl_label = "Combine Mesh"
    errorHandlingType = "EXCEPTION"

    skipValidation = BoolProperty(name = "Skip Validation", default = False,
        description = "Skipping validation might cause Blender to crash when the data is not valid.",
        update = propertyChanged)

    def create(self):
        self.newInput("Vector List", "Vertex Locations", "vertexLocations", dataIsModified = True)
        self.newInput("Edge Indices List", "Edge Indices", "edgeIndices", dataIsModified = True)
        self.newInput("Polygon Indices List", "Polygon Indices", "polygonIndices", dataIsModified = True)
        self.newOutput("an_MeshSocket", "Mesh", "meshData")

    def draw(self, layout):
        if self.skipValidation:
            layout.label("Validation skipped", icon = "INFO")

    def drawAdvanced(self, layout):
        layout.prop(self, "skipValidation")

    def execute(self, vertexLocations, edgeIndices, polygonIndices):
        try:
            return Mesh(vertexLocations, edgeIndices, polygonIndices, skipValidation = self.skipValidation)
        except Exception as e:
            self.raiseErrorMessage(str(e))
