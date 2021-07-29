import bpy
from .. utils.names import getRandomString
from .. utils.nodes import idToNode, idToSocket

callbackByIdentifier = {}

def newCallback(function):
    identifier = getRandomString(10)
    callbackByIdentifier[identifier] = function
    return identifier

def insertCallback(identifier, function):
    callbackByIdentifier[identifier] = function
    return identifier

def newParameterizedCallback(identifier, *parameters):
    return "#" + repr((identifier, parameters))

def executeCallback(identifier, *args, **kwargs):
    if identifier == "":
        return
    if identifier.startswith("#"):
        realIdentifier, parameters = eval(identifier[1:])
        callback = callbackByIdentifier[realIdentifier]
        callback(*parameters, args, kwargs)
    else:
        callback = callbackByIdentifier[identifier]
        callback(*args, **kwargs)


# Callback Utils
########################################

def executeNodeCallback(nodeID, functionName, args, kwargs):
    try: node = idToNode(nodeID)
    except: node = None
    if node is None:
        print("Node not found:", nodeID)
        return
    getattr(node, functionName)(*args, **kwargs)

def executeSocketCallback(socketID, functionName, args, kwargs):
    try: socket = idToSocket(socketID)
    except: socket = None
    if socket is None:
        print("Socket not found:", socketID)
        return
    getattr(socket, functionName)(*args, **kwargs)

def newNodeCallback(node, functionName):
    return newParameterizedCallback("executeNodeCallback", node.toID(), functionName)

def newSocketCallback(socket, node, functionName):
    socketID = (node.toID(), socket.isOutput, socket.identifier)
    return newParameterizedCallback("executeSocketCallback", socketID, functionName)

insertCallback("executeNodeCallback", executeNodeCallback)
insertCallback("executeSocketCallback", executeSocketCallback)
