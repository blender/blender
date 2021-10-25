import bpy
from mathutils import Vector
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import VirtualVector3DList, VirtualDoubleList
from . c_utils import intersect_SpherePlane_List, intersect_SpherePlane_Single

class IntersectSpherePlaneNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectSpherePlaneNode"
    bl_label = "Intersect Sphere Plane"
    bl_width_default = 160

    useSphereCenterList = VectorizedSocket.newProperty()
    useSphereRadiusList = VectorizedSocket.newProperty()
    usePlanePointList = VectorizedSocket.newProperty()
    usePlaneNormalList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useSphereCenterList",
            ("Sphere Center", "sphereCenter", dict(value = (0, 0, 0))),
            ("Sphere Centers", "sphereCenters"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Float", "useSphereRadiusList",
            ("Sphere Radius", "sphereRadius", dict(value = 1)),
            ("Sphere Radii", "sphereRadii"),
            codeProperties = dict(default = 1)))

        self.newInput(VectorizedSocket("Vector", "usePlanePointList",
            ("Plane Point", "planePoint", dict(value = (0, 0, 0))),
            ("Plane Points", "planePoints"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Vector", "usePlaneNormalList",
            ("Plane Normal", "planeNormal", dict(value = (0, 0, 1))),
            ("Plane Normals", "planeNormals"),
            codeProperties = dict(default = (0, 0, 1))))

        props = ["usePlanePointList", "usePlaneNormalList",
                 "useSphereCenterList", "useSphereRadiusList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("Circle Center", "circleCenter"),
            ("Circle Centers", "circleCenters")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Circle Radius", "circleRadius"),
            ("Circle Radii", "circleRadii")))

        self.newOutput(VectorizedSocket("Boolean", props,
            ("Valid", "valid"),
            ("Valids", "valids")))

    def getExecutionFunctionName(self):
        useList = any((self.usePlanePointList, self.usePlaneNormalList,
                       self.useSphereCenterList, self.useSphereRadiusList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, sphereCenters, sphereRadii, planePoints, planeNormals):
        sphereCenters = VirtualVector3DList.create(sphereCenters, Vector((0, 0, 0)))
        sphereRadii = VirtualDoubleList.create(sphereRadii, 1)
        planePoints = VirtualVector3DList.create(planePoints, Vector((0, 0, 0)))
        planeNormals = VirtualVector3DList.create(planeNormals, Vector((0, 0, 1)))
        amount = VirtualVector3DList.getMaxRealLength(sphereCenters, sphereRadii,
                                                      planePoints, planeNormals)
        return intersect_SpherePlane_List(amount,
            sphereCenters, sphereRadii,
            planePoints, planeNormals)

    def execute_Single(self, sphereCenter, sphereRadius, planePoint, planeNormal):
        return intersect_SpherePlane_Single(sphereCenter, sphereRadius, planePoint, planeNormal)
