import bpy
from bpy.props import *
from ... utils.layout import writeText
from ... base_types import AnimationNode

layerChoosingTypeItems = [
    ("SINGLE", "Single", ""),
    ("MULTIPLE", "Multiple", "") ]

class ObjectLayerVisibilityOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectLayerVisibilityOutputNode"
    bl_label = "Object Layer Visibility Output"

    errorMessage = StringProperty()

    layerChoosingType = EnumProperty(name = "Layer Choosing Type", default = "MULTIPLE",
        items = layerChoosingTypeItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Object", "Object", "object", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Object", "Object", "object")
        self.createLayerInputSockets()

    def createLayerInputSockets(self):
        if self.layerChoosingType == "SINGLE":
            self.newInput("Integer", "Layer Index", "layerIndex")
        elif self.layerChoosingType == "MULTIPLE":
            for i in range(1, 21):
                self.newInput("Boolean", "Layer " + str(i), "layer" + str(i), value = False)
            for socket in self.inputs[4:]:
                socket.hide = True
            self.inputs[1].value = True

    def draw(self, layout):
        layout.prop(self, "layerChoosingType", text = "Type")
        if self.errorMessage != "":
            writeText(layout, self.errorMessage, icon = "ERROR", width = 20)

    def getExecutionCode(self):
        yield "if object:"
        if self.layerChoosingType == "MULTIPLE":
            yield "    visibilities = [{}]".format(", ".join("layer" + str(i) for i in range(1, 21)))
            yield "    object.layers = visibilities"
            yield "    self.errorMessage = '' if any(visibilities) else 'The target has to be visible on at least one layer'"

        if self.layerChoosingType == "SINGLE":
            yield "    if 0 <= layerIndex <= 19:"
            yield "        self.errorMessage = ''"
            yield "        layers = [False] * 20"
            yield "        layers[layerIndex] = True"
            yield "        object.layers = layers"
            yield "    else:"
            yield "        self.errorMessage = 'The layer index has to be between 0 and 19'"
