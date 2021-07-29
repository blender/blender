import bpy
from bpy.props import *
from .. tree_info import getNodeByIdentifier
from .. utils.blender_ui import getDpiFactor

class NodePopup(bpy.types.Operator):
    bl_idname = "an.node_popup"
    bl_label = "Node Popup"

    nodeIdentifier = StringProperty(default = "")

    width = IntProperty(default = 250)
    drawFunctionName = StringProperty(default = "")
    executeFunctionName = StringProperty(default = "")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width = self.width * getDpiFactor())

    def draw(self, context):
        try: node = getNodeByIdentifier(self.nodeIdentifier)
        except: self.layout.label("Node not found", icon = "INFO")

        drawFunction = getattr(node, self.drawFunctionName)
        drawFunction(self.layout)

    def check(self, context):
        return True

    def execute(self, context):
        if self.executeFunctionName != "":
            try: node = getNodeByIdentifier(self.nodeIdentifier)
            except: return {"CANCELLED"}

            executeFunction = getattr(node, self.executeFunctionName)
            executeFunction()
        return {"FINISHED"}
