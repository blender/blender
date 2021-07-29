import bpy

def newNodeAtCursor(type):
    bpy.ops.node.add_and_link_node(type = type)
    return bpy.context.space_data.node_tree.nodes[-1]

def invokeTranslation():
    bpy.ops.node.translate_attach("INVOKE_DEFAULT")

def idToSocket(socketID):
    node = bpy.data.node_groups[socketID[0][0]].nodes[socketID[0][1]]
    identifier = socketID[2]
    sockets = node.outputs if socketID[1] else node.inputs
    for socket in sockets:
        if socket.identifier == identifier: return socket

def idToNode(nodeID):
    return bpy.data.node_groups[nodeID[0]].nodes[nodeID[1]]

def createNodeByIdDict():
    nodeByID = dict()
    for tree in getAnimationNodeTrees():
        treeName = tree.name
        for node in tree.nodes:
            nodeByID[(treeName, node.name)] = node
    return nodeByID

def getSocket(treeName, nodeName, isOutput, identifier):
    node = bpy.data.node_groups[treeName].nodes[nodeName]
    sockets = node.outputs if isOutput else node.inputs
    for socket in sockets:
        if socket.identifier == identifier: return socket

def getNode(treeName, nodeName):
    return bpy.data.node_groups[treeName].nodes[nodeName]

def getLabelFromIdName(idName):
    cls = getattr(bpy.types, idName, None)
    return getattr(cls, "bl_label", "")

def iterAnimationNodesSockets():
    for node in iterAnimationNodes():
        yield from node.inputs
        yield from node.outputs

def iterAnimationNodes():
    for nodeTree in getAnimationNodeTrees():
        yield from (node for node in nodeTree.nodes if node.isAnimationNode)

def iterNodesInAnimationNodeTrees():
    for nodeTree in getAnimationNodeTrees():
        yield from nodeTree.nodes

def getAnimationNodeTrees(skipLinkedTrees = True):
    nodeTrees = []
    for nodeTree in bpy.data.node_groups:
        if nodeTree.bl_idname != "an_AnimationNodeTree": continue
        if skipLinkedTrees and nodeTree.library is not None: continue
        nodeTrees.append(nodeTree)
    return nodeTrees

def iterAnimationNodeClasses():
    from .. base_types import AnimationNode
    yield from iterSubclassesWithAttribute(AnimationNode, "bl_idname")

def iterSubclassesWithAttribute(cls, attribute):
    for subcls in cls.__subclasses__():
        if hasattr(subcls, attribute):
            yield subcls
        else:
            yield from iterSubclassesWithAttribute(subcls, attribute)
