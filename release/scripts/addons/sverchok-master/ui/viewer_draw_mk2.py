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

import math
import time
from math import pi
import traceback

import bpy
import mathutils
from mathutils import Vector, Matrix
from mathutils.geometry import normal
from mathutils.geometry import tessellate_polygon as tessellate

from sverchok.data_structure import Vector_generate, Matrix_generate

drawlists_3dview = {}
callback_dict = {}
SpaceView3D = bpy.types.SpaceView3D

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

# ------------------------------------------------------------------------ #
# parts taken from  "Math Vis (Console)" addon, author Campbell Barton     #
# ------------------------------------------------------------------------ #


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

    def draw_matrix(self, mat):
        bb = self.bb
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
        for col, axial in axis:
            glColor3f(*col)
            glBegin(GL_LINES)
            glVertex3f(*(zero_tx))
            glVertex3f(*(mat * axial))
            glEnd()

        # bounding box vertices
        i = 0
        glColor3f(1.0, 1.0, 1.0)
        series1 = (-0.5, -0.3, -0.1, 0.1, 0.3, 0.5)
        series2 = (-0.5, 0.5)
        z = 0

        for x in series1:
            for y in series2:
                bb[i][:] = x, y, z
                bb[i] = mat * bb[i]
                i += 1

        for y in series1:
            for x in series2:
                bb[i][:] = x, y, z
                bb[i] = mat * bb[i]
                i += 1

        # bounding box drawing
        glLineWidth(1.0)
        glLineStipple(1, 0xAAAA)
        glEnable(GL_LINE_STIPPLE)

        for i in range(0, 24, 2):
            glBegin(GL_LINE_STRIP)
            glVertex3f(*bb[i])
            glVertex3f(*bb[i+1])
            glEnd()
        glDisable(GL_LINE_STIPPLE)


def tag_redraw_all_view3d():
    context = bpy.context

    # Py cant access notifers
    for window in context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'VIEW_3D':
                for region in area.regions:
                    if region.type == 'WINDOW':
                        region.tag_redraw()


def callback_enable(n_id, cached_view, options):
    global callback_dict
    if n_id in callback_dict:
        return

    config = (n_id, cached_view, options)
    handle_view = SpaceView3D.draw_handler_add(
        draw_callback_view, config, 'WINDOW', 'POST_VIEW')

    callback_dict[n_id] = handle_view
    tag_redraw_all_view3d()


def callback_disable_all():
    global callback_dict
    temp_list = list(callback_dict.keys())
    for name in temp_list:
        if name:
            callback_disable(name)


def callback_disable(n_id):
    global callback_dict
    handle_view = callback_dict.get(n_id, None)
    if not handle_view:
        return

    SpaceView3D.draw_handler_remove(handle_view, 'WINDOW')
    del callback_dict[n_id]
    tag_redraw_all_view3d()


def get_color_from_normal(dvk, pol, num_verts, vectorlight, colo):
    if num_verts <= 4:
        normal_no = normal(dvk[pol[0]], dvk[pol[1]], dvk[pol[2]])
    else:
        normal_no = normal(dvk[pol[0]], dvk[pol[1]], dvk[pol[2]], dvk[pol[3]])

    normal_no = (normal_no.angle(vectorlight, 0)) / pi

    r = (normal_no * colo[0]) - 0.1
    g = (normal_no * colo[1]) - 0.1
    b = (normal_no * colo[2]) - 0.1
    return (r+0.2, g+0.2, b+0.2)


def display_face(options, pol, data_vector, data_matrix, k, i):

    colo = options['face_colors']
    shade = options['shading']
    forced_tessellation = options['forced_tessellation']

    num_verts = len(pol)
    dvk = data_vector[k]

    if shade:
        vectorlight = options['light_direction']
        face_color = get_color_from_normal(dvk, pol, num_verts, vectorlight, colo)
    else:
        face_color = colo[:]

    glColor3f(*face_color)

    if (num_verts in {3, 4}) or (not forced_tessellation):
        glBegin(GL_POLYGON)
        for point in pol:
            vec = data_matrix[i] * dvk[point]
            glVertex3f(*vec)
        glEnd()

    else:
        ''' ngons, we tessellate '''
        glBegin(GL_TRIANGLES)
        v = [dvk[i] for i in pol]
        for pol in tessellate([v]):
            for point in pol:
                vec = data_matrix[i] * v[point]
                glVertex3f(*vec)
        glEnd()


