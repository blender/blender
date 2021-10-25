import bpy
from mathutils import Vector
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import VirtualVector3DList, VirtualDoubleList
from . c_utils import intersect_SphereSphere_List, intersect_SphereSphere_Single

class IntersectSphereSphereNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectSphereSphereNode"
    bl_label = "Intersect Sphere Sphere"
    bl_width_default = 160

    useFirstSphereCenterList = VectorizedSocket.newProperty()
    useFirstSphereRadiusList = VectorizedSocket.newProperty()
    useSecondSphereCenterList = VectorizedSocket.newProperty()
    useSecondSphereRadiusList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useFirstSphereCenterList",
            ("First Sphere Center", "firstSphereCenter", dict(value = (0, 0, -0.5))),
            ("First Sphere Centers", "firstSphereCenters"),
            codeProperties = dict(default = (0, 0, -0.5))))
        self.newInput(VectorizedSocket("Float", "useFirstSphereRadiusList",
            ("First Sphere Radius", "firstSphereRadius", dict(value = 1)),
            ("First Sphere Radii", "firstSphereRadii"),
            codeProperties = dict(default = 1)))

        self.newInput(VectorizedSocket("Vector", "useSecondSphereCenterList",
            ("Second Sphere Center", "secondSphereCenter", dict(value = (0, 0, 0.5))),
            ("Second Sphere Centers", "secondSphereCenters"),
            codeProperties = dict(default = (0, 0, 0.5))))
        self.newInput(VectorizedSocket("Float", "useSecondSphereRadiusList",
            ("Second Sphere Radius", "secondSphereRadius", dict(value = 1)),
            ("Second Sphere Radii", "secondSphereRadii"),
            codeProperties = dict(default = 1)))

        props = ["useSecondSphereCenterList", "useSecondSphereRadiusList",
                 "useFirstSphereCenterList", "useFirstSphereRadiusList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("Circle Center", "circleCenter"),
            ("Circle Centers", "circleCenters")))
        self.newOutput(VectorizedSocket("Vector", props,
            ("Circle Normal", "circleNormal"),
            ("Circle Normals", "circleNormals")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Circle Radius", "circleRadius"),
            ("Circle Radii", "circleRadii")))

        self.newOutput(VectorizedSocket("Boolean", props,
            ("Valid", "valid"),
            ("Valids", "valids")))

    def getExecutionFunctionName(self):
        useList = any((self.useSecondSphereCenterList, self.useSecondSphereRadiusList,
                       self.useFirstSphereCenterList, self.useFirstSphereRadiusList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, firstSphereCenters, firstSphereRadii,
                           secondSphereCenters, secondSphereRadii):
        firstSphereCenters = VirtualVector3DList.create(firstSphereCenters, Vector((0, 0, -0.5)))
        firstSphereRadii = VirtualDoubleList.create(firstSphereRadii, 1)
        secondSphereCenters = VirtualVector3DList.create(secondSphereCenters, Vector((0, 0, 0.5)))
        secondSphereRadii = VirtualDoubleList.create(secondSphereRadii, 1)
        amount = VirtualVector3DList.getMaxRealLength(firstSphereCenters, firstSphereRadii,
                                                      secondSphereCenters, secondSphereRadii)
        return intersect_SphereSphere_List(amount,
            firstSphereCenters, firstSphereRadii,
            secondSphereCenters, secondSphereRadii)

    def execute_Single(self, firstSphereCenter, firstSphereRadius,
                             secondSphereCenter, secondSphereRadius):
        return intersect_SphereSphere_Single(firstSphereCenter, firstSphereRadius,
                                             secondSphereCenter, secondSphereRadius)
