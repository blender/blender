import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, VectorizedSocket

trackAxisItems = [(axis, axis, "") for axis in ("X", "Y", "Z", "-X", "-Y", "-Z")]
guideAxisItems  = [(axis, axis, "") for axis in ("X", "Y", "Z")]

class DirectionToRotationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DirectionToRotationNode"
    bl_label = "Direction to Rotation"
    bl_width_default = 160

    trackAxis = EnumProperty(items = trackAxisItems, update = propertyChanged, default = "Z")
    guideAxis = EnumProperty(items = guideAxisItems, update = propertyChanged, default = "X")

    useDirectionList = VectorizedSocket.newProperty()
    useGuideList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useDirectionList",
            ("Direction", "direction"), ("Directions", "directions")))

        self.newInput(VectorizedSocket("Vector", "useGuideList",
            ("Guide", "guide", dict(value = (0, 0, 1))),
            ("Guides", "guides")))

        useListProperties = ["useDirectionList", "useGuideList"]

        self.newOutput(VectorizedSocket("Euler", useListProperties,
            ("Euler Rotation", "eulerRotation"),
            ("Euler Rotations", "eulerRotations")))

        self.newOutput(VectorizedSocket("Quaternion", useListProperties,
            ("Quaternion Rotation", "quaternionRotation", dict(hide = True)),
            ("Quaternion Rotations", "quaternionRotations", dict(hide = True))))

        self.newOutput(VectorizedSocket("Matrix", useListProperties,
            ("Matrix Rotation", "matrixRotation"),
            ("Matrix Rotations", "matrixRotations")))

    def draw(self, layout):
        layout.prop(self, "trackAxis", expand = True)
        layout.prop(self, "guideAxis", expand = True)

        if self.trackAxis[-1:] == self.guideAxis[-1:]:
            layout.label("Must be different", icon = "ERROR")

    def getExecutionCode(self, required):
        generateList = self.useDirectionList or self.useGuideList

        if generateList:
            yield "_directions = " + self.inputs[0].identifier
            yield "_guides = " + self.inputs[1].identifier
            yield "matrixRotations = AN.algorithms.rotations.directionsToMatrices(_directions, _guides, self.trackAxis, self.guideAxis)"
            if "eulerRotations" in required: yield "eulerRotations = matrixRotations.toEulers(isNormalized = True)"
            if "quaternionRotations" in required: yield "quaternionRotations = matrixRotations.toQuaternions(isNormalized = True)"
        else:
            yield "matrixRotation = AN.algorithms.rotations.directionToMatrix(direction, guide, self.trackAxis, self.guideAxis)"
            if "eulerRotation" in required: yield "eulerRotation = matrixRotation.to_euler()"
            if "quaternionRotation" in required: yield "quaternionRotation = matrixRotation.to_quaternion()"
