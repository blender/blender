import bpy
from ... base_types import AnimationNode
from ... algorithms.interpolations import Linear, FCurveMapping

class InterpolationFromFCurveNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InterpolationFromFCurveNode"
    bl_label = "Interpolation from FCurve"

    def create(self):
        self.newInput("FCurve", "FCurve", "fCurve")
        self.newOutput("Interpolation", "Interpolation", "interpolation")

    def execute(self, fCurve):
        if fCurve is None: return Linear()
        startX, endX = fCurve.range()
        if startX == endX: return Linear()
        startY = fCurve.evaluate(startX)
        endY = fCurve.evaluate(endX)
        if startY == endY: return Linear()

        return FCurveMapping(fCurve, startX, endX - startX, -startY, 1 / (endY - startY))
