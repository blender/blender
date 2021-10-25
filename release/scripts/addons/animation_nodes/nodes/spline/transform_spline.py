import bpy
from ... base_types import AnimationNode, VectorizedSocket

class TransformSplineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TransformSplineNode"
    bl_label = "Transform Spline"
    codeEffects = [VectorizedSocket.CodeEffect]

    useSplineList = VectorizedSocket.newProperty()

    def create(self):
        socket = self.newInput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines")))
        socket.dataIsModified = True
        socket.defaultDrawType = "PROPERTY_ONLY"

        self.newInput("Matrix", "Transformation", "matrix")
        self.newOutput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines")))

    def getExecutionCode(self, required):
        return "spline.transform(matrix)"
