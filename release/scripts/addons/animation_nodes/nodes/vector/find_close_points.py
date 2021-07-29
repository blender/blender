import bpy
from bpy.props import *
from mathutils.kdtree import KDTree
from ... base_types import AnimationNode
from ... data_structures import EdgeIndicesList
from .. mesh.c_utils import calculateEdgeLengths

modeItems = [
    ("AMOUNT", "Amount", "Find a specific amount of neighbors for each point", "NONE", 0),
    ("DISTANCE", "Distance", "Find all points in a specific distance", "NONE", 1)
]

class FindClosePointsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FindClosePointsNode"
    bl_label = "Find Close Points"

    mode = EnumProperty(name = "Mode", default = "AMOUNT",
        items = modeItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Vector List", "Points", "points")

        if self.mode == "AMOUNT":
            self.newInput("Integer", "Amount", "amount", value = 3, minValue = 0)
        elif self.mode == "DISTANCE":
            self.newInput("Float", "Max Distance", "maxDistance", value = 0.3, minValue = 0)

        self.newOutput("Edge Indices List", "Edges", "edges")
        self.newOutput("Float List", "Distances", "distances")

    def draw(self, layout):
        layout.prop(self, "mode")

    def getExecutionCode(self):
        if self.mode == "AMOUNT":
            yield "edges = self.execute_Amount(points, amount)"
        elif self.mode == "DISTANCE":
            yield "edges = self.execute_Distance(points, maxDistance)"

        if self.outputs["Distances"].isLinked:
            yield "distances = self.calculateEdgeLengths(points, edges)"

    def execute_Amount(self, points, amount):
        kdTree = self.buildKDTree(points)
        amount = max(0, amount)

        edges = EdgeIndicesList()
        edgeSet = set()

        for index, point in enumerate(points):
            for foundPoint, foundIndex, distance in kdTree.find_n(point, amount + 1):
                if foundIndex == index:
                    continue

                edge = (foundIndex, index) if foundIndex < index else (index, foundIndex)
                if edge not in edgeSet:
                    edgeSet.add(edge)
                    edges.append(edge)

        return edges

    def execute_Distance(self, points, maxDistance):
        kdTree = self.buildKDTree(points)
        maxDistance = max(0, maxDistance)

        edges = EdgeIndicesList()
        edgeSet = set()

        for index, point in enumerate(points):
            for foundPoint, foundIndex, distance in kdTree.find_range(point, maxDistance):
                if foundIndex == index:
                    continue

                edge = (foundIndex, index) if foundIndex < index else (index, foundIndex)
                if edge not in edgeSet:
                    edgeSet.add(edge)
                    edges.append(edge)

        return edges

    def buildKDTree(self, points):
        kdTree = KDTree(len(points))
        for i, vector in enumerate(points):
            kdTree.insert(vector, i)
        kdTree.balance()
        return kdTree

    def calculateEdgeLengths(self, points, edges):
        return calculateEdgeLengths(points, edges)
