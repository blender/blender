import bpy
from bpy.props import *
from .. events import propertyChanged
from .. data_structures import Falloff
from .. nodes.falloff.constant_falloff import ConstantFalloff
from .. base_types import AnimationNodeSocket, PythonListSocket

class FalloffSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_FalloffSocket"
    bl_label = "Falloff Socket"
    dataType = "Falloff"
    allowedInputTypes = ["Falloff"]
    drawColor = (0.32, 1, 0.18, 1)
    comparable = False
    storable = False

    value = FloatProperty(default = 1, soft_min = 0, soft_max = 1, update = propertyChanged)

    def drawProperty(self, layout, text, node):
        layout.prop(self, "value", text = text, slider = True)

    def getValue(self):
        return ConstantFalloff(self.value)

    def setProperty(self, data):
        self.value = data

    def getProperty(self):
        return self.value

    @classmethod
    def getDefaultValue(cls):
        return ConstantFalloff(1.0)

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Falloff):
            return value, 0
        return cls.getDefaultValue(), 2


class FalloffListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_FalloffListSocket"
    bl_label = "Falloff List Socket"
    dataType = "Falloff List"
    baseDataType = "Falloff"
    allowedInputTypes = ["Falloff List"]
    drawColor = (0.32, 1, 0.18, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, Falloff) for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
