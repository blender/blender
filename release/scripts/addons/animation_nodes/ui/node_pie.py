import bpy
from .. sockets.info import isList
from .. utils.blender_ui import PieMenuHelper
from .. nodes.subprogram.subprogram_base import SubprogramBaseNode

class ContextPie(bpy.types.Menu, PieMenuHelper):
    bl_idname = "an.context_pie"
    bl_label = "Context Pie"

    @classmethod
    def poll(cls, context):
        try: return context.active_node.isAnimationNode
        except: return False

    def drawLeft(self, layout):
        amount = countUsableSockets(self.activeNode.inputs)
        if amount == 0: self.empty(layout, text = "Has no usable inputs")
        else: layout.operator("an.insert_data_creation_node", text = "Data Input")

    def drawBottom(self, layout):
        amount = countUsableSockets(self.activeNode.outputs)
        if amount == 0: self.empty(layout, text = "Has no usable outputs")
        else: layout.operator("an.insert_viewer_node", text = "Viewer")

    def drawRight(self, layout):
        col = layout.column(align = False)
        self.insertInvokeNodeTemplate(col)
        self.insertLoopTemplate(col)

    def insertInvokeNodeTemplate(self, layout):
        col = layout.column(align = True)
        if getattr(self.activeNode, "isSubprogramNode", False):
            props = col.operator("an.insert_invoke_subprogram_node",
                text = "Create Invoke Node", icon = "GROUP_VERTEX")
            props.subprogramIdentifier = self.activeNode.identifier

    def insertLoopTemplate(self, layout):
        col = layout.column(align = True)
        for socket in self.activeNode.outputs:
            if not socket.hide and isList(socket.bl_idname):
                props = col.operator("an.insert_loop_for_iterator",
                    text = "Loop through {}".format(repr(socket.getDisplayedName())),
                    icon = "MOD_ARRAY")
                props.nodeIdentifier = self.activeNode.identifier
                props.socketIndex = socket.getIndex()

    @property
    def activeNode(self):
        return bpy.context.active_node

def countUsableSockets(sockets):
    counter = 0
    for socket in sockets:
        if not socket.hide and len(socket.allowedInputTypes) > 0:
            counter += 1
    return counter
