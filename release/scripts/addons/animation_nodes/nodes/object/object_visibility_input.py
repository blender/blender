import bpy
from ... base_types import VectorizedNode

class ObjectVisibilityInputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ObjectVisibilityInputNode"
    bl_label = "Object Visibility Input"
    autoVectorizeExecution = True

    useObjectList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        self.newVectorizedOutput("Boolean", "useObjectList",
            ("Hide", "hide"), ("Hide", "hide"))
        self.newVectorizedOutput("Boolean", "useObjectList",
            ("Hide Render", "hideRender"), ("Hide Render", "hideRender"))
        self.newVectorizedOutput("Boolean", "useObjectList",
            ("Hide Select", "hideSelect"), ("Hide Select", "hideSelect"))
        self.newVectorizedOutput("Boolean", "useObjectList",
            ("Show Name", "showName"), ("Show Name", "showName"))
        self.newVectorizedOutput("Boolean", "useObjectList",
            ("Show Axis", "showAxis"), ("Show Axis", "showAxis"))
        self.newVectorizedOutput("Boolean", "useObjectList",
            ("Show Xray", "showXray"), ("Show Xray", "showXray"))

        for socket in self.outputs[2:]:
            socket.hide = True

    def getExecutionCode(self):
        isLinked = self.getLinkedBaseOutputsDict()
        if not any(isLinked.values()): return

        yield "if object is not None:"

        if isLinked["hide"]:        yield "    hide = object.hide"
        if isLinked["hideSelect"]:  yield "    hideSelect = object.hide_select"
        if isLinked["hideRender"]:  yield "    hideRender = object.hide_render"
        if isLinked["showName"]:    yield "    showName = object.show_name"
        if isLinked["showAxis"]:    yield "    showAxis = object.show_axis"
        if isLinked["showXray"]:    yield "    showXray = object.show_x_ray"

        yield "else: hide = hideSelect = hideRender = showName = showAxis = showXray = None"
