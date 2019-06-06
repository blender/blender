"""
Poll Function
+++++++++++++++

The :class:`NodeTree.poll` function determines if a node tree is visible
in the given context (similar to how :class:`Panel.poll`
and :class:`Menu.poll` define visibility). If it returns False,
the node tree type will not be selectable in the node editor.

A typical condition for shader nodes would be to check the active render engine
of the scene and only show nodes of the renderer they are designed for.
"""
import bpy


class CyclesNodeTree(bpy.types.NodeTree):
    """ This operator is only visible when Cycles is the selected render engine"""
    bl_label = "Cycles Node Tree"
    bl_icon = 'NONE'

    @classmethod
    def poll(cls, context):
        return context.scene.render.engine == 'CYCLES'


bpy.utils.register_class(CyclesNodeTree)
