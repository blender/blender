import traceback
from .. import tree_info
from .. utils.timing import measureTime
from .. utils.nodes import getAnimationNodeTrees, iterAnimationNodes

@measureTime
def updateFile():
    removeUndefinedSockets()

    socketProperties = getSocketProperties()
    links = getLinks()
    removeLinks()

    tree_info.updateIfNecessary()
    for node in iterAnimationNodes():
        try:
            if node.isRefreshable:
                node._refresh()
        except: traceback.print_exc()

    setSocketProperties(socketProperties)
    setLinks(links)

    tree_info.updateIfNecessary()
    for node in iterAnimationNodes():
        node.updateNode()
        tree_info.updateIfNecessary()


# Undefined Sockets
##############################################

def removeUndefinedSockets():
    for node in iterAnimationNodes():
        for socket in node.inputs:
            if not socket.isAnimationNodeSocket:
                removeSocket(node.inputs, socket)
        for socket in node.outputs:
            if not socket.isAnimationNodeSocket:
                removeSocket(node.outputs, socket)

def removeSocket(sockets, socket):
    print("WARNING: socket removed")
    print("    Tree: {}".format(repr(socket.id_data.name)))
    print("    Node: {}".format(repr(socket.node.name)))
    print("    Name: {}".format(repr(socket.name)))
    sockets.remove(socket)


# Socket Data
##############################################

def getSocketProperties():
    socketsByNode = {}
    for node in iterAnimationNodes():
        inputs = {s.identifier : getSocketInfo(s) for s in node.inputs}
        outputs = {s.identifier : getSocketInfo(s) for s in node.outputs}
        socketsByNode[node] = (inputs, outputs)
    return socketsByNode

def setSocketProperties(socketsByNode):
    for node, (inputs, outputs) in socketsByNode.items():
        for socket in node.inputs:
            if socket.identifier in inputs:
                setSocketInfo(socket, inputs[socket.identifier])
        for socket in node.outputs:
            if socket.identifier in outputs:
                setSocketInfo(socket, outputs[socket.identifier])

def getSocketInfo(socket):
    return socket.dataType, socket.getProperty(), socket.hide, socket.isUsed

def setSocketInfo(socket, data):
    socket.hide = data[2]
    socket.isUsed = data[3]
    if socket.dataType == data[0]:
        socket.setProperty(data[1])


# Links
##############################################

def getLinks():
    linksByTree = {}
    for tree in getAnimationNodeTrees():
        links = []
        for link in tree.links:
            links.append((link.from_node, link.from_socket.identifier,
                          link.to_node, link.to_socket.identifier))
        linksByTree[tree] = links
    return linksByTree

def removeLinks():
    for tree in getAnimationNodeTrees():
        tree.links.clear()

def setLinks(linksByTree):
    for tree, links in linksByTree.items():
        for fromNode, fromIdentifier, toNode, toIdentifier in links:
            fromSocket = getSocketByIdentifier(fromNode.outputs, fromIdentifier)
            toSocket = getSocketByIdentifier(toNode.inputs, toIdentifier)
            if fromSocket is not None and toSocket is not None:
                tree.links.new(toSocket, fromSocket)
            else:
                print("WARNING: link removed")
                print("    Tree: {}".format(repr(tree.name)))
                print("    From Socket: {} -> {}".format(repr(fromNode.name), repr(fromIdentifier)))
                print("    To Socket: {} -> {}\n".format(repr(toNode.name), repr(toIdentifier)))

def getSocketByIdentifier(sockets, identifier):
    for socket in sockets:
        if socket.identifier == identifier:
            return socket
