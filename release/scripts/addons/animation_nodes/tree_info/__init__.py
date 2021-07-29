import functools
from itertools import chain
from .. utils.timing import measureTime
from .. utils.nodes import idToNode, idToSocket, createNodeByIdDict

def __setup():
    from . forest_data import ForestData
    from . networks import NodeNetworks

    global _needsUpdate, _forestData, _networks

    _needsUpdate = True
    _forestData = ForestData()
    _networks = NodeNetworks()


# Public API
##################################

@measureTime
def update():
    _forestData.update()

    nodeByID = createNodeByIdDict()
    _networks.update(_forestData, nodeByID)
    nodeByID.clear()

    global _needsUpdate
    _needsUpdate = False

def updateIfNecessary():
    if _needsUpdate:
        update()

def treeChanged():
    global _needsUpdate
    _needsUpdate = True



def getNodeByIdentifier(identifier):
    return idToNode(_forestData.nodeByIdentifier[identifier])

def getIdentifierAmount():
    return len(_forestData.nodeByIdentifier)

def getNodesByType(idName, nodeByID = None):
    if nodeByID is None:
        return [idToNode(nodeID) for nodeID in _forestData.nodesByType[idName]]
    else:
        return [nodeByID[nodeID] for nodeID in _forestData.nodesByType[idName]]

def nodeOfTypeExists(idName):
    return len(_forestData.nodesByType[idName]) > 0


def isSocketLinked(socket, node):
    socketID = ((node.id_data.name, node.name), socket.is_output, socket.identifier)
    return len(_forestData.linkedSockets[socketID]) > 0

def getDirectlyLinkedSockets(socket):
    socketID = socket.toID()
    linkedIDs = _forestData.linkedSocketsWithReroutes[socketID]
    return [idToSocket(linkedID) for linkedID in linkedIDs]

def getDirectlyLinkedSocket(socket):
    socketID = socket.toID()
    linkedSocketIDs = _forestData.linkedSocketsWithReroutes[socketID]
    if len(linkedSocketIDs) > 0:
        return idToSocket(linkedSocketIDs[0])

def getLinkedSockets(socket):
    socketID = socket.toID()
    linkedIDs = _forestData.linkedSockets[socketID]
    return [idToSocket(linkedID) for linkedID in linkedIDs]

def iterSocketsThatNeedUpdate():
    for socketID in _forestData.socketsThatNeedUpdate:
        yield idToSocket(socketID)

def getUndefinedNodes(nodeByID):
    return [nodeByID[nodeID] for nodeID in _forestData.nodesByType["NodeUndefined"]]

def iterLinkedSocketsWithInfo(socket, node, nodeByID):
    socketID = ((node.id_data.name, node.name), socket.is_output, socket.identifier)
    linkedIDs = _forestData.linkedSockets[socketID]
    for linkedID in linkedIDs:
        linkedIdentifier = linkedID[2]
        linkedNode = nodeByID[linkedID[0]]
        sockets = linkedNode.outputs if linkedID[1] else linkedNode.inputs
        for socket in sockets:
            if socket.identifier == linkedIdentifier:
                yield socket


# improve performance of higher level functions

def getOriginNodes(node, nodeByID = None):
    nodeID = node.toID()
    linkedNodeIDs = set()
    for socketID in _forestData.socketsByNode[nodeID][0]:
        for linkedSocketID in _forestData.linkedSockets[socketID]:
            linkedNodeIDs.add(linkedSocketID[0])

    if nodeByID is None:
        return [idToNode(nodeID) for nodeID in linkedNodeIDs]
    else:
        return [nodeByID[nodeID] for nodeID in linkedNodeIDs]

def getAllDataLinkIDs():
    linkDataIDs = set()
    dataType = _forestData.dataTypeBySocket
    for socketID, linkedIDs in _forestData.linkedSockets.items():
        for linkedID in linkedIDs:
            if socketID[1]: # check which one is origin/target
                linkDataIDs.add((socketID, linkedID, dataType[socketID], dataType[linkedID]))
            else:
                linkDataIDs.add((linkedID, socketID, dataType[linkedID], dataType[socketID]))
    return linkDataIDs

def getLinkedInputsDict(node):
    linkedSockets = _forestData.linkedSockets
    socketIDs = _forestData.socketsByNode[node.toID()][0]
    return {socketID[2] : len(linkedSockets[socketID]) > 0 for socketID in socketIDs}

def getLinkedOutputsDict(node):
    linkedSockets = _forestData.linkedSockets
    socketIDs = _forestData.socketsByNode[node.toID()][1]
    return {socketID[2] : len(linkedSockets[socketID]) > 0 for socketID in socketIDs}

def getLinkedOutputsDict_ChangedIdentifiers(node, replacements):
    linkedSockets = _forestData.linkedSockets
    socketIDs = _forestData.socketsByNode[node.toID()][1]
    isLinked = {}
    for socketID in socketIDs:
        identifier = socketID[2]
        linked = len(linkedSockets[socketID]) > 0
        if identifier in replacements:
            isLinked[replacements[identifier]] = linked
        else:
            isLinked[identifier] = linked
    return isLinked

