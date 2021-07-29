from bpy.props import *
from ... events import networkChanged
from ... tree_info import getNodesByType
from ... preferences import getColorSettings
from ... ui.node_colors import colorAllNodes
from ... algorithms.random import getRandomColor

class SubprogramBaseNode:
    isSubprogramNode = True

    subprogramName = StringProperty(name = "Subprogram Name", default = "Subprogram",
        description = "Subprogram name to identify this group elsewhere",
        update = networkChanged)

    subprogramDescription = StringProperty(name = "Subprogram Description", default = "",
        description = "Short description about what this subprogram does",
        update = networkChanged)

    def networkColorChanged(self, context):
        colorAllNodes()

    networkColor = FloatVectorProperty(name = "Network Color",
        default = [0.5, 0.5, 0.5], subtype = "COLOR",
        soft_min = 0.0, soft_max = 1.0,
        update = networkColorChanged)

    def randomizeNetworkColor(self):
        colors = getColorSettings()
        value = colors.subprogramValue
        saturation = colors.subprogramSaturation
        self.networkColor = getRandomColor(value = value, saturation = saturation)

    def getInvokeNodes(self):
        nodes = []
        for node in getNodesByType("an_InvokeSubprogramNode"):
            if node.subprogramIdentifier == self.identifier:
                nodes.append(node)
        return nodes
