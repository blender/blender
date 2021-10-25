import bpy
from bpy.props import *
from mathutils import Matrix

from ... events import executionCodeChanged
from ... base_types import AnimationNode, VectorizedSocket

from ... data_structures import (
    VirtualList,
    VirtualVector3DList,
    VirtualEulerList,
    Matrix4x4List
)

from . c_utils import (
    composeMatrices,
    scale3x3Parts,
    setLocations,
    scalesFromVirtualVectors,
    rotationsFromVirtualEulers
)

class ComposeMatrixNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ComposeMatrixNode"
    bl_label = "Compose Matrix"
    bl_width_default = 180

    onlySearchTags = True
    searchTags = [
        ("Translation Matrix", {"useTranslation" : repr(True)}),
        ("Rotation Matrix", {"useRotation" : repr(True)}),
        ("Scale Matrix", {"useScale" : repr(True)}),
        ("Compose Matrix", {"useTranslation" : repr(True),
                            "useRotation" : repr(True),
                            "useScale" : repr(True)})
    ]

    def checkedPropertiesChanged(self, context):
        self.updateSocketVisibility()
        executionCodeChanged()

    useTranslation = BoolProperty(name = "Use Translation", default = False,
        update = checkedPropertiesChanged)
    useRotation = BoolProperty(name = "Use Rotation", default = False,
        update = checkedPropertiesChanged)
    useScale = BoolProperty(name = "Use Scale", default = False,
        update = checkedPropertiesChanged)

    useTranslationList = VectorizedSocket.newProperty()
    useRotationList = VectorizedSocket.newProperty()
    useScaleList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useTranslationList",
            ("Translation", "translation"),
            ("Translations", "translations")))

        self.newInput(VectorizedSocket("Euler", "useRotationList",
            ("Rotation", "rotation"),
            ("Rotations", "rotations")))

        self.newInput(VectorizedSocket("Vector", "useScaleList",
            ("Scale", "scale", dict(value = (1, 1, 1))),
            ("Scales", "scales"),
            dict(default = (1, 1, 1))))

        self.newOutput(VectorizedSocket("Matrix",
            ["useTranslationList", "useRotationList", "useScaleList"],
            ("Matrix", "matrix"), ("Matrices", "matrices")))

        self.updateSocketVisibility()

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "useTranslation", text = "Loc", icon = "MAN_TRANS")
        row.prop(self, "useRotation", text = "Rot", icon = "MAN_ROT")
        row.prop(self, "useScale", text = "Scale", icon = "MAN_SCALE")

    def updateSocketVisibility(self):
        self.inputs[0].hide = not self.useTranslation
        self.inputs[1].hide = not self.useRotation
        self.inputs[2].hide = not self.useScale

    def getExecutionCode(self, required):
        if self.useTranslationList or self.useRotationList or self.useScaleList:
            args = ", ".join(socket.identifier for socket in self.inputs)
            yield "matrices = self.calculateMatrices({})".format(args)
        else:
            yield from self.getExecutionCode_Single()

    def getExecutionCode_Single(self):
        loc, rot, scale = self.useTranslation, self.useRotation, self.useScale
        activatedAmount = [loc, rot, scale].count(True)
        if activatedAmount == 0:
            yield "matrix = Matrix.Identity(4)"
        elif activatedAmount == 1:
            if loc:
                yield "matrix = Matrix.Translation(translation)"
            elif rot:
                yield "matrix = rotation.to_matrix()"
                yield "matrix.resize_4x4()"
            elif scale:
                yield "matrix = AN.utils.math.scaleMatrix(scale)"
        elif activatedAmount == 2:
            if loc and rot:
                yield "matrix = rotation.to_matrix()"
                yield "matrix.resize_4x4()"
                yield "matrix.col[3][:3] = translation"
            elif loc and scale:
                yield "matrix = AN.utils.math.scaleMatrix(scale)"
                yield "matrix.col[3][:3] = translation"
            elif rot and scale:
                yield "matrix = rotation.to_matrix()"
                yield "matrix[0] *= scale.x"
                yield "matrix[1] *= scale.y"
                yield "matrix[2] *= scale.z"
                yield "matrix.resize_4x4()"
        elif activatedAmount == 3:
            yield "matrix = AN.utils.math.composeMatrix(translation, rotation, scale)"

    def calculateMatrices(self, translation, rotation, scale):
        translations = VirtualVector3DList.create(translation, (0, 0, 0))
        rotations = VirtualEulerList.create(rotation, (0, 0, 0))
        scales = VirtualVector3DList.create(scale, (1, 1, 1))

        lists = []
        if self.useTranslation: lists.append(translations)
        if self.useRotation: lists.append(rotations)
        if self.useScale: lists.append(scales)
        amount = VirtualList.getMaxRealLength(*lists)

        if self.useRotation:
            matrices = rotationsFromVirtualEulers(amount, rotations)
            if self.useScale:
                scale3x3Parts(matrices, scales)
        else:
            if self.useScale:
                matrices = scalesFromVirtualVectors(amount, scales)
            else:
                identity = Matrix.Identity(4)
                matrices = Matrix4x4List.fromValue(identity, amount)
        if self.useTranslation:
            setLocations(matrices, translations)

        return matrices
