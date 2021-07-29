import bpy
from ... base_types import VectorizedNode
from . spline_evaluation_base import SplineEvaluationBase

class TrimSplineNode(bpy.types.Node, VectorizedNode, SplineEvaluationBase):
    bl_idname = "an_TrimSplineNode"
    bl_label = "Trim Spline"
    autoVectorizeExecution = True

    useSplineList = VectorizedNode.newVectorizeProperty()
    useStartList = VectorizedNode.newVectorizeProperty()
    useEndList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Spline", "useSplineList",
            ("Spline", "spline", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Splines", "splines"))

        self.newVectorizedInput("Float", ("useStartList", ["useSplineList"]),
            ("Start", "start", dict(value = 0, minValue = 0, maxValue = 1)),
            ("Starts", "starts"))

        self.newVectorizedInput("Float", ("useEndList", ["useSplineList"]),
            ("End", "end", dict(value = 1, minValue = 0, maxValue = 1)),
            ("Ends", "ends"))

        self.newVectorizedOutput("Spline", "useSplineList",
            ("Spline", "trimmedSpline"),
            ("Splines", "trimmedSplines"))

    def draw(self, layout):
        layout.prop(self, "parameterType", text = "")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.active = self.parameterType == "UNIFORM"
        col.prop(self, "resolution")

    def getExecutionCode(self):
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
