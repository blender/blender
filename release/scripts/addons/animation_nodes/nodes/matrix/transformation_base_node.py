import bpy
from bpy.props import *
from ... events import propertyChanged
from .. falloff.invert_falloff import InvertFalloff
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import FloatList, BoundedAction
from . c_utils import evaluateTransformationAction, evaluateBoundedTransformationAction

from ... data_structures import (
    Matrix4x4List,
    VirtualEulerList,
    VirtualVector3DList
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

transformationSourceItems = [
    ("LOC_ROT_SCALE", "Loc/Rot/Scale", "", "NONE", 0),
    ("ACTION", "Action", "", "NONE", 1)
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

evaluationTimeModeItems = [
    ("FIXED", "Fixed", "Evaluate all actions at the same frame", "NONE", 0),
    ("FALLOFF", "Falloff", "Use Falloff to determine the frame to evaluate (only works with bounded actions)", "NONE", 1)
]

class MatrixTransformationBase:
    transformationSource = EnumProperty(name = "Transformation Source", default = "LOC_ROT_SCALE",
        items = transformationSourceItems, update = AnimationNode.refresh)

    specifiedState = EnumProperty(name = "Specified State", default = "START",
        description = "Specify wether the given matrices are the start or end state",
        items = specifiedStateItems, update = propertyChanged)

    useTranslation = BoolProperty(name = "Use Translation", default = False,
        update = AnimationNode.refresh)
    useRotation = BoolProperty(name = "Use Rotation", default = False,
        update = AnimationNode.refresh)
    useScale = BoolProperty(name = "Use Scale", default = False,
        update = AnimationNode.refresh)

    useTranslationList = VectorizedSocket.newProperty()
    useRotationList = VectorizedSocket.newProperty()
    useScaleList = VectorizedSocket.newProperty()

    translationMode = EnumProperty(name = "Translation Mode", default = "GLOBAL_AXIS",
        items = translationModeItems, update = propertyChanged)

    rotationMode = EnumProperty(name = "Rotation Mode", default = "GLOBAL_AXIS__LOCAL_PIVOT",
        items = rotationModeItems, update = propertyChanged)

    scaleMode = EnumProperty(name = "Scale Mode", default = "LOCAL_AXIS",
        items = scaleModeItems, update = propertyChanged)

    evaluationTimeMode = EnumProperty(name = "Evaluation Time Mode", default = "FIXED",
        items = evaluationTimeModeItems, update = AnimationNode.refresh)

    def createMatrixTransformationInputs(self, useMatrixList):
        if self.transformationSource == "LOC_ROT_SCALE":
            self.newInput("Falloff", "Falloff", "falloff")
            if useMatrixList:
                if self.useTranslation:
                    self.newInput(VectorizedSocket("Vector", "useTranslationList",
                        ("Translation", "translation"),
                        ("Translations", "translations")))
                if self.useRotation:
                    self.newInput(VectorizedSocket("Euler", "useRotationList",
                        ("Rotation", "rotation"),
                        ("Rotations", "rotations")))
                if self.useScale:
                    self.newInput(VectorizedSocket("Vector", "useScaleList",
                        ("Scale", "scale", dict(value = (1, 1, 1))),
                        ("Scales", "scales")))
            else:
                if self.useTranslation:
                    self.newInput("Vector", "Translation", "translation")
                if self.useRotation:
                    self.newInput("Euler", "Rotation", "rotation")
                if self.useScale:
                    self.newInput("Vector", "Scale", "scale", value = (1, 1, 1))
        elif self.transformationSource == "ACTION":
            self.newInput("Action", "Action", "action")
            if self.evaluationTimeMode == "FIXED":
                self.newInput("Float", "Frame", "frame")
            elif self.evaluationTimeMode == "FALLOFF":
                self.newInput("Falloff", "Falloff", "falloff")

    def draw_MatrixTransformationProperties(self, layout):
        col = layout.column()
        col.prop(self, "transformationSource", text = "")

        if self.transformationSource == "LOC_ROT_SCALE":
            row = col.row(align = True)
            row.prop(self, "useTranslation", text = "Loc", icon = "MAN_TRANS")
            row.prop(self, "useRotation", text = "Rot", icon = "MAN_ROT")
            row.prop(self, "useScale", text = "Scale", icon = "MAN_SCALE")
        elif self.transformationSource == "ACTION":
            col.prop(self, "evaluationTimeMode", text = "")

        #col.prop(self, "specifiedState", expand = True)

    def drawAdvanced_MatrixTransformationProperties(self, layout):
        col = layout.column(align = True)
        col.prop(self, "translationMode", text = "Translation")
        col.prop(self, "rotationMode", text = "Rotation")
        col.prop(self, "scaleMode", text = "Scale")

        if self.scaleMode in ("GLOBAL_AXIS", "INCLUDE_TRANSLATION"):
            layout.label("May result in invalid object matrices", icon = "INFO")

    def getMatrixTransformationFunctionName(self, useMatrixList):
        if self.transformationSource == "LOC_ROT_SCALE":
            if useMatrixList:
                return "transform_LocRotScale_List"
            else:
                return "transform_LocRotScale_Single"
        elif self.transformationSource == "ACTION":
            if self.evaluationTimeMode == "FIXED":
                if useMatrixList:
                    return "transform_Action_FixedFrame_List"
                else:
                    return "transform_Action_FixedFrame_Single"
            elif self.evaluationTimeMode == "FALLOFF":
                if useMatrixList:
                    return "transform_Action_FalloffFrame_List"
                else:
                    return "transform_Action_FalloffFrame_Single"

    # Loc/Rot/Scale
    #########################################################

    def transform_LocRotScale_Single(self, matrix, falloff, *args):
        inMatrices = Matrix4x4List.fromValue(matrix)
        outMatrices = self.transform_LocRotScale_List(inMatrices, falloff, *args)
        return outMatrices[0]

    def transform_LocRotScale_List(self, matrices, falloff, *args):
        influences = self.evaluateFalloff(matrices, falloff)

        # count index backwards
        index = -1
        if self.useScale:
            scales = VirtualVector3DList.create(args[index], (1, 1, 1))
            scaleMatrixList(matrices, self.scaleMode, scales, influences)
            index -= 1
        if self.useRotation:
            rotations = VirtualEulerList.create(args[index], (0, 0, 0))
            matrices = getRotatedMatrixList(matrices, self.rotationMode, rotations, influences)
            index -= 1
        if self.useTranslation:
            translations = VirtualVector3DList.create(args[index], (0, 0, 0))
            translateMatrixList(matrices, self.translationMode, translations, influences)

        return matrices


    # Action - Fixed Frame
    #########################################################

    def transform_Action_FixedFrame_Single(self, matrix, action, frame):
        inMatrices = Matrix4x4List.fromValue(matrix)
        outMatrices = self.transform_Action_FixedFrame_List(inMatrices, action, frame)
        return outMatrices[0]

    def transform_Action_FixedFrame_List(self, matrices, action, frame):
        if action is None:
            return matrices

        loc, rot, scale = evaluateTransformationAction(action, frame, len(matrices))
        return self.computeNewMatrices(matrices, loc, rot, scale)


    # Action - Falloff Frame
    #########################################################

    def transform_Action_FalloffFrame_Single(self, matrix, action, falloff):
        inMatrices = Matrix4x4List.fromValue(matrix)
        outMatrices = self.transform_Action_FalloffFrame_List(inMatrices, action, falloff)
        return outMatrices[0]

    def transform_Action_FalloffFrame_List(self, matrices, action, falloff):
        if action is None:
            return matrices
        if not isinstance(action, BoundedAction):
            self.raiseErrorMessage("action is not bounded (has no start and end)")

        parameters = self.evaluateFalloff(matrices, falloff)
        loc, rot, scale = evaluateBoundedTransformationAction(action, parameters)
        return self.computeNewMatrices(matrices, loc, rot, scale)


    # Utilities
    #########################################################

    def computeNewMatrices(self, matrices, translations, rotations, scales):
        influences = FloatList.fromValue(1, length = len(matrices))

        _translations = VirtualVector3DList.fromList(translations, (0, 0, 0))
        _rotations = VirtualEulerList.fromList(rotations, (0, 0, 0))
        _scales = VirtualVector3DList.fromList(scales, (1, 1, 1))

        scaleMatrixList(matrices, self.scaleMode, _scales, influences)
        matrices = getRotatedMatrixList(matrices, self.rotationMode, _rotations, influences)
        translateMatrixList(matrices, self.translationMode, _translations, influences)

        return matrices

    def evaluateFalloff(self, matrices, falloff):
        if self.specifiedState == "END":
            falloff = InvertFalloff(falloff)

        try: evaluator = falloff.getEvaluator("Transformation Matrix")
        except: self.raiseErrorMessage("cannot evaluate falloff with matrices")

        return evaluator.evaluateList(matrices)

    @property
    def transformsOriginalMatrixList(self):
        return (self.transformationSource == "ACTION"
                or self.useScale
                or (self.useTranslation and not self.useRotation))
