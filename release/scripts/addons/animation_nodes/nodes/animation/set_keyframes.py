import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import propertyChanged

pathTypes = ("Custom", "Location", "Rotation", "Scale", "LocRotScale")
pathTypeItems = [(pathType, pathType, "") for pathType in pathTypes]

class KeyframePath(bpy.types.PropertyGroup):
    bl_idname = "an_KeyframePath"
    path = StringProperty(default = "", update = propertyChanged, description = "Path to the property")
    index = IntProperty(default = -1, update = propertyChanged, min = -1, soft_max = 2, description = "Used index if the path points to an array (-1 will set a keyframe on every index)")

class SetKeyframesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SetKeyframesNode"
    bl_label = "Set Keyframes"
    bl_width_default = 200

    paths = CollectionProperty(type = KeyframePath)

    selectedPathType = EnumProperty(default = "Location", items = pathTypeItems, name = "Path Type")
    attributePath = StringProperty(default = "", name = "Attribute Path")

    def create(self):
        self.newInput("Boolean", "Enable", "enable", value = False)
        self.newInput("Boolean", "Set Keyframe", "setKeyframe")
        self.newInput("Boolean", "Remove Unwanted", "removeUnwanted")
        self.newInput("Object", "Object", "object")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "selectedPathType", text = "")
        self.invokeFunction(row, "addKeyframePath", icon = "PLUS")

        col = layout.column(align = True)
        for i, item in enumerate(self.paths):
            row = col.row(align = True)
            split = row.split(align = True, percentage = 0.7)
            split.prop(item, "path", text = "")
            split.prop(item, "index", text = "")
            self.invokeFunction(row, "removeItemFromList", icon = "X", data = str(i))

    def execute(self, enable, setKeyframe, removeUnwanted, object):
        if not enable: return
        frame = self.nodeTree.scene.frame_current_final
        if setKeyframe:
            for item in self.paths:
                try:
                    obj, path = self.getResolvedNestedPath(object, item.path)
                    obj.keyframe_insert(data_path = path, frame = frame, index = item.index)
                except: pass
        elif removeUnwanted:
            for item in self.paths:
                try:
                    obj, path = self.getResolvedNestedPath(object, item.path)
                    obj.keyframe_delete(data_path = path, frame = frame, index = item.index)
                except: pass

    def getResolvedNestedPath(self, object, path):
        index = path.find(".")
        if index == -1: return object, path
        else:
            data = eval("object." + path[:index])
            return data, path[index+1:]

    def newPath(self, path, index = -1):
        item = self.paths.add()
        item.path = path
        item.index = index

    def addKeyframePath(self):
        type = self.selectedPathType
        if type == "Custom": self.newPath("")
        elif type == "Location": self.newPath("location")
        elif type == "Rotation": self.newPath("rotation_euler")
        elif type == "Scale": self.newPath("scale")
        elif type == "LocRotScale":
            self.newPath("location")
            self.newPath("rotation_euler")
            self.newPath("scale")

    def removeItemFromList(self, strIndex):
        self.paths.remove(int(strIndex))
