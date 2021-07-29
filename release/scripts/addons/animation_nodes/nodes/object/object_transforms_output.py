import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... events import executionCodeChanged

class an_ObjectTransformsOutputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ObjectTransformsOutputNode"
    bl_label = "Object Transforms Output"
    bl_width_default = 165
    autoVectorizeExecution = True

    def checkedPropertiesChanged(self, context):
        self.updateSocketVisibility()
        executionCodeChanged()

    useLocation = BoolVectorProperty(update = checkedPropertiesChanged)
    useRotation = BoolVectorProperty(update = checkedPropertiesChanged)
    useScale = BoolVectorProperty(update = checkedPropertiesChanged)

    deltaTransforms = BoolProperty(name = "Delta Transforms", default = False,
        description = "Apply changes on delta transforms",
        update = executionCodeChanged)

    useObjectList = VectorizedNode.newVectorizeProperty()
    useLocationList = VectorizedNode.newVectorizeProperty()
    useRotationList = VectorizedNode.newVectorizeProperty()
    useScaleList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        self.newVectorizedInput("Vector", ("useLocationList", ["useObjectList"]),
            ("Location", "location"), ("Locations", "locations"))
        self.newVectorizedInput("Euler", ("useRotationList", ["useObjectList"]),
            ("Rotation", "rotation"), ("Rotations", "rotations"))
        self.newVectorizedInput("Vector", ("useScaleList", ["useObjectList"]),
            ("Scale", "scale", dict(value = (1, 1, 1))),
            ("Scales", "scales"))

        self.newVectorizedOutput("Object", "useObjectList",
            ("Object", "object"), ("Objects", "objects"))

        self.updateSocketVisibility()

    def draw(self, layout):
        col = layout.column()

        row = col.row()
        row.label("", icon = "MAN_TRANS")
        subrow = row.row(align = True)
        subrow.prop(self, "useLocation", index = 0, text = "X", toggle = True)
        subrow.prop(self, "useLocation", index = 1, text = "Y", toggle = True)
        subrow.prop(self, "useLocation", index = 2, text = "Z", toggle = True)

        row = col.row()
        row.label("", icon = "MAN_ROT")
        subrow = row.row(align = True)
        subrow.prop(self, "useRotation", index = 0, text = "X", toggle = True)
        subrow.prop(self, "useRotation", index = 1, text = "Y", toggle = True)
        subrow.prop(self, "useRotation", index = 2, text = "Z", toggle = True)

        row = col.row()
        row.label("", icon = "MAN_SCALE")
        subrow = row.row(align = True)
        subrow.prop(self, "useScale", index = 0, text = "X", toggle = True)
        subrow.prop(self, "useScale", index = 1, text = "Y", toggle = True)
        subrow.prop(self, "useScale", index = 2, text = "Z", toggle = True)

        if self.deltaTransforms:
            col.label("Delta Transforms", icon = "INFO")

    def drawAdvanced(self, layout):
        layout.prop(self, "deltaTransforms")

    def updateSocketVisibility(self):
        self.inputs[1].hide = not any(self.useLocation)
        self.inputs[2].hide = not any(self.useRotation)
        self.inputs[3].hide = not any(self.useScale)

    def getExecutionCode(self):
        useLoc = self.useLocation
        useRot = self.useRotation
        useScale = self.useScale

        if not any((*useLoc, *useRot, *useScale)):
            return

        yield "if object is not None:"

        # Location
        if all((*useLoc, )):
            yield "    object.{} = location".format(self.locationPath)
        else:
            for i in range(3):
                if useLoc[i]: yield "    object.{0}[{1}] = location[{1}]".format(self.locationPath, i)

        # Rotation
        if all((*useRot, )):
            yield "    object.{} = rotation".format(self.rotationPath)
        else:
            for i in range(3):
                if useRot[i]: yield "    object.{0}[{1}] = rotation[{1}]".format(self.rotationPath, i)

        # Scale
        if all((*useScale, )):
            yield "    object.{} = scale".format(self.scalePath)
        else:
            for i in range(3):
                if useScale[i]: yield "    object.{0}[{1}] = scale[{1}]".format(self.scalePath, i)

    def getBakeCode(self):
        yield "if object is not None:"

        for i in range(3):
            if self.useLocation[i]:
                yield "    object.keyframe_insert('{}', index = {})".format(self.locationPath, i)

        for i in range(3):
            if self.useRotation[i]:
                yield "    object.keyframe_insert('{}', index = {})".format(self.rotationPath, i)

        for i in range(3):
            if self.useScale[i]:
                yield "    object.keyframe_insert('{}', index = {})".format(self.scalePath, i)

    @property
    def locationPath(self):
        return "delta_location" if self.deltaTransforms else "location"

    @property
    def rotationPath(self):
        return "delta_rotation_euler" if self.deltaTransforms else "rotation_euler"

    @property
    def scalePath(self):
        return "delta_scale" if self.deltaTransforms else "scale"
