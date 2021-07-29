import blf
import textwrap
from bgl import *
from . rectangle import Rectangle
from .. utils.blender_ui import getDpi, getDpiFactor

font = 1

class TextBox:
    def __init__(self, text, position, width, fontSize, lineHeightFactor = 1, maxRows = 1e5):
        self.text = text
        self.padding = 5
        self.width = width
        self.maxRows = maxRows
        self.position = position
        self.fontSize = int(fontSize)
        self.lineHeight = self.fontSize * lineHeightFactor * getDpiFactor() * 1.2

        self.boundary = Rectangle()
        self.boundary.color = (0.9, 0.9, 0.9, 0.6)
        self.boundary.borderThickness = -1
        self.boundary.borderColor = (0.9, 0.76, 0.4, 1.0)

    def draw(self):
        self.prepareFontDrawing()
        self.separateLines()
        self.calculateBoundaries()
        self.boundary.draw()
        self.drawLines()

    def prepareFontDrawing(self):
        blf.size(font, self.fontSize, int(getDpi()))

    def separateLines(self):
        self.lines = []
        characterWidth = blf.dimensions(font, "abcde")[0] / 5
        maxCharactersPerLine = int((self.width - 2 * self.padding) / characterWidth)

        paragraphs = self.text.splitlines()
        for i, paragraph in enumerate(paragraphs):
            if len(paragraph) <= maxCharactersPerLine:
                self.lines.append(paragraph)
            else:
                paragraphLines = textwrap.wrap(paragraph, max(maxCharactersPerLine, 1))
                if len(paragraphLines) == 0: paragraphLines = [""]
                self.lines.extend(paragraphLines)

            if len(self.lines) > self.maxRows:
                self.lines = self.lines[:self.maxRows - 1]
                self.lines.extend(textwrap.wrap("Some rows don't fit", maxCharactersPerLine))
                break

    def calculateBoundaries(self):
        lineAmount = len(self.lines)

        x1 = self.position.x
        x2 = x1 + self.width
        y1 = self.position.y
        y2 = y1 - lineAmount * self.lineHeight - 2 * self.padding

        self.boundary.resetPosition(x1, y1, x2, y2)

    def drawLines(self):
        offset = blf.dimensions(font, "Vg")[1]
        textBoundary = self.boundary.getInsetRectangle(self.padding)

        glColor4f(0, 0, 0, 1)
        for i, line in enumerate(self.lines):
            blf.position(font, textBoundary.left, textBoundary.top - i * self.lineHeight - offset, 0)
            blf.draw(font, line)
