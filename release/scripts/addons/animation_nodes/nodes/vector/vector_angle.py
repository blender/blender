import bpy
from ... base_types import AnimationNode, VectorizedSocket

class VectorAngleNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VectorAngleNode"
    bl_label = "Vector Angle"
    codeEffects = [VectorizedSocket.CodeEffect]

    useListA = VectorizedSocket.newProperty()
    useListB = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useListA",
            ("A", "a", dict(value = (1, 0, 0))), ("A", "a")))

        self.newInput(VectorizedSocket("Vector", "useListB",
            ("B", "b", dict(value = (0, 0, 1))), ("B", "b")))

        self.newOutput(VectorizedSocket("Float", ["useListA", "useListB"],
            ("Angle", "angle"), ("Angles", "angles")))

        self.newOutput(VectorizedSocket("Quaternion", ["useListA", "useListB"],
            ("Rotation Difference", "rotationDifference"),
            ("Rotation Differences", "rotationDifferences")))

    def getExecutionCode(self, required):
        if "angle" in required:
            yield "angle = a.angle(b, 0.0)"
        if "rotationDifference" in required:
            yield "rotationDifference = a.rotation_difference(b)"
