# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####


import bpy
from mathutils import Matrix, Vector

from bgl import (
    glEnable, glDisable, glBegin, glEnd,
    Buffer, GL_FLOAT, GL_BYTE, GL_INT,
    glGetIntegerv, glGetFloatv,
    glColor3f, glVertex3f, glColor4f, glPointSize, glLineWidth,
    glLineStipple, glPolygonStipple, glHint, glShadeModel,
    #
    GL_MATRIX_MODE, GL_MODELVIEW_MATRIX, GL_MODELVIEW, GL_PROJECTION,
    glMatrixMode, glLoadMatrixf, glPushMatrix, glPopMatrix, glLoadIdentity,
    glGenLists, glNewList, glEndList, glCallList, glFlush, GL_COMPILE,
    #
    GL_POINTS, GL_POINT_SIZE, GL_POINT_SMOOTH, GL_POINT_SMOOTH_HINT,
    GL_LINE, GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINE_STIPPLE,
    GL_POLYGON, GL_POLYGON_STIPPLE, GL_TRIANGLES, GL_QUADS,
    GL_NICEST, GL_FASTEST, GL_FLAT, GL_SMOOTH, GL_LINE_SMOOTH, GL_LINE_SMOOTH_HINT
)


class MatrixDraw(object):

    def __init__(self):
        self.zero = Vector((0.0, 0.0, 0.0))
        self.x_p = Vector((0.5, 0.0, 0.0))
        self.x_n = Vector((-0.5, 0.0, 0.0))
        self.y_p = Vector((0.0, 0.5, 0.0))
        self.y_n = Vector((0.0, -0.5, 0.0))
        self.z_p = Vector((0.0, 0.0, 0.5))
        self.z_n = Vector((0.0, 0.0, -0.5))
        self.bb = [Vector() for i in range(24)]

    def draw_matrix(self, mat, bbcol=(1.0, 1.0, 1.0), skip=False, grid=True):
        bb = self.bb

        if not isinstance(mat, Matrix):
            mat = Matrix(mat)
            
        zero_tx = mat * self.zero

        axis = [
            [(1.0, 0.2, 0.2), self.x_p],
            [(0.6, 0.0, 0.0), self.x_n],
            [(0.2, 1.0, 0.2), self.y_p],
            [(0.0, 0.6, 0.0), self.y_n],
            [(0.2, 0.2, 1.0), self.z_p],
            [(0.0, 0.0, 0.6), self.z_n]
        ]

        glLineWidth(2.0)
        for idx, (col, axial) in enumerate(axis):
            if idx % 2 and skip:
                continue
            glColor3f(*col)
            glBegin(GL_LINES)
            glVertex3f(*(zero_tx))
            glVertex3f(*(mat * axial))
            glEnd()

        # bounding box vertices
        i = 0
        glColor3f(*bbcol)
        series1 = (-0.5, -0.3, -0.1, 0.1, 0.3, 0.5)
        series2 = (-0.5, 0.5)
        z = 0

        if not grid:
            return

        # bounding box drawing
        glLineWidth(1.0)

        def yield_xy():
            for x in series1:
                for y in series2:
                    yield x, y
            for y in series1:
                for x in series2:
                    yield x, y

        for x, y in yield_xy():
            bb[i][:] = x, y, z
            bb[i] = mat * bb[i]
            i += 1


        glLineStipple(1, 0xAAAA)
        glEnable(GL_LINE_STIPPLE)

        for i in range(0, 24, 2):
            glBegin(GL_LINE_STRIP)
            glVertex3f(*bb[i])
            glVertex3f(*bb[i+1])
            glEnd()
        glDisable(GL_LINE_STIPPLE)
