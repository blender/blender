import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... tree_info import getNodeByIdentifier, keepNodeState

class ReassignLoopParameterNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReassignLoopParameterNode"
    bl_label = "Reassign Loop Parameter"
    bl_width_default = 180
    onlySearchTags = True

    def identifierChanged(self, context):
        socket = self.linkedParameterSocket
        if socket:
            self.parameterDataType = self.linkedParameterSocket.dataType
            self.generateSockets()

    loopInputIdentifier = StringProperty(update = identifierChanged)
    parameterIdentifier = StringProperty(update = identifierChanged)
    parameterDataType = StringProperty()

    def draw(self, layout):
        socket = self.linkedParameterSocket
        if socket:
            layout.label("{} > {}".format(repr(socket.node.subprogramName), socket.text), icon = "GROUP_VERTEX")
        else:
            layout.label("Target does not exist", icon = "ERROR")

    def edit(self):
        network = self.network
        if network.type != "Invalid": return
        if network.loopInAmount != 1: return
        loopInput = network.getLoopInputNode()
        if self.loopInputIdentifier == loopInput.identifier: return
        self.loopInputIdentifier = loopInput.identifier

    @keepNodeState
    def generateSockets(self):
        self.clearSockets()
        self.newInput(self.parameterDataType, "New Value", "newValue", defaultDrawType = "TEXT_ONLY")
        self.newInput("Boolean", "Condition", "condition", hide = True)

    @property
    def linkedParameterSocket(self):
        try:
            inputNode = self.loopInputNode
            return inputNode.outputsByIdentifier[self.parameterIdentifier]
        except: pass

    @property
    def loopInputNode(self):
        try: return getNodeByIdentifier(self.loopInputIdentifier)
        except: return None

    @property
    def conditionSocket(self):
        try: return self.inputs["Condition"]
        except: return None
