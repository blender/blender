import blf
from bgl import *

dpi = 72

def setTextDrawingDpi(new_dpi):
    global dpi
    dpi = new_dpi

def drawHorizontalLine(x, y, length, color = None, thickness = None):
    drawLine(x, y, x + length, y, color, thickness)

def drawVerticalLine(x, y, length, color = None, thickness = None):
    drawLine(x, y, x, y + length, color, thickness)

def drawLine(x1, y1, x2, y2, color = None, thickness = None):
    if thickness: glLineWidth(thickness)
    if color: glColor4f(*color)
    glEnable(GL_BLEND)
    glBegin(GL_LINES)
    glVertex2f(x1, y1)
    glVertex2f(x2, y2)
    glEnd()
    if thickness: glLineWidth(1)

def drawText(text, x, y, font = 0, align = "LEFT", verticalAlignment = "BASELINE", size = 12, color = (1, 1, 1, 1)):
    text = str(text)
    blf.size(font, size, int(dpi))
    glColor4f(*color)

    if align == "LEFT" and verticalAlignment == "BASELINE":
        blf.position(font, x, y, 0)
    else:
        width, height = blf.dimensions(font, text)
        newX, newY = x, y
        if align == "RIGHT": newX -= width
        elif align == "CENTER": newX -= width / 2
        if verticalAlignment == "CENTER": newY -= blf.dimensions(font, "x")[1] * 0.75

        blf.position(font, newX, newY, 0)

    blf.draw(font, text)

def drawPolygon(vertices, color):
    glColor4f(*color)
    glEnable(GL_BLEND)
    glBegin(GL_POLYGON)
    for x, y in vertices:
        glVertex2f(x, y)
    glEnd()
    glDisable(GL_BLEND)
