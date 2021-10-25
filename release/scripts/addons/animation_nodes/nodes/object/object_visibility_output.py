import bpy
from bpy.props import *
from ... base_types import AnimationNode, VectorizedSocket

attributes = [
    ("Hide", "hide", "hide", "useHideList"),
    ("Hide Render", "hideRender", "hide_render", "useHideRenderList"),
    ("Hide Select", "hideSelect", "hide_select", "useHideSelectList"),
    ("Show Name", "showName", "show_name", "useShowNameList"),
    ("Show Axis", "showAxis", "show_axis", "useShowAxisList"),
    ("Show X-Ray", "showXRay", "show_x_ray", "useShowXRayList")
]

class ObjectVisibilityOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectVisibilityOutputNode"
    bl_label = "Object Visibility Output"
    codeEffects = [VectorizedSocket.CodeEffect]

    useObjectList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"),
            codeProperties = dict(allowListExtension = False)))

        for name, identifier, _, useListName in attributes:
            self.newInput(VectorizedSocket("Boolean", [useListName, "useObjectList"],
                (name, identifier), (name, identifier)))

        self.newOutput(VectorizedSocket("Object", "useObjectList",
            ("Object", "object"), ("Objects", "objects")))

        for socket in self.inputs[1:]:
            socket.useIsUsedProperty = True
            socket.isUsed = False
            socket.value = False

        for socket in self.inputs[3:]:
            socket.hide = True

    def getExecutionCode(self, required):
        yield "if object is not None:"
        for name, identifier, attr, _ in attributes:
            if self.inputs[name].isUsed:
                yield "    object.{} = {}".format(attr, identifier)
        yield "    pass"

    def getBakeCode(self):
        yield "if object is not None:"
        for name, _, attr, _ in attributes:
            if self.inputs[name].isUsed:
                yield "    object.keyframe_insert('{}')".format(attr)
        yield "    pass"

for *_, useListName in attributes:
    setattr(ObjectVisibilityOutputNode, useListName, VectorizedSocket.newProperty())
