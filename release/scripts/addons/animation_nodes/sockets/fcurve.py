import bpy
from bpy.types import FCurve
from .. base_types import AnimationNodeSocket, PythonListSocket

class FCurveSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_FCurveSocket"
    bl_label = "FCurve Socket"
    dataType = "FCurve"
    allowedInputTypes = ["FCurve"]
    drawColor = (0.2, 0.26, 0.19, 1)
    storable = True
    comparable = True

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def getDefaultValueCode(cls):
        return "None"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, FCurve) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class FCurveListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_FCurveListSocket"
    bl_label = "FCurve List Socket"
    dataType = "FCurve List"
    baseDataType = "FCurve"
    allowedInputTypes = ["FCurve List"]
    drawColor = (0.2, 0.26, 0.19, 0.5)
    storable = True
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, FCurve) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
