import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import VectorizedNode
from ... utils.handlers import validCallback
from .. falloff.invert_falloff import InvertFalloff
from . c_utils import evaluateFalloffForMatrixList

from ... data_structures import (
    CDefaultList,
    Vector3DList,
    EulerList,
    Matrix4x4List
)

from ... algorithms.matrices import (
    translateMatrixList,
    getRotatedMatrixList,
    scaleMatrixList
)

specifiedStateItems = [
    ("START", "Start", "Given matrices set the start state", "NONE", 0),
    ("END", "End", "Given matrices set the end state", "NONE", 1)
]

translationModeItems = [
    ("LOCAL_AXIS", "Local Axis", "", "NONE", 0),
    ("GLOBAL_AXIS", "Global Axis", "", "NONE", 1)
]

rotationModeItems = [
    ("LOCAL_AXIS__LOCAL_PIVOT", "Local Axis - Local Pivot", "", "NONE", 0),
    ("GLOBAL_AXIS__LOCAL_PIVOT", "Global Axis - Local Pivot", "", "NONE", 1),
    ("GLOBAL_AXIS__GLOBAL_PIVOT", "Global Axis - Global Pivot", "", "NONE", 2)
]

scaleModeItems = [
    ("LOCAL_AXIS", "Local Axis", "", "NONE", 0),
    ("GLOBAL_AXIS", "Global Axis", "", "NONE", 1),
    ("INCLUDE_TRANSLATION", "Include Translation", "", "NONE", 2),
    ("TRANSLATION_ONLY", "Translation Only", "", "NONE", 3)
]

class OffsetMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_OffsetMatrixNode"
    bl_label = "Offset Matrix"
    bl_width_default = 190
    onlySearchTags = True
    searchTags = [("Offset Matrices", {"useMatrixList" : repr(True)})]

    errorMessage = StringProperty()

    useMatrixList = BoolProperty(name = "Use Matrix List", default = False,
        update = VectorizedNode.refresh)

    specifiedState = EnumProperty(name = "Specified State", default = "START",
        description = "Specify wether the given matrices are the start or end state",
        items = specifiedStateItems, update = propertyChanged)

    @validCallback
    def checkedPropertiesChanged(self, context):
        self.updateSocketVisibility()
        propertyChanged()

    useTranslation = BoolProperty(name = "Use Translation", default = False,
        update = checkedPropertiesChanged)
    useRotation = BoolProperty(name = "Use Rotation", default = False,
        update = checkedPropertiesChanged)
    useScale = BoolProperty(name = "Use Scale", default = False,
        update = checkedPropertiesChanged)

    useTranslationList = VectorizedNode.newVectorizeProperty()
    useRotationList = VectorizedNode.newVectorizeProperty()
    useScaleList = VectorizedNode.newVectorizeProperty()

    translationMode = EnumProperty(name = "Translation Mode", default = "GLOBAL_AXIS",
        items = translationModeItems, update = propertyChanged)

    rotationMode = EnumProperty(name = "Rotation Mode", default = "GLOBAL_AXIS__LOCAL_PIVOT",
        items = rotationModeItems, update = propertyChanged)

    scaleMode = EnumProperty(name = "Scale Mode", default = "LOCAL_AXIS",
        items = scaleModeItems, update = propertyChanged)

    def create(self):
        if self.useMatrixList:
            self.newInput("Matrix List", "Matrices", "inMatrices", dataIsModified = self.modifiesOriginalList)
            self.newInput("Falloff", "Falloff", "falloff")

            self.newVectorizedInput("Vector", "useTranslationList",
                ("Translation", "translation"),
                ("Translations", "translations"))

            self.newVectorizedInput("Euler", "useRotationList",
                ("Rotation", "rotation"),
                ("Rotations", "rotations"))

            self.newVectorizedInput("Vector", "useScaleList",
                ("Scale", "scale", dict(value = (1, 1, 1))),
                ("Scales", "scales"))

            self.newOutput("Matrix List", "Matrices", "outMatrices")
        else:
            self.newInput("Matrix", "Matrix", "inMatrix")
            self.newInput("Falloff", "Falloff", "falloff")
            self.newInput("Vector", "Translation", "translation")
            self.newInput("Euler", "Rotation", "rotation")
            self.newInput("Vector", "Scale", "scale", value = (1, 1, 1))
            self.newOutput("Matrix", "Matrix", "outMatrix")

        self.updateSocketVisibility()

    def updateSocketVisibility(self):
        self.inputs[2].hide = not self.useTranslation
        self.inputs[3].hide = not self.useRotation
        self.inputs[4].hide = not self.useScale

    def draw(self, layout):
        col = layout.column()

        row = col.row(align = True)
        row.prop(self, "useTranslation", text = "Loc", icon = "MAN_TRANS")
        row.prop(self, "useRotation", text = "Rot", icon = "MAN_ROT")
        row.prop(self, "useScale", text = "Scale", icon = "MAN_SCALE")

        row = col.row(align = True)
        row.prop(self, "specifiedState", expand = True)
        row.prop(self, "useMatrixList", text = "", icon = "LINENUMBERS_ON")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        col = layout.column(align = True)
        col.prop(self, "translationMode", text = "Translation")
        col.prop(self, "rotationMode", text = "Rotation")
        col.prop(self, "scaleMode", text = "Scale")

        if self.scaleMode in ("GLOBAL_AXIS", "INCLUDE_TRANSLATION"):
            layout.label("May result in invalid object matrices", icon = "INFO")

    def getExecutionFunctionName(self):
        if self.useMatrixList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_Single(self, matrix, falloff, translation, rotation, scale):
        inMatrices = Matrix4x4List.fromValue(matrix)
        outMatrices = self.execute_List(inMatrices, falloff, translation, rotation, scale)
        return outMatrices[0]

    def execute_List(self, matrices, falloff, translation, rotation, scale):
        self.errorMessage = ""

        influences = self.evaluateFalloff(matrices, falloff)
        if influences is None:
            self.errorMessage = "cannot evaluate falloff"
            return matrices

        if self.useScale:
            scales = CDefaultList(Vector3DList, scale, (1, 1, 1))
            scaleMatrixList(matrices, self.scaleMode, scales, influences)
        if self.useRotation:
            rotations = CDefaultList(EulerList, rotation, (0, 0, 0))
            matrices = getRotatedMatrixList(matrices, self.rotationMode, rotations, influences)
        if self.useTranslation:
            translations = CDefaultList(Vector3DList, translation, (0, 0, 0))
            translateMatrixList(matrices, self.translationMode, translations, influences)

        return matrices

    def evaluateFalloff(self, matrices, falloff):
        if self.specifiedState == "END":
            falloff = InvertFalloff(falloff)

        return evaluateFalloffForMatrixList(falloff, matrices)

    @property
    def modifiesOriginalList(self):
        return self.useScale or (self.useTranslation and not self.useRotation)
