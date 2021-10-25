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

def getDirectlyLinkedSocketsIDs(socket):
    return _forestData.linkedSocketsWithReroutes[socket.toID()]

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


# Network Utilities
##################################################

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