def draw_geometry(n_id, options, data_vector, data_polygons, data_matrix, data_edges):

    show_verts = options['show_verts']
    show_edges = options['show_edges']
    show_faces = options['show_faces']

    vertex_colors = options['vertex_colors']
    edge_colors = options['edge_colors']
    edge_width = options['edge_width']

    tran = options['transparent']
    shade = options['shading']

    verlen = options['verlen']
    max_verts_ = [len(d) for d in data_vector]

    if tran:
        polyholy = GL_POLYGON_STIPPLE
        edgeholy = GL_LINE_STIPPLE
        edgeline = GL_LINE_STRIP
    else:
        polyholy = GL_POLYGON
        edgeholy = GL_LINE
        edgeline = GL_LINES

    def get_max_k(i, verlen):
        k = i
        if i > verlen:
            k = verlen
        return k

    ''' vertices '''

    glEnable(GL_POINT_SIZE)
    glEnable(GL_POINT_SMOOTH)
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST)
    # glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST)

    vsize = options['vertex_size']

    if show_verts and data_vector:

        glPointSize(vsize)
        glColor3f(*vertex_colors)
        glBegin(GL_POINTS)

        for i, matrix in enumerate(data_matrix):
            k = get_max_k(i, verlen)
            for vert in data_vector[k]:
                vec = data_matrix[i] * vert
                glVertex3f(*vec)

        glEnd()

    glDisable(GL_POINT_SIZE)
    glDisable(GL_POINT_SMOOTH)

    ''' polygons '''

    if data_polygons and data_vector:
        num_datapolygon_lists = len(data_polygons)

        glEnable(polyholy)
        for i, matrix in enumerate(data_matrix):

            k = get_max_k(i, verlen)
            mesh_edges = set()
            if k >= num_datapolygon_lists:
                k = (num_datapolygon_lists-1)
                #break

            if len(data_vector[k]) < 3:
                print("can't make faces between fewer than 3 vertices")
                continue

            for j, pol in enumerate(data_polygons[k]):

                if max(pol) >= max_verts_[k]:
                    continue

                if show_faces:
                    display_face(options, pol, data_vector, data_matrix, k, i)

                # collect raw edges, sort by index, use set to prevent dupes.
                if show_edges:
                    er = list(pol) + [pol[0]]
                    kb = {tuple(sorted((e, er[i+1]))) for i, e in enumerate(er[:-1])}
                    mesh_edges.update(kb)

            if show_edges and mesh_edges:
                # glHint(GL_LINE_SMOOTH_HINT, GL_NICEST)
                glEnable(edgeholy)
                glLineWidth(edge_width)
                glColor3f(*edge_colors)
                glBegin(GL_LINES)
                for edge in mesh_edges:
                    for p in edge:
                        vec = data_matrix[i] * data_vector[k][p]
                        glVertex3f(*vec)

                glEnd()
                glDisable(edgeholy)

        glDisable(polyholy)

    ''' edges '''

    if data_edges and data_vector and show_edges:


        glColor3f(*edge_colors)
        glLineWidth(edge_width)
        # glEnable(GL_LINE_SMOOTH)
        glEnable(edgeholy)
        # glHint(GL_LINE_SMOOTH_HINT, GL_NICEST)

        for i, matrix in enumerate(data_matrix):
            k = get_max_k(i, verlen)

            if k >= len(data_edges):
                continue

            if len(data_vector[k]) < 2:
                print("can't make edges between fewer than 2 vertices")
                continue

            for line in data_edges[k]:

                # i think this catches edges which refer to indices not present in
                # the accompanying vertex list.
                if max(line) >= max_verts_[k]:
                    continue

                glBegin(edgeline)
                for p in line:
                    vec = data_matrix[i] * data_vector[k][p]
                    glVertex3f(*vec)
                glEnd()

        glDisable(edgeholy)
        # glDisable(GL_LINE_SMOOTH)

    ''' matrix '''

    if data_matrix and not data_vector:
        md = MatrixDraw()
        for mat in data_matrix:
            md.draw_matrix(mat)


def draw_callback_view(n_id, cached_view, options):

    def Vector_generate2(prop):
        # try:
        #     return [[Vector(v[:3]) for v in obj] for obj in prop]
        # except ValueEror:
        #     return []
        return [[Vector(v) for v in obj] for obj in prop]

    # context = bpy.context
    if options["timings"]:
        start = time.perf_counter()

    if options['draw_list'] == 0:

        sl1 = cached_view[n_id + 'v']
        sl2 = cached_view[n_id + 'ep']
        sl3 = cached_view[n_id + 'm']

        if sl1:
            data_vector = Vector_generate2(sl1)
            verlen = len(data_vector)-1
        else:
            if not sl3:
                # end early: no matrix and no vertices
                callback_disable(n_id)
                return

            # display matrix repr only.
            data_vector = []
            verlen = 0

        options['verlen'] = verlen
        data_polygons = []
        data_edges = []

        if sl2 and sl2[0]:
            if isinstance(sl2[0], int):
                #callback_disable(n_id)
                return

            len_sl2 = len(sl2[0][0])
            if len_sl2 == 2:
                data_edges = sl2
            elif len_sl2 > 2:
                data_polygons = sl2

        if sl3:
            data_matrix = Matrix_generate(sl3)
        else:
            data_matrix = [Matrix() for i in range(verlen+1)]

        if (data_vector, data_polygons, data_matrix, data_edges) == (0, 0, 0, 0):
            return

        try:
            existing_list = drawlists_3dview.get(n_id)
            if existing_list:
                the_display_list = existing_list
            else:
                the_display_list = glGenLists(1)
                drawlists_3dview[n_id] = the_display_list

            glNewList(the_display_list, GL_COMPILE)
            draw_geometry(n_id, options, data_vector, data_polygons, data_matrix, data_edges)
        except Exception as err:
            print("Error in callback!:")
            traceback.print_exc()
            options['error'] = True
        finally:
            glEndList()

    elif options['draw_list'] == 1:
        # this is called when all you do is rotate around the already obtained geometry
        the_display_list = drawlists_3dview.get(n_id)

    if not 'error' in options:
        glCallList(the_display_list)
        # print(the_display_list, n_id)
        glFlush()

    # restore to system state
    glLineWidth(1)

    if options["timings"]:
        stop = time.perf_counter()
        print("callback drawn in {:4f}".format(stop-start))

    # has drawn once with success.
    options['draw_list'] = 1


def unregister():
    callback_disable_all()
