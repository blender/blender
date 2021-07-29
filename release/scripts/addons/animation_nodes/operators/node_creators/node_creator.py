import bpy
from bpy.props import *
from mathutils import Vector
from ... tree_info import getNodeByIdentifier
from ... nodes.subprogram import subprogram_sockets
from ... utils.nodes import newNodeAtCursor, invokeTranslation

class NodeCreator:
    bl_options = {"INTERNAL"}
    nodeOffset = (0, 0)
    menuWidth = 400

    usedMenu = BoolProperty(default = False)

    @classmethod
    def poll(cls, context):
        try: return context.space_data.node_tree.bl_idname == "an_AnimationNodeTree"
        except: return False

    def invoke(self, context, event):
        if hasattr(self, "drawDialog"):
            return context.window_manager.invoke_props_dialog(self, width = self.menuWidth)
        if hasattr(self, "drawMenu") and getattr(self, "needsMenu", False):
            self.usedMenu = True
            context.window_manager.popup_menu(self.drawPopupMenu)
            return {"FINISHED"}
        self.usedMenu = False
        return self.execute(context)

    def draw(self, context):
        self.drawDialog(self.layout)

    def drawPopupMenu(self, menu, context):
        col = menu.layout.column()
        self.drawMenu(col)

    def check(self, context):
        return True

    def execute(self, context):
        self.nodesToMove = []
        self.nodesToOffset = []
        self.finalActiveNode = None
        self.insert()
        self.offsetNodesToMouse()
        self.moveInsertedNodes()
        if self.finalActiveNode is not None:
            self._setActiveNode(self.finalActiveNode)
        return {"FINISHED"}

    def insert(self):
        pass

    def newNode(self, type, x = 0, y = 0, move = True, mouseOffset = True, label = ""):
        node = self.nodeTree.nodes.new(type = type)
        node.location = (x, y)
        node.label = label
        if mouseOffset: self.nodesToOffset.append(node)
        if move: self.nodesToMove.append(node)
        return node

    def newLink(self, fromSocket, toSocket):
        self.nodeTree.links.new(toSocket, fromSocket)

    def nodeByIdentifier(self, identifier):
        try: return getNodeByIdentifier(identifier)
        except: return None

    def offsetNodesToMouse(self):
        tempNode = newNodeAtCursor("an_DataInputNode")
        offset = tempNode.location
        self.nodeTree.nodes.remove(tempNode)
        for node in self.nodesToOffset:
            node.location += offset + Vector(self.nodeOffset)

    def moveInsertedNodes(self):
        self.deselectAllNodes()
        for node in self.nodesToMove:
            node.select = True
        invokeTranslation()

    @property
    def nodeTree(self):
        return bpy.context.space_data.edit_tree

    @property
    def activeNode(self):
        return getattr(bpy.context, "active_node", None)

    def deselectAllNodes(self):
        for node in self.nodeTree.nodes:
            node.select = False

    def updateSubprograms(self):
        subprogram_sockets.updateIfNecessary()

    def setActiveNode(self, node):
        self.finalActiveNode = node

    def _setActiveNode(self, node):
        self.deselectAllNodes()
        self.finalActiveNode.select = True
        self.nodeTree.nodes.active = self.finalActiveNode
