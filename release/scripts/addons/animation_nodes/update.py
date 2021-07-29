from . import problems
from . import tree_info
from . utils.timing import measureTime
from . tree_info import getOriginNodes
from . ui.node_colors import colorNetworks
from . nodes.subprogram import subprogram_sockets
from . execution.units import createExecutionUnits
from . node_link_conversion import correctForbiddenNodeLinks
from . utils.nodes import iterAnimationNodes, getAnimationNodeTrees, createNodeByIdDict

@measureTime
def updateEverything():
    '''
    Call when the node tree changed in a way that the execution code does
    not work anymore.
    '''
    tree_info.update()
    problems.reset()
    enableUseFakeUser()
    updateIndividualNodes()
    correctForbiddenNodeLinks()

    # from now on no nodes will be created or removed
    nodeByID = createNodeByIdDict()

    subprogram_sockets.updateIfNecessary()
    checkIfNodeTreeIsLinked()
    checkUndefinedNodes(nodeByID)
    nodesByNetwork = checkNetworks(nodeByID)
    checkIdentifiers()

    if problems.canCreateExecutionUnits():
        createExecutionUnits(nodeByID)

    colorNetworks(nodesByNetwork, nodeByID)

    nodeByID.clear()


def enableUseFakeUser():
    # Make sure the node trees will not be removed when closing the file.
    for tree in getAnimationNodeTrees():
        tree.use_fake_user = True

def updateIndividualNodes():
    tree_info.updateIfNecessary()
    nodeByID = createNodeByIdDict()
    updatedNodes = set()
    currentNodes = set()

    def editNode(node):
        if node in updatedNodes: return
        currentNodes.add(node)

        for dependencyNode in getOriginNodes(node, nodeByID):
            if dependencyNode not in currentNodes:
                editNode(dependencyNode)

        node.updateNode()
        updatedNodes.add(node)
        currentNodes.remove(node)
        tree_info.updateIfNecessary()

    for node in iterAnimationNodes():
        editNode(node)


def checkNetworks(nodeByID):
    invalidNetworkExists = False
    nodesByNetworkDict = {}

    for network in tree_info.getNetworks():
        if network.type == "Invalid":
            invalidNetworkExists = True
        nodes = network.getAnimationNodes(nodeByID)
        markInvalidNodes(network, nodes)
        checkNodeOptions(network, nodes)
        nodesByNetworkDict[network] = nodes

    if invalidNetworkExists:
        problems.InvalidNetworksExist().report()

    return nodesByNetworkDict

def markInvalidNodes(network, nodes):
    isInvalid = network.type == "Invalid"
    for node in nodes:
        node.inInvalidNetwork = isInvalid

def checkNodeOptions(network, nodes):
    for node in nodes:
        if "NO_EXECUTION" in node.options:
            problems.NodeDoesNotSupportExecution(node.identifier).report()
        if "NOT_IN_SUBPROGRAM" in node.options and network.type in ("Group", "Loop"):
            problems.NodeMustNotBeInSubprogram(node.identifier).report()
        if "NO_AUTO_EXECUTION" in node.options:
            problems.NodeShouldNotBeUsedInAutoExecution(node.identifier).report()

def checkIdentifiers():
    identifierAmount = tree_info.getIdentifierAmount()
    nodeAmount = len(list(iterAnimationNodes()))
    if nodeAmount > identifierAmount:
        problems.IdentifierExistsTwice().report()

def checkIfNodeTreeIsLinked():
    for tree in getAnimationNodeTrees(skipLinkedTrees = False):
        if tree.library is not None:
            problems.LinkedAnimationNodeTreeExists().report()
            break

def checkUndefinedNodes(nodeByID):
    undefinedNodes = tree_info.getUndefinedNodes(nodeByID)
    if len(undefinedNodes) > 0:
        problems.UndefinedNodeExists(undefinedNodes).report()
