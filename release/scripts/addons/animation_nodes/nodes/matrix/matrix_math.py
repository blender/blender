import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... events import executionCodeChanged
from ... data_structures import Matrix4x4List
from . c_utils import vectorizedMatrixMultiplication

operationItems = [("MULTIPLY", "Multiply", "", "NONE", 0)]

class MatrixMathNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_MatrixMathNode"
    bl_label = "Matrix Math"

    operation = EnumProperty(name = "Operation", items = operationItems,
        update = executionCodeChanged)

    useListA = VectorizedNode.newVectorizeProperty()
    useListB = VectorizedNode.newVectorizeProperty()

    errorMessage = StringProperty()

    def create(self):
        self.newVectorizedInput("Matrix", "useListA",
            ("A", "a"), ("A", "a"))
        self.newVectorizedInput("Matrix", "useListB",
            ("B", "b"), ("B", "b"))
        self.newVectorizedOutput("Matrix", [("useListA", "useListB")],
            ("Result", "result"), ("Results", "results"))

    def draw(self, layout):
        layout.prop(self, "operation", text = "")
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def getExecutionCode(self):
        yield "self.errorMessage = ''"
        if self.operation == "MULTIPLY":
            if self.useListA and self.useListB:
                yield "results = self.multMatrixLists(a, b)"
            elif self.useListA or self.useListB:
                yield "results = self.multMatrixWithList(a, b)"
            else:
                yield "result = a * b"

    def multMatrixLists(self, listA, listB):
        if len(listA) != len(listB):
            self.errorMessage = "different length"
            return Matrix4x4List()
        return vectorizedMatrixMultiplication(listA, listB)

    def multMatrixWithList(self, a, b):
        return vectorizedMatrixMultiplication(a, b)
