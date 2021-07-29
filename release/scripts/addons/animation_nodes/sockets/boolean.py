import bpy
from bpy.props import *
from .. events import propertyChanged
from .. data_structures import BooleanList
from .. base_types import AnimationNodeSocket, CListSocket
from .. utils.nodes import newNodeAtCursor, invokeTranslation

class BooleanSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_BooleanSocket"
    bl_label = "Boolean Socket"
    dataType = "Boolean"
    allowedInputTypes = ["Boolean", "Integer", "Float"]
    drawColor = (0.7, 0.7, 0.4, 1)
    storable = True
    comparable = True

    value = BoolProperty(default = True, update = propertyChanged)
    showCreateCompareNodeButton = BoolProperty(default = False)

    def drawProperty(self, layout, text, node):
        row = layout.row()
        row.prop(self, "value", text = text)

        if self.showCreateCompareNodeButton and self.isUnlinked:
            self.invokeFunction(row, node, "createCompareNode", icon = "PLUS", emboss = False,
                description = "Create compare node")

    def getValue(self):
        return self.value

    def setProperty(self, data):
        self.value = data

    def getProperty(self):
        return self.value

    def createCompareNode(self):
        node = newNodeAtCursor("an_CompareNode")
        self.linkWith(node.outputs[0])
        invokeTranslation()

    @classmethod
    def getConversionCode(cls, dataType):
        if dataType in ("Float", "Integer"):
            return "bool(value)"

    @classmethod
    def getDefaultValue(cls):
        return False

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, bool):
            return value, 0
        else:
            try: return bool(value), 1
            except: return cls.getDefaultValue(), 2


class BooleanListSocket(bpy.types.NodeSocket, CListSocket):
    bl_idname = "an_BooleanListSocket"
    bl_label = "Boolean List Socket"
    dataType = "Boolean List"
    baseDataType = "Boolean"
    allowedInputTypes = ["Boolean List", "Integer List", "Float List"]
    drawColor = (0.7, 0.7, 0.4, 0.5)
    storable = True
    comparable = False
    listClass = BooleanList

    @classmethod
    def getConversionCode(cls, dataType):
        if dataType == "Float List":
            return "AN.nodes.boolean.c_utils.convert_DoubleList_to_BooleanList(value)"
        if dataType == "Integer List":
            return "AN.nodes.boolean.c_utils.convert_LongList_to_BooleanList(value)"
