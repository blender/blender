import bpy
import sys
from bpy.props import *
from .. events import propertyChanged
from .. data_structures import DoubleList
from .. base_types import AnimationNodeSocket, CListSocket

def getValue(self):
    return min(max(self.minValue, self.get("value", 0)), self.maxValue)
def setValue(self, value):
    self["value"] = min(max(self.minValue, value), self.maxValue)

class FloatSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_FloatSocket"
    bl_label = "Float Socket"
    dataType = "Float"
    allowedInputTypes = ["Float", "Integer", "Boolean"]
    drawColor = (0.4, 0.4, 0.7, 1)
    comparable = True
    storable = True

    value = FloatProperty(default = 0.0,
        set = setValue, get = getValue,
        update = propertyChanged)

    minValue = FloatProperty(default = -1e10)
    maxValue = FloatProperty(default = sys.float_info.max)

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
    def getConversionCode(cls, dataType):
        if dataType in "Boolean":
            return "float(value)"

    @classmethod
    def getDefaultValue(cls):
        return 0

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, (float, int)):
            return value, 0
        else:
            try: return float(value), 1
            except: return cls.getDefaultValue(), 2


class FloatListSocket(bpy.types.NodeSocket, CListSocket):
    bl_idname = "an_FloatListSocket"
    bl_label = "Float List Socket"
    dataType = "Float List"
    baseDataType = "Float"
    allowedInputTypes = ["Float List", "Integer List", "Edge Indices", "Polygon Indices", "Boolean List"]
    drawColor = (0.4, 0.4, 0.7, 0.5)
    storable = True
    comparable = False
    listClass = DoubleList

    @classmethod
    def getConversionCode(cls, dataType):
        if dataType == "Boolean List":
            return "AN.nodes.boolean.c_utils.convert_BooleanList_to_DoubleList(value)"
        if dataType in cls.allowedInputTypes:
            return "DoubleList.fromValues(value)"
