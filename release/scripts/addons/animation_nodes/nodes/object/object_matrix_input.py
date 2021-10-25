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

    def getExecutionCode(self, required):
        if len(required) == 0:
            return

        yield "if object is None:"
        if "world" in required:         yield "    world = Matrix.Identity(4)"
        if "basis" in required:         yield "    basis = Matrix.Identity(4)"
        if "local" in required:         yield "    local = Matrix.Identity(4)"
        if "parentInverse" in required: yield "    parentInverse = Matrix.Identity(4)"
        yield "else:"
        if "world" in required:         yield "    world = object.matrix_world"
        if "basis" in required:         yield "    basis = object.matrix_basis"
        if "local" in required:         yield "    local = object.matrix_local"
        if "parentInverse" in required: yield "    parentInverse = object.matrix_parent_inverse"
