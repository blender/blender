import bpy
from .. utils.timing import prettyTime
from .. draw_handler import drawHandler
from .. utils.blender_ui import getDpi, getDpiFactor
from .. graphics.drawing_2d import drawText, setTextDrawingDpi

@drawHandler("SpaceNodeEditor", "WINDOW")
def drawNodeEditorHud():
    tree = bpy.context.getActiveAnimationNodeTree()
    if tree is None:
        return

    setTextDrawingDpi(getDpi())
    dpiFactor = getDpiFactor()

    region = bpy.context.region
    top = region.height

    executionTime = prettyTime(tree.lastExecutionInfo.executionTime)
    drawText(executionTime, 10 * dpiFactor, top - 20 * dpiFactor,
        size = 11, color = (1, 1, 1, 0.5))
