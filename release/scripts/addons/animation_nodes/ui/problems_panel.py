import bpy
import sys
from .. import problems
from .. utils.layout import writeText
from .. draw_handler import drawHandler
from .. graphics.rectangle import Rectangle
from .. utils.blender_ui import getDpiFactor

class ProblemsPanel(bpy.types.Panel):
    bl_idname = "an_problems_panel"
    bl_label = "Problems"
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "TOOLS"
    bl_category = "Animation Nodes"

    @classmethod
    def poll(cls, context):
        tree = cls.getTree()
        if tree is None: return False
        return tree.bl_idname == "an_AnimationNodeTree" and problems.problemsExist()

    def draw_header(self, context):
        self.layout.label("", icon = "ERROR")

    def draw(self, context):
        layout = self.layout
        col = layout.column(align = True)
        subcol = col.column(align = True)
        subcol.scale_y = 1.5
        subcol.operator("an.tag_retry_execution", text = "Retry", icon = "FILE_REFRESH")
        if sys.platform == "win32":
            col.operator("wm.console_toggle", text = "Toogle Console", icon = "CONSOLE")

        layout.separator()
        problems.drawCurrentProblemInfo(layout)
        layout.separator()

        col = layout.column(align = True)
        tree = self.getTree()
        lastExec = tree.lastExecutionInfo
        col.label("Last successful execution using:")
        col.label("    Blender:   v{}".format(lastExec.blenderVersionString))
        col.label("    Animation Nodes:   v{}".format(lastExec.animationNodesVersionString))

        if lastExec.isDefault:
            writeText(col,
                ("These versions are only guesses. This file has not been executed "
                 "in a version that supports storing of version information yet."),
                autoWidth = True)

    @classmethod
    def getTree(cls):
        return bpy.context.space_data.edit_tree


@drawHandler("SpaceNodeEditor", "WINDOW")
def drawWarningOverlay():
    if problems.problemsExist():
        rectangle = Rectangle.fromRegionDimensions(bpy.context.region)
        rectangle.color = (0, 0, 0, 0)
        rectangle.borderColor = (0.9, 0.1, 0.1, 0.6)
        rectangle.borderThickness = 4 * getDpiFactor()
        rectangle.draw()
