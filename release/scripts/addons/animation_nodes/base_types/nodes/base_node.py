import re
import bpy
import types
import random
from bpy.props import *
from collections import defaultdict
from ... utils.handlers import eventHandler
from ... ui.node_colors import colorAllNodes
from ... preferences import getExecutionCodeType
from ... operators.callbacks import newNodeCallback
from ... sockets.info import toIdName as toSocketIdName
from ... utils.blender_ui import iterNodeCornerLocations
from ... execution.measurements import getMinExecutionTimeString
from ... operators.dynamic_operators import getInvokeFunctionOperator
from ... utils.nodes import getAnimationNodeTrees, iterAnimationNodes
from ... tree_info import (getNetworkWithNode, getOriginNodes,
                           getLinkedInputsDict, getLinkedOutputsDict, iterLinkedOutputSockets,
                           iterUnlinkedInputSockets, keepNodeState)

socketEffectsByIdentifier = defaultdict(list)

class AnimationNode:
    bl_width_min = 40
    bl_width_max = 5000
    _isAnimationNode = True

    def useNetworkColorChanged(self, context):
        colorAllNodes()

    # unique string for each node; don't change it at all
    identifier = StringProperty(name = "Identifier", default = "")
    inInvalidNetwork = BoolProperty(name = "In Invalid Network", default = False)
    useNetworkColor = BoolProperty(name = "Use Network Color", default = True, update = useNetworkColorChanged)

    # used for the listboxes in the sidebar
    activeInputIndex = IntProperty()
    activeOutputIndex = IntProperty()

    searchTags = []
    onlySearchTags = False
    # can contain: 'NO_EXECUTION', 'NOT_IN_SUBPROGRAM',
    #              'NO_AUTO_EXECUTION', 'NO_TIMING',
    options = set()

    # can be "NONE", "ALWAYS" or "HIDDEN_ONLY"
    dynamicLabelType = "NONE"

    @classmethod
    def poll(cls, nodeTree):
        return nodeTree.bl_idname == "an_AnimationNodeTree"


    # functions subclasses can override
    ######################################

    def setup(self):
        pass

    def preCreate(self):
        pass

    def postCreate(self):
        pass

    # may be defined in nodes
    #def create(self):
    #    pass

    def edit(self):
        pass

    def duplicate(self, sourceNode):
        pass

    def delete(self):
        pass

    def save(self):
        pass

    def draw(self, layout):
        pass

    def drawLabel(self):
        return self.bl_label

    def drawAdvanced(self, layout):
        layout.label("Has no advanced settings")

    def socketRemoved(self):
        self.socketChanged()

    def socketMoved(self):
        self.socketChanged()

    def customSocketNameChanged(self, socket):
        self.socketChanged()

    def socketChanged(self):
        """
        Use this function when you don't need
        to know what happened exactly to the sockets
        """
        pass

    def getExecutionFunctionName(self):
        return "execute"

    def getExecutionCode(self):
        return []

    def getCodeEffects(self):
        return []

    def getBakeCode(self):
        return []

    def getUsedModules(self):
        return []

    def drawControlSocket(self, layout, socket):
        layout.alignment = "LEFT" if socket.isInput else "RIGHT"
        layout.label(socket.name)

    @classmethod
    def getSearchTags(cls):
        return cls.searchTags


    # Don't override these functions
    ######################################

    def init(self, context):
        self.width_hidden = 100
        self.identifier = createIdentifier()
        self.setup()
        if self.isRefreshable:
            self.refresh()

    def update(self):
        '''Don't use this function at all!!'''
        pass

    def copy(self, sourceNode):
        self.identifier = createIdentifier()
        self.copySocketEffects(sourceNode)
        self.duplicate(sourceNode)
        for socket, sourceSocket in zip(self.sockets, sourceNode.sockets):
            socket.alternativeIdentifiers = sourceSocket.alternativeIdentifiers

    def free(self):
        self.delete()
        self._clear()

    def draw_buttons(self, context, layout):
        if self.inInvalidNetwork: layout.label("Invalid Network", icon = "ERROR")
        if self.nodeTree.editNodeLabels: layout.prop(self, "label", text = "")
        self.draw(layout)

    def draw_label(self):
        if nodeLabelMode == "MEASURE" and self.hide:
            return getMinExecutionTimeString(self)

        if self.dynamicLabelType == "NONE":
            return self.bl_label
        elif self.dynamicLabelType == "ALWAYS":
            return self.drawLabel()
        elif self.dynamicLabelType == "HIDDEN_ONLY" and self.hide:
            return self.drawLabel()

        return self.bl_label


    # Update and Refresh
    ####################################################

    def updateNode(self):
        self.applySocketEffects()
        self.edit()

    def refresh(self, context = None):
        if not self.isRefreshable:
            raise Exception("node is not refreshable")

        @keepNodeState
        def refreshAndKeepNodeState(self):
            self._refresh()

        refreshAndKeepNodeState(self)

    def _refresh(self):
        self._clear()
        self._create()

    def _clear(self):
        self.clearSockets()
        self._clearSocketEffects()

    def _create(self):
        self.preCreate()
        self.create()
        self.postCreate()

    @property
    def isRefreshable(self):
        return hasattr(self, "create")


    # Socket Effects
    ####################################################

    def applySocketEffects(self):
        for effect in socketEffectsByIdentifier[self.identifier]:
            effect.apply(self)

    def _clearSocketEffects(self):
        if self.identifier in socketEffectsByIdentifier:
            del socketEffectsByIdentifier[self.identifier]

    def copySocketEffects(self, sourceNode):
        socketEffectsByIdentifier[self.identifier] = socketEffectsByIdentifier[sourceNode.identifier]

    def newSocketEffect(self, effect):
        socketEffectsByIdentifier[self.identifier].append(effect)


    # Remove Utilities
    ####################################################

    def removeLinks(self):
        removedLink = False
        for socket in self.sockets:
            if socket.removeLinks():
                removedLink = True
        return removedLink

    def remove(self):
        self.nodeTree.nodes.remove(self)


    # Create/Update/Remove Sockets
    ####################################################

    def newInput(self, type, name, identifier = None, alternativeIdentifier = None, **kwargs):
        idName = toSocketIdName(type)
        if idName is None:
            raise ValueError("Socket type does not exist: {}".format(repr(type)))
        if identifier is None: identifier = name
        socket = self.inputs.new(idName, name, identifier)
        self._setAlternativeIdentifier(socket, alternativeIdentifier)
        self._setSocketProperties(socket, kwargs)
        return socket

    def newOutput(self, type, name, identifier = None, alternativeIdentifier = None, **kwargs):
        idName = toSocketIdName(type)
        if idName is None:
            raise ValueError("Socket type does not exist: {}".format(repr(type)))
        if identifier is None: identifier = name
        socket = self.outputs.new(idName, name, identifier)
        self._setAlternativeIdentifier(socket, alternativeIdentifier)
        self._setSocketProperties(socket, kwargs)
        return socket

    def _setAlternativeIdentifier(self, socket, alternativeIdentifier):
        if isinstance(alternativeIdentifier, str):
            socket.alternativeIdentifiers = [alternativeIdentifier]
        elif isinstance(alternativeIdentifier, (list, tuple, set)):
            socket.alternativeIdentifiers = list(alternativeIdentifier)

    def _setSocketProperties(self, socket, properties):
        for key, value in properties.items():
            setattr(socket, key, value)

    def newInputGroup(self, selector, *socketsData):
        return self._newSocketGroup(int(selector), socketsData, self.newInput)

    def newOutputGroup(self, selector, *socketsData):
        return self._newSocketGroup(int(selector), socketsData, self.newOutput)

    def _newSocketGroup(self, index, socketsData, newSocketFunction):
        if 0 <= index < len(socketsData):
            data = socketsData[index]
            if len(data) == 3:
                socket = newSocketFunction(data[0], data[1], data[2])
            elif len(data) == 4:
                socket = newSocketFunction(data[0], data[1], data[2], **data[3])
            else:
                raise ValueError("invalid socket data")
            socket.alternativeIdentifiers = [data[2] for data in socketsData]
            return socket
        else:
            raise ValueError("invalid selector")

    def clearSockets(self):
        self.clearInputs()
        self.clearOutputs()

    def clearInputs(self):
        for socket in self.inputs:
            socket.free()
        self.inputs.clear()

    def clearOutputs(self):
        for socket in self.outputs:
            socket.free()
        self.outputs.clear()

    def removeSocket(self, socket):
        index = socket.getIndex(self)
        if socket.isOutput:
            if index < self.activeOutputIndex: self.activeOutputIndex -= 1
        else:
            if index < self.activeInputIndex: self.activeInputIndex -= 1
        socket.sockets.remove(socket)


    # Draw Utilities
    ####################################################

    def invokeFunction(self, layout, functionName, text = "", icon = "NONE",
                       description = "", emboss = True, confirm = False,
                       data = None, passEvent = False):
        idName = getInvokeFunctionOperator(description)
        props = layout.operator(idName, text = text, icon = icon, emboss = emboss)
        props.callback = self.newCallback(functionName)
        props.invokeWithData = data is not None
        props.confirm = confirm
        props.data = str(data)
        props.passEvent = passEvent

    def invokeSelector(self, layout, selectorType, functionName,
                       text = "", icon = "NONE", description = "", emboss = True, **kwargs):
        data, executionName = self._getInvokeSelectorData(selectorType, functionName, kwargs)
        self.invokeFunction(layout, executionName,
            text = text, icon = icon, description = description,
            emboss = emboss, data = data)

    def _getInvokeSelectorData(self, selector, function, kwargs):
        if selector == "DATA_TYPE":
            dataTypes = kwargs.get("dataTypes", "ALL")
            return function + "," + dataTypes, "_selector_DATA_TYPE"
        elif selector == "PATH":
            return function, "_selector_PATH"
        elif selector == "ID_KEY":
            return function, "_selector_ID_KEY"
        elif selector == "AREA":
            return function, "_selector_AREA"
        else:
            raise Exception("invalid selector type")

    def _selector_DATA_TYPE(self, data):
        functionName, socketGroup = data.split(",")
        bpy.ops.an.choose_socket_type("INVOKE_DEFAULT",
            socketGroup = socketGroup,
            callback = self.newCallback(functionName))

    def _selector_PATH(self, data):
        bpy.ops.an.choose_path("INVOKE_DEFAULT",
            callback = self.newCallback(data))

    def _selector_ID_KEY(self, data):
        bpy.ops.an.choose_id_key("INVOKE_DEFAULT",
            callback = self.newCallback(data))

    def _selector_AREA(self, data):
        bpy.ops.an.select_area("INVOKE_DEFAULT",
            callback = self.newCallback(data))

    def invokePopup(self, layout, drawFunctionName, executeFunctionName = "",
                    text = "", icon = "NONE", description = "", emboss = True, width = 250):
        data = drawFunctionName + "," + executeFunctionName + "," + str(width)
        self.invokeFunction(layout, "_openNodePopup", text = text, icon = icon,
                            description = description, emboss = emboss, data = data)

    def _openNodePopup(self, data):
        drawFunctionName, executeFunctionName, width = data.split(",")
        bpy.ops.an.node_popup("INVOKE_DEFAULT",
            nodeIdentifier = self.identifier,
            drawFunctionName = drawFunctionName,
            executeFunctionName = executeFunctionName,
            width = int(width))

    def newCallback(self, functionName):
        return newNodeCallback(self, functionName)


    # More Utilities
    ####################################################

    def getLinkedInputsDict(self):
        return getLinkedInputsDict(self)

    def getLinkedOutputsDict(self):
        return getLinkedOutputsDict(self)

    def iterInnerLinks(self):
        names = {}
        for identifier, variable in self.getInputSocketVariables().items():
            names[variable] = identifier
        for identifier, variable in self.getOutputSocketVariables().items():
            if variable in names:
                yield (names[variable], identifier)

    @property
    def nodeTree(self):
        return self.id_data

    @property
    def inputsByIdentifier(self):
        return {socket.identifier : socket for socket in self.inputs}

    @property
    def outputsByIdentifier(self):
        return {socket.identifier : socket for socket in self.outputs}

    @property
    def linkedOutputs(self):
        return tuple(iterLinkedOutputSockets(self))

    @property
    def activeInputSocket(self):
        if len(self.inputs) == 0: return None
        return self.inputs[self.activeInputIndex]

    @property
    def activeOutputSocket(self):
        if len(self.outputs) == 0: return None
        return self.outputs[self.activeOutputIndex]

    @property
    def originNodes(self):
        return getOriginNodes(self)

    @property
    def unlinkedInputs(self):
        return tuple(iterUnlinkedInputSockets(self))

    @property
    def network(self):
        return getNetworkWithNode(self)

    @property
    def sockets(self):
        return list(self.inputs) + list(self.outputs)

    def getInputSocketVariables(self):
        return {socket.identifier : socket.identifier for socket in self.inputs}

    def getOutputSocketVariables(self):
        return {socket.identifier : socket.identifier for socket in self.outputs}


    # Code Generation
    ####################################################

    def getLocalExecutionCode(self):
        inputVariables = self.getInputSocketVariables()
        outputVariables = self.getOutputSocketVariables()

        functionName = self.getExecutionFunctionName()
        if functionName is not None and hasattr(self, self.getExecutionFunctionName()):
            code = self.getLocalExecutionCode_ExecutionFunction(inputVariables, outputVariables)
        else:
            code = self.getLocalExecutionCode_GetExecutionCode(inputVariables, outputVariables)

        return self.applyCodeEffects(code)

    def getLocalExecutionCode_ExecutionFunction(self, inputVariables, outputVariables):
        parameterString = ", ".join(inputVariables[socket.identifier] for socket in self.inputs)
        executionString = "self.{}({})".format(self.getExecutionFunctionName(), parameterString)

        outputString = ", ".join(outputVariables[socket.identifier] for socket in self.outputs)

        if outputString == "": return executionString
        else: return "{} = {}".format(outputString, executionString)

    def getLocalExecutionCode_GetExecutionCode(self, inputVariables, outputVariables):
        return toString(self.getExecutionCode())

    def getLocalBakeCode(self):
        return self.applyCodeEffects(toString(self.getBakeCode()))

    def applyCodeEffects(self, code):
        for effect in self.getCodeEffects():
            code = toString(effect.apply(self, code))
        return code


