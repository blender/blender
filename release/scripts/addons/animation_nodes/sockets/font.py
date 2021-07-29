import bpy
from bpy.props import *
from bpy.types import VectorFont
from .. events import propertyChanged
from .. base_types import AnimationNodeSocket, PythonListSocket

class FontSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_FontSocket"
    bl_label = "Font Socket"
    dataType = "Font"
    allowedInputTypes = ["Font"]
    drawColor = (0.444, 0.444, 0, 1)
    storable = False
    comparable = True

    fontName = StringProperty(update = propertyChanged)

    def drawProperty(self, layout, text, node):
        row = layout.row(align = True)
        row.prop_search(self, "fontName",  bpy.data, "fonts", icon = "NONE", text = text)
        self.invokeFunction(row, node, "assignFontOfActiveObject", icon = "EYEDROPPER")

    def getValue(self):
        return bpy.data.fonts.get(self.fontName)

    def setProperty(self, data):
        self.fontName = data

    def getProperty(self):
        return self.fontName

    def assignFontOfActiveObject(self):
        object = bpy.context.active_object
        if getattr(object, "type", "") == "FONT":
            self.fontName = object.data.font.name

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, VectorFont) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class FontListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_FontListSocket"
    bl_label = "Font List Socket"
    dataType = "Font List"
    baseDataType = "Font"
    allowedInputTypes = ["Font List"]
    drawColor = (0.444, 0.444, 0, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, VectorFont) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
