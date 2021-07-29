import bpy, colorsys
from bpy.props import *
from ... base_types import AnimationNode

# using linear conversion here, unlike BL colorpicker hsv/hex
# BL Color() funcion does this also and has only rgb+hsv, so we'l use colorsys
# only hsv/hex in the colorpicker are gamma corrected for colorspaces
# we shall not use other functions, till they are in context (BL color space)

sourceTypeItems = [
    ("RGB", "RGB", "Red, Green, Blue"),
    ("HSV", "HSV", "Hue, Saturation, Value"),
    ("HSL", "HSL", "Hue, Saturation, Lightness"),
    ("YIQ", "YIQ", "Luma, Chrominance")]

class CombineColorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CombineColorNode"
    bl_label = "Combine Color"
    dynamicLabelType = "HIDDEN_ONLY"

    sourceType = EnumProperty(name = "Source Type", default = "RGB",
        items = sourceTypeItems, update = AnimationNode.refresh)

    def create(self):
        if self.sourceType == "RGB":
            self.newInput("Float", "Red", "red")
            self.newInput("Float", "Green", "green")
            self.newInput("Float", "Blue", "blue")
        elif self.sourceType == "HSV":
            self.newInput("Float", "Hue", "hue")
            self.newInput("Float", "Saturation", "saturation")
            self.newInput("Float", "Value", "value")
        elif self.sourceType == "HSL":
            self.newInput("Float", "Hue", "hue")
            self.newInput("Float", "Saturation", "saturation")
            self.newInput("Float", "Lightness", "lightness")
        elif self.sourceType == "YIQ":
            self.newInput("Float", "Y Luma", "y")
            self.newInput("Float", "I In phase", "i")
            self.newInput("Float", "Q Quadrature", "q")

        self.newInput("Float", "Alpha", "alpha", value = 1)
        self.newOutput("Color", "Color", "color")

    def draw(self, layout):
        layout.prop(self, "sourceType", expand = True)

    def drawAdvanced(self, layout):
        layout.label("Uses linear color space", icon = "INFO")

    def drawLabel(self):
        return "Color from {}a".format(self.sourceType)

    def getExecutionCode(self):
        if self.sourceType == "RGB":    yield "color = [red, green, blue, alpha]"
        elif self.sourceType == "HSV":  yield "color = [*colorsys.hsv_to_rgb(hue, saturation, value), alpha]"
        elif self.sourceType == "HSL":  yield "color = [*colorsys.hls_to_rgb(hue, lightness, saturation), alpha]"
        elif self.sourceType == "YIQ":  yield "color = [*colorsys.yiq_to_rgb(y, i, q), alpha]"

    def getUsedModules(self):
        return ["colorsys"]
