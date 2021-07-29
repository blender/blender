from blf import dimensions
from . rectangle import Rectangle
from . drawing_2d import drawText, drawVerticalLine, drawHorizontalLine

class Table:
    def __init__(self):
        self._rows = []
        self._columns = []

        self.rowHeight = 50
        self.headerRowHeight = 100

        self.dataFontSize = 12
        self.headerFontSize = 14

        self.cellPadding = 5
        self.lineThickness = 1
        self.lineColor = (0, 0, 0, 0.3)

    def clearColumns(self):
        self._columns.clear()

    def newColumn(self, name, width = 200, alignment = "CENTER", font = 0):
        self._columns.append(Column(name, width, alignment, font))

    def newRow(self, data):
        self._rows.append(data)

    def draw(self, position):
        x, y = position[:2]
        self.drawBackground(x, y)
        self.drawVerticalLines(x, y)
        self.drawHorizontalLines(x, y)
        self.drawHeaderRow(x, y)
        self.drawDataRows(x, y)

    def drawBackground(self, x, y):
        bg = Rectangle(x, y, x + self.width, y - self.height)
        bg.color = (1, 1, 1, 0.5)
        bg.draw()

    def drawVerticalLines(self, x, y):
        xOffset = 0
        height = self.height

        for col in self._columns:
            drawVerticalLine(x + xOffset, y, -height, self.lineColor, self.lineThickness)
            xOffset += col.width

        drawVerticalLine(x + xOffset, y, -height, self.lineColor, self.lineThickness)

    def drawHorizontalLines(self, x, y):
        width = self.width
        drawHorizontalLine(x, y, width, self.lineColor, self.lineThickness)
        drawHorizontalLine(x, y - self.headerRowHeight, width, self.lineColor, self.lineThickness)

        for i in range(len(self._rows)):
            drawHorizontalLine(x, y - self.headerRowHeight - (i+1) * self.rowHeight, width, self.lineColor, self.lineThickness)

    def drawHeaderRow(self, x, y):
        xOffset = x
        for col in self._columns:
            drawText(col.name, xOffset + col.width / 2, y - self.headerRowHeight / 2,
                align = "CENTER", size = self.headerFontSize,
                verticalAlignment = "CENTER", color = (0.1, 0.1, 0.1, 0.9))
            xOffset += col.width

    def drawDataRows(self, x, y):
        for i, rowData in enumerate(self._rows):
            rowContent = [rowData.get(col.name, "-") for col in self._columns]
            self.drawRowData(x, y - self.headerRowHeight - (i + 0.5) * self.rowHeight, rowContent, size = self.dataFontSize)

    def drawRowData(self, x, y, texts, size = 12):
        xOffset = 0
        for col, text in zip(self._columns, texts):
            position = x + xOffset
            align = col.alignment
            if align == "LEFT": position += self.cellPadding
            elif align == "CENTER": position += col.width / 2
            elif align == "RIGHT": position += col.width - self.cellPadding

            drawText(text, position, y, align = align, size = size, color = (0.1, 0.1, 0.1, 0.9), verticalAlignment = "CENTER")
            xOffset += col.width

    @property
    def height(self):
        return self.rowHeight * len(self._rows) + self.headerRowHeight

    @property
    def width(self):
        return sum(col.width for col in self._columns)

class Column:
    def __init__(self, name, width, alignment, font):
        self.name = name
        self.width = width
        self.alignment = alignment
        self.font = font
