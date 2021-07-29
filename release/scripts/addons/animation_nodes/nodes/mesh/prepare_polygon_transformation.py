import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures import Vector3DList, PolygonIndicesList, Matrix4x4List
from . c_utils import transformPolygons, separatePolygons, extractPolygonTransforms

class PreparePolygonTransformationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_PreparePolygonTransformationNode"
    bl_label = "Prepare Polygon Transformation"

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Vertices", "inVertices")
        self.newInput("Polygon Indices List", "Polygon Indices", "inPolygonIndices")

        self.newOutput("Vector List", "Vertices", "outVertices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "outPolygonIndices")
        self.newOutput("Matrix List", "Transformations")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, oldVertices, oldPolygonIndices):
        self.errorMessage = ""
        if len(oldPolygonIndices) != 0 and oldPolygonIndices.getMaxIndex() >= len(oldVertices):
            self.errorMessage = "Invalid polygon indices"
            return Matrix4x4List(), Vector3DList(), PolygonIndicesList()

        newVertices, newPolygons = separatePolygons(oldVertices, oldPolygonIndices)
        transforms, invertedTransforms = extractPolygonTransforms(newVertices, newPolygons, calcInverted = True)
        transformPolygons(newVertices, newPolygons, invertedTransforms)

        return newVertices, newPolygons, transforms
