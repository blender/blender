import bpy
from bpy.props import *
from mathutils import Euler
from .. events import propertyChanged
from .. data_structures import EulerList
from .. base_types import AnimationNodeSocket, CListSocket

class EulerSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_EulerSocket"
    bl_label = "Euler Socket"
    dataType = "Euler"
    allowedInputTypes = ["Euler"]
    drawColor = (0.1, 0.0, 0.4, 1.0)
    storable = True
    comparable = False

    value = FloatVectorProperty(default = [0, 0, 0], update = propertyChanged, subtype = "EULER")

    def drawProperty(self, layout, text, node):
        col = layout.column(align = True)
        if text != "": col.label(text)
        col.prop(self, "value", index = 0, text = "X")
        col.prop(self, "value", index = 1, text = "Y")
        col.prop(self, "value", index = 2, text = "Z")

    def getValue(self):
        return Euler(self.value)

    def setProperty(self, data):
        self.value = data

    def getProperty(self):
        return self.value[:]

    @classmethod
    def getDefaultValue(cls):
        return Euler((0, 0, 0))

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Euler):
            return value, 0
        else:
            try: return Euler(value), 1
            except: return cls.getDefaultValue(), 2


class EulerListSocket(bpy.types.NodeSocket, CListSocket):
    bl_idname = "an_EulerListSocket"
    bl_label = "Euler List Socket"
    dataType = "Euler List"
    baseDataType = "Euler"
    allowedInputTypes = ["Euler List"]
    drawColor = (0.1, 0.0, 0.4, 0.5)
    storable = True
    comparable = False
    listClass = EulerList
