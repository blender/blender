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

# <pep8 compliant>

import bpy
import blf

from . import utils
from mathutils import Vector

SpaceView3D = bpy.types.SpaceView3D
callback_handle = []


def tag_redraw_areas():
    context = bpy.context

    # Py cant access notifers
    for window in context.window_manager.windows:
        for area in window.screen.areas:
            if area.type in ['VIEW_3D', 'PROPERTIES']:
                area.tag_redraw()


def callback_enable():
    if callback_handle:
        return

    handle_pixel = SpaceView3D.draw_handler_add(draw_callback_px, (), 'WINDOW', 'POST_PIXEL')
    handle_view = SpaceView3D.draw_handler_add(draw_callback_view, (), 'WINDOW', 'POST_VIEW')
    callback_handle[:] = handle_pixel, handle_view

    tag_redraw_areas()


def callback_disable():
    if not callback_handle:
        return

    handle_pixel, handle_view = callback_handle
    SpaceView3D.draw_handler_remove(handle_pixel, 'WINDOW')
    SpaceView3D.draw_handler_remove(handle_view, 'WINDOW')
    callback_handle[:] = []

    tag_redraw_areas()


def draw_callback_px():
    context = bpy.context

    from bgl import glColor3f
    font_id = 0  # XXX, need to find out how best to get this.
    blf.size(font_id, 12, 72)

    data_matrix, data_quat, data_euler, data_vector, data_vector_array = utils.console_math_data()

    name_hide = context.window_manager.MathVisProp.name_hide

    if name_hide:
        return

    if not data_matrix and not data_quat and not data_euler and not data_vector and not data_vector_array:

        '''
        # draw some text
        glColor3f(1.0, 0.0, 0.0)
        blf.position(font_id, 180, 10, 0)
        blf.draw(font_id, "Python Console has no mathutils definitions")
        '''
        return

    glColor3f(1.0, 1.0, 1.0)

    region = context.region
    region3d = context.space_data.region_3d

    region_mid_width = region.width / 2.0
    region_mid_height = region.height / 2.0

    # vars for projection
    perspective_matrix = region3d.perspective_matrix.copy()

    def draw_text(text, vec, dx=3.0, dy=-4.0):
        vec_4d = perspective_matrix * vec.to_4d()
        if vec_4d.w > 0.0:
            x = region_mid_width + region_mid_width * (vec_4d.x / vec_4d.w)
            y = region_mid_height + region_mid_height * (vec_4d.y / vec_4d.w)

            blf.position(font_id, x + dx, y + dy, 0.0)
            blf.draw(font_id, text)

    # points
    if data_vector:
        for key, vec in data_vector.items():
            draw_text(key, vec)

    # lines
    if data_vector_array:
        for key, vec in data_vector_array.items():
            if vec and len(vec) > 0:
                draw_text(key, vec[0])

    # matrix
    if data_matrix:
        for key, mat in data_matrix.items():
            loc = Vector((mat[0][3], mat[1][3], mat[2][3]))
            draw_text(key, loc, dx=10, dy=-20)

    line = 20
    if data_quat:
        loc = context.scene.cursor_location.copy()
        for key, mat in data_quat.items():
            draw_text(key, loc, dy=-line)
            line += 20

    if data_euler:
        loc = context.scene.cursor_location.copy()
        for key, mat in data_euler.items():
            draw_text(key, loc, dy=-line)
            line += 20


