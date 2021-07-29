import bpy
from ... base_types import AnimationNode


class ObjectMatrixInputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectMatrixInputNode"
    bl_label = "Object Matrix Input"

    def create(self):
        self.newInput("Object", "Object", "object").defaultDrawType = "PROPERTY_ONLY"
        self.newOutput("Matrix", "World", "world")
        self.newOutput("Matrix", "Basis", "basis", hide = True)
        self.newOutput("Matrix", "Local", "local", hide = True)
        self.newOutput("Matrix", "Parent Inverse", "parentInverse", hide = True)

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        yield "if object is None:"
        if isLinked["world"]:         yield "    world = Matrix.Identity(4)"
        if isLinked["basis"]:         yield "    basis = Matrix.Identity(4)"
        if isLinked["local"]:         yield "    local = Matrix.Identity(4)"
        if isLinked["parentInverse"]: yield "    parentInverse = Matrix.Identity(4)"
        yield "else:"
        if isLinked["world"]:         yield "    world = object.matrix_world"
        if isLinked["basis"]:         yield "    basis = object.matrix_basis"
        if isLinked["local"]:         yield "    local = object.matrix_local"
        if isLinked["parentInverse"]: yield "    parentInverse = object.matrix_parent_inverse"
