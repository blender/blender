import re
import bpy
import types
import random
import functools
from bpy.props import *
from collections import defaultdict
from ... import tree_info
from ... utils.handlers import eventHandler
from ... ui.node_colors import colorAllNodes
from .. socket_templates import SocketTemplate
from . node_ui_extension import TextUIExtension, ErrorUIExtension
from ... preferences import getExecutionCodeType
from ... operators.callbacks import newNodeCallback
from ... sockets.info import toIdName as toSocketIdName
from ... utils.attributes import setattrRecursive, getattrRecursive
from ... operators.dynamic_operators import getInvokeFunctionOperator
from ... utils.nodes import getAnimationNodeTrees, iterAnimationNodes, idToSocket
from .. effects import PrependCodeEffect, ReturnDefaultsOnExceptionCodeEffect

from ... utils.blender_ui import (
    getNodeCornerLocation_BottomLeft,
    getNodeCornerLocation_BottomRight
)

from ... execution.measurements import (
    getMinExecutionTimeString,
    getMeasurementResultString
)
from ... tree_info import (
    getNetworkWithNode, getOriginNodes,
    getLinkedInputsDict, getLinkedOutputsDict, iterLinkedOutputSockets,
    iterUnlinkedInputSockets, getDirectlyLinkedSocketsIDs
)


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
    #              'NO_AUTO_EXECUTION'
    options = set()

    # can be "NONE", "ALWAYS" or "HIDDEN_ONLY"
    dynamicLabelType = "NONE"

    # can be "CUSTOM", "MESSAGE" or "EXCEPTION"
    errorHandlingType = "CUSTOM"
    class ControlledExecutionException(Exception):
        pass

    # should be a list of functions
    # each function takes a node as input
    # it should always return a CodeEffect
    codeEffects = []

    @classmethod
    def poll(cls, nodeTree):
        return nodeTree.bl_idname == "an_AnimationNodeTree"


    # functions subclasses can override
    ######################################

    def setup(self):
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

    def getExecutionCode(self, required):
        return []

    def getCodeEffects(self):
        return []

    def getBakeCode(self):
        return []

    def getUsedModules(self):
        return []

    def getUIExtensions(self):
        return None

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
        infoByNode[self.identifier] = infoByNode[sourceNode.identifier]
        self.duplicate(sourceNode)

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
        self._applySocketTemplates()
        tree_info.updateIfNecessary()
        self.edit()

    def refresh(self, context = None):
        if not self.isRefreshable:
            raise Exception("node is not refreshable")

        @AnimationNode.keepNodeState
        def refreshAndKeepNodeState(self):
            self._refresh()

        refreshAndKeepNodeState(self)

    def _refresh(self):
        self._clear()
        self._create()

    def _clear(self):
        self.clearSockets()
        infoByNode[self.identifier].clearCurrentStateData()

    def _create(self):
        self.create()
        infoByNode[self.identifier].codeEffects = list(self._iterNewCodeEffects())

    def _iterNewCodeEffects(self):
        for createCodeEffect in self.codeEffects:
            yield createCodeEffect(self)
        yield from self.getCodeEffects()

    @property
    def isRefreshable(self):
        return hasattr(self, "create")

    @classmethod
    def keepNodeState(cls, function):
        @functools.wraps(function)
        def wrapper(self, *args, **kwargs):
            infoByNode[self.identifier].fullState.update(self)
            result = function(self, *args, **kwargs)
            infoByNode[self.identifier].fullState.tryToReapplyState(self)
            return result
        return wrapper




    # Socket Templates
    ####################################################

    def _applySocketTemplates(self):
        propertyUpdates = dict()
        fixedProperties = set()

        for socket, template in self.iterSocketsWithTemplate():
            if template is None:
                continue
            # check if the template can actually influence the result
            if len(template.getRelatedPropertyNames() - fixedProperties) > 0:
                result = template.applyWithContext(self, socket, propertyUpdates, fixedProperties)
                if result is None: continue
                updates, fixed = result
                for name, value in updates.items():
                    if name not in fixedProperties:
                        propertyUpdates[name] = value
                fixedProperties.update(fixed)

        propertiesChanged = False
        for name, value in propertyUpdates.items():
            if getattrRecursive(self, name) != value:
                setattrRecursive(self, name, value)
                propertiesChanged = True
        if propertiesChanged:
            self.refresh()

    def iterSocketsWithTemplate(self):
        yield from self.iterInputSocketsWithTemplate()
        yield from self.iterOutputSocketsWithTemplate()

    def iterInputSocketsWithTemplate(self):
        inputsInfo = infoByNode[self.identifier].inputs
        for socket in self.inputs:
            yield (socket, inputsInfo[socket.identifier].template)

    def iterOutputSocketsWithTemplate(self):
        outputsInfo = infoByNode[self.identifier].outputs
        for socket in self.outputs:
            yield (socket, outputsInfo[socket.identifier].template)


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

    def newInput(self, *args, **kwargs):
        if len(args) == 1:
            if isinstance(args[0], SocketTemplate):
                template = args[0]
                socket = template.createInput(self)
                socketData = infoByNode[self.identifier].inputs[socket.identifier]
                socketData.initialize(template)
        elif len(args) == 2:
            socket = self.inputs.new(toSocketIdName(args[0]), args[1], args[1])
        elif len(args) == 3:
            socket = self.inputs.new(toSocketIdName(args[0]), args[1], args[2])
        else:
            raise Exception("invalid arguments")
        self._setSocketProperties(socket, kwargs)
        return socket

    def newOutput(self, *args, **kwargs):
        if len(args) == 1:
            if isinstance(args[0], SocketTemplate):
                template = args[0]
                socket = template.createOutput(self)
                socketData = infoByNode[self.identifier].outputs[socket.identifier]
                socketData.initialize(template)
        elif len(args) == 2:
            socket = self.outputs.new(toSocketIdName(args[0]), args[1], args[1])
        elif len(args) == 3:
            socket = self.outputs.new(toSocketIdName(args[0]), args[1], args[2])
        else:
            raise Exception("invalid arguments")
        socket.setAttributes(kwargs)
        return socket

    def _setSocketProperties(self, socket, properties):
        for key, value in properties.items():
            setattr(socket, key, value)

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


    # UI Extensions
    ####################################################

    def getAllUIExtensions(self):
        extensions = []

        if getExecutionCodeType() == "MEASURE":
            text = getMeasurementResultString(self)
            extensions.append(TextUIExtension(text))

        errorType = self.getErrorHandlingType()
        if errorType in ("MESSAGE", "EXCEPTION"):
            data = infoByNode[self.identifier]
            message = data.errorMessage
            if message is not None and data.showErrorMessage:
                extensions.append(ErrorUIExtension(message))

        extraExtensions = self.getUIExtensions()
        if extraExtensions is not None:
            extensions.extend(extraExtensions)

        return extensions


    # Error Handling
    ####################################################

    def getErrorHandlingType(self):
        return self.errorHandlingType

    def resetErrorMessage(self):
        infoByNode[self.identifier].errorMessage = None

    def setErrorMessage(self, message, show = True):
        data = infoByNode[self.identifier]
        data.errorMessage = message
        data.showErrorMessage = show

    def raiseErrorMessage(self, message, show = True):
        self.setErrorMessage(message, show)
        raise self.ControlledExecutionException(message)


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

    def getAllIdentifiersOfSocket(self, socket):
        ident = socket.identifier
        if socket.is_output:
            return infoByNode[self.identifier].outputs[ident].extraIdentifiers | {ident}
        else:
            return infoByNode[self.identifier].inputs[ident].extraIdentifiers | {ident}

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

    def getLocalExecutionCode(self, required, bake = False):
        inputVariables = self.getInputSocketVariables()
        outputVariables = self.getOutputSocketVariables()

        functionName = self.getExecutionFunctionName()
        if functionName is not None and hasattr(self, self.getExecutionFunctionName()):
            code = self.getLocalExecutionCode_ExecutionFunction(inputVariables, outputVariables)
        else:
            code = self.getLocalExecutionCode_GetExecutionCode(inputVariables, outputVariables, required)

        if bake:
            code = "\n".join((code, toString(self.getBakeCode())))

        return self.applyCodeEffects(code, required)

    def getLocalExecutionCode_ExecutionFunction(self, inputVariables, outputVariables):
        parameterString = ", ".join(inputVariables[socket.identifier] for socket in self.inputs)
        executionString = "self.{}({})".format(self.getExecutionFunctionName(), parameterString)

        outputString = ", ".join(outputVariables[socket.identifier] for socket in self.outputs)

        if outputString == "": return executionString
        else: return "{} = {}".format(outputString, executionString)

    def getLocalExecutionCode_GetExecutionCode(self, inputVariables, outputVariables, required):
        return toString(self.getExecutionCode(required))

    def applyCodeEffects(self, code, required):
        for effect in self.iterCodeEffectsToApply():
            code = toString(effect.apply(self, code, required))
        return code

    def iterCodeEffectsToApply(self):
        yield from infoByNode[self.identifier].codeEffects

        errorType = self.getErrorHandlingType()
        if errorType in ("MESSAGE", "EXCEPTION"):
            yield PrependCodeEffect("self.resetErrorMessage()")
        if errorType == "EXCEPTION":
            yield ReturnDefaultsOnExceptionCodeEffect("self.ControlledExecutionException")



