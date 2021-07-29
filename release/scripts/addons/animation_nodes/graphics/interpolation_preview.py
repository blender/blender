from bgl import *
from . rectangle import Rectangle

class InterpolationPreview:
    def __init__(self, interpolation, position, width, resolution):
        self.interpolation = interpolation
        self.position = position
        self.width = width
        self.normalHeight = width
        self.resolution = resolution
        self.padding = 5
        self.boundary = Rectangle()
        self.boundary.color = (0.9, 0.9, 0.9, 0.6)
        self.boundary.borderThickness = -1
        self.boundary.borderColor = (0.9, 0.76, 0.4, 1.0)
        self.samples = interpolation.sample(amount = resolution)

    def calculateBoundaries(self):
        minSample = self.samples.getMinValue()
        maxSample = self.samples.getMaxValue()

        bottomOvershoot = abs(min(0, minSample) * self.normalHeight)
        topOvershoot = abs(max(0, maxSample - 1) * self.normalHeight)

        x1 = self.position.x
        x2 = x1 + self.width
        y1 = self.position.y
        y2 = y1 - self.normalHeight - bottomOvershoot - topOvershoot

        self.boundary.resetPosition(x1, y1, x2, y2)

        self.interpolationLeft = x1
        self.interpolationRight = x2
        self.interpolationTop = y1 - topOvershoot - self.padding
        self.interpolationBottom = y2 + bottomOvershoot + self.padding

    def draw(self):
        self.boundary.draw()
        self.drawInterpolationCurve()
        self.drawRangeLines()

    def drawInterpolationCurve(self):
        glColor4f(0.2, 0.2, 0.2, 0.8)
        glLineWidth(2)
        glEnable(GL_BLEND)
        glEnable(GL_LINE_SMOOTH)

        glBegin(GL_LINE_STRIP)
        divisor = len(self.samples) - 1
        for i, y in enumerate(self.samples):
            x = i / divisor

            regionX = self.interpolationLeft * (1 - x) + self.interpolationRight * x
            regionY = self.interpolationTop * y + self.interpolationBottom * (1 - y)
            glVertex2f(regionX, regionY)
        glEnd()

        glDisable(GL_LINE_SMOOTH)
        glDisable(GL_BLEND)
        glLineWidth(1)

    def drawRangeLines(self):
        glColor4f(0.2, 0.2, 0.2, 0.5)
        glLineWidth(1)
        glEnable(GL_BLEND)

        glBegin(GL_LINES)
        glVertex2f(self.boundary.left, self.interpolationTop)
        glVertex2f(self.boundary.right, self.interpolationTop)
        glVertex2f(self.boundary.left, self.interpolationBottom)
        glVertex2f(self.boundary.right, self.interpolationBottom)
        glEnd()

        glDisable(GL_BLEND)
