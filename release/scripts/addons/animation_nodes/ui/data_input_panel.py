import bpy
from .. utils.layout import writeText
from .. tree_info import getNodesByType

class DataInputPanel(bpy.types.Panel):
    bl_idname = "an_data_input_panel"
    bl_label = "Data Input"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "AN"

    def draw(self, context):
        layout = self.layout
        nodes = getNodesByType("an_DataInputNode")
        amount = 0
        for node in nodes:
            if not node.showInViewport: continue
            socket = node.inputs[0]
            socket.drawSocket(layout, text = node.label, node = node, drawType = "TEXT_PROPERTY_OR_NONE")
            amount += 1
        if amount == 0:
            writeText(layout, "Enable 'Show in Viewport' in the advanced settings of a Data Input node", icon = "INFO")
