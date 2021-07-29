import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged
from ... algorithms.interpolations import (
              Linear,
              SinInOut, SinIn, SinOut,
              BackInOut, BackIn, BackOut,
              PowerInOut, PowerIn, PowerOut,
              CircularInOut, CircularIn, CircularOut,
              BounceInOut, BounceIn, BounceOut,
              ElasticInOut, ElasticIn, ElasticOut,
              ExponentialInOut, ExponentialIn, ExponentialOut)

categoryItems = [
    ("LINEAR", "Linear", "", "IPO_LINEAR", 0),
    ("SINUSOIDAL", "Sinusoidal", "", "IPO_SINE", 1),
    ("POWER", "Power", "", "IPO_QUAD", 2),
    ("EXPONENTIAL", "Exponential", "", "IPO_EXPO", 3),
    ("CIRCULAR", "Circular", "", "IPO_CIRC", 4),
    ("BACK", "Back", "", "IPO_BACK", 5),
    ("BOUNCE", "Bounce", "", "IPO_BOUNCE", 6),
    ("ELASTIC", "Elastic", "", "IPO_ELASTIC", 7)]

class ConstructInterpolationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConstructInterpolationNode"
    bl_label = "Construct Interpolation"
    bl_width_default = 160

    category = EnumProperty(name = "Category", default = "LINEAR",
        items = categoryItems, update = AnimationNode.refresh)

    easeIn = BoolProperty(name = "Ease In", default = False, update = executionCodeChanged)
    easeOut = BoolProperty(name = "Ease Out", default = True, update = executionCodeChanged)

    def create(self):
        c = self.category

        if c in ("BOUNCE", "ELASTIC"):
            self.newInput("Integer", "Bounces", "intBounces", value = 4, minValue = 0)
        if c in ("POWER", "EXPONENTIAL"):
            self.newInput("Integer", "Exponent", "intExponent", value = 2, minValue = 1)
        if c in ("EXPONENTIAL", "ELASTIC"):
            self.newInput("Float", "Base", "floatBase", value = 2, minValue = 0.001)
        if c == "ELASTIC":
            self.newInput("Float", "Exponent", "floatExponent", value = 2)
        if c in ("BACK", "BOUNCE"):
            self.newInput("Float", "Size", "floatSize", value = 1.4)

        self.newOutput("Interpolation", "Interpolation", "interpolation")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "category", text = "")
        if self.category != "LINEAR":
            row.prop(self, "easeIn", text = "", icon = "IPO_EASE_IN")
            row.prop(self, "easeOut", text = "", icon = "IPO_EASE_OUT")

    def getExecutionCode(self):
        c = self.category
        if not (self.easeIn or self.easeOut): return "interpolation = self.getLinear()"
        if c == "LINEAR":      return "interpolation = self.getLinear()"
        if c == "SINUSOIDAL":  return "interpolation = self.getSinusoidal()"
        if c == "POWER":       return "interpolation = self.getPower(intExponent)"
        if c == "EXPONENTIAL": return "interpolation = self.getExponential(floatBase, intExponent)"
        if c == "CIRCULAR":    return "interpolation = self.getCircular()"
        if c == "BACK":        return "interpolation = self.getBack(floatSize)"
        if c == "BOUNCE":      return "interpolation = self.getBounce(intBounces, floatSize)"
        if c == "ELASTIC":     return "interpolation = self.getElastic(floatBase, floatExponent, intBounces)"

    def getLinear(self):
        return Linear()

    def getSinusoidal(self):
        if self.easeIn and self.easeOut: return SinInOut()
        if self.easeIn: return SinIn()
        return SinOut()

    def getPower(self, exponent):
        exponent = max(0, int(exponent))
        if self.easeIn and self.easeOut: return PowerInOut(exponent)
        if self.easeIn: return PowerIn(exponent)
        return PowerOut(exponent)

    def getExponential(self, base, exponent):
        if self.easeIn and self.easeOut: return ExponentialInOut(base, exponent)
        if self.easeIn: return ExponentialIn(base, exponent)
        return ExponentialOut(base, exponent)

    def getCircular(self):
        if self.easeIn and self.easeOut: return CircularInOut()
        if self.easeIn: return CircularIn()
        return CircularOut()

    def getBack(self, back):
        if self.easeIn and self.easeOut: return BackInOut(back)
        if self.easeIn: return BackIn(back)
        return BackOut(back)

    def getBounce(self, bounces, base):
        if self.easeIn and self.easeOut: return BounceInOut(bounces, base)
        if self.easeIn: return BounceIn(bounces, base)
        return BounceOut(bounces, base)

    def getElastic(self, base, exponent, bounces):
        if self.easeIn and self.easeOut: return ElasticInOut(bounces, base, exponent)
        if self.easeIn: return ElasticIn(bounces, base, exponent)
        return ElasticOut(bounces, base, exponent)