# Non-Persistent data (will be removed when Blender is closed)
###########################################################################

nodeLabelMode = "DEFAULT"

def updateNodeLabelMode():
    global nodeLabelMode
    nodeLabelMode = "DEFAULT"
    if getExecutionCodeType() == "MEASURE":
        nodeLabelMode = "MEASURE"


class CurrentSocketData:
    def __init__(self):
        self.extraIdentifiers = set()
        self.template = None

    def initialize(self, template):
        self.extraIdentifiers = template.getSocketIdentifiers()
        self.template = template

class SocketPropertyState:
    @classmethod
    def fromSocket(cls, node, socket):
        self = cls()
        self.hide = socket.hide
        self.isUsed = socket.isUsed
        self.data = socket.getProperty()
        self.allIdentifiers = node.getAllIdentifiersOfSocket(socket)
        return self

class FullNodeState:
    def __init__(self):
        self.inputProperties = dict()
        self.outputProperties = dict()
        self.inputStates = dict()
        self.outputStates = dict()

    def update(self, node):
        for socket in node.inputs:
            self.inputProperties[(socket.identifier, socket.dataType)] = socket.getProperty()
            linkedSocketIDs = getDirectlyLinkedSocketsIDs(socket)
            for identifier in node.getAllIdentifiersOfSocket(socket):
                self.inputStates[identifier] = (socket.hide, socket.isUsed, linkedSocketIDs)

        for socket in node.outputs:
            self.outputProperties[(socket.identifier, socket.dataType)] = socket.getProperty()
            linkedSocketIDs = getDirectlyLinkedSocketsIDs(socket)
            for identifier in node.getAllIdentifiersOfSocket(socket):
                self.outputStates[identifier] = (socket.hide, socket.isUsed, linkedSocketIDs)

    def tryToReapplyState(self, node):
        for socket in node.inputs:
            self.tryToReapplySocketState(socket, self.inputProperties, self.inputStates)
        for socket in node.outputs:
            self.tryToReapplySocketState(socket, self.outputProperties, self.outputStates)

    def tryToReapplySocketState(self, socket, propertyData, stateData):
        if (socket.identifier, socket.dataType) in propertyData:
            socket.setProperty(propertyData[(socket.identifier, socket.dataType)])

        if socket.identifier in stateData:
            data = stateData[socket.identifier]
            socket.hide = data[0]
            socket.isUsed = data[1]
            for socketID in data[2]:
                try:
                    otherSocket = idToSocket(socketID)
                    if otherSocket.is_output or not otherSocket.is_linked:
                        socket.linkWith(idToSocket(socketID))
                except: pass

