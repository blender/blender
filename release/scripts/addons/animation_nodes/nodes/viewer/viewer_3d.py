import bpy
from bpy.props import *
from ... draw_handler import drawHandler
from ... base_types import AnimationNode
from ... tree_info import getNodesByType
from ... utils.blender_ui import redrawAll
from ... graphics.c_utils import drawVector3DListPoints, drawMatrix4x4List
from ... graphics.opengl import createDisplayList, drawDisplayList, freeDisplayList

from mathutils import Vector, Matrix
from ... data_structures import Vector3DList, Matrix4x4List

dataByIdentifier = {}

class DrawData:
    def __init__(self, data, displayList):
        self.data = data
        self.displayList = displayList

drawableDataTypes = (Vector3DList, Matrix4x4List, Vector, Matrix)

class Viewer3DNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_Viewer3DNode"
    bl_label = "3D Viewer"

    def drawPropertyChanged(self, context):
        self.execute(self.getCurrentData())
        self.redrawViewport(context)

    def redrawViewport(self, context):
        redrawAll()

    enabled = BoolProperty(name = "Enabled", default = True,
        update = redrawViewport)

    pointSize = IntProperty(name = "Point Size", default = 2, min = 1,
        update = drawPropertyChanged)

    matrixSize = FloatProperty(name = "Matrix Size", default = 1, min = 0,
        update = drawPropertyChanged)

    drawColor = FloatVectorProperty(name = "Draw Color",
        default = [0.9, 0.9, 0.9], subtype = "COLOR",
        soft_min = 0.0, soft_max = 1.0,
        update = drawPropertyChanged)

    def create(self):
        self.newInput("Generic", "Data", "data")

    def draw(self, layout):
        data = self.getCurrentData()

        if data is None:
            return

        col = layout.column()
        row = col.row(align = True)
        row.prop(self, "drawColor", text = "")
        icon = "LAYER_ACTIVE" if self.enabled else "LAYER_USED"
        row.prop(self, "enabled", text = "", icon = icon)

        if isinstance(data, (Matrix, Matrix4x4List)):
            col.prop(self, "matrixSize", text = "Size")
        elif isinstance(data, (Vector, Vector3DList)):
            col.prop(self, "pointSize", text = "Size")

    def execute(self, data):
        self.freeDrawingData()
        if isinstance(data, drawableDataTypes):
            displayList = None

            if isinstance(data, Vector3DList):
                displayList = createDisplayList(drawVectors, data, self.pointSize, self.drawColor)
            elif isinstance(data, Matrix4x4List):
                displayList = createDisplayList(drawMatrices, data, self.matrixSize, self.drawColor)
            elif isinstance(data, Vector):
                displayList = createDisplayList(drawVector, data, self.pointSize, self.drawColor)
            elif isinstance(data, Matrix):
                displayList = createDisplayList(drawMatrix, data, self.matrixSize, self.drawColor)

            if displayList is not None:
                dataByIdentifier[self.identifier] = DrawData(data, displayList)

    def delete(self):
        self.freeDrawingData()

    def freeDrawingData(self):
        if self.identifier in dataByIdentifier:
            freeDisplayList(dataByIdentifier[self.identifier].displayList)
            del dataByIdentifier[self.identifier]

    def getCurrentData(self):
        if self.identifier in dataByIdentifier:
            return dataByIdentifier[self.identifier].data


from bgl import *

def drawVector(vector, pointSize, color):
    drawVectors(Vector3DList.fromValues([vector]), pointSize, color)

def drawMatrix(matrix, size, color):
    drawMatrices(Matrix4x4List.fromValues([matrix]), size, color)

def drawVectors(vectors, pointSize, color):
    glEnable(GL_POINT_SIZE)
    glEnable(GL_POINT_SMOOTH)
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST)
    glPointSize(pointSize)
    glColor3f(*color)

    drawVector3DListPoints(vectors)

    glDisable(GL_POINT_SIZE)
    glDisable(GL_POINT_SMOOTH)

def drawMatrices(matrices, size, color):
    glColor3f(*color)
    drawMatrix4x4List(matrices, size)

@drawHandler("SpaceView3D", "WINDOW", "POST_VIEW")
def draw():
    for node in getNodesByType("an_Viewer3DNode"):
        if node.enabled and node.identifier in dataByIdentifier:
            drawDisplayList(dataByIdentifier[node.identifier].displayList)
