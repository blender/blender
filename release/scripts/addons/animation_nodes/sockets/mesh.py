import bpy
from .. data_structures import Mesh
from .. base_types import AnimationNodeSocket, PythonListSocket

class MeshSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_MeshSocket"
    bl_label = "Mesh Socket"
    dataType = "Mesh"
    drawColor = (0.2, 0.7, 1, 1)
    storable = True
    comparable = False

    @classmethod
    def getDefaultValue(cls):
        return Mesh()

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Mesh):
            return value, 0
        return cls.getDefaultValue(), 2


class MeshListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_MeshListSocket"
    bl_label = "Mesh List Socket"
    dataType = "Mesh List"
    baseType = MeshSocket
    drawColor = (0.18, 0.32, 1, 0.5)
    storable = True
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "[element.copy() for element in value]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, Mesh) for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
