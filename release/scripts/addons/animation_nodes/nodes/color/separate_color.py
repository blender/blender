import bpy, colorsys
from bpy.props import *
from ... base_types import AnimationNode

# using linear conversion here, unlike BL colorpicker hsv/hex
# BL Color() funcion does this also and has only rgb+hsv, so we'l use colorsys
# only hsv/hex in the colorpicker are gamma corrected for colorspaces
# we shall not use other functions, till they are in context (BL color space)

targetTypeItems = [
    ("RGB", "RGB", "Red, Green, Blue"),
    ("HSV", "HSV", "Hue, Saturation, Value"),
    ("HSL", "HSL", "Hue, Saturation, Lightness"),
    ("YIQ", "YIQ", "Luma, Chrominance")]

class SeparateColorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparateColorNode"
    bl_label = "Separate Color"
    dynamicLabelType = "HIDDEN_ONLY"

    def targetTypeChanged(self, context):
        self.recreateOutputs()

    targetType = EnumProperty(name = "Target Type", items = targetTypeItems,
        default = "RGB", update = AnimationNode.refresh)

    def create(self):
        self.newInput("Color", "Color", "color")

        if self.targetType == "RGB":
            self.newOutput("Float", "Red", "r")
            self.newOutput("Float", "Green", "g")
            self.newOutput("Float", "Blue", "b")
        elif self.targetType == "HSV":
            self.newOutput("Float", "Hue", "h")
            self.newOutput("Float", "Saturation", "s")
            self.newOutput("Float", "Value", "v")
        elif self.targetType == "HSL":
            self.newOutput("Float", "Hue", "h")
            self.newOutput("Float", "Saturation", "s")
            self.newOutput("Float", "Lightness", "l")
        elif self.targetType == "YIQ":
            self.newOutput("Float", "Y Luma", "y")
            self.newOutput("Float", "I In phase", "i")
            self.newOutput("Float", "Q Quadrature", "q")
        self.newOutput("Float", "Alpha", "alpha")

    def draw(self, layout):
        layout.prop(self, "targetType", expand = True)

    def drawLabel(self):
        return "{}a from Color".format(self.targetType)

    def getExecutionCode(self):
        if self.targetType == "RGB":    yield "r, g, b = color[0], color[1], color[2]"
        elif self.targetType == "HSV":  yield "h, s, v = colorsys.rgb_to_hsv(color[0], color[1], color[2])"
        elif self.targetType == "HSL":  yield "h, l, s = colorsys.rgb_to_hls(color[0], color[1], color[2])"
        elif self.targetType == "YIQ":  yield "y, i, q = colorsys.rgb_to_yiq(color[0], color[1], color[2])"
        yield "alpha = color[3]"

    def getUsedModules(self):
        return ["colorsys"]
