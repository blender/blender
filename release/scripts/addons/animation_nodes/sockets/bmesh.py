import bpy
import bmesh
from .. base_types import AnimationNodeSocket

class BMeshSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_BMeshSocket"
    bl_label = "BMesh Socket"
    dataType = "BMesh"
    allowedInputTypes = ["BMesh"]
    drawColor = (0.1, 0.4, 0.1, 1) 
    storable = False
    comparable = True

    @classmethod
    def getDefaultValue(cls):
        return bmesh.new()

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, bmesh.types.BMesh):
            return value, 0
        return cls.getDefaultValue(), 2
