import re
import bpy
from bpy.props import *
from operator import attrgetter
from ... events import networkChanged
from ... utils.names import getRandomString
from ... utils.layout import splitAlignment
from ... tree_info import getNodeByIdentifier
from ... base_types import AnimationNode
from . subprogram_base import SubprogramBaseNode
from ... utils.nodes import newNodeAtCursor, invokeTranslation
from ... sockets.info import toListDataType, toIdName, isBase, toListIdName, toBaseDataType
from . subprogram_sockets import SubprogramData, subprogramInterfaceChanged, NoDefaultValue

class LoopInputNode(bpy.types.Node, AnimationNode, SubprogramBaseNode):
    bl_idname = "an_LoopInputNode"
    bl_label = "Loop Input"
    bl_width_default = 180

    def setup(self):
        self.randomizeNetworkColor()
        self.subprogramName = "My Loop"
        self.newOutput("Integer", "Index")
        self.newOutput("Integer", "Iterations")
        self.newOutput("Node Control", "New Iterator").margin = 0.15
        self.newOutput("Node Control", "New Parameter").margin = 0.15

    def draw(self, layout):
        layout.separator()
        left, right = splitAlignment(layout)
        self.invokeSelector(left, "DATA_TYPE", "createGeneratorOutputNode",
            dataTypes = "LIST", text = "", icon = "ZOOMIN", emboss = False)
        right.label("New Generator Output")
        layout.prop(self, "subprogramName", text = "", icon = "GROUP_VERTEX")

    def drawAdvanced(self, layout):
        col = layout.column()
        col.label("Description:")
        col.prop(self, "subprogramDescription", text = "")

        layout.separator()

        col = layout.column()
        col.label("Iterator Sockets:")
        box = col.box()
        for socket in self.getIteratorSockets():
            box.prop(socket.loop, "useAsOutput", text = "Use {} as Output".format(repr(socket.text)))
        self.invokeSelector(box, "DATA_TYPE", "newIterator",
            dataTypes = "LIST", text = "New Iterator", icon = "PLUS")

        layout.separator()

        col = layout.column()
        col.label("Parameter Sockets:")
        box = col.box()
        for socket in self.getParameterSockets():
            subcol = box.column(align = False)
            row = subcol.row()
            row.label(repr(socket.text))
            self.invokeFunction(row, "createReassignParameterNode", text = "Reassign", data = socket.identifier)
            row = subcol.row()
            row.prop(socket.loop, "useAsInput", text = "Input")
            row.prop(socket.loop, "useAsOutput", text = "Output")
            subrow = row.row()
            subrow.active = socket.isCopyable()
            subrow.prop(socket.loop, "copyAlways", text = "Copy")
            socket.drawSocket(subcol, text = "Default", node = self, drawType = "PROPERTY_ONLY")
        self.invokeSelector(box, "DATA_TYPE", "newParameter",
            text = "New Parameter", icon = "PLUS")

        layout.separator()

        col = layout.column()
        col.label("List Generators:")
        box = col.box()
        subcol = box.column(align = True)
        for i, node in enumerate(self.getSortedGeneratorNodes()):
            row = subcol.row(align = True)
            row.label("{} - {}".format(repr(node.outputName), node.listDataType))
            self.invokeFunction(row, "moveGeneratorOutput", data = "{};-1".format(i), icon = "TRIA_UP")
            self.invokeFunction(row, "moveGeneratorOutput", data = "{};1".format(i), icon = "TRIA_DOWN")
        self.invokeSelector(box, "DATA_TYPE", "createGeneratorOutputNode",
            dataTypes = "LIST", text = "New Generator", icon = "PLUS")

        self.invokeFunction(layout, "createBreakNode", text = "New Break Condition", icon = "PLUS")

    def edit(self):
        for target in self.newIteratorSocket.dataTargets:
            if target.dataType == "Node Control": continue
            if not isBase(target.dataType): continue
            listDataType = toListDataType(target.dataType)
            socket = self.newIterator(listDataType, target.getDisplayedName())
            socket.linkWith(target)

        for target in self.newParameterSocket.dataTargets:
            if target.dataType == "Node Control": continue
            socket = self.newParameter(target.dataType, target.getDisplayedName(), target.getProperty())
            socket.linkWith(target)

        self.newIteratorSocket.removeLinks()
        self.newParameterSocket.removeLinks()

    def drawControlSocket(self, layout, socket):
        isParameterSocket = socket == self.outputs[-1]
        function, dataTypes = ("newParameter", "ALL") if isParameterSocket else ("newIterator", "LIST")

        left, right = splitAlignment(layout)
        self.invokeSelector(left, "DATA_TYPE", function,
            dataTypes = dataTypes, icon = "ZOOMIN", emboss = False)
        right.label(socket.name)


    def newIterator(self, listDataType, name = None):
        baseDataType = toBaseDataType(listDataType)
        if name is None: name = baseDataType

        socket = self.newOutput(baseDataType, name, "iterator_" + getRandomString(5))
        socket.moveTo(self.newIteratorSocket.getIndex())
        self.setupSocket(socket, name, moveGroup = 1)
        return socket

    def newParameter(self, dataType, name = None, defaultValue = None):
        if name is None: name = dataType

        socket = self.newOutput(dataType, name, "parameter_" + getRandomString(5))
        if defaultValue: socket.setProperty(defaultValue)
        socket.moveTo(self.newParameterSocket.getIndex())
        socket.loop.copyAlways = False
        self.setupSocket(socket, name, moveGroup = 2)
        return socket

    def setupSocket(self, socket, name, moveGroup):
        socket.text = name
        socket.moveGroup = moveGroup
        socket.moveable = True
        socket.removeable = True
        socket.display.text = True
        socket.textProps.editable = True
        socket.display.textInput = True
        socket.display.removeOperator = True
        socket.loop.useAsInput = True


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
        if len(self.outputs) == 0: return data

        self.insertIteratorData(data)
        self.insertGeneratorData(data)
        self.insertParameterData(data)

        return data

    def insertIteratorData(self, data):
        iteratorSockets = self.getIteratorSockets()
        if len(iteratorSockets) == 0:
            data.newInput("an_IntegerSocket", "loop_iterations", "Iterations", 0)
        else:
            for socket in iteratorSockets:
                name = socket.text + " List"
                data.newInput(toListIdName(socket.bl_idname), socket.identifier, name, NoDefaultValue)
                if socket.loop.useAsOutput:
                    data.newOutput(toListIdName(socket.bl_idname), socket.identifier, name)

    def insertParameterData(self, data):
        for socket in self.getParameterSockets():
            if socket.loop.useAsInput:
                socketData = data.newInputFromSocket(socket)
                socketData.identifier += "_input"
            if socket.loop.useAsOutput:
                socketData = data.newOutputFromSocket(socket)
                socketData.identifier += "_output"

    def insertGeneratorData(self, data):
        for node in self.getSortedGeneratorNodes():
            data.newOutput(toIdName(node.listDataType), node.identifier, node.outputName)


    def createGeneratorOutputNode(self, dataType):
        node = newNodeAtCursor("an_LoopGeneratorOutputNode")
        node.loopInputIdentifier = self.identifier
        node.listDataType = dataType
        node.outputName = dataType
        invokeTranslation()
        subprogramInterfaceChanged()

    def createReassignParameterNode(self, socketIdentifier):
        socket = self.outputsByIdentifier[socketIdentifier]
        node = newNodeAtCursor("an_ReassignLoopParameterNode")
        node.loopInputIdentifier = self.identifier
        node.parameterIdentifier = socketIdentifier
        invokeTranslation()
        subprogramInterfaceChanged()

    def createBreakNode(self):
        node = newNodeAtCursor("an_LoopBreakNode")
        node.loopInputIdentifier = self.identifier
        invokeTranslation()

    def moveGeneratorOutput(self, data):
        index, direction = data.split(";")
        index = int(index)
        direction = int(direction)

        sortedGenerators = self.getSortedGeneratorNodes()
        if index == 0 and direction == -1: return
        if index == len(sortedGenerators) - 1 and direction == 1: return

        node = sortedGenerators[index]
        otherNode = sortedGenerators[index + direction]

        node.sortIndex, otherNode.sortIndex = otherNode.sortIndex, node.sortIndex
        subprogramInterfaceChanged()

    @property
    def newIteratorSocket(self):
        return self.outputs["New Iterator"]

    @property
    def newParameterSocket(self):
        return self.outputs["New Parameter"]

    @property
    def indexSocket(self):
        return self.outputs["Index"]

    @property
    def iterationsSocket(self):
        return self.outputs["Iterations"]

    @property
    def iterateThroughLists(self):
        return len(self.getIteratorSockets()) > 0

    def getIteratorSockets(self):
        return self.outputs[2:self.newIteratorSocket.getIndex(self)]

    def getParameterSockets(self):
        return self.outputs[self.newIteratorSocket.getIndex(self) + 1:self.newParameterSocket.getIndex(self)]

    def getBreakNodes(self, nodeByID):
        return self.network.getBreakNodes(nodeByID)

    def getSortedGeneratorNodes(self, nodeByID = None):
        nodes = self.network.getGeneratorOutputNodes(nodeByID)
        nodes.sort(key = attrgetter("sortIndex"))
        try:
            for i, node in enumerate(nodes):
                node.sortIndex = i
        except: pass # The function has been called while drawing the interface
        return nodes

    def getReassignParameterNodes(self, nodeByID = None):
        return [node for node in self.network.getReassignParameterNodes(nodeByID) if node.linkedParameterSocket]
