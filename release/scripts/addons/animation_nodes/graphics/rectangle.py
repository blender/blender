from bgl import *
from mathutils import Vector

class Rectangle:
    def __init__(self, x1 = 0, y1 = 0, x2 = 0, y2 = 0):
        self.resetPosition(x1, y1, x2, y2)
        self.color = (0.8, 0.8, 0.8, 1.0)
        self.borderColor = (0.1, 0.1, 0.1, 1.0)
        self.borderThickness = 0

    @classmethod
    def fromRegionDimensions(cls, region):
        return cls(0, 0, region.width, region.height)

    def resetPosition(self, x1 = 0, y1 = 0, x2 = 0, y2 = 0):
        self.x1 = float(x1)
        self.y1 =  float(y1)
        self.x2 =  float(x2)
        self.y2 =  float(y2)

    @property
    def width(self):
        return abs(self.x1 - self.x2)

    @property
    def height(self):
        return abs(self.y1 - self.y2)

    @property
    def left(self):
        return min(self.x1, self.x2)

    @property
    def right(self):
        return max(self.x1, self.x2)

    @property
    def top(self):
        return max(self.y1, self.y2)

    @property
    def bottom(self):
        return min(self.y1, self.y2)

    @property
    def center(self):
        return Vector((self.centerX, self.centerY))

    @property
    def centerX(self):
        return (self.x1 + self.x2) / 2

    @property
    def centerY(self):
        return (self.y1 + self.y2) / 2

    def getInsetRectangle(self, amount):
        return Rectangle(self.left + amount, self.top - amount, self.right - amount, self.bottom + amount)

    def contains(self, point):
        return self.left <= point[0] <= self.right and self.bottom <= point[1] <= self.top

    def draw(self):
        glColor4f(*self.color)
        glEnable(GL_BLEND)
        glBegin(GL_POLYGON)
        glVertex2f(self.x1, self.y1)
        glVertex2f(self.x2, self.y1)
        glVertex2f(self.x2, self.y2)
        glVertex2f(self.x1, self.y2)
        glEnd()

        if self.borderThickness != 0:
            if abs(self.borderThickness) == 1:
                self.drawBorderWithLines()
            else:
                self.drawBorderwithRectangles()

    def drawBorderwithRectangles(self):
        thickness = self.borderThickness
        thickness = min(abs(self.x1 - self.x2) / 2, abs(self.y1 - self.y2) / 2, thickness)
        left, right = sorted([self.x1, self.x2])
        bottom, top = sorted([self.y1, self.y2])

        if thickness > 0:
            topBorder = Rectangle(left + thickness, top, right - thickness, top - thickness)
            bottomBorder = Rectangle(left + thickness, bottom + thickness, right - thickness, bottom)
        else:
            topBorder = Rectangle(left + thickness, top, right - thickness, top - thickness)
            bottomBorder = Rectangle(left + thickness, bottom + thickness, right - thickness, bottom)
        leftBorder = Rectangle(left, top, left + thickness, bottom)
        rightBorder = Rectangle(right - thickness, top, right, bottom)

        for border in (topBorder, bottomBorder, leftBorder, rightBorder):
            border.color = self.borderColor
            border.draw()

    def drawBorderWithLines(self):
        glColor4f(*self.borderColor)
        glLineWidth(self.borderThickness)
        glBegin(GL_LINE_STRIP)
        glVertex2f(self.left, self.bottom)
        glVertex2f(self.right, self.bottom)
        glVertex2f(self.right, self.top)
        glVertex2f(self.left, self.top)
        glVertex2f(self.left, self.bottom)
        glEnd()
        glLineWidth(1)

    def __repr__(self):
        return "({}, {}) - ({}, {})".format(self.x1, self.y1, self.x2, self.y2)
