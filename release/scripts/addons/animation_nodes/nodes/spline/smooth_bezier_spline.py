import bpy
from ... base_types import VectorizedNode

class SmoothBezierSplineNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_SmoothBezierSplineNode"
    bl_label = "Smooth Bezier Spline"
    autoVectorizeExecution = True

    useSplineList = VectorizedNode.newVectorizeProperty()

    def create(self):
        socket = self.newVectorizedInput("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines"))
        socket.defaultDrawType = "TEXT_ONLY"
        socket.dataIsModified = True

        self.newInput("Float", "Smoothness", "smoothness", value = 0.3333)

        self.newVectorizedOutput("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines"))

    def getExecutionCode(self):
        yield "if spline.type == 'BEZIER':"
        yield "    spline.calculateSmoothHandles(smoothness)"
