import bpy
from .. utils.blender_ui import PieMenuHelper

class SelectionPie(bpy.types.Menu, PieMenuHelper):
    bl_idname = "an.selection_pie"
    bl_label = "Selection Pie"

    @classmethod
    def poll(cls, context):
        tree = context.getActiveAnimationNodeTree()
        if tree is None: return False
        if tree.nodes.active is None: return False
        return True

    def drawLeft(self, layout):
        layout.operator("an.select_dependencies")

    def drawRight(self, layout):
        layout.operator("an.select_dependent_nodes")

    def drawBottom(self, layout):
        layout.operator("an.select_network")
