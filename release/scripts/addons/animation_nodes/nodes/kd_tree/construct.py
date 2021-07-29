import bpy
from ... base_types import AnimationNode

class ConstructKDTreeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConstructKDTreeNode"
    bl_label = "Construct KDTree"

    def create(self):
        self.newInput("Vector List", "Vector List", "vectorList")
        self.newOutput("KDTree", "KDTree", "kdTree")

    def getExecutionCode(self):
        yield "kdTree = mathutils.kdtree.KDTree(len(vectorList))"
        yield "for i, vector in enumerate(vectorList): kdTree.insert(vector, i)"
        yield "kdTree.balance()"

    def getUsedModules(self):
        return ["mathutils"]
