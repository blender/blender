import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... tree_info import getNodeByIdentifier
from ... events import treeChanged, propertyChanged

class LoopBreakNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LoopBreakNode"
    bl_label = "Loop Break"

    loopInputIdentifier = StringProperty(update = treeChanged)

    def create(self):
        self.newInput("Boolean", "Continue", "continueCondition", value = True)

    def draw(self, layout):
        node = self.loopInputNode
        if node: layout.label(node.subprogramName, icon = "GROUP_VERTEX")

    def edit(self):
        network = self.network
        if network.type != "Invalid": return
        if network.loopInAmount != 1: return
        loopInput = network.getLoopInputNode()
        if self.loopInputIdentifier == loopInput.identifier: return
        self.loopInputIdentifier = loopInput.identifier

    @property
    def loopInputNode(self):
        try: return getNodeByIdentifier(self.loopInputIdentifier)
        except: return None
