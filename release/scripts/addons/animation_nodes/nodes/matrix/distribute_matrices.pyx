import bpy
from bpy.props import *
from math import pi as _pi
from libc.math cimport sin, cos
from ... utils.limits cimport INT_MAX
from ... base_types import AnimationNode
from ... data_structures cimport Matrix4x4List, Vector3DList, CDefaultList
from ... algorithms.rotations.rotation_and_direction cimport directionToMatrix_LowLevel
from ... math cimport (Matrix4, Vector3, setTranslationMatrix,
    setMatrixTranslation, setRotationZMatrix, toVector3)
cdef double PI = _pi # cimporting pi does not work for some reason...

modeItems = [
    ("LINEAR", "Linear", "", "", 0),
    ("GRID", "Grid", "", "", 1),
    ("CIRCLE", "Circle", "", "", 2),
    ("VERTICES", "Vertices", "", "", 3)
]

distanceModeItems = [
    ("STEP", "Step", "Define the distance between two points", 0),
    ("SIZE", "Size", "Define how large the grid will be in total", 1)
]

class DistributeMatricesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DistributeMatricesNode"
    bl_label = "Distribute Matrices"

    mode = EnumProperty(name = "Mode", default = "LINEAR",
        items = modeItems, update = AnimationNode.refresh)

    distanceMode = EnumProperty(name = "Distance Mode", default = "SIZE",
        items = distanceModeItems, update = AnimationNode.refresh)

    exactCircleSegment = BoolProperty(name = "Exact Circle Segment", default = False)

    def create(self):
        if self.mode == "LINEAR":
            self.newInput("Integer", "Amount", "amount", value = 5)
            if self.distanceMode == "STEP":
                self.newInput("Float", "Distance", "distance", value = 1)
            elif self.distanceMode == "SIZE":
                self.newInput("Float", "Size", "size", value = 4)
        elif self.mode == "GRID":
            self.newInput("Integer", "X Divisions", "xDivisions", value = 3, minValue = 0)
            self.newInput("Integer", "Y Divisions", "yDivisions", value = 3, minValue = 0)
            self.newInput("Integer", "Z Divisions", "zDivisions", value = 1, minValue = 0)
            if self.distanceMode == "STEP":
                self.newInput("Float", "X Distance", "xDistance", value = 1)
                self.newInput("Float", "Y Distance", "yDistance", value = 1)
                self.newInput("Float", "Z Distance", "zDistance", value = 1)
            elif self.distanceMode == "SIZE":
                self.newInput("Float", "Width", "width", value = 5)
                self.newInput("Float", "Length", "length", value = 5)
                self.newInput("Float", "Height", "height", value = 5)
        elif self.mode == "CIRCLE":
            self.newInput("Integer", "Amount", "amount", value = 10, minValue = 0)
            self.newInput("Float", "Radius", "radius", value = 4)
            self.newInput("Float", "Segment", "segment", value = 1)
        elif self.mode == "VERTICES":
            self.newInput("Vector List", "Vertices", "vertices")
            self.newInput("Vector List", "Normals", "normals")

        self.newOutput("Matrix List", "Matrices", "matrices")

    def draw(self, layout):
        col = layout.column()
        layout.prop(self, "mode", text = "")
        if self.mode in ("LINEAR", "GRID"):
            layout.prop(self, "distanceMode", text = "")

    def drawAdvanced(self, layout):
        if self.mode == "CIRCLE":
            layout.prop(self, "exactCircleSegment")

    def getExecutionFunctionName(self):
        if self.mode == "LINEAR":
            return "execute_Linear"
        elif self.mode == "GRID":
            return "execute_Grid"
        elif self.mode == "CIRCLE":
            return "execute_Circle"
        elif self.mode == "VERTICES":
            return "execute_Vertices"

    def execute_Linear(self, amount, size):
        return self.execute_Grid(amount, 1, 1, size, 0, 0)

    def execute_Grid(self, xDivisions, yDivisions, zDivisions, size1, size2, size3):
        cdef:
            int xDiv = limitAmount(xDivisions)
            int yDiv = limitAmount(yDivisions)
            int zDiv = limitAmount(zDivisions)
            double xDis, yDis, zDis
            double xOffset, yOffset
            long x, y, z, index
            Vector3 vector
            Matrix4x4List matrices = Matrix4x4List(length = xDiv * yDiv * zDiv)

        if self.distanceMode == "STEP":
            xDis, yDis, zDis = size1, size2, size3
        elif self.distanceMode == "SIZE":
            xDis = size1 / max(xDiv - 1, 1)
            yDis = size2 / max(yDiv - 1, 1)
            zDis = size3 / max(zDiv - 1, 1)

        xOffset = xDis * (xDiv - 1) / 2
        yOffset = yDis * (yDiv - 1) / 2

        for x in range(xDiv):
            for y in range(yDiv):
                for z in range(zDiv):
                    index = x * yDiv * zDiv + y * zDiv + z
                    vector.x = <float>(x * xDis - xOffset)
                    vector.y = <float>(y * yDis - yOffset)
                    vector.z = <float>(z * zDis)
                    setTranslationMatrix(matrices.data + index, &vector)

        return matrices

    def execute_Circle(self, _amount, float radius, float segment):
        cdef:
            int i
            double currentAngle
            Vector3 vector
            int amount = limitAmount(_amount)
            double factor
            Matrix4x4List matrices = Matrix4x4List(length = amount)

        if self.exactCircleSegment: factor = segment * 2 * PI / max(amount - 1, 1)
        else:                       factor = segment * 2 * PI / max(amount, 1)

        for i in range(amount):
            currentAngle = i * factor
            vector.x = <float>cos(currentAngle) * radius
            vector.y = <float>sin(currentAngle) * radius
            vector.z = 0
            setRotationZMatrix(matrices.data + i, currentAngle)
            setMatrixTranslation(matrices.data + i, &vector)

        return matrices

    def execute_Vertices(self, Vector3DList vertices, Vector3DList normals):
        cdef:
            int i
            CDefaultList _vertices = CDefaultList(Vector3DList, vertices, (0, 0, 0))
            CDefaultList _normals = CDefaultList(Vector3DList, normals, (0, 0, 0))
            int amount = CDefaultList.getMaxLength(_vertices, _normals)
            Matrix4x4List matrices = Matrix4x4List(length = amount)
            Vector3 *normal
            Vector3 *position
            Vector3 guide = toVector3((0, 0, 1))

        for i in range(amount):
            normal = <Vector3*>_normals.get(i)
            position = <Vector3*>_vertices.get(i)
            directionToMatrix_LowLevel(matrices.data + i, normal, &guide, 2, 0)
            setMatrixTranslation(matrices.data + i, position)

        return matrices

cdef int limitAmount(n):
    return max(min(n, INT_MAX), 0)
