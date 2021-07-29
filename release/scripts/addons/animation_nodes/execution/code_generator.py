import re
import traceback
from itertools import chain
from .. utils.names import replaceVariableName
from .. problems import NodeFailesToCreateExecutionCode
from .. preferences import addonName, getExecutionCodeType
from .. tree_info import (iterLinkedSocketsWithInfo, isSocketLinked,
                          iterLinkedInputSocketsWithOriginDataType)



# Initial Socket Variables
##########################################

def getInitialVariables(nodes):
    variables = {}
    for node in nodes:
        nodeIdentifierPart = node.identifier[:4]
        for index, socket in enumerate(chain(node.inputs, node.outputs)):
            if socket.identifier.isidentifier():
                variables[socket] = "_" + socket.identifier + nodeIdentifierPart + str(index)
            else:
                variables[socket] = "__socket_" + str(socket.is_output) + "_" + str(index) + nodeIdentifierPart
    return variables


# Setup Code
##########################################

def iterSetupCodeLines(nodes, variables):
    yield from iter_Imports(nodes)
    yield get_LoadMeasurementsDict()
    yield from iter_GetNodeReferences(nodes)
    yield from iter_GetSocketValues(nodes, variables)

def iter_Imports(nodes = []):
    yield get_ImportModules(nodes)
    yield "import itertools"
    yield "from time import perf_counter as getCurrentTime"
    yield "from mathutils import Vector, Matrix, Quaternion, Euler"
    yield "AN = animation_nodes = sys.modules.get({})".format(repr(addonName))
    yield "from animation_nodes.data_structures import *"
    yield "from animation_nodes import algorithms"

def get_ImportModules(nodes):
    neededModules = {"bpy", "sys"}
    neededModules.update(getModulesNeededByNodes(nodes))
    modulesString = ", ".join(neededModules)
    return "import " + modulesString

def getModulesNeededByNodes(nodes):
    moduleNames = set()
    for node in nodes:
        moduleNames.update(node.getUsedModules())
    return list(moduleNames)

def get_LoadMeasurementsDict():
    return "_node_execution_times = animation_nodes.execution.measurements.getMeasurementsDict()"

def iter_GetNodeReferences(nodes):
    yield "nodes = bpy.data.node_groups[{}].nodes".format(repr(nodes[0].nodeTree.name))
    for node in nodes:
        yield "{} = nodes[{}]".format(node.identifier, repr(node.name))

def iter_GetSocketValues(nodes, variables):
    for node in nodes:
        for i, socket in enumerate(node.inputs):
            if not isSocketLinked(socket, node):
                yield getLoadSocketValueLine(socket, node, variables, i)

def getLoadSocketValueLine(socket, node, variables, index = None):
    return "{} = {}".format(variables[socket], getSocketValueExpression(socket, node, index))

def getSocketValueExpression(socket, node, index = None):
    socketsName = "inputs" if socket.isInput else "outputs"
    if index is None: index = socket.getIndex(node)

    if hasattr(socket, "getValue"):
        return "{}.{}[{}].getValue()".format(node.identifier, socketsName, index)
    elif hasattr(socket, "getDefaultValueCode"):
        return socket.getDefaultValueCode()
    else:
        return "{}.{}[{}].getDefaultValue()".format(node.identifier, socketsName, index)



# Node Execution Code
##########################################

def getGlobalizeStatement(nodes, variables):
    socketNames = [variables[socket] for socket in iterUnlinkedSockets(nodes) if socket.dataType != "Node Control"]
    if len(socketNames) == 0: return ""
    return "global " + ", ".join(socketNames)

def iterUnlinkedSockets(nodes):
    for node in nodes:
        yield from node.unlinkedInputs

def getFunction_IterNodeExecutionLines():
    mode = getExecutionCodeType()
    if mode == "DEFAULT":
        return iterNodeExecutionLines_Basic
    elif mode == "MONITOR":
        return iterNodeExecutionLines_Monitored
    elif mode == "MEASURE":
        return iterNodeExecutionLines_MeasureTimes
    elif mode == "BAKE":
        return iterNodeExecutionLines_Bake

def iterNodeExecutionLines_Basic(node, variables):
    yield from iterNodeCommentLines(node)
    yield from setupNodeForExecution(node, variables)
    try:
        yield from iterRealNodeExecutionLines(node, variables)
    except:
        handleExecutionCodeCreationException(node)

def iterNodeExecutionLines_Monitored(node, variables):
    yield from iterNodeCommentLines(node)
    yield from setupNodeForExecution(node, variables)
    yield "try:"
    try:
        for line in iterRealNodeExecutionLines(node, variables):
            yield "    " + line
        for socket in node.linkedOutputs:
            yield "    if not ({0} in globals() or {0} in locals()): raise Exception({1})".format(
                repr(variables[socket]), repr("Socket output has not been calculated: " + repr(socket.getDisplayedName())))
    except:
        handleExecutionCodeCreationException(node)
    yield "    pass"
    yield "except Exception as e:"
    yield "    animation_nodes.problems.NodeRaisesExceptionDuringExecution({}).report()".format(repr(node.identifier))
    yield "    raise"

