import bpy, bmesh
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

class BMeshRecalculateFaceNormalsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BMeshRecalculateFaceNormalsNode"
    bl_label = "BMesh Recalculate Normals"

    invert = BoolProperty(name = "Invert Normals", update = executionCodeChanged)

    def create(self):
        self.newInput("BMesh", "BMesh", "bm").dataIsModified = True
        self.newOutput("BMesh", "BMesh", "bm")

    def draw(self, layout):
        layout.prop(self, "invert")

    def getExecutionCode(self):
        recalcString = "bmesh.ops.recalc_face_normals(bm, faces = bm.faces)"
        return [recalcString] * 2 if self.invert else recalcString

    def getUsedModules(self):
        return ["bmesh"]
