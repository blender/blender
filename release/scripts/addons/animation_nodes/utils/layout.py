import bpy
import textwrap
from . blender_ui import getDpiFactor

def splitAlignment(layout):
    row = layout.row()
    left = row.row()
    left.alignment = "LEFT"
    right = row.row()
    right.alignment = "RIGHT"
    return left, right

def writeText(layout, text, width = 30, icon = "NONE", autoWidth = False):
    if autoWidth == True:
        try: width = bpy.context.region.width / getDpiFactor() / 7
        except: width = 30

    col = layout.column(align = True)
    col.scale_y = 0.85
    prefix = " "
    for paragraph in text.split("\n"):
        lines = textwrap.wrap(paragraph, width)

        if len(lines) == 0:
            subcol = col.column()
            subcol.scale_y = 0.3
            subcol.label("")

        for line in lines:
            col.label(prefix + line, icon = icon)
            if icon != "NONE": prefix = "     "
            icon = "NONE"
