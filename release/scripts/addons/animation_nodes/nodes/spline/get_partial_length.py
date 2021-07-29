import bpy
from ... base_types import AnimationNode
from . spline_evaluation_base import SplineEvaluationBase

class GetSplineLengthNode(bpy.types.Node, AnimationNode, SplineEvaluationBase):
    bl_idname = "an_GetSplineLengthNode"
    bl_label = "Get Spline Length"

    def create(self):
        self.newInput("Spline", "Spline", "spline", defaultDrawType = "PROPERTY_ONLY")
        self.newInput("Float", "Start", "start").setRange(0, 1)
        self.newInput("Float", "End", "end", value = 1.0).setRange(0, 1)
        self.newOutput("Float", "Length", "length")

    def draw(self, layout):
        layout.prop(self, "parameterType", text = "")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.active = self.parameterType == "UNIFORM"
        col.prop(self, "resolution")

    def execute(self, spline, start, end):
        if spline.isEvaluable():
            start = min(max(start, 0), 1)
            end = min(max(end, 0), 1)

            if start == 0 and end == 1:
                # to get a more exact result on polysplines currently
                return spline.getLength(self.resolution)

            if self.parameterType == "UNIFORM":
                spline.ensureUniformConverter(self.resolution)
                start = spline.toUniformParameter(start)
                end = spline.toUniformParameter(end)
            return spline.getPartialLength(start, end, self.resolution)
        return 0.0