def iterNodeExecutionLines_MeasureTimes(node, variables):
    yield from iterNodeCommentLines(node)
    try:
        yield "_execution_start_time = getCurrentTime()"
        yield from setupNodeForExecution(node, variables)
        yield from iterRealNodeExecutionLines(node, variables)
        yield "_execution_end_time = getCurrentTime()"
        yield "_node_execution_times[{}].registerTime(_execution_end_time - _execution_start_time)".format(repr(node.identifier))
    except:
        handleExecutionCodeCreationException(node)

def iterNodeExecutionLines_Bake(node, variables):
    yield from iterNodeCommentLines(node)
    yield from setupNodeForExecution(node, variables)
    try:
        yield from iterRealNodeExecutionLines(node, variables)
        yield from iterNodeBakeLines(node, variables)
    except:
        handleExecutionCodeCreationException(node)

def iterNodeCommentLines(node):
    yield ""
    yield "# Node: {} - {}".format(repr(node.nodeTree.name), repr(node.name))

def setupNodeForExecution(node, variables):
    yield from iterNodePreExecutionLines(node, variables)
    resolveInnerLinks(node, variables)

def iterNodePreExecutionLines(node, variables):
    yield from iterInputConversionLines(node, variables)
    yield from iterInputCopyLines(node, variables)

def iterInputConversionLines(node, variables):
    for socket, dataType in iterLinkedInputSocketsWithOriginDataType(node):
        if socket.dataType != dataType and socket.dataType != "All":
            convertCode = socket.getConversionCode(dataType)
            if convertCode is not None:
                socketPath = node.identifier + ".inputs[{}]".format(socket.getIndex(node))
                convertCode = replaceVariableName(convertCode, "value", variables[socket])
                convertCode = replaceVariableName(convertCode, "self", socketPath)
                newVariableName = variables[socket] + "_converted"
                yield "{} = {}".format(newVariableName, convertCode)
                variables[socket] = newVariableName

def iterInputCopyLines(node, variables):
    for socket in node.inputs:
        if socket.dataIsModified and socket.isCopyable() and not isSocketLinked(socket, node):
            newName = variables[socket] + "_copy"
            if hasattr(socket, "getDefaultValueCode"): line = "{} = {}".format(newName, socket.getDefaultValueCode())
            else: line = getCopyLine(socket, newName, variables)
            variables[socket] = newName
            yield line

def iterRealNodeExecutionLines(node, variables):
    localCode = node.getLocalExecutionCode()
    globalCode = makeGlobalExecutionCode(localCode, node, variables)
    yield from globalCode.splitlines()

def iterNodeBakeLines(node, variables):
    localCode = node.getLocalBakeCode()
    globalCode = makeGlobalExecutionCode(localCode, node, variables)
    yield from globalCode.splitlines()

def makeGlobalExecutionCode(localCode, node, variables):
    code = replaceVariableName(localCode, "self", node.identifier)
    nodeInputs = node.inputsByIdentifier
    for name, variable in node.getInputSocketVariables().items():
        code = replaceVariableName(code, variable, variables[nodeInputs[name]])
    nodeOutputs = node.outputsByIdentifier
    for name, variable in node.getOutputSocketVariables().items():
        code = replaceVariableName(code, variable, variables[nodeOutputs[name]])
    return code

def handleExecutionCodeCreationException(node):
    print("\n"*5)
    traceback.print_exc()
    NodeFailesToCreateExecutionCode(node.identifier).report()
    raise Exception("Node failed to create execution code")



# Modify Socket Variables
##########################################

def resolveInnerLinks(node, variables):
    inputs, outputs = node.inputsByIdentifier, node.outputsByIdentifier
    for inputName, outputName in node.iterInnerLinks():
        variables[outputs[outputName]] = variables[inputs[inputName]]

def linkOutputSocketsToTargets(node, variables, nodeByID):
    for socket in node.linkedOutputs:
        yield from linkSocketToTargets(socket, node, variables, nodeByID)

def linkSocketToTargets(socket, node, variables, nodeByID):
    targets = tuple(iterLinkedSocketsWithInfo(socket, node, nodeByID))
    needACopy = getTargetsThatNeedACopy(socket, targets)
    socket.execution.neededCopies = len(needACopy)

    for target in targets:
        if target in needACopy:
            yield getCopyLine(socket, variables[target], variables)
        else:
            variables[target] = variables[socket]

def getTargetsThatNeedACopy(socket, targets):
    if not socket.isCopyable(): return []
    modifiedTargets = [target for target in targets if target.dataIsModified]
    if socket.loop.copyAlways: return modifiedTargets
    if len(targets) == 1: return []
    if len(targets) > len(modifiedTargets): return modifiedTargets
    else: return modifiedTargets[1:]

def getCopyLine(fromSocket, targetName, variables):
    return "{} = {}".format(targetName, getCopyExpression(fromSocket, variables))

def getCopyExpression(socket, variables):
    return socket.getCopyExpression().replace("value", variables[socket])
