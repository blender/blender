import bpy
from .. data_structures import PolygonIndicesList
from .. base_types import AnimationNodeSocket, ListSocket

class PolygonIndicesSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_PolygonIndicesSocket"
    bl_label = "Polygon Indices Socket"
    dataType = "Polygon Indices"
    allowedInputTypes = ["Polygon Indices"]
    drawColor = (0.6, 0.3, 0.8, 1)
    comparable = True
    storable = True

    @classmethod
    def getDefaultValue(cls):
        return (0, 1, 2)

    @classmethod
    def getDefaultValueCode(cls):
        return "(0, 1, 2)"

    @classmethod
    def correctValue(cls, value):
        if isPolygon(value): return value, 0
        else: return cls.getDefaultValue(), 2


class PolygonIndicesListSocket(bpy.types.NodeSocket, ListSocket):
    bl_idname = "an_PolygonIndicesListSocket"
    bl_label = "Polygon Indices List Socket"
    dataType = "Polygon Indices List"
    baseDataType = "Polygon Indices"
    allowedInputTypes = ["Polygon Indices List"]
    drawColor = (0.6, 0.3, 0.8, 0.5)
    storable = True
    comparable = False

    @classmethod
    def getDefaultValue(cls):
        return PolygonIndicesList()

    @classmethod
    def getDefaultValueCode(cls):
        return "PolygonIndicesList()"

    @classmethod
    def getCopyExpression(cls):
        return "value.copy()"

    @classmethod
    def getFromValuesCode(cls):
        return "PolygonIndicesList.fromValues(value)"

    @classmethod
    def getJoinListsCode(cls):
        return "PolygonIndicesList.join(value)"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, PolygonIndicesList):
            return value, 0
        try: return PolygonIndicesList.fromValues(value), 1
        except: return cls.getDefaultValue(), 2
