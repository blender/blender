import bpy
from mathutils import Vector
from ... data_structures import VirtualVector3DList
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import intersect_LineLine_List, intersect_LineLine_Single

class IntersectLineLineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectLineLineNode"
    bl_label = "Intersect Line Line"
    bl_width_default = 160
    searchTags = ["Nearest Points on 2 Lines"]

    useFirstLineStartList = VectorizedSocket.newProperty()
    useFirstLineEndList = VectorizedSocket.newProperty()
    useSecondLineStartList = VectorizedSocket.newProperty()
    useSecondLineEndList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useFirstLineStartList",
            ("First Line Start", "firstLineStart", dict(value = (1, 1, 0))),
            ("First Line Starts", "firstLineStarts"),
            codeProperties = dict(default = (1, 1, 0))))
        self.newInput(VectorizedSocket("Vector", "useFirstLineEndList",
            ("First Line End", "firstLineEnd", dict(value = (-1, -1, 0))),
            ("First Line Ends", "firstLineEnds"),
            codeProperties = dict(default = (-1, -1, 0))))

        self.newInput(VectorizedSocket("Vector", "useSecondLineStartList",
            ("Second Line Start", "secondLineStart", dict(value = (1, -1, 0))),
            ("Second Line Starts", "secondLineStarts"),
            codeProperties = dict(default = (1, -1, 0))))
        self.newInput(VectorizedSocket("Vector", "useSecondLineEndList",
            ("Second Line End", "secondLineEnd", dict(value = (-1, 1, 0))),
            ("Second Line Ends", "secondLineEnds"),
            codeProperties = dict(default = (-1, 1, 0))))

        props = ["useFirstLineStartList", "useFirstLineEndList",
                 "useSecondLineStartList", "useSecondLineEndList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("First Nearest Point", "firstNearestPoint"),
            ("First Nearest Points", "firstNearestPoints")))
        self.newOutput(VectorizedSocket("Vector", props,
            ("Second Nearest Point", "secondNearestPoint"),
            ("Second Nearest Points", "secondNearestPoints")))

        self.newOutput(VectorizedSocket("Float", props,
            ("First Parameter", "firstParameter"),
            ("First Parameters", "firstParameters")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Second Parameter", "secondParameter"),
            ("Second Parameters", "secondParameters")))

        self.newOutput(VectorizedSocket("Boolean", props,
            ("Valid", "valid"),
            ("Valids", "valids")))

    def getExecutionFunctionName(self):
        useList = any((self.useFirstLineStartList, self.useFirstLineEndList,
                       self.useSecondLineStartList, self.useSecondLineEndList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, firstLineStarts, firstLineEnds, secondLineStarts, secondLineEnds):
        starts1, ends1, starts2, ends2 = VirtualVector3DList.createMultiple(
            (firstLineStarts, (1, 1, 0)),
            (firstLineEnds, (-1, -1, 0)),
            (secondLineStarts, (1, -1, 0)),
            (secondLineEnds, (-1, 1, 0))
        )
        amount = VirtualVector3DList.getMaxRealLength(starts1, ends1, starts2, ends2)
        return intersect_LineLine_List(amount, starts1, ends1, starts2, ends2)

    def execute_Single(self, firstLineStart, firstLineEnd, secondLineStart, secondLineEnd):
        return intersect_LineLine_Single(firstLineStart, firstLineEnd,
            secondLineStart, secondLineEnd)
