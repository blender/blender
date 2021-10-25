import bpy
from bpy.props import *
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import (
    calculateEdgeLengths, calculateEdgeCenters,
    getEdgeStartPoints, getEdgeEndPoints
)

class EdgeInfoNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EdgeInfoNode"
    bl_label = "Edge Info"
    errorHandlingType = "MESSAGE"

    useEdgeList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Vector List", "Points", "points")

        self.newInput(VectorizedSocket("Edge Indices", "useEdgeList",
            ("Edge Indices", "edgeIndices"),
            ("Edge Indices List", "edgeIndicesList")))

        self.newOutput(VectorizedSocket("Vector", "useEdgeList",
            ("Start", "start"),
            ("Starts", "starts")))

        self.newOutput(VectorizedSocket("Vector", "useEdgeList",
            ("End", "end"),
            ("Ends", "ends")))

        self.newOutput(VectorizedSocket("Vector", "useEdgeList",
            ("Center", "center"),
            ("Centers", "centers")))

        self.newOutput(VectorizedSocket("Float", "useEdgeList",
            ("Length", "length"),
            ("Lengths", "lengths")))

    def getExecutionCode(self, required):
        if self.useEdgeList:
            yield from self.iterExecutionCode_List(required)
        else:
            yield from self.iterExecutionCode_Single(required)

    def iterExecutionCode_Single(self, required):
        yield "try:"
        yield "    i1, i2 = edgeIndices"
        if "length" in required: yield "    length = (points[i1] - points[i2]).length"
        if "center" in required: yield "    center = (points[i1] + points[i2]) / 2"
        if "start" in required:  yield "    start = points[i1]"
        if "end" in required:    yield "    end = points[i2]"
        yield "except IndexError:"
        yield "    self.setErrorMessage('invalid edge', show = self.inputs[1].isLinked)"
        yield "    length, center = Vector((0, 0, 0)), Vector((0, 0, 0))"
        yield "    start, end = Vector((0, 0, 0)), Vector((0, 0, 0))"

    def iterExecutionCode_List(self, required):
        yield "try:"
        if "lengths" in required:
            yield "    lengths = self.calculateEdgeLengths(points, edgeIndicesList)"
        if "centers" in required:
            yield "    centers = self.calculateEdgeCenters(points, edgeIndicesList)"
        if "starts" in required:
            yield "    starts = self.getEdgeStartPoints(points, edgeIndicesList)"
        if "ends" in required:
            yield "    ends = self.getEdgeEndPoints(points, edgeIndicesList)"
        yield "    pass"
        yield "except IndexError:"
        yield "    self.setErrorMessage('invalid edge')"
        yield "    lengths, centers = DoubleList(), Vector3DList()"

    def calculateEdgeLengths(self, points, edges):
        return calculateEdgeLengths(points, edges)

    def calculateEdgeCenters(self, points, edges):
        return calculateEdgeCenters(points, edges)

    def getEdgeStartPoints(self, points, edges):
        return getEdgeStartPoints(points, edges)

    def getEdgeEndPoints(self, points, edges):
        return getEdgeEndPoints(points, edges)
