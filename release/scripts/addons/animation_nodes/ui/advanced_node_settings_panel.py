import bpy

class NodeSettingsPanel(bpy.types.Panel):
    bl_idname = "an_node_settings_panel"
    bl_label = "Advanced Node Settings"
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_options = {"DEFAULT_CLOSED"}

    @classmethod
    def poll(cls, context):
        node = context.active_node
        return getattr(node, "isAnimationNode", False)

    def draw(self, context):
        node = bpy.context.active_node
        node.drawAdvanced(self.layout)
