import bpy
from .. import problems
from .. utils.nodes import idToNode

class NodeNetwork:
    def __init__(self, nodeIDs, forestData, nodeByID):
        self.nodeIDs = nodeIDs
        self.forestData = forestData
        self.type = "Invalid"
        self.name = ""
        self.description = ""
        self.identifier = None
        self.analyse(nodeByID)

    def analyse(self, nodeByID):
        self.findSystemNodes()

        groupNodeAmount = self.groupInAmount + self.groupOutAmount
        loopNodeAmount = self.loopInAmount + self.generatorAmount + self.reassignParameterAmount + self.breakAmount

        self.type = "Invalid"

        if groupNodeAmount + loopNodeAmount + self.scriptAmount == 0:
            self.type = "Main"
        elif self.scriptAmount == 1:
            self.type = "Script"
        elif loopNodeAmount == 0:
            if self.groupInAmount == 0 and self.groupOutAmount == 1:
                self.identifier = self.getGroupOutputNode(nodeByID).groupInputIdentifier
                if self.identifier == "": self.identifier = None
            elif self.groupInAmount == 1 and self.groupOutAmount == 0:
                self.type = "Group"
            elif self.groupInAmount == 1 and self.groupOutAmount == 1:
                if self.getGroupInputNode(nodeByID).identifier == self.getGroupOutputNode(nodeByID).groupInputIdentifier:
                    self.type = "Group"
        elif groupNodeAmount == 0:
            additionalLoopNodeIDs = self.generatorOutputIDs + self.reassignParameterIDs + self.breakIDs
            possibleIdentifiers = list({nodeByID[nodeID].loopInputIdentifier for nodeID in additionalLoopNodeIDs})
            if self.loopInAmount == 0 and len(possibleIdentifiers) == 1:
                self.identifier = possibleIdentifiers[0]
            elif self.loopInAmount == 1 and len(possibleIdentifiers) == 0:
                self.type = "Loop"
            elif self.loopInAmount == 1 and len(possibleIdentifiers) == 1:
                if self.getLoopInputNode(nodeByID).identifier == possibleIdentifiers[0]:
                    self.type = "Loop"

        if self.type == "Script": owner = self.getScriptNode(nodeByID)
        elif self.type == "Group": owner = self.getGroupInputNode(nodeByID)
        elif self.type == "Loop": owner = self.getLoopInputNode(nodeByID)

        if self.type in ("Group", "Loop", "Script"):
            self.identifier = owner.identifier
            self.name = owner.subprogramName
            self.description = owner.subprogramDescription

            # check if a subprogram invokes itself
            if self.identifier in self.getInvokedSubprogramIdentifiers(nodeByID):
                self.type = "Invalid"

    def findSystemNodes(self):
        self.groupInputIDs = []
        self.groupOutputIDs = []
        self.loopInputIDs = []
        self.generatorOutputIDs = []
        self.reassignParameterIDs = []
        self.breakIDs = []
        self.scriptIDs = []
        self.invokeSubprogramIDs = []

        appendToList = {
            "an_GroupInputNode" :            self.groupInputIDs.append,
            "an_GroupOutputNode" :           self.groupOutputIDs.append,
            "an_LoopInputNode" :             self.loopInputIDs.append,
            "an_LoopGeneratorOutputNode" :   self.generatorOutputIDs.append,
            "an_ReassignLoopParameterNode" : self.reassignParameterIDs.append,
            "an_LoopBreakNode" :             self.breakIDs.append,
            "an_ScriptNode" :                self.scriptIDs.append,
            "an_InvokeSubprogramNode" :      self.invokeSubprogramIDs.append }


        typeByNode = self.forestData.typeByNode
        for nodeID in self.nodeIDs:
            if typeByNode[nodeID] in appendToList:
                appendToList[typeByNode[nodeID]](nodeID)

        self.groupInAmount = len(self.groupInputIDs)
        self.groupOutAmount = len(self.groupOutputIDs)
        self.loopInAmount = len(self.loopInputIDs)
        self.generatorAmount = len(self.generatorOutputIDs)
        self.reassignParameterAmount = len(self.reassignParameterIDs)
        self.breakAmount = len(self.breakIDs)
        self.scriptAmount = len(self.scriptIDs)

    def getInvokedSubprogramIdentifiers(self, nodeByID):
        return {nodeByID[nodeID].subprogramIdentifier for nodeID in self.invokeSubprogramIDs}

    @staticmethod
    def join(networks, nodeByID):
        forestData = next(iter(networks)).forestData

        nodeIDs = []
        for network in networks:
            nodeIDs.extend(network.nodeIDs)
        return NodeNetwork(nodeIDs, forestData, nodeByID)

    def getNodes(self, nodeByID = None):
        if nodeByID is None:
            return [idToNode(nodeID) for nodeID in self.nodeIDs]
        else:
            return [nodeByID[nodeID] for nodeID in self.nodeIDs]

    def getAnimationNodes(self, nodeByID = None):
        return [node for node in self.getNodes(nodeByID) if node.isAnimationNode]

    @property
    def treeName(self):
        return next(iter(self.nodeIDs))[0]

    @property
    def nodeTree(self):
        return bpy.data.node_groups[self.treeName]

    @property
    def isSubnetwork(self):
        return self.type in ("Group", "Loop", "Script")

    def getOwnerNode(self, nodeByID = None):
        return self.getNodeByID(self.forestData.nodeByIdentifier[self.identifier], nodeByID)

    def getGroupInputNode(self, nodeByID = None):
        try: return self.getNodeByID(self.groupInputIDs[0], nodeByID)
        except: return None

    def getGroupOutputNode(self, nodeByID = None):
        try: return self.getNodeByID(self.groupOutputIDs[0], nodeByID)
        except: return None

    def getLoopInputNode(self, nodeByID = None):
        try: return self.getNodeByID(self.loopInputIDs[0], nodeByID)
        except: return None

    def getGeneratorOutputNodes(self, nodeByID = None):
        return [self.getNodeByID(nodeID, nodeByID) for nodeID in self.generatorOutputIDs]

    def getReassignParameterNodes(self, nodeByID = None):
        return [self.getNodeByID(nodeID, nodeByID) for nodeID in self.reassignParameterIDs]

    def getBreakNodes(self, nodeByID = None):
        return [self.getNodeByID(nodeID, nodeByID) for nodeID in self.breakIDs]

    def getScriptNode(self, nodeByID = None):
        try: return self.getNodeByID(self.scriptIDs[0], nodeByID)
        except: return None

    def getNodeByID(self, nodeID, nodeByID):
        if nodeByID is None:
            return idToNode(nodeID)
        return nodeByID[nodeID]


    def getSortedAnimationNodes(self, nodeByID = None):
        '''
        Used Algorithm:
        https://en.wikipedia.org/wiki/Topological_sorting#Depth-first_search
        '''

        # localize variables
        socketsByNode = self.forestData.socketsByNode
        linkedSockets = self.forestData.linkedSockets
        animationNodes = self.forestData.animationNodes

        markedNodeIDs = set()
        temporaryMarkedNodeIDs = set()
        unmarkedNodeIDs = self.nodeIDs.copy()
        sortedAnimationNodesIDs = list()

        nodeTree = self.nodeTree

        def sort():
            while unmarkedNodeIDs:
                visit(unmarkedNodeIDs.pop())

        def visit(nodeID):
            if nodeID in temporaryMarkedNodeIDs:
                problems.NodeLinkRecursion().report()
                raise Exception()

            if nodeID not in markedNodeIDs:
                temporaryMarkedNodeIDs.add(nodeID)

                for dependentNode in iterDependentNodes(nodeID):
                    visit(dependentNode)

                temporaryMarkedNodeIDs.remove(nodeID)
                markedNodeIDs.add(nodeID)

                if nodeID in animationNodes:
                    sortedAnimationNodesIDs.append(nodeID)

        def iterDependentNodes(nodeID):
            for socketID in socketsByNode[nodeID][0]:
                for otherSocketID in linkedSockets[socketID]:
                    yield otherSocketID[0]

        def idsToNodes(nodeIDs):
            if nodeByID is None:
                nodes = nodeTree.nodes
                return [nodes[nodeID[1]] for nodeID in nodeIDs]
            else:
                return [nodeByID[nodeID] for nodeID in nodeIDs]

        sort()

        return idsToNodes(sortedAnimationNodesIDs)
