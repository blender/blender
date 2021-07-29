import bpy
from bpy.props import *
from bpy.types import Scene
from .. events import propertyChanged
from .. base_types import AnimationNodeSocket, PythonListSocket

class SceneSocket(bpy.types.NodeSocket, AnimationNodeSocket):
    bl_idname = "an_SceneSocket"
    bl_label = "Scene Socket"
    dataType = "Scene"
    allowedInputTypes = ["Scene"]
    drawColor = (0.2, 0.3, 0.4, 1)
    storable = False
    comparable = True

    sceneName = StringProperty(name = "Scene", update = propertyChanged)
    useGlobalScene = BoolProperty(name = "Use Global Scene", default = True,
        description = "Use the global scene for this node tree", update = propertyChanged)

    def drawProperty(self, layout, text, node):
        row = layout.row(align = True)
        if self.useGlobalScene:
            if text != "": text += ": "
            row.label(text + repr(self.nodeTree.scene.name))
        else:
            row.prop_search(self, "sceneName",  bpy.data, "scenes", text = text)
        row.prop(self, "useGlobalScene", text = "", icon = "WORLD")

    def getValue(self):
        if self.useGlobalScene:
            return self.nodeTree.scene
        return bpy.data.scenes.get(self.sceneName)

    def setProperty(self, data):
        self.sceneName, self.useGlobalScene = data

    def getProperty(self):
        return self.sceneName, self.useGlobalScene

    @classmethod
    def getDefaultValue(cls):
        return None

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, Scene) or value is None:
            return value, 0
        return cls.getDefaultValue(), 2


class SceneListSocket(bpy.types.NodeSocket, PythonListSocket):
    bl_idname = "an_SceneListSocket"
    bl_label = "Scene List Socket"
    dataType = "Scene List"
    baseDataType = "Scene"
    allowedInputTypes = ["Scene List"]
    drawColor = (0.2, 0.3, 0.4, 0.5)
    storable = False
    comparable = False

    useGlobalScene = BoolProperty(name = "Use Global Scene", default = True,
        description = "Use the global scene for this node tree", update = propertyChanged)

    def drawProperty(self, layout, text, node):
        row = layout.row(align = True)
        if self.useGlobalScene:
            if text != "": text += ": "
            row.label(text + "[{}]".format(repr(self.nodeTree.scene.name)))
        else:
            if text is "": text = self.text
            row.label(text)
        row.prop(self, "useGlobalScene", icon = "WORLD", text = "")

    def getValue(self):
        return [self.nodeTree.scene]

    @classmethod
    def getCopyExpression(cls):
        return "value[:]"

    @classmethod
    def correctValue(cls, value):
        if isinstance(value, list):
            if all(isinstance(element, Scene) or element is None for element in value):
                return value, 0
        return cls.getDefaultValue(), 2
