import bpy
from .. base_types import AnimationNodeSocket, PythonListSocket

class GenericSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_GenericSocket"
    bl_label = "Generic Socket"
    dataType = "Generic"
    allowedInputTypes = ["All"]
    drawColor = (0.6, 0.3, 0.3, 1.0)
    storable = True
    comparable = False

    def getCurrentDataType(self):
        linkedDataTypes = tuple(self.linkedDataTypes)
        if len(linkedDataTypes) == 0:
            return "Generic"
        else:
            return linkedDataTypes[0]

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def getDefaultValueCode(cls):
        return "None"

    @classmethod
    def correctValue(cls, value):
        return value, 0


class GenericListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_GenericListSocket"
    bl_label = "GenericListSocket"
    dataType = "Generic List"
    baseDataType = "Generic"
    allowedInputTypes = ["All"]
    drawColor = (0.6, 0.3, 0.3, 0.5)
    storable = True
    comparable = False

    @classmethod
    def correctValue(cls, value):
        return value, 0
