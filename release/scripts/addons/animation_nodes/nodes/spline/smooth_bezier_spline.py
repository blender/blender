import bpy
from ... base_types import AnimationNode, VectorizedSocket

class SmoothBezierSplineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SmoothBezierSplineNode"
    bl_label = "Smooth Bezier Spline"
    codeEffects = [VectorizedSocket.CodeEffect]

    useSplineList = VectorizedSocket.newProperty()

    def create(self):
        socket = self.newInput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines")))
        socket.defaultDrawType = "TEXT_ONLY"
        socket.dataIsModified = True

        self.newInput("Float", "Smoothness", "smoothness", value = 0.3333)

        self.newOutput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines")))

    def getExecutionCode(self, required):
        yield "if spline.type == 'BEZIER':"
        yield "    spline.smoothAllHandles(smoothness)"
