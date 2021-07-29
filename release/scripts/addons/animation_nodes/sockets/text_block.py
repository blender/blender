import bpy
from bpy.props import *
from .. events import propertyChanged
from .. base_types import AnimationNodeSocket, PythonListSocket

class TextBlockSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_TextBlockSocket"
    bl_label = "Text Block Socket"
    dataType = "Text Block"
    allowedInputTypes = ["Text Block"]
    drawColor = (0.5, 0.5, 0.5, 1)
    storable = False
    comparable = True

    textBlockName = StringProperty(update = propertyChanged)

    def drawProperty(self, layout, text, node):
        row = layout.row(align = True)
        row.prop_search(self, "textBlockName",  bpy.data, "texts", text = text)
        if self.getValue() is None:
            self.invokeFunction(row, node, "createTextBlock", icon = "ZOOMIN")
        else:
            self.invokeSelector(row, "AREA", node, "viewTextBlockInArea",
                icon = "ZOOM_SELECTED")

    def getValue(self):
        return bpy.data.texts.get(self.textBlockName)

    def setProperty(self, data):
        self.textBlockName = data

    def getProperty(self):
        return self.textBlockName

    @classmethod
    def getDefaultValue(cls):
        return None

    def createTextBlock(self):
        textBlock = bpy.data.texts.new("Text Block")
        self.textBlockName = textBlock.name

    def viewTextBlockInArea(self, area):
        area.type = "TEXT_EDITOR"
        area.spaces.active.text = self.getValue()

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, bpy.types.Text) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class TextBlockListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_TextBlockListSocket"
    bl_label = "Text Block List Socket"
    dataType = "Text Block List"
    baseDataType = "Text Block"
    allowedInputTypes = ["Text Block List"]
    drawColor = (0.5, 0.5, 0.5, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, bpy.types.Text) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