class NonPersistentNodeData:
    def __init__(self):
        self.clearCurrentStateData()
        self.fullState = FullNodeState()

    def clearCurrentStateData(self):
        '''data that can be cleared during every refresh'''
        self.inputs = defaultdict(CurrentSocketData)
        self.outputs = defaultdict(CurrentSocketData)
        self.codeEffects = []
        self.errorMessage = None
        self.showErrorMessage = True

infoByNode = defaultdict(NonPersistentNodeData)



# Identifiers
#####################################################

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
        location += node.location
    return location

def getRegionBottomLeft(node, region):
    return getNodeCornerLocation_BottomLeft(node, region)

def getRegionBottomRight(node, region):
    return getNodeCornerLocation_BottomRight(node, region)

def register():
    bpy.types.Node.toID = nodeToID
    bpy.types.Node.isAnimationNode = BoolProperty(name = "Is Animation Node", get = isAnimationNode)
    bpy.types.Node.viewLocation = FloatVectorProperty(name = "Region Location", size = 2, subtype = "XYZ", get = getViewLocation)
    bpy.types.Node.getNodeTree = getNodeTree
    bpy.types.Node.getRegionBottomLeft = getRegionBottomLeft
    bpy.types.Node.getRegionBottomRight = getRegionBottomRight

def unregister():
    pass
