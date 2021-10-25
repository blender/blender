import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... data_structures import Matrix4x4List
from . c_utils import vectorizedMatrixMultiplication
from ... base_types import AnimationNode, VectorizedSocket

operationItems = [("MULTIPLY", "Multiply", "", "NONE", 0)]

class MatrixMathNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MatrixMathNode"
    bl_label = "Matrix Math"
    errorHandlingType = "MESSAGE"

    operation = EnumProperty(name = "Operation", items = operationItems,
        update = executionCodeChanged)

    useListA = VectorizedSocket.newProperty()
    useListB = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Matrix", "useListA",
            ("A", "a"), ("A", "a")))
        self.newInput(VectorizedSocket("Matrix", "useListB",
            ("B", "b"), ("B", "b")))
        self.newOutput(VectorizedSocket("Matrix", ["useListA", "useListB"],
            ("Result", "result"), ("Results", "results")))

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def getExecutionCode(self, required):
        if self.operation == "MULTIPLY":
            if self.useListA and self.useListB:
                yield "results = self.multMatrixLists(a, b)"
            elif self.useListA or self.useListB:
                yield "results = self.multMatrixWithList(a, b)"
            else:
                yield "result = a * b"

    def multMatrixLists(self, listA, listB):
        if len(listA) != len(listB):
            self.setErrorMessage("different length")
            return Matrix4x4List()
        return vectorizedMatrixMultiplication(listA, listB)

    def multMatrixWithList(self, a, b):
        return vectorizedMatrixMultiplication(a, b)