def iterLinkedOutputSockets(node):
    linkedSockets = _forestData.linkedSockets
    socketIDs = _forestData.socketsByNode[node.toID()][1]
    for socket, socketID in zip(node.outputs, socketIDs):
        if len(linkedSockets[socketID]) > 0:
            yield socket

def iterUnlinkedInputSockets(node):
    linkedSockets = _forestData.linkedSockets
    socketIDs = _forestData.socketsByNode[node.toID()][0]
    for socket, socketID in zip(node.inputs, socketIDs):
        if len(linkedSockets[socketID]) == 0:
            yield socket

def getLinkedDataTypes(socket):
    dataTypes = set()
    for linkedSocketID in _forestData.linkedSockets[socket.toID()]:
        dataTypes.add(_forestData.dataTypeBySocket[linkedSocketID])
    return dataTypes

def iterLinkedInputSocketsWithOriginDataType(node):
    linkedSocketsDict = _forestData.linkedSockets
    inputIDs = _forestData.socketsByNode[node.toID()][0]
    for socket, socketID in zip(node.inputs, inputIDs):
        linkedSockets = linkedSocketsDict[socketID]
        if len(linkedSockets) > 0:
            yield socket, _forestData.dataTypeBySocket[linkedSockets[0]]


# keep node state

def keepNodeState(function):
    @functools.wraps(function)
    def wrapper(node, *args, **kwargs):
        return keepNodeLinks(keepSocketValues(function))(node, *args, **kwargs)
    return wrapper

# keep node links

def keepNodeLinks(function):
    @functools.wraps(function)
    def wrapper(node, *args, **kwargs):
        connections = getNodeConnections(node)
        output = function(node, *args, **kwargs)
        setConnections(connections)
        return output
    return wrapper

def getNodeConnections(node):
    inputIDs, outputIDs = _forestData.socketsByNode[node.toID()]

    connections = []
    for socketID, socket in zip(chain(inputIDs, outputIDs), node.sockets):
        for linkedID in _forestData.linkedSocketsWithReroutes[socketID]:
            connections.append((socketID, linkedID))

            for identifier in socket.alternativeIdentifiers:
                connections.append(((socketID[0], socketID[1], identifier), linkedID))
    return connections

def setConnections(connections):
    for id1, id2 in connections:
        try: idToSocket(id1).linkWith(idToSocket(id2))
        except: pass

# keep socket values

def keepSocketValues(function):
    @functools.wraps(function)
    def wrapper(node, *args, **kwargs):
        inputs, outputs = getSocketValues(node)
        output = function(node, *args, **kwargs)
        setSocketValues(node, inputs, outputs)
        return output
    return wrapper

def getSocketValues(node):
    inputs = [data for socket in node.inputs for data in getSocketData(socket)]
    outputs = [data for socket in node.outputs for data in getSocketData(socket)]
    return inputs, outputs

def getSocketData(socket):
    s = socket
    yield (s.identifier, s.dataType, s.getProperty(), s.hide, s.isUsed, s.dataIsModified)
    for identifier in socket.alternativeIdentifiers:
        yield (identifier, s.dataType, s.getProperty(), s.hide, s.isUsed, s.dataIsModified)

def setSocketValues(node, inputs, outputs):
    inputsByIdentifier = node.inputsByIdentifier
    for identifier, dataType, value, hide, isUsed, dataIsModified in inputs:
        socket = inputsByIdentifier.get(identifier)
        if socket is None: continue
        socket.hide = hide
        socket.isUsed = isUsed
        if socket.dataType == dataType:
            socket.setProperty(value)
            socket.dataIsModified = dataIsModified

    outputsByIdentifier = node.outputsByIdentifier
    for identifier, dataType, value, hide, isUsed, dataIsModified in outputs:
        socket = outputsByIdentifier.get(identifier)
        if socket is None: continue
        socket.hide = hide
        if socket.dataType == dataType:
            socket.setProperty(value)
            socket.isUsed = isUsed
            socket.dataIsModified = dataIsModified


def getNetworkWithNode(node):
    return _networks.networkByNode[node.toID()]

def getNetworks():
    return _networks.networks

def getSubprogramNetworks():
    return [network for network in _networks.networks if network.isSubnetwork]

def getNetworksByType(type = "Main"):
    return [network for network in _networks.networks if network.type == type]

def getNetworkByIdentifier(identifier):
    for network in getNetworks():
        if network.identifier == identifier: return network
    return None

def getNetworksByNodeTree(nodeTree):
    return [network for network in getNetworks() if network.treeName == nodeTree.name]

def getSubprogramNetworksByNodeTree(nodeTree):
    return [network for network in _networks.networks if network.isSubnetwork and network.treeName == nodeTree.name]

__setup()
