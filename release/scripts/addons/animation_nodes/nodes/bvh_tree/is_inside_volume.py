import bpy
from random import random
from mathutils import Vector
from ... base_types import VectorizedNode

# in some cases multiple tests have to done
# to reduce the probability for errors
direction1 = Vector((random(), random(), random())).normalized()
direction2 = Vector((random(), random(), random())).normalized()
direction3 = Vector((random(), random(), random())).normalized()

class IsInsideVolumeBVHTreeNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_IsInsideVolumeBVHTreeNode"
    bl_label = "Is Inside Volume"
    autoVectorizeExecution = True

    useVectorList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newInput("BVHTree", "BVHTree", "bvhTree")

        self.newVectorizedInput("Vector", "useVectorList",
            ("Vector", "vector", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Vectors", "vectors", dict(defaultDrawType = "PROPERTY_ONLY")))

        self.newVectorizedOutput("Boolean", "useVectorList",
            ("Is Inside", "isInside"), ("Are Inside", "areInside"))

    def getExecutionCode(self):
        return "isInside = self.execute_Single(bvhTree, vector)"

    def execute_Single(self, bvhTree, vector):
        hits1 = self.countHits(bvhTree, vector, direction1)
        if hits1 == 0: return False
        if hits1 == 1: return True

        hits2 = self.countHits(bvhTree, vector, direction2)
        if hits1 % 2 == hits2 % 2:
            return hits1 % 2 == 1

        hits3 = self.countHits(bvhTree, vector, direction3)
        return hits3 % 2 == 1

    def countHits(self, bvhTree, start, direction):
        hits = 0
        offset = direction * 0.0001
        location = bvhTree.ray_cast(start, direction)[0]

        while location is not None:
            hits += 1
            location = bvhTree.ray_cast(location + offset, direction)[0]

        return hits
