import bpy
from .. data_structures import MeshData
from .. base_types import AnimationNodeSocket, PythonListSocket

class MeshDataSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_MeshDataSocket"
    bl_label = "Mesh Data Socket"
    dataType = "Mesh Data"
    allowedInputTypes = ["Mesh Data"]
    drawColor = (0.3, 0.4, 0.18, 1)
    storable = True
    comparable = False

    @classmethod
    def getDefaultValue(cls):
        return MeshData()

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, MeshData):
            return value, 0
        return cls.getDefaultValue(), 2


class MeshDataListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_MeshDataListSocket"
    bl_label = "Mesh Data List Socket"
    dataType = "Mesh Data List"
    baseDataType = "Mesh Data"
    allowedInputTypes = ["Mesh Data List"]
    drawColor = (0.3, 0.4, 0.18, 0.5)
    storable = True
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "[element.copy() for element in value]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, MeshData) for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
