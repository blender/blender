import bpy
from bpy.props import *

class SwitchTreeOperator(bpy.types.Operator):
    bl_idname = "an.switch_tree"
    bl_label = "Switch Tree"
    bl_description = "Switch to that tree and view all nodes"

    treeName = StringProperty()

    @classmethod
    def poll(cls, context):
        return context.area.type == "NODE_EDITOR"

    def execute(self, context):
        if self.treeName not in bpy.data.node_groups:
            return {"CANCELLED"}

        context.space_data.node_tree = bpy.data.node_groups[self.treeName]
        bpy.ops.node.view_all()
        return {"FINISHED"}
