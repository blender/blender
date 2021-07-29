import bpy
from bpy.props import *
from .... base_types import AnimationNode
from .... algorithms.mesh_generation import grid

modeItems = [
    ("STEP", "Step", "Define the distance between the vertices", 0),
    ("SIZE", "Size", "Define how large the grid will be in total", 1)]

class GridMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GridMeshNode"
    bl_label = "Grid Mesh"
    bl_width_default = 160

    mode = EnumProperty(name = "Mode", default = "SIZE",
        update = AnimationNode.refresh, items = modeItems)

    def create(self):
        self.newInput("Integer", "X Divisions", "xDivisions", value = 10, minValue = 2)
        self.newInput("Integer", "Y Divisions", "yDivisions", value = 10, minValue = 2)

        if self.mode == "STEP":
            self.newInput("Float", "X Distance", "xDistance", value = 1)
            self.newInput("Float", "Y Distance", "yDistance", value = 1)
        elif self.mode == "SIZE":
            self.newInput("Float", "Length", "length", value = 10)
            self.newInput("Float", "Width", "width", value = 10)

        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")

    def draw(self, layout):
        layout.prop(self, "mode")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        yield "_xDivisions =  max(xDivisions, 2)"
        yield "_yDivisions =  max(yDivisions, 2)"
        if isLinked["vertices"]:
            if self.mode == "STEP":
                yield "vertices = self.calcVertices_Step(xDistance, yDistance, _xDivisions, _yDivisions)"
            elif self.mode == "SIZE":
                yield "vertices = self.calcVertices_Size(length, width, _xDivisions, _yDivisions)"
        if isLinked["edgeIndices"]:
            yield "edgeIndices = self.calcEdgeIndices(_xDivisions, _yDivisions)"
        if isLinked["polygonIndices"]:
            yield "polygonIndices = self.calcPolygonIndices(_xDivisions, _yDivisions)"

    def calcVertices_Step(self, xDistance, yDistance, xDivisions, yDivisions):
        return grid.vertices_Step(xDistance, yDistance, xDivisions, yDivisions)

    def calcVertices_Size(self, length, width, xDivisions, yDivisions):
        return grid.vertices_Size(length, width, xDivisions, yDivisions)

    def calcEdgeIndices(self, xDivisions, yDivisions):
        return grid.innerQuadEdges(xDivisions, yDivisions)

    def calcPolygonIndices(self, xDivisions, yDivisions):
        return grid.innerQuadPolygons(xDivisions, yDivisions)
