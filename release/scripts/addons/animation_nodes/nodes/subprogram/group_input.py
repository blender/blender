import re
import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... utils.names import getRandomString
from ... utils.layout import splitAlignment
from . subprogram_base import SubprogramBaseNode
from ... utils.nodes import newNodeAtCursor, invokeTranslation
from . subprogram_sockets import SubprogramData, subprogramInterfaceChanged

class GroupInputNode(bpy.types.Node, AnimationNode, SubprogramBaseNode):
    bl_idname = "an_GroupInputNode"
    bl_label = "Group Input"
    bl_width_default = 180

    def setup(self):
        self.randomizeNetworkColor()
        self.subprogramName = "My Group"
        self.newOutput("Node Control", "New Input", margin = 0.15)

    def draw(self, layout):
        layout.separator()
        layout.prop(self, "subprogramName", text = "", icon = "GROUP_VERTEX")
        if self.outputNode is None:
            self.invokeFunction(layout, "createGroupOutputNode", text = "Output Node", icon = "PLUS")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.label("Description:")
        col.prop(self, "subprogramDescription", text = "")

        col = layout.column()
        col.label("Parameter Defaults:")
        box = col.box()
        for socket in list(self.outputs)[:-1]:
            socket.drawSocket(box, socket.text, node = self, drawType = "TEXT_PROPERTY_OR_NONE")

    def drawControlSocket(self, layout, socket):
        left, right = splitAlignment(layout)
        self.invokeSelector(left, "DATA_TYPE", "newGroupInput",
            icon = "ZOOMIN", emboss = False)
        right.label(socket.name)

    def edit(self):
        for target in self.outputs[-1].dataTargets:
            if target.dataType == "Node Control": continue
            socket = self.newGroupInput(target.dataType, target.getDisplayedName(), target.getProperty())
            socket.linkWith(target)
        self.outputs[-1].removeLinks()

    def newGroupInput(self, dataType, name = None, defaultValue = None):
        if name is None: name = dataType
        socket = self.newOutput(dataType, name, getRandomString(10))
        if defaultValue is not None: socket.setProperty(defaultValue)
        socket.text = name
        socket.moveable = True
        socket.removeable = True
        socket.display.text = True
        socket.textProps.editable = True
        socket.display.textInput = True
        socket.display.removeOperator = True
        socket.moveUp()
        return socket

    def socketChanged(self):
        subprogramInterfaceChanged()

    def delete(self):
        self.clearSockets()
        subprogramInterfaceChanged()

    def duplicate(self, sourceNode):
        self.randomizeNetworkColor()
        match = re.search("(.*) ([0-9]+)$", self.subprogramName)
        if match: self.subprogramName = match.group(1) + " " + str(int(match.group(2)) + 1)
        else: self.subprogramName += " 2"

    def getSocketData(self):
        data = SubprogramData()
        for socket in self.outputs[:-1]:
            data.newInputFromSocket(socket)
        if self.outputNode is not None:
            for socket in self.outputNode.inputs[:-1]:
                data.newOutputFromSocket(socket)
        return data

    @property
    def outputNode(self):
        return self.network.getGroupOutputNode()

    def createGroupOutputNode(self):
        node = newNodeAtCursor("an_GroupOutputNode")
        node.groupInputIdentifier = self.identifier
        invokeTranslation()
