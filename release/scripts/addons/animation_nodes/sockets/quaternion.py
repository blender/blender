import bpy
from bpy.props import *
from mathutils import Quaternion
from .. events import propertyChanged
from .. data_structures import QuaternionList
from .. base_types import AnimationNodeSocket, CListSocket

class QuaternionSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_QuaternionSocket"
    bl_label = "Quaternion Socket"
    dataType = "Quaternion"
    allowedInputTypes = ["Quaternion"]
    drawColor = (0.8, 0.6, 0.3, 1.0)
    storable = True
    comparable = False

    value = FloatVectorProperty(default = [1, 0, 0, 0], size = 4, update = propertyChanged)

    def drawProperty(self, layout, text, node):
        col = layout.column(align = True)
        if text != "": col.label(text)
        col.prop(self, "value", index = 0, text = "W")
        col.prop(self, "value", index = 1, text = "X")
        col.prop(self, "value", index = 2, text = "Y")
        col.prop(self, "value", index = 3, text = "Z")

    def getValue(self):
        return Quaternion(self.value)

    def setProperty(self, data):
        self.value = data

    def getProperty(self):
        return self.value[:]

    @classmethod
    def getDefaultValue(cls):
        return Quaternion((1, 0, 0, 0))

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Quaternion):
            return value, 0
        else:
            try: return Quaternion(value), 1
            except: return cls.getDefaultValue(), 2


class QuaternionListSocket(bpy.types.NodeSocket, CListSocket):
    bl_idname = "an_QuaternionListSocket"
    bl_label = "Quaternion List Socket"
    dataType = "Quaternion List"
    baseDataType = "Quaternion"
    allowedInputTypes = ["Quaternion List"]
    drawColor = (0.8, 0.6, 0.3, 0.5)
    storable = True
    comparable = False
    listClass = QuaternionList
