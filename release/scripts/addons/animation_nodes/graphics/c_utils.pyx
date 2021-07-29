from bgl import *
from .. data_structures cimport Vector3DList, Matrix4x4List
from .. math cimport Vector3, Matrix4
from .. math cimport transformVec3AsPoint_InPlace

cdef object vertex3f = glVertex3f

def drawVector3DListPoints(Vector3DList vectors):
    cdef Py_ssize_t i
    glBegin(GL_POINTS)
    for i in range(len(vectors)):
        vertex3f(vectors.data[i].x,
                 vectors.data[i].y,
                 vectors.data[i].z)
    glEnd()

def drawMatrix4x4List(Matrix4x4List matrices, float scale = 1):
    cdef Py_ssize_t i
    glBegin(GL_LINES)
    for i in range(len(matrices)):
        drawMatrixRepresentationLines(matrices.data + i, scale)
    glEnd()

cdef void drawMatrixRepresentationLines(Matrix4 *matrix, float s):
    drawTransformedPoint(matrix, {"x" : -s, "y" :  0, "z" :  0})
    drawTransformedPoint(matrix, {"x" :  s, "y" :  0, "z" :  0})
    drawTransformedPoint(matrix, {"x" :  0, "y" : -s, "z" :  0})
    drawTransformedPoint(matrix, {"x" :  0, "y" :  s, "z" :  0})
    drawTransformedPoint(matrix, {"x" :  0, "y" :  0, "z" : -s})
    drawTransformedPoint(matrix, {"x" :  0, "y" :  0, "z" :  s})

cdef void drawTransformedPoint(Matrix4 *matrix, Vector3 point):
    transformVec3AsPoint_InPlace(&point, matrix)
    vertex3f(point.x, point.y, point.z)
