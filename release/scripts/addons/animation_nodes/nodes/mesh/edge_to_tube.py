import bpy
from bpy.props import *
from . c_utils import edgesToTubes
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import Vector3DList, EdgeIndicesList, PolygonIndicesList

class EdgeToTubeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EdgeToTubeNode"
    bl_label = "Edge to Tube"
    errorHandlingType = "EXCEPTION"

    useEdgeIndicesList = VectorizedSocket.newProperty()
    useRadiusList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Vector List", "Points", "inPoints")

        self.newInput(VectorizedSocket("Edge Indices", "useEdgeIndicesList",
            ("Edge Indices", "inEdge"), ("Edge Indices List", "inEdges")))

        self.newInput(VectorizedSocket("Float", ["useRadiusList", "useEdgeIndicesList"],
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii")))

        self.newInput("Integer", "Resolution", "resolution", value = 3, minValue = 2)
        self.newInput("Boolean", "Caps", "caps", value = True)

        self.newOutput("Mesh", "Mesh", "mesh")

    def getExecutionFunctionName(self):
        if self.useEdgeIndicesList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_Single(self, inPoints, inEdge, radius, resolution, caps):
        try:
            return edgesToTubes(inPoints, EdgeIndicesList.fromValue(inEdge), radius, max(resolution, 2), caps)
        except Exception as e:
            self.raiseErrorMessage(str(e), show = self.inputs["Edge Indices"].isLinked)

    def execute_List(self, inPoints, inEdges, radius, resolution, caps):
        try:
            return edgesToTubes(inPoints, inEdges, radius, max(resolution, 2), caps)
        except Exception as e:
            self.raiseErrorMessage(str(e))
