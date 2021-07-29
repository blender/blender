import bpy
from bpy.props import *
from ... base_types import AnimationNode

operationItems = [
    ("ADD", "Add", "A + B", "", 0),
    ("SUBTRACT", "Subtract", "A - B", "", 1),
    ("COMBINE", "Combine Rotations", "Resambles matrix multiplication, may be more appropriate than addition", "", 3),
    ("ROTATION_DIFFERENCE", "Rotation Difference", "Rotation difference of A and B, may be more appropriate than substraction", "", 4),
    ("MULTIPLY", "Multiply", "Multiply element by element", "", 5),
    ("DIVIDE", "Divide", "Divide element by element", "", 6),
    ("CROSS", "Cross Product", "A quaternion that is orthogonal to both input directions", "", 7),
    ("NORMALIZE", "Normalize", "Scale to a length of 1, no impact on rotation", "", 8),
    ("SCALE", "Scale", "A * scale", "", 9),
    ("ABSOLUTE", "Absolute", "", "", 10),
    ("INVERT", "Invert", "- A", "", 11),
    ("CONJUGATE", "Conjugate", "Negate x,y,z", "", 12),
    ("SNAP", "Snap", "Snap the individual axis rotations", "", 13) ]

operationLabels = {
    "ADD" : "A + B",
    "SUBTRACT" : "A - B",
    "COMBINE" : "Combine Rotations",
    "ROTATION_DIFFERENCE" : "Rotation Difference",
    "MULTIPLY" : "A * B",
    "DIVIDE" : "A / B",
    "CROSS" : "A cross B",
    "NORMALIZE" : "A normalize",
    "SCALE" : "A * scale",
    "ABSOLUTE" : "abs A",
    "INVERT" : "- A",
    "CONJUGATE" : "- xyz (A)",
    "SNAP" : "snap A" }

operationsWithFloat = ["NORMALIZE", "SCALE"]
operationsWithSecondQuaternion = ["ADD","SUBTRACT", "COMBINE", "ROTATION_DIFFERENCE", "MULTIPLY", "DIVIDE", "CROSS"]
operationsWithStepQuaternion = ["SNAP"]

class QuaternionMathNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_QuaternionMathNode"
    bl_label = "Quaternion Math"
    dynamicLabelType = "HIDDEN_ONLY"

    operation = EnumProperty(name = "Operation", default = "ADD",
        items = operationItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Quaternion", "A", "a")
        if self.operation in operationsWithSecondQuaternion:
            self.newInput("Quaternion", "B", "b")
        if self.operation in operationsWithFloat:
            self.newInput("Float", "Scale", "scale").value = 1.0
        if self.operation in operationsWithStepQuaternion:
            self.newInput("Quaternion", "Step Size", "stepSize").value = (0.1, 0.1, 0.1)

        self.newOutput("Quaternion", "Result", "result")

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def drawLabel(self):
        return operationLabels[self.operation]

    def getExecutionCode(self):
        op = self.operation

        if op == "ADD": return "result = a + b"
        elif op == "SUBTRACT": return "result = a - b"
        elif op == "COMBINE": return "result = a * b"
        elif op == "ROTATION_DIFFERENCE": return "result = a.rotation_difference(b)"
        elif op == "MULTIPLY": return "result = Quaternion((A * B for A, B in zip(a, b)))"
        elif op == "DIVIDE": return ("result = Quaternion((1, 0, 0, 0))",
                                     "if b[0] != 0: result[0] = a[0] / b[0]",
                                     "if b[1] != 0: result[1] = a[1] / b[1]",
                                     "if b[2] != 0: result[2] = a[2] / b[2]",
                                     "if b[3] != 0: result[3] = a[3] / b[3]")
        elif op == "CROSS": return "result = a.cross(b)"
        elif op == "NORMALIZE": return "result = a.normalized() * scale"
        elif op == "SCALE": return "result = a * scale"
        elif op == "ABSOLUTE": return "result = Quaternion((abs(A) for A in a))"
        elif op == "INVERT": return "result = a.inverted()"
        elif op == "CONJUGATE": return "result = a.conjugated()"
        elif op == "SNAP":
            return ("result = a.copy()",
                    "if stepSize[0] != 0: result[0] = round(a[0] / stepSize[0]) * stepSize[0]",
                    "if stepSize[1] != 0: result[1] = round(a[1] / stepSize[1]) * stepSize[1]",
                    "if stepSize[2] != 0: result[2] = round(a[2] / stepSize[2]) * stepSize[2]",
                    "if stepSize[3] != 0: result[3] = round(a[3] / stepSize[3]) * stepSize[3]")
