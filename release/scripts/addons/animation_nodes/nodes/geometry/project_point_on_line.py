import bpy
from mathutils import Vector
from ... data_structures import VirtualVector3DList
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import project_PointOnLine_List, project_PointOnLine_Single

class ProjectPointOnLineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ProjectPointOnLineNode"
    bl_label = "Project Point on Line"
    bl_width_default = 160
    searchTags = ["Distance Point to Line", "Closest Point on Line"]

    useLineStartList = VectorizedSocket.newProperty()
    useLineEndList = VectorizedSocket.newProperty()
    usePointList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useLineStartList",
            ("Line Start", "lineStart", dict(value = (0, 0, 0))),
            ("Line Starts", "lineStarts"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Vector", "useLineEndList",
            ("Line End", "lineEnd", dict(value = (1, 0, 0))),
            ("Line End", "lineEnds"),
            codeProperties = dict(default = (1, 0, 0))))

        self.newInput(VectorizedSocket("Vector", "usePointList",
            ("Point", "point", dict(value = (1, 1, 0))),
            ("Points", "points"),
            codeProperties = dict(default = (1, 1, 0))))

        props = ["useLineStartList", "useLineEndList", "usePointList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("Projection", "projection"),
            ("Projections", "projections")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Parameter", "parameter"),
            ("Parameters", "parameters")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Distance", "distance"),
            ("Distances", "distances")))

    def getExecutionFunctionName(self):
        useList = any((self.useLineStartList, self.useLineEndList,
                       self.usePointList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, lineStarts, lineEnds, points):
        lineStarts = VirtualVector3DList.create(lineStarts, Vector((0, 0, 0)))
        lineEnds = VirtualVector3DList.create(lineEnds, Vector((1, 0, 0)))
        points = VirtualVector3DList.create(points, Vector((1, 1, 0)))
        amount = VirtualVector3DList.getMaxRealLength(lineStarts, lineEnds, points)
        return project_PointOnLine_List(amount, lineStarts, lineEnds, points)

    def execute_Single(self, lineStart, lineEnd, point):
        return project_PointOnLine_Single(lineStart, lineEnd, point)
