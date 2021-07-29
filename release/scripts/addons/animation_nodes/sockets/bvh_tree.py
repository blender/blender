import bpy
from mathutils.bvhtree import BVHTree
from .. base_types import AnimationNodeSocket

class BVHTreeSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_BVHTreeSocket"
    bl_label = "BVHTree Socket"
    dataType = "BVHTree"
    allowedInputTypes = ["BVHTree"]
    drawColor = (0.18, 0.32, 0.32, 1)
    comparable = True
    storable = True

    @classmethod
    def getDefaultValue(cls):
        return BVHTree.FromPolygons(vertices = [], polygons = [])

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, BVHTree):
            return value, 0
        return cls.getDefaultValue(), 2
