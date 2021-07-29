import bpy
from ... base_types import VectorizedNode

class TransformSplineNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_TransformSplineNode"
    bl_label = "Transform Spline"
    autoVectorizeExecution = True

    useSplineList = VectorizedNode.newVectorizeProperty()

    def create(self):
        socket = self.newVectorizedInput("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines"))
        socket.dataIsModified = True
        socket.defaultDrawType = "PROPERTY_ONLY"

        self.newInput("Matrix", "Transformation", "matrix")
        self.newVectorizedOutput("Spline", "useSplineList",
            ("Spline", "spline"), ("Splines", "splines"))

    def getExecutionCode(self):
        return "spline.transform(matrix)"
