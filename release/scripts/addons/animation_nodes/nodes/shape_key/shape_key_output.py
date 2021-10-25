import bpy
from ... base_types import AnimationNode, VectorizedSocket

class ShapeKeyOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ShapeKeyOutputNode"
    bl_label = "Shape Key Output"
    bl_width_default = 160
    codeEffects = [VectorizedSocket.CodeEffect]

    useShapeKeyList = VectorizedSocket.newProperty()
    useValueList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Shape Key", "useShapeKeyList",
            ("Shape Key", "shapeKey", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Shape Keys", "shapeKeys"),
            codeProperties = dict(allowListExtension = False)))

        self.newInput(VectorizedSocket("Float", "useValueList",
            ("Value", "value", dict(minValue = 0, maxValue = 1)),
            ("Values", "values")))

        self.newInput("Float", "Slider Min", "sliderMin")
        self.newInput("Float", "Slider Max", "sliderMax")
        self.newInput("Boolean", "Mute", "mute")
        self.newInput("Text", "Name", "name")

        self.newOutput(VectorizedSocket("Shape Key", "useShapeKeyList",
            ("Shape Key", "shapeKey"),
            ("Shape Keys", "shapeKeys")))

        for socket in self.inputs[1:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False
        for socket in self.inputs[2:]:
            socket.hide = True

    def getExecutionCode(self, required):
        yield "if shapeKey is not None:"
        s = self.inputs
        if s[1].isUsed: yield "    shapeKey.value = value"
        if s[2].isUsed: yield "    shapeKey.slider_min = sliderMin"
        if s[3].isUsed: yield "    shapeKey.slider_max = sliderMax"
        if s[4].isUsed: yield "    shapeKey.mute = mute"
        if s[5].isUsed: yield "    shapeKey.name = name"
        yield "    pass"

    def getBakeCode(self):
        yield "if shapeKey is not None:"
        s = self.inputs
        if s[1].isUsed: yield "    shapeKey.keyframe_insert('value')"
        if s[2].isUsed: yield "    shapeKey.keyframe_insert('slider_min')"
        if s[3].isUsed: yield "    shapeKey.keyframe_insert('slider_max')"
        if s[4].isUsed: yield "    shapeKey.keyframe_insert('mute')"
