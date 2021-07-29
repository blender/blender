import bpy
from bpy.props import *
from .. events import propertyChanged
from .. base_types import AnimationNodeSocket, PythonListSocket

class ColorSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_ColorSocket"
    bl_label = "Color Socket"
    dataType = "Color"
    allowedInputTypes = ["Color"]
    drawColor = (0.8, 0.8, 0.2, 1)
    storable = True
    comparable = False

    value = FloatVectorProperty(
        default = [0.5, 0.5, 0.5], subtype = "COLOR",
        soft_min = 0.0, soft_max = 1.0,
        update = propertyChanged)

    def drawProperty(self, layout, text, node):
        layout.prop(self, "value", text = text)

    def getValue(self):
        return list(self.value) + [1.0]

    def setProperty(self, data):
        self.value = data[:3]

    def getProperty(self):
        return self.value[:]

    @classmethod
    def getDefaultValue(cls):
        return [0, 0, 0, 1]

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isColor(value): return value, 0
        else: return cls.getDefaultValue(), 2


class ColorListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_ColorListSocket"
    bl_label = "Color List Socket"
    dataType = "Color List"
    baseDataType = "Color"
    allowedInputTypes = ["Color List"]
    drawColor = (0.8, 0.8, 0.2, 0.5)
    storable = True
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "[element[:] for element in value]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isColor(element) for element in value):
                return value, 0
        return cls.getDefaultValue(), 2


def isColor(value):
    if isinstance(value, list):
        return len(value) == 4 and all(isinstance(element, (int, float)) for element in value)
    return False
