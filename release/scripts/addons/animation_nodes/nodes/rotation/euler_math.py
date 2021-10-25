import bpy
import math
from bpy.props import *
from ... base_types import AnimationNode, VectorizedSocket

operationItems = [
    ("ADD", "Add", "", "", 0),
    ("SUBTRACT", "Subtract", "", "", 1),
    ("MULTIPLY", "Multiply", "Multiply element by element", "", 2),
    ("DIVIDE", "Divide", "Divide element by element", "", 3),
    ("SCALE", "Scale", "", "", 4),
    ("ABSOLUTE", "Absolute", "", "", 5),
    ("SNAP", "Snap", "Snap the individual axis rotations", "", 6)
]

operationLabels = {
    "ADD" : "A + B",
    "SUBTRACT" : "A - B",
    "MULTIPLY" : "A * B",
    "DIVIDE" : "A / B",
    "SCALE" : "A * scale",
    "ABSOLUTE" : "abs A",
    "SNAP" : "snap A"
}

operationsWithFloat = ["SCALE"]
operationsWithSecondEuler = ["ADD", "SUBTRACT"]
operationsWithVector = ["MULTIPLY", "DIVIDE"]
operationsWithStepEuler = ["SNAP"]

class EulerMathNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EulerMathNode"
    bl_label = "Euler Math"
    dynamicLabelType = "HIDDEN_ONLY"
    codeEffects = [VectorizedSocket.CodeEffect]

    operation = EnumProperty(name = "Operation", default = "ADD",
        items = operationItems, update = AnimationNode.refresh)

    useListA = VectorizedSocket.newProperty()
    useListEulerB = VectorizedSocket.newProperty()
    useListVectorB = VectorizedSocket.newProperty()
    useListScale = VectorizedSocket.newProperty()
    useListStep = VectorizedSocket.newProperty()

    def create(self):
        usedProperties = ["useListA"]
        self.newInput(VectorizedSocket("Euler", "useListA",
            ("A", "a"), ("A", "a")))

        if self.operation in operationsWithSecondEuler:
            usedProperties.append("useListEulerB")
            self.newInput(VectorizedSocket("Euler", "useListEulerB",
                ("B", "b"), ("B", "b")))

        if self.operation in operationsWithVector:
            usedProperties.append("useListVectorB")
            self.newInput(VectorizedSocket("Vector", "useListVectorB",
                ("B", "b", dict(value = (1, 1, 1))),
                ("B", "b"),
                codeProperties = dict(default = (1,1,1))))

        if self.operation in operationsWithFloat:
            usedProperties.append("useListScale")
            self.newInput(VectorizedSocket("Float", "useListScale",
                ("Scale", "scale", dict(value = 1)),
                ("Scales", "scales")))

        if self.operation in operationsWithStepEuler:
            v = math.radians(10)
            usedProperties.append("useListStep")
            self.newInput(VectorizedSocket("Euler", "useListStep",
                ("Step Size", "stepSize", dict(value = (v, v, v))),
                ("Step Sizes", "stepSizes"),
                codeProperties = dict(default = (v, v, v))))

        self.newOutput(VectorizedSocket("Euler", usedProperties,
            ("Result", "result"), ("Results", "results")))

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def drawLabel(self):
        return operationLabels[self.operation]

    def getExecutionCode(self, required):
        op = self.operation

        if op == "ADD":
            yield "result = Euler((a[0] + b[0], a[1] + b[1], a[2] + b[2]), a.order)"
        elif op == "SUBTRACT":
            yield "result = Euler((a[0] - b[0], a[1] - b[1], a[2] - b[2]), a.order)"
        elif op == "MULTIPLY":
            yield "result = Euler((a[0] * b[0], a[1] * b[1], a[2] * b[2]), a.order)"
        elif op == "DIVIDE":
            yield "result = Euler((0, 0, 0), a.order)"
            yield "if b[0] != 0: result[0] = a[0] / b[0]"
            yield "if b[1] != 0: result[1] = a[1] / b[1]"
            yield "if b[2] != 0: result[2] = a[2] / b[2]"
        elif op == "SCALE":
            yield "result = Euler((a[0] * scale, a[1] * scale, a[2] * scale), a.order)"
        elif op == "ABSOLUTE":
            yield "result = Euler((abs(a[0]), abs(a[1]), abs(a[2])), a.order)"
        elif op == "SNAP":
            yield "result = a.copy()"
            yield "if stepSize.x != 0: result[0] = round(a[0] / stepSize[0]) * stepSize[0]"
            yield "if stepSize.y != 0: result[1] = round(a[1] / stepSize[1]) * stepSize[1]"
            yield "if stepSize.z != 0: result[2] = round(a[2] / stepSize[2]) * stepSize[2]"
