import bpy
from bpy.props import *
from bpy.types import Group
from .. events import propertyChanged
from .. base_types import AnimationNodeSocket, PythonListSocket

class ObjectGroupSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_ObjectGroupSocket"
    bl_label = "Object Group Socket"
    dataType = "Object Group"
    allowedInputTypes = ["Object Group"]
    drawColor = (0.3, 0.1, 0.1, 1.0)
    storable = False
    comparable = True

    groupName = StringProperty(update = propertyChanged)

    def drawProperty(self, layout, text, node):
        layout.prop_search(self, "groupName", bpy.data, "groups", text = text)

    def getValue(self):
        return bpy.data.groups.get(self.groupName)

    def setProperty(self, data):
        self.groupName = data

    def getProperty(self):
        return self.groupName

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Group) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class ObjectGroupListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_ObjectGroupListSocket"
    bl_label = "Object Group List Socket"
    dataType = "Object Group List"
    baseDataType = "Object Group"
    allowedInputTypes = ["Object Group List"]
    drawColor = (0.3, 0.1, 0.1, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, Group) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
