import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures.splines.bezier_spline import BezierSpline
from ... data_structures.splines.from_blender import (
    createSplinesFromBlenderObject,
    createSplineFromBlenderSpline
)

importTypeItems = [
    ("SINGLE", "Single", "Only load one spline from the object", "", 0),
    ("ALL", "All", "Load all splines from the object", "", 1) ]

class SplinesFromObjectNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SplinesFromObjectNode"
    bl_label = "Splines from Object"
    errorHandlingType = "EXCEPTION"

    importType = EnumProperty(name = "Import Type", default = "ALL",
        items = importTypeItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Boolean", "Use World Space", "useWorldSpace", default = True)
        if self.importType == "SINGLE":
            self.newInput("Integer", "Index", "index", minValue = 0)
            self.newOutput("Spline", "Spline", "spline")
        else:
            self.newOutput("Spline List", "Splines", "splines")

    def drawAdvanced(self, layout):
        layout.prop(self, "importType")

    def getExecutionCode(self, required):
        if self.importType == "SINGLE":
            yield "spline = self.getSingleSpline(object, useWorldSpace, index)"
        elif self.importType == "ALL":
            yield "splines = self.getAllSplines(object, useWorldSpace)"

    def getSingleSpline(self, object, useWorldSpace, index):
        if object is None: return BezierSpline()
        if object.type != "CURVE":
            self.raiseErrorMessage("Not a curve object")

        bSplines = object.data.splines
        if 0 <= index < len(bSplines):
            bSpline = bSplines[index]
            if bSpline.type in ("POLY", "BEZIER"):
                spline = createSplineFromBlenderSpline(bSpline)
                if useWorldSpace:
                    spline.transform(object.matrix_world)
                return spline
            else:
                self.raiseErrorMessage("Spline type not supported: " + bSpline.type)
        else:
            self.raiseErrorMessage("Index out of range")

    def getAllSplines(self, object, useWorldSpace):
        splines = createSplinesFromBlenderObject(object)
        if useWorldSpace:
            for spline in splines:
                spline.transform(object.matrix_world)
        return splines
