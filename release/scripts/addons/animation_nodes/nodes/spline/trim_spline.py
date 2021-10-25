import bpy
from . spline_evaluation_base import SplineEvaluationBase
from ... base_types import AnimationNode, VectorizedSocket

class TrimSplineNode(bpy.types.Node, AnimationNode, SplineEvaluationBase):
    bl_idname = "an_TrimSplineNode"
    bl_label = "Trim Spline"
    codeEffects = [VectorizedSocket.CodeEffect]

    useSplineList = VectorizedSocket.newProperty()
    useStartList = VectorizedSocket.newProperty()
    useEndList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Splines", "splines")))

        self.newInput(VectorizedSocket("Float", "useStartList",
            ("Start", "start", dict(value = 0, minValue = 0, maxValue = 1)),
            ("Starts", "starts")))

        self.newInput(VectorizedSocket("Float", "useEndList",
            ("End", "end", dict(value = 1, minValue = 0, maxValue = 1)),
            ("Ends", "ends")))

        self.newOutput(VectorizedSocket("Spline", ["useSplineList", "useStartList", "useEndList"],
            ("Spline", "trimmedSpline"),
            ("Splines", "trimmedSplines")))

    def draw(self, layout):
        layout.prop(self, "parameterType", text = "")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.active = self.parameterType == "UNIFORM"
        col.prop(self, "resolution")

    def getExecutionCode(self, required):
        return "trimmedSpline = self.trimSpline(spline, start, end)"

    def trimSpline(self, spline, start, end):
        if not spline.isEvaluable() or (start < 0.00001 and end > 0.99999):
            return spline.copy()

        start = min(max(start, 0.0), 1.0)
        end = min(max(end, 0.0), 1.0)

        if self.parameterType == "UNIFORM":
            spline.ensureUniformConverter(self.resolution)
            start = spline.toUniformParameter(start)
            end = spline.toUniformParameter(end)
        return spline.getTrimmedCopy(start, end)
