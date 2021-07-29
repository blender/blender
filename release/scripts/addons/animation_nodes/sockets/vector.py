import bpy
from bpy.props import *
from mathutils import Vector
from .. events import propertyChanged
from .. data_structures import Vector3DList
from .. base_types import AnimationNodeSocket, CListSocket

class VectorSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_VectorSocket"
    bl_label = "Vector Socket"
    dataType = "Vector"
    allowedInputTypes = ["Vector"]
    drawColor = (0.15, 0.15, 0.8, 1.0)
    storable = True
    comparable = False

    value = FloatVectorProperty(default = [0, 0, 0], update = propertyChanged, subtype = "XYZ")

    def drawProperty(self, layout, text, node):
        col = layout.column(align = True)
        if text != "": col.label(text)
        col.prop(self, "value", index = 0, text = "X")
        col.prop(self, "value", index = 1, text = "Y")
        col.prop(self, "value", index = 2, text = "Z")

    def getValue(self):
        return Vector(self.value)

    def setProperty(self, data):
        self.value = data

    def getProperty(self):
        return self.value[:]

    @classmethod
    def getDefaultValue(cls):
        return Vector((0, 0, 0))

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Vector) and len(value) == 3:
            return value, 0
        try:
            if len(value) == 3: return Vector(value), 1
        except:
            pass
        return cls.getDefaultValue(), 2


class VectorListSocket(bpy.types.NodeSocket, CListSocket):
    bl_idname = "an_VectorListSocket"
    bl_label = "Vector List Socket"
    dataType = "Vector List"
    baseDataType = "Vector"
    allowedInputTypes = ["Vector List"]
    drawColor = (0.15, 0.15, 0.8, 0.5)
    storable = True
    comparable = False
    listClass = Vector3DList
