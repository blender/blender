import bpy
import bmesh
from ... base_types import AnimationNode

class BMeshRemoveDoublesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BMeshRemoveDoublesNode"
    bl_label = "BMesh Remove Doubles"

    def create(self):
        self.newInput("BMesh", "BMesh", "bm").dataIsModified = True
        self.newInput("Float", "Distance", "distance", value = 0.0001, minValue = 0.0)
        self.newOutput("BMesh", "BMesh", "bm")

    def getExecutionCode(self):
        return "bmesh.ops.remove_doubles(bm, verts = bm.verts, dist = distance)"

    def getUsedModules(self):
        return ["bmesh"]
