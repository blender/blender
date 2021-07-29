import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... sockets.info import getSocketClasses

class DataInputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DataInputNode"
    bl_label = "Data Input"
    dynamicLabelType = "ALWAYS"
    onlySearchTags = True

    @classmethod
    def getSearchTags(cls):
        return [(socket.dataType + " Input", {"assignedType" : repr(socket.dataType)})
                 for socket in getSocketClasses() if socket.hasProperty()]

    assignedType = StringProperty(default = "Float", update = AnimationNode.refresh)

    showInViewport = BoolProperty(default = False, name = "Show in Viewport",
        description = "Draw the input of that node in the 'AN' category of the 3D view (Use the node label as name)")

    def create(self):
        socket = self.newInput(self.assignedType, "Input", "value",
            dataIsModified = True, hide = True)
        self.setupSocket(socket)
        socket = self.newOutput(self.assignedType, "Output", "value")
        self.setupSocket(socket)

    def setupSocket(self, socket):
        socket.display.text = True
        socket.text = self.assignedType
        socket.defaultDrawType = "PREFER_PROPERTY"

    def draw(self, layout):
        inputSocket = self.inputs[0]
        if inputSocket.hide:
            if hasattr(inputSocket, "drawProperty"):
                inputSocket.drawProperty(layout, "", self)
            else:
                layout.label("Default Used", icon = "INFO")

    def drawLabel(self):
        return self.inputs[0].dataType + " Input"

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignSocketType",
            text = "Change Type", icon = "TRIA_RIGHT")

        col = layout.column()
        col.active = self.inputs[0].hasProperty()
        col.prop(self, "showInViewport")

    def getExecutionCode(self):
        # needs no execution, because no value is changed
        return []

    def assignSocketType(self, dataType):
        # this automatically recreates the sockets
        self.assignedType = dataType
