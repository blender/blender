from itertools import chain
from collections import defaultdict
from . network import NodeNetwork

class NodeNetworks:
    def __init__(self):
        self._reset()

    def _reset(self):
        self.networks = []
        self.networkByNode = {}

    def update(self, forestData, nodeByID):
        self._reset()
        self.forestData = forestData

        networksByIdentifier = defaultdict(list)
        for nodes in self.iterNodeGroups():
            if not self.groupContainsAnimationNodes(nodes): continue

            network = NodeNetwork(nodes, self.forestData, nodeByID)
            networksByIdentifier[network.identifier].append(network)

        for identifier, networks in networksByIdentifier.items():
            if identifier is None:
                # this are the main networks
                self.networks.extend(networks)
            else:
                # join subprogram networks if they are not connected with links
                self.networks.append(NodeNetwork.join(networks, nodeByID))

        for network in self.networks:
            for nodeID in network.nodeIDs:
                self.networkByNode[nodeID] = network

    def groupContainsAnimationNodes(self, nodes):
        typeByNode = self.forestData.typeByNode
        nonAnimationNodes = ("NodeFrame", "NodeReroute")
        return any(typeByNode[node] not in nonAnimationNodes for node in nodes)

    def iterNodeGroups(self):
        foundNodes = set()
        for node in self.forestData.nodes:
            if node not in foundNodes:
                nodeGroup = self.getAllConnectedNodes(node)
                foundNodes.update(nodeGroup)
                yield nodeGroup

    def getAllConnectedNodes(self, nodeInGroup):
        connectedNodes = set()
        uncheckedNodes = {nodeInGroup}
        while uncheckedNodes:
            node = uncheckedNodes.pop()

            connectedNodes.add(node)
            for linkedNode in self.iterDirectlyLinkedNodes(node):
                if linkedNode not in uncheckedNodes and linkedNode not in connectedNodes:
                    uncheckedNodes.add(linkedNode)

        return connectedNodes

    def iterDirectlyLinkedNodes(self, node):
        for socket in chain.from_iterable(self.forestData.socketsByNode[node]):
            yield from (socket[0] for socket in self.forestData.linkedSocketsWithReroutes[socket])
