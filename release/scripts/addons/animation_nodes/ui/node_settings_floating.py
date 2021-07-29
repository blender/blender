import bpy
from bpy.props import *
from .. tree_info import getNodeByIdentifier
from .. utils.blender_ui import getDpiFactor
from . node_panel import drawSocketLists

modeItems = [
    ("ADVANCED_SETTINGS", "Advanced Settings", "", "NONE", 0),
    ("SOCKET_SETTINGS", "Socket Settings", "", "NONE", 1)
]

class FloatingNodeSettingsPanel(bpy.types.Operator):
    bl_idname = "an.floating_node_settings_panel"
    bl_label = "Node Settings"

    nodeIdentifier = StringProperty(default = "")

    mode = EnumProperty(name = "Mode", items = modeItems, default = "ADVANCED_SETTINGS")

    @classmethod
    def poll(cls, context):
        try: return context.active_node.isAnimationNode
        except: return False

    def check(self, context):
        return True

    def invoke(self, context, event):
        self.nodeIdentifier = context.active_node.identifier
        return context.window_manager.invoke_props_dialog(self, width = 250 * getDpiFactor())

    def draw(self, context):
        try:
            node = getNodeByIdentifier(self.nodeIdentifier)
        except:
            self.layout.label("An error occured during drawing", icon = "INFO")
            return

        layout = self.layout
        layout.prop(self, "mode", expand = True)
        layout.separator()

        if self.mode == "ADVANCED_SETTINGS":
            node.drawAdvanced(layout)
        elif self.mode == "SOCKET_SETTINGS":
            drawSocketLists(layout, node)

    def execute(self, context):
        return {"INTERFACE"}