def draw_callback_view():
    context = bpy.context

    from bgl import (
        glEnable,
        glDisable,
        glColor3f,
        glVertex3f,
        glPointSize,
        glLineWidth,
        glBegin,
        glEnd,
        glLineStipple,
        GL_POINTS,
        GL_LINE_STRIP,
        GL_LINES,
        GL_LINE_STIPPLE
    )

    data_matrix, data_quat, data_euler, data_vector, data_vector_array = utils.console_math_data()

    # draw_matrix modifiers
    bbox_hide = context.window_manager.MathVisProp.bbox_hide
    bbox_scale = context.window_manager.MathVisProp.bbox_scale

    # draw_matrix vars
    zero = Vector((0.0, 0.0, 0.0))
    x_p = Vector((bbox_scale, 0.0, 0.0))
    x_n = Vector((-bbox_scale, 0.0, 0.0))
    y_p = Vector((0.0, bbox_scale, 0.0))
    y_n = Vector((0.0, -bbox_scale, 0.0))
    z_p = Vector((0.0, 0.0, bbox_scale))
    z_n = Vector((0.0, 0.0, -bbox_scale))
    bb = [Vector() for i in range(8)]

    def draw_matrix(mat):
        zero_tx = mat * zero

        glLineWidth(2.0)

        # x
        glColor3f(1.0, 0.2, 0.2)
        glBegin(GL_LINES)
        glVertex3f(*(zero_tx))
        glVertex3f(*(mat * x_p))
        glEnd()

        glColor3f(0.6, 0.0, 0.0)
        glBegin(GL_LINES)
        glVertex3f(*(zero_tx))
        glVertex3f(*(mat * x_n))
        glEnd()

        # y
        glColor3f(0.2, 1.0, 0.2)
        glBegin(GL_LINES)
        glVertex3f(*(zero_tx))
        glVertex3f(*(mat * y_p))
        glEnd()

        glColor3f(0.0, 0.6, 0.0)
        glBegin(GL_LINES)
        glVertex3f(*(zero_tx))
        glVertex3f(*(mat * y_n))
        glEnd()

        # z
        glColor3f(0.4, 0.4, 1.0)
        glBegin(GL_LINES)
        glVertex3f(*(zero_tx))
        glVertex3f(*(mat * z_p))
        glEnd()

        glColor3f(0.0, 0.0, 0.6)
        glBegin(GL_LINES)
        glVertex3f(*(zero_tx))
        glVertex3f(*(mat * z_n))
        glEnd()

        # bounding box
        if bbox_hide:
            return

        i = 0
        glColor3f(1.0, 1.0, 1.0)
        for x in (-bbox_scale, bbox_scale):
            for y in (-bbox_scale, bbox_scale):
                for z in (-bbox_scale, bbox_scale):
                    bb[i][:] = x, y, z
                    bb[i] = mat * bb[i]
                    i += 1

        # strip
        glLineWidth(1.0)
        glLineStipple(1, 0xAAAA)
        glEnable(GL_LINE_STIPPLE)

        glBegin(GL_LINE_STRIP)
        for i in 0, 1, 3, 2, 0, 4, 5, 7, 6, 4:
            glVertex3f(*bb[i])
        glEnd()

        # not done by the strip
        glBegin(GL_LINES)
        glVertex3f(*bb[1])
        glVertex3f(*bb[5])

        glVertex3f(*bb[2])
        glVertex3f(*bb[6])

        glVertex3f(*bb[3])
        glVertex3f(*bb[7])
        glEnd()
        glDisable(GL_LINE_STIPPLE)

    # points
    if data_vector:
        glPointSize(3.0)
        glBegin(GL_POINTS)
        glColor3f(0.5, 0.5, 1)
        for key, vec in data_vector.items():
            glVertex3f(*vec.to_3d())
        glEnd()
        glPointSize(1.0)

    # lines
    if data_vector_array:
        glColor3f(0.5, 0.5, 1)
        glLineWidth(2.0)

        for line in data_vector_array.values():
            glBegin(GL_LINE_STRIP)
            for vec in line:
                glVertex3f(*vec)
            glEnd()
            glPointSize(1.0)

        glLineWidth(1.0)

    # matrix
    if data_matrix:
        for mat in data_matrix.values():
            draw_matrix(mat)

    if data_quat:
        loc = context.scene.cursor_location.copy()
        for quat in data_quat.values():
            mat = quat.to_matrix().to_4x4()
            mat.translation = loc
            draw_matrix(mat)

    if data_euler:
        loc = context.scene.cursor_location.copy()
        for eul in data_euler.values():
            mat = eul.to_matrix().to_4x4()
            mat.translation = loc
            draw_matrix(mat)
