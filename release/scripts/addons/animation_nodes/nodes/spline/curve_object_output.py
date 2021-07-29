import bpy
from bpy.props import *
from ... utils.layout import writeText
from ... base_types import VectorizedNode
from ... data_structures.splines.to_blender import setSplinesOnBlenderObject

class CurveObjectOutputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_CurveObjectOutputNode"
    bl_label = "Curve Object Output"
    bl_width_default = 175

    errorMessage = StringProperty()

    useSplineList = VectorizedNode.newVectorizeProperty()

    def create(self):
        socket = self.newInput("Object", "Object", "object")
        socket.defaultDrawType = "PROPERTY_ONLY"
        socket.objectCreationType = "CURVE"

        self.newVectorizedInput("Spline", "useSplineList",
            ("Spline", "spline", dict(defaultDrawType = "TEXT_ONLY")),
            ("Splines", "splines", dict(defaultDrawType = "TEXT_ONLY")))

        self.newInput("Float", "Bevel Depth", "bevelDepth", minValue = 0)
        self.newInput("Integer", "Bevel Resolution", "bevelResolution")
        self.newInput("Float", "Extrude", "extrude")
        self.newInput("Float", "Bevel Start", "bevelStart")
        self.newInput("Float", "Bevel End", "bevelEnd", value = 1.0)
        self.newInput("Float", "Offset", "offset")
        self.newInput("Integer", "Preview Resolution", "previewResolution", value = 12)
        self.newInput("Object", "Taper Object", "taperObject")
        self.newInput("Object", "Bevel Object", "bevelObject")
        self.newInput("Text", "Fill Mode", "fillMode", value = "FRONT")

        self.newOutput("Object", "Object", "object")

        for socket in self.inputs[1:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False
        for socket in self.inputs[4:]:
            socket.hide = True

    def draw(self, layout):
        if self.errorMessage != "":
            writeText(layout, self.errorMessage, width = 25, icon = "ERROR")

    def drawAdvanced(self, layout):
        writeText(layout, "Possible values for 'Fill Mode' are: \n3D Curve: 'FULL', 'HALF', 'BACK' and 'FRONT' \n2D Curve: 'NONE', 'BACK', 'FRONT' and 'BOTH'")

    def getExecutionCode(self):
        yield "if getattr(object, 'type', '') == 'CURVE':"
        yield "    curve = object.data"

        s = self.inputs
        if self.useSplineList:
            if s["Splines"].isUsed:         yield "    self.setSplines(object, splines)"
        else:
            if s["Spline"].isUsed:          yield "    self.setSplines(object, [spline])"
        if s["Bevel Depth"].isUsed:         yield "    curve.bevel_depth = bevelDepth"
        if s["Bevel Resolution"].isUsed:    yield "    curve.bevel_resolution = bevelResolution"
        if s["Bevel Start"].isUsed:         yield "    curve.bevel_factor_start = bevelStart"
        if s["Bevel End"].isUsed:           yield "    curve.bevel_factor_end = bevelEnd"
        if s["Extrude"].isUsed:             yield "    curve.extrude = extrude"
        if s["Offset"].isUsed:              yield "    curve.offset = offset"
        if s["Preview Resolution"].isUsed:  yield "    curve.resolution_u = previewResolution"
        if s["Taper Object"].isUsed:        yield "    curve.taper_object = taperObject"
        if s["Bevel Object"].isUsed:        yield "    curve.bevel_object = bevelObject"
        if s["Fill Mode"].isUsed:           yield "    self.setFillMode(curve, fillMode)"

    def setSplines(self, object, splines):
        setSplinesOnBlenderObject(object, splines)

    def setFillMode(self, curve, fillMode):
        isCorrectFillMode = fillMode in ("FULL", "BACK", "FRONT", "HALF") if curve.dimensions == "3D" else fillMode in ("NONE", "BACK", "FRONT", "BOTH")
        if isCorrectFillMode:
            curve.fill_mode = fillMode
            self.errorMessage = ""
        else:
            self.errorMessage = "The fill mode is invalid. Look in the advanced panels to see all possible values."

    def getBakeCode(self):
        yield "if getattr(object, 'type', '') == 'CURVE':"
        yield "    curve = object.data"

        s = self.inputs
        if s["Bevel Depth"].isUsed:         yield "    curve.keyframe_insert('bevel_depth')"
        if s["Bevel Resolution"].isUsed:    yield "    curve.keyframe_insert('bevel_resolution')"
        if s["Bevel Start"].isUsed:         yield "    curve.keyframe_insert('bevel_factor_start')"
        if s["Bevel End"].isUsed:           yield "    curve.keyframe_insert('bevel_factor_end')"
        if s["Extrude"].isUsed:             yield "    curve.keyframe_insert('extrude')"
        if s["Offset"].isUsed:              yield "    curve.keyframe_insert('offset')"
        if s["Preview Resolution"].isUsed:  yield "    curve.keyframe_insert('resolution_u')"
