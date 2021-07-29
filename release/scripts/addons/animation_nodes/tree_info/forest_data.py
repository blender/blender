from itertools import chain
from collections import defaultdict
from .. utils.nodes import getAnimationNodeTrees, iterAnimationNodesSockets

class ForestData:
    def __init__(self):
        self._reset()

    def _reset(self):
        self.nodes = []
        self.nodesByType = defaultdict(set)
        self.typeByNode = defaultdict(None)
        self.nodeByIdentifier = defaultdict(None)
        self.animationNodes = set()

        self.socketsByNode = defaultdict(lambda: ([], []))

        self.linkedSockets = defaultdict(list)
        self.linkedSocketsWithReroutes = defaultdict(list)
        self.reroutePairs = defaultdict(list)

        self.dataTypeBySocket = dict()
        self.socketsThatNeedUpdate = set()

    def update(self):
        self._reset()
        self.insertNodeTrees()
        self.rerouteNodes = self.nodesByType["NodeReroute"]
        self.findLinksSkippingReroutes()

    def insertNodeTrees(self):
        for tree in getAnimationNodeTrees():
            self.insertNodes(tree.nodes, tree.name)
            self.insertLinks(tree.links, tree.name)

    def insertNodes(self, nodes, treeName):
        appendNode = self.nodes.append
        nodesByType = self.nodesByType
        typeByNode = self.typeByNode
        nodeByIdentifier = self.nodeByIdentifier
        socketsByNode = self.socketsByNode
        reroutePairs = self.reroutePairs
        dataTypeBySocket = self.dataTypeBySocket
        animationNodes = self.animationNodes
        socketsThatNeedUpdate = self.socketsThatNeedUpdate

        for node in nodes:
            nodeID = (treeName, node.name)

            inputIDs = [(nodeID, False, socket.identifier) for socket in node.inputs]
            outputIDs = [(nodeID, True, socket.identifier) for socket in node.outputs]

            appendNode(nodeID)
            typeByNode[nodeID] = node.bl_idname
            nodesByType[node.bl_idname].add(nodeID)

            socketsByNode[nodeID] = (inputIDs, outputIDs)

            if node.bl_idname == "NodeReroute":
                reroutePairs[inputIDs[0]] = outputIDs[0]
                reroutePairs[outputIDs[0]] = inputIDs[0]
            elif node.bl_idname == "NodeFrame":
                pass
            else:
                if node.bl_idname != "NodeUndefined":
                    animationNodes.add(nodeID)
                    nodeByIdentifier[node.identifier] = nodeID

                chainedSockets = chain(node.inputs, node.outputs)
                chainedSocketIDs = chain(inputIDs, outputIDs)
                for socket, socketID in zip(chainedSockets, chainedSocketIDs):
                        dataTypeBySocket[socketID] = socket.dataType
                        if hasattr(socket, "updateProperty"):
                            socketsThatNeedUpdate.add(socketID)


    def insertLinks(self, links, treeName):
        linkedSocketsWithReroutes = self.linkedSocketsWithReroutes

        for link in links:
            if (link.from_node.bl_idname == "NodeUndefined" or
                link.to_node.bl_idname == "NodeUndefined"):
                continue
            originSocket = link.from_socket
            targetSocket = link.to_socket
            originID = ((treeName, link.from_node.name), originSocket.is_output, originSocket.identifier)
            targetID = ((treeName, link.to_node.name), targetSocket.is_output, targetSocket.identifier)

            linkedSocketsWithReroutes[originID].append(targetID)
            linkedSocketsWithReroutes[targetID].append(originID)

    def findLinksSkippingReroutes(self):
        rerouteNodes = self.rerouteNodes
        nonRerouteNodes = filter(lambda n: n not in rerouteNodes, self.nodes)

        socketsByNode = self.socketsByNode
        linkedSockets = self.linkedSockets
        iterLinkedSockets = self.iterLinkedSockets
        chainIterable = chain.from_iterable

        for node in nonRerouteNodes:
            for socket in chainIterable(socketsByNode[node]):
                linkedSockets[socket] = tuple(iterLinkedSockets(socket, set()))

    def iterLinkedSockets(self, socket, visitedReroutes):
        """If the socket is linked to a reroute node the function
        tries to find the next socket that is linked to the reroute"""
        for socket in self.linkedSocketsWithReroutes[socket]:
            if socket[0] in self.rerouteNodes:
                if socket[0] in visitedReroutes:
                    print("Reroute recursion detected in: {}".format(repr(socket[0][0])))
                    return
                visitedReroutes.add(socket[0])
                yield from self.iterLinkedSockets(self.reroutePairs[socket], visitedReroutes)
            else:
                yield socket
