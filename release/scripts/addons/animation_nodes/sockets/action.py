import bpy
from .. base_types import AnimationNodeSocket, PythonListSocket

class ActionSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname  = "an_ActionSocket"
    bl_label = "Action Socket"
    dataType = "Action"
    drawColor = (1, 0.32, 0.18, 1)
    comparable = False
    storable = False

    def getValue(self):
        return None

    @classmethod
    def getDefaultValue(self):
        return None

    @classmethod
    def correctValue(cls, value):
        # TODO: use real Action class
        if isinstance(value, Action):
            return value, 0
        return cls.getDefaultValue(), 2


class ActionListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_ActionListSocket"
    bl_label = "Action List Socket"
    dataType = "Action List"
    baseType = ActionSocket
    drawColor = (1, 0.5, 0.2, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, Action) for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
