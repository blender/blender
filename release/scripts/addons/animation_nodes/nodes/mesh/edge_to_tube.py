import bpy
from bpy.props import *
from . c_utils import edgesToTubes
from ... base_types import VectorizedNode
from ... data_structures import Vector3DList, EdgeIndicesList, PolygonIndicesList

class EdgeToTubeNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_EdgeToTubeNode"
    bl_label = "Edge to Tube"

    useEdgeIndicesList = VectorizedNode.newVectorizeProperty()
    useRadiusList = VectorizedNode.newVectorizeProperty()

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Points", "inPoints")

        self.newVectorizedInput("Edge Indices", "useEdgeIndicesList",
            ("Edge Indices", "inEdge"), ("Edge Indices List", "inEdges"))

        self.newVectorizedInput("Float", ("useRadiusList", ["useEdgeIndicesList"]),
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii"))

        self.newInput("Integer", "Resolution", "resolution", value = 3, minValue = 2)
        self.newInput("Boolean", "Caps", "caps", value = True)

        self.newOutput("Vector List", "Vertices", "outVertices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "outIndices")

    def draw(self, layout):
        if self.errorMessage != "" and self.inputs[1].isLinked:
            layout.label(self.errorMessage, icon = "ERROR")

    def getExecutionFunctionName(self):
        if self.useEdgeIndicesList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_Single(self, inPoints, inEdge, radius, resolution, caps):
        try:
            self.errorMessage = ""
            return edgesToTubes(inPoints, EdgeIndicesList.fromValue(inEdge), radius, max(resolution, 2), caps)
        except Exception as e:
            self.errorMessage = str(e)
            return Vector3DList(), PolygonIndicesList()

    def execute_List(self, inPoints, inEdges, radius, resolution, caps):
        try:
            self.errorMessage = ""
            return edgesToTubes(inPoints, inEdges, radius, max(resolution, 2), caps)
        except Exception as e:
            self.errorMessage = str(e)
            return Vector3DList(), PolygonIndicesList()
