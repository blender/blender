import bpy
from bpy.props import *
from bpy.types import NlaStrip
from .. base_types import AnimationNodeSocket

class NLAStripSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_NLAStripSocket"
    bl_label = "NLA Strip Socket"
    dataType = "NlaStrip"
    allowedInputTypes = ["NlaStrip"]
    drawColor = (0.26, 0.20, 0.06, 1)
    storable = False
    comparable = True

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def getDefaultValueCode(cls):
        return "None"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, NlaStrip) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class NLAStripListSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_NLAStripListSocket"
    bl_label = "NLA Strip List Socket"
    dataType = "NlaStrip List"
    baseDataType = "NlaStrip"
    allowedInputTypes = ["NlaStrip List"]
    drawColor = (0.26, 0.20, 0.06, 0.5)
    storable = False
    comparable = False

    @classmethod
    def getDefaultValue(cls):
        return []

    @classmethod
    def getDefaultValueCode(cls):
        return "[]"

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, NlaStrip) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
