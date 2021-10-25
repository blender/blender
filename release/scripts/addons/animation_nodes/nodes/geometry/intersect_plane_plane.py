import bpy
from mathutils import Vector
from ... data_structures import VirtualVector3DList
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import intersect_PlanePlane_List, intersect_PlanePlane_Single

class IntersectPlanePlaneNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectPlanePlaneNode"
    bl_label = "Intersect Plane Plane"
    bl_width_default = 160

    useFirstPlanePointList = VectorizedSocket.newProperty()
    useFirstPlaneNormalList = VectorizedSocket.newProperty()
    useSecondPlanePointList = VectorizedSocket.newProperty()
    useSecondPlaneNormalList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useFirstPlanePointList",
            ("First Plane Point", "firstPlanePoint", dict(value = (0, 0, 0))),
            ("First Plane Points", "firstPlanePoints"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Vector", "useFirstPlaneNormalList",
            ("First Plane Normal", "firstPlaneNormal", dict(value = (0, 0, 1))),
            ("First Plane Normals", "firstPlaneNormals"),
            codeProperties = dict(default = (0, 0, 1))))

        self.newInput(VectorizedSocket("Vector", "useSecondPlanePointList",
            ("Second Plane Point", "secondPlanePoint", dict(value = (0, 0, 0))),
            ("Second Plane Points", "secondPlanePoints"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Vector", "useSecondPlaneNormalList",
            ("Second Plane Normal", "secondPlaneNormal", dict(value = (1, 0, 0))),
            ("Second Plane Normals", "secondPlaneNormals"),
            codeProperties = dict(default = (1, 0, 0))))

        props = ["useFirstPlanePointList", "useFirstPlaneNormalList",
                 "useSecondPlanePointList", "useSecondPlaneNormalList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("Line Direction", "lineDirection"),
            ("Line Directions", "lineDirections")))
        self.newOutput(VectorizedSocket("Vector", props,
            ("Line Point", "linePoint"),
            ("Line Points", "linePoints")))
        self.newOutput(VectorizedSocket("Boolean", props,
            ("Valid", "valid"),
            ("Valids", "valids")))

    def getExecutionFunctionName(self):
        useList = any((self.useFirstPlanePointList, self.useFirstPlaneNormalList,
                       self.useSecondPlanePointList, self.useSecondPlaneNormalList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, firstPlanePoints, firstPlaneNormals,
                           secondPlanePoints, secondPlaneNormals):
        firstPlanePoints = VirtualVector3DList.create(firstPlanePoints, Vector((0, 0, 0)))
        firstPlaneNormals = VirtualVector3DList.create(firstPlaneNormals, Vector((0, 0, 1)))
        secondPlanePoints = VirtualVector3DList.create(secondPlanePoints, Vector((0, 0, 0)))
        secondPlaneNormals = VirtualVector3DList.create(secondPlaneNormals, Vector((1, 0, 0)))
        amount = VirtualVector3DList.getMaxRealLength(firstPlanePoints, firstPlaneNormals,
                                                      secondPlanePoints, secondPlaneNormals)
        return intersect_PlanePlane_List(amount,
            firstPlanePoints, firstPlaneNormals,
            secondPlanePoints, secondPlaneNormals)

    def execute_Single(self, firstPlanePoint, firstPlaneNormal,
                             secondPlanePoint, secondPlaneNormal):
        return intersect_PlanePlane_Single(firstPlanePoint, firstPlaneNormal,
                                           secondPlanePoint, secondPlaneNormal)
