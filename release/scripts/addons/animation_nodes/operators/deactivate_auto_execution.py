import bpy
from .. utils.blender_ui import redrawAreaType
from .. utils.nodes import getAnimationNodeTrees

class DeactivateAutoExecution(bpy.types.Operator):
    bl_idname = "an.deactivate_auto_execution"
    bl_label = "Deactivate Auto Execution"

    def execute(self, context):
        for tree in getAnimationNodeTrees():
            tree.autoExecution.enabled = False
        redrawAreaType("NODE_EDITOR")
        return {"FINISHED"}