@eventHandler("SCENE_UPDATE_POST")
def createMissingIdentifiers(scene = None):
    def unidentifiedNodes():
        for tree in getAnimationNodeTrees():
            for node in tree.nodes:
                if not issubclass(type(node), AnimationNode): continue
                if node.identifier == "": yield node

    for node in unidentifiedNodes():
        node.identifier = createIdentifier()

def createIdentifier():
    identifierLength = 15
    characters = "abcdefghijklmnopqrstuvwxyz" + "0123456789"
    choice = random.SystemRandom().choice
    return "_" + ''.join(choice(characters) for _ in range(identifierLength))

def toString(code):
    if isinstance(code, str):
        return code
    if hasattr(code, "__iter__"):
        return "\n".join(code)
    return ""


@eventHandler("FILE_SAVE_PRE")
def callSaveMethods():
    for node in iterAnimationNodes():
        node.save()


nodeLabelMode = "DEFAULT"

def updateNodeLabelMode():
    global nodeLabelMode
    nodeLabelMode = "DEFAULT"
    if getExecutionCodeType() == "MEASURE":
        nodeLabelMode = "MEASURE"



# Register
###############################################################

def nodeToID(node):
    return (node.id_data.name, node.name)

def isAnimationNode(node):
    return hasattr(node, "_isAnimationNode")

def getNodeTree(node):
    return node.id_data

def getViewLocation(node):
    location = node.location.copy()
    while node.parent:
        node = node.parent
        location += node.location.copy()
    return location

def getRegionBottomLeft(node, region):
    return next(iterNodeCornerLocations([node], region, horizontal = "LEFT"))

def getRegionBottomRight(node, region):
    return next(iterNodeCornerLocations([node], region, horizontal = "RIGHT"))

def register():
    bpy.types.Node.toID = nodeToID
    bpy.types.Node.isAnimationNode = BoolProperty(name = "Is Animation Node", get = isAnimationNode)
    bpy.types.Node.viewLocation = FloatVectorProperty(name = "Region Location", size = 2, subtype = "XYZ", get = getViewLocation)
    bpy.types.Node.getNodeTree = getNodeTree
    bpy.types.Node.getRegionBottomLeft = getRegionBottomLeft
    bpy.types.Node.getRegionBottomRight = getRegionBottomRight

def unregister():
    pass
