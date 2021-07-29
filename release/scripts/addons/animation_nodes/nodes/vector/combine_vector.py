import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... data_structures import Vector3DList, DoubleList
from . c_utils import combineDoubleListsToVectorList

class CombineVectorNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_CombineVectorNode"
    bl_label = "Combine Vector"
    dynamicLabelType = "HIDDEN_ONLY"

    useListX = VectorizedNode.newVectorizeProperty()
    useListY = VectorizedNode.newVectorizeProperty()
    useListZ = VectorizedNode.newVectorizeProperty()

    errorMessage = StringProperty()

    def create(self):
        self.newVectorizedInput("Float", "useListX", ("X", "x"), ("X", "x"))
        self.newVectorizedInput("Float", "useListY", ("Y", "y"), ("Y", "y"))
        self.newVectorizedInput("Float", "useListZ", ("Z", "z"), ("Z", "z"))

        self.newVectorizedOutput("Vector", [("useListX", "useListY", "useListZ")],
            ("Vector", "vector"), ("Vectors", "vectors"))

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawLabel(self):
        label = "<X, Y, Z>"
        for socket in self.inputs:
            axis = socket.name
            if not getattr(self, "useList" + axis) and not socket.isLinked:
                label = label.replace(axis, str(round(socket.value, 4)))
        return label

    def getExecutionCode(self):
        if self.generatesList:
            yield "vectors = self.createVectorList(x, y, z)"
        else:
            yield "vector = Vector((x, y, z))"

    def createVectorList(self, x, y, z):
        self.errorMessage = ""
        _x, _y, _z = self.prepareInputLists(x, y, z)
        if _x is None: return Vector3DList()
        return combineDoubleListsToVectorList(_x, _y, _z)

    def prepareInputLists(self, x, y, z):
        maxLength = max(len(l) for l in (x, y, z) if isinstance(l, DoubleList))
        if any(len(l) != maxLength for l in (x, y, z) if isinstance(l, DoubleList)):
            self.errorMessage = "lists have different length"
            return None, None, None

        if isinstance(x, DoubleList): _x = x
        else: _x = DoubleList.fromValues((x, )) * maxLength
        if isinstance(y, DoubleList): _y = y
        else: _y = DoubleList.fromValues((y, )) * maxLength
        if isinstance(z, DoubleList): _z = z
        else: _z = DoubleList.fromValues((z, )) * maxLength
        return _x, _y, _z

    @property
    def generatesList(self):
        return any((self.useListX, self.useListY, self.useListZ))
