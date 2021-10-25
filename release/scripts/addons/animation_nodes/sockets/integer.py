import bpy
from bpy.props import *
from .. events import propertyChanged
from .. data_structures import LongList
from .. base_types import AnimationNodeSocket, CListSocket
from . implicit_conversion import registerImplicitConversion

def getValue(self):
    return min(max(self.minValue, self.get("value", 0)), self.maxValue)
def setValue(self, value):
    self["value"] = min(max(self.minValue, value), self.maxValue)

class IntegerSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_IntegerSocket"
    bl_label = "Integer Socket"
    dataType = "Integer"
    drawColor = (0.3, 0.4, 1.0, 1.0)
    comparable = True
    storable = True

    value = IntProperty(default = 0,
        set = setValue, get = getValue,
        update = propertyChanged)

    minValue = IntProperty(default = -2**31)
    maxValue = IntProperty(default = 2**31-1)

    def drawProperty(self, layout, text, node):
        layout.prop(self, "value", text = text)

    def getValue(self):
        return self.value

    def setProperty(self, data):
        self.value = data

    def getProperty(self):
        return self.value

    def setRange(self, min, max):
        self.minValue = min
        self.maxValue = max

    @classmethod
    def getDefaultValue(cls):
        return 0

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, int):
            return value, 0
        else:
            try: return int(value), 1
            except: return cls.getDefaultValue(), 2

registerImplicitConversion("Float", "Integer", "int(value)")
registerImplicitConversion("Boolean", "Integer", "int(value)")


class IntegerListSocket(bpy.types.NodeSocket, CListSocket):
    bl_idname = "an_IntegerListSocket"
    bl_label = "Integer List Socket"
    dataType = "Integer List"
    baseType = IntegerSocket
    drawColor = (0.3, 0.4, 1.0, 0.5)
    storable = True
    comparable = False
    listClass = LongList

from .. nodes.boolean.c_utils import convert_BooleanList_to_LongList
registerImplicitConversion("Boolean List", "Integer List", convert_BooleanList_to_LongList)

for dataType in ["Float List", "Edge Indices", "Polygon Indices"]:
    registerImplicitConversion(dataType, "Integer List", "LongList.fromValues(value)")
