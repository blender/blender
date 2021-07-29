# -*- coding:utf-8 -*-

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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------

from math import cos, sin, tan, sqrt, atan2, pi
from mathutils import Vector


class Panel():
    """
        Define a bevel profil
        index: array associate each y with a coord circle and a x
        x = array of x of unique points in the profil relative to origin (0, 0) is bottom left
        y = array of y of all points in the profil relative to origin (0, 0) is bottom left
        idmat = array of material index for each segment
        when path is not closed, start and end caps are generated

        shape is the loft profile
        path is the loft path

        Open shape:

        x = [0,1]
        y = [0,1,1, 0]
        index = [0, 0,1,1]
        closed_shape = False

        1 ____2
         |    |
         |    |
         |    |
        0     3

        Closed shape:

        x = [0,1]
        y = [0,1,1, 0]
        index = [0, 0,1,1]
        closed_shape = True

        1 ____2
         |    |
         |    |
         |____|
        0     3

        Side Caps (like glass for window):

        x = [0,1]
        y = [0,1,1, 0.75, 0.25, 0]
        index = [0, 0,1,1,1,1]
        closed_shape = True
        side_caps = [3,4]

        1 ____2        ____
         |   3|__cap__|    |
         |   4|_______|    |
         |____|       |____|
        0     5

    """
    def __init__(self, closed_shape, index, x, y, idmat, side_cap_front=-1, side_cap_back=-1, closed_path=True,
            subdiv_x=0, subdiv_y=0, user_path_verts=0, user_path_uv_v=None):

        self.closed_shape = closed_shape
        self.closed_path = closed_path
        self.index = index
        self.x = x
        self.y = y
        self.idmat = idmat
        self.side_cap_front = side_cap_front
        self.side_cap_back = side_cap_back
        self.subdiv_x = subdiv_x
        self.subdiv_y = subdiv_y
        self.user_path_verts = user_path_verts
        self.user_path_uv_v = user_path_uv_v

    @property
    def n_pts(self):
        return len(self.y)

    @property
    def profil_faces(self):
        """
            number of faces for each section
        """
        if self.closed_shape:
            return len(self.y)
        else:
            return len(self.y) - 1

    @property
    def uv_u(self):
        """
            uvs of profil (absolute value)
        """
        x = [self.x[i] for i in self.index]
        x.append(x[0])
        y = [y for y in self.y]
        y.append(y[0])
        uv_u = []
        uv = 0
        uv_u.append(uv)
        for i in range(len(self.index)):
            dx = x[i + 1] - x[i]
            dy = y[i + 1] - y[i]
            uv += sqrt(dx * dx + dy * dy)
            uv_u.append(uv)
        return uv_u

    def path_sections(self, steps, path_type):
        """
            number of verts and faces sections along path
        """
        n_path_verts = 2
        if path_type in ['QUADRI', 'RECTANGLE']:
            n_path_verts = 4 + self.subdiv_x + 2 * self.subdiv_y
            if self.closed_path:
                n_path_verts += self.subdiv_x
        elif path_type in ['ROUND', 'ELLIPSIS']:
            n_path_verts = steps + 3
        elif path_type == 'CIRCLE':
            n_path_verts = steps
        elif path_type == 'TRIANGLE':
            n_path_verts = 3
        elif path_type == 'PENTAGON':
            n_path_verts = 5
        elif path_type == 'USER_DEFINED':
            n_path_verts = self.user_path_verts
        if self.closed_path:
            n_path_faces = n_path_verts
        else:
            n_path_faces = n_path_verts - 1
        return n_path_verts, n_path_faces

    def n_verts(self, steps, path_type):
        n_path_verts, n_path_faces = self.path_sections(steps, path_type)
        return self.n_pts * n_path_verts

    ############################
    # Geomerty
    ############################

    def _intersect_line(self, center, basis, x):
        """ upper intersection of line parallel to y axis and a triangle
            where line is given by x origin
            top by center, basis size as float
            return float y of upper intersection point

            center.x and center.y are absolute
            a 0 center.x lie on half size
            a 0 center.y lie on basis
        """
        if center.x > 0:
            dx = x - center.x
        else:
            dx = center.x - x
        p = center.y / basis
        return center.y + dx * p

    def _intersect_triangle(self, center, basis, x):
        """ upper intersection of line parallel to y axis and a triangle
            where line is given by x origin
            top by center, basis size as float
            return float y of upper intersection point

            center.x and center.y are absolute
            a 0 center.x lie on half size
            a 0 center.y lie on basis
        """
        if x > center.x:
            dx = center.x - x
            sx = 0.5 * basis - center.x
        else:
            dx = x - center.x
            sx = 0.5 * basis + center.x
        if sx == 0:
            sx = basis
        p = center.y / sx
        return center.y + dx * p

    def _intersect_circle(self, center, radius, x):
        """ upper intersection of line parallel to y axis and a circle
            where line is given by x origin
            circle by center, radius as float
            return float y of upper intersection point, float angle
        """
        dx = x - center.x
        d = (radius * radius) - (dx * dx)
        if d <= 0:
            if x > center.x:
                return center.y, 0
            else:
                return center.y, pi
        else:
            y = sqrt(d)
            return center.y + y, atan2(y, dx)

    def _intersect_elipsis(self, center, radius, x):
        """ upper intersection of line parallel to y axis and an ellipsis
            where line is given by x origin
            circle by center, radius.x and radius.y semimajor and seminimor axis (half width and height) as float
            return float y of upper intersection point, float angle
        """
        dx = x - center.x
        d2 = dx * dx
        A = 1 / radius.y / radius.y
        C = d2 / radius.x / radius.x - 1
        d = - 4 * A * C
        if d <= 0:
            if x > center.x:
                return center.y, 0
            else:
                return center.y, pi
        else:
            y0 = sqrt(d) / 2 / A
            d = (radius.x * radius.x) - d2
            y = sqrt(d)
            return center.y + y0, atan2(y, dx)

    def _intersect_arc(self, center, radius, x_left, x_right):
        y0, a0 = self._intersect_circle(center, radius.x, x_left)
        y1, a1 = self._intersect_circle(center, radius.x, x_right)
        da = (a1 - a0)
        if da < -pi:
            da += 2 * pi
        if da > pi:
            da -= 2 * pi
        return y0, y1, a0, da

    def _intersect_arc_elliptic(self, center, radius, x_left, x_right):
        y0, a0 = self._intersect_elipsis(center, radius, x_left)
        y1, a1 = self._intersect_elipsis(center, radius, x_right)
        da = (a1 - a0)
        if da < -pi:
            da += 2 * pi
        if da > pi:
            da -= 2 * pi
        return y0, y1, a0, da

    def _get_ellispe_coords(self, steps, offset, center, origin, size, radius, x, pivot, bottom_y=0):
        """
            Rectangle with single arc on top
        """
        x_left = size.x / 2 * (pivot - 1) + x
        x_right = size.x / 2 * (pivot + 1) - x
        cx = center.x - origin.x
        cy = offset.y + center.y - origin.y
        y0, y1, a0, da = self._intersect_arc_elliptic(center, radius, origin.x + x_left, origin.x + x_right)
        da /= steps
        coords = []
        # bottom left
        if self.closed_path:
            coords.append((offset.x + x_left, offset.y + x + bottom_y))
        else:
            coords.append((offset.x + x_left, offset.y + bottom_y))
        # top left
        coords.append((offset.x + x_left, offset.y + y0 - origin.y))
        for i in range(1, steps):
            a = a0 + i * da
            coords.append((offset.x + cx + cos(a) * radius.x, cy + sin(a) * radius.y))
        # top right
        coords.append((offset.x + x_right, offset.y + y1 - origin.y))
        # bottom right
        if self.closed_path:
            coords.append((offset.x + x_right, offset.y + x + bottom_y))
        else:
            coords.append((offset.x + x_right, offset.y + bottom_y))
        return coords

    def _get_arc_coords(self, steps, offset, center, origin, size, radius, x, pivot, bottom_y=0):
        """
            Rectangle with single arc on top
        """
        x_left = size.x / 2 * (pivot - 1) + x
        x_right = size.x / 2 * (pivot + 1) - x
        cx = offset.x + center.x - origin.x
        cy = offset.y + center.y - origin.y
        y0, y1, a0, da = self._intersect_arc(center, radius, origin.x + x_left, origin.x + x_right)
        da /= steps
        coords = []

        # bottom left
        if self.closed_path:
            coords.append((offset.x + x_left, offset.y + x + bottom_y))
        else:
            coords.append((offset.x + x_left, offset.y + bottom_y))

        # top left
        coords.append((offset.x + x_left, offset.y + y0 - origin.y))

        for i in range(1, steps):
            a = a0 + i * da
            coords.append((cx + cos(a) * radius.x, cy + sin(a) * radius.x))

        # top right
        coords.append((offset.x + x_right, offset.y + y1 - origin.y))

        # bottom right
        if self.closed_path:
            coords.append((offset.x + x_right, offset.y + x + bottom_y))
        else:
            coords.append((offset.x + x_right, offset.y + bottom_y))

        return coords

    def _get_circle_coords(self, steps, offset, center, origin, radius):
        """
            Full circle
        """
        cx = offset.x + center.x - origin.x
        cy = offset.y + center.y - origin.y
        a = -2 * pi / steps
        return [(cx + cos(i * a) * radius.x, cy + sin(i * a) * radius.x) for i in range(steps)]

    def _get_rectangular_coords(self, offset, size, x, pivot, bottom_y=0):
        coords = []

        x_left = offset.x + size.x / 2 * (pivot - 1) + x
        x_right = offset.x + size.x / 2 * (pivot + 1) - x

        if self.closed_path:
            y0 = offset.y + x + bottom_y
        else:
            y0 = offset.y + bottom_y
        y1 = offset.y + size.y - x

        dy = (y1 - y0) / (1 + self.subdiv_y)
        dx = (x_right - x_left) / (1 + self.subdiv_x)

        # bottom left
        # coords.append((x_left, y0))

        # subdiv left
        for i in range(self.subdiv_y + 1):
            coords.append((x_left, y0 + i * dy))

        # top left
        # coords.append((x_left, y1))

        # subdiv top
        for i in range(self.subdiv_x + 1):
            coords.append((x_left + dx * i, y1))

        # top right
        # coords.append((x_right, y1))
        # subdiv right
        for i in range(self.subdiv_y + 1):
            coords.append((x_right, y1 - i * dy))

        # subdiv bottom
        if self.closed_path:
            for i in range(self.subdiv_x + 1):
                coords.append((x_right - dx * i, y0))
        else:
            # bottom right
            coords.append((x_right, y0))

        return coords

    def _get_vertical_rectangular_trapezoid_coords(self, offset, center, origin, size, basis, x, pivot, bottom_y=0):
        """
            Rectangular trapezoid vertical
            basis is the full width of a triangular area the trapezoid lie into
            center.y is the height of triagular area from top
            center.x is the offset from basis center

            |\
            | \
            |__|
        """
        coords = []
        x_left = size.x / 2 * (pivot - 1) + x
        x_right = size.x / 2 * (pivot + 1) - x
        sx = x * sqrt(basis * basis + center.y * center.y) / basis
        dy = size.y + offset.y - sx
        y0 = self._intersect_line(center, basis, origin.x + x_left)
        y1 = self._intersect_line(center, basis, origin.x + x_right)
        # bottom left
        if self.closed_path:
            coords.append((offset.x + x_left, offset.y + x + bottom_y))
        else:
            coords.append((offset.x + x_left, offset.y + bottom_y))
        # top left
        coords.append((offset.x + x_left, dy - y0))
        # top right
        coords.append((offset.x + x_right, dy - y1))
        # bottom right
        if self.closed_path:
            coords.append((offset.x + x_right, offset.y + x + bottom_y))
        else:
            coords.append((offset.x + x_right, offset.y + bottom_y))
        return coords

    def _get_horizontal_rectangular_trapezoid_coords(self, offset, center, origin, size, basis, x, pivot, bottom_y=0):
        """
            Rectangular trapeze horizontal
            basis is the full width of a triangular area the trapezoid lie into
            center.y is the height of triagular area from top to basis
            center.x is the offset from basis center
             ___
            |   \
            |____\

            TODO: correct implementation
        """
        raise NotImplementedError

    def _get_pentagon_coords(self, offset, center, origin, size, basis, x, pivot, bottom_y=0):
        """
            TODO: correct implementation
                /\
               /  \
              |    |
              |____|
        """
        raise NotImplementedError

    def _get_triangle_coords(self, offset, center, origin, size, basis, x, pivot, bottom_y=0):
        coords = []
        x_left = offset.x + size.x / 2 * (pivot - 1) + x
        x_right = offset.x + size.x / 2 * (pivot + 1) - x

        # bottom left
        if self.closed_path:
            coords.append((x_left, offset.y + x + bottom_y))
        else:
            coords.append((x_left, offset.y + bottom_y))
        # top center
        coords.append((center.x, offset.y + center.y))
        # bottom right
        if self.closed_path:
            coords.append((x_right, offset.y + x + bottom_y))
        else:
            coords.append((x_right, offset.y + bottom_y))
        return coords

    def _get_horizontal_coords(self, offset, size, x, pivot):
        coords = []
        x_left = offset.x + size.x / 2 * (pivot - 1)
        x_right = offset.x + size.x / 2 * (pivot + 1)
        # left
        coords.append((x_left, offset.y + x))
        # right
        coords.append((x_right, offset.y + x))
        return coords

    def _get_vertical_coords(self, offset, size, x, pivot):
        coords = []
        x_left = offset.x + size.x / 2 * (pivot - 1) + x
        # top
        coords.append((x_left, offset.y + size.y))
        # bottom
        coords.append((x_left, offset.y))
        return coords

    def choose_a_shape_in_tri(self, center, origin, size, basis, pivot):
        """
            Choose wich shape inside either a tri or a pentagon
        """
        cx = (0.5 * basis + center.x) - origin.x
        cy = center.y - origin.y
        x_left = size.x / 2 * (pivot - 1)
        x_right = size.x / 2 * (pivot + 1)
        y0 = self.intersect_triangle(cx, cy, basis, x_left)
        y1 = self.intersect_triangle(cx, cy, basis, x_right)
        if (y0 == 0 and y1 == 0) or ((y0 == 0 or y1 == 0) and (y0 == cy or y1 == cy)):
            return 'TRIANGLE'
        elif x_right <= cx or x_left >= cx:
            # single side of triangle
            # may be horizontal or vertical rectangular trapezoid
            # horizontal if size.y < center.y
            return 'QUADRI'
        else:
            # both sides of triangle
            # may be horizontal trapezoid or pentagon
            # horizontal trapezoid if size.y < center.y
            return 'PENTAGON'

    ############################
    # Vertices
    ############################

    def vertices(self, steps, offset, center, origin, size, radius,
            angle_y, pivot, shape_z=None, path_type='ROUND', axis='XZ'):

        verts = []
        if shape_z is None:
            shape_z = [0 for x in self.x]
        if path_type == 'ROUND':
            coords = [self._get_arc_coords(steps, offset, center, origin,
                size, Vector((radius.x - x, 0)), x, pivot, shape_z[i]) for i, x in enumerate(self.x)]
        elif path_type == 'ELLIPSIS':
            coords = [self._get_ellispe_coords(steps, offset, center, origin,
                size, Vector((radius.x - x, radius.y - x)), x, pivot, shape_z[i]) for i, x in enumerate(self.x)]
        elif path_type == 'QUADRI':
            coords = [self._get_vertical_rectangular_trapezoid_coords(offset, center, origin,
                size, radius.x, x, pivot) for i, x in enumerate(self.x)]
        elif path_type == 'HORIZONTAL':
            coords = [self._get_horizontal_coords(offset, size, x, pivot)
                for i, x in enumerate(self.x)]
        elif path_type == 'VERTICAL':
            coords = [self._get_vertical_coords(offset, size, x, pivot)
                for i, x in enumerate(self.x)]
        elif path_type == 'CIRCLE':
            coords = [self._get_circle_coords(steps, offset, center, origin, Vector((radius.x - x, 0)))
                for i, x in enumerate(self.x)]
        else:
            coords = [self._get_rectangular_coords(offset, size, x, pivot, shape_z[i])
                for i, x in enumerate(self.x)]
        # vertical panel (as for windows)
        if axis == 'XZ':
            for i in range(len(coords[0])):
                for j, p in enumerate(self.index):
                    x, z = coords[p][i]
                    y = self.y[j]
                    verts.append((x, y, z))
        # horizontal panel (table and so on)
        elif axis == 'XY':
            for i in range(len(coords[0])):
                for j, p in enumerate(self.index):
                    x, y = coords[p][i]
                    z = self.y[j]
                    verts.append((x, y, z))
        return verts

    ############################
    # Faces
    ############################

    def _faces_cap(self, faces, n_path_verts, offset):
        if self.closed_shape and not self.closed_path:
            last_point = offset + self.n_pts * n_path_verts - 1
            faces.append(tuple([offset + i for i in range(self.n_pts)]))
            faces.append(tuple([last_point - i for i in range(self.n_pts)]))

    def _faces_closed(self, n_path_faces, offset):
        faces = []
        n_pts = self.n_pts
        for i in range(n_path_faces):
            k0 = offset + i * n_pts
            if self.closed_path and i == n_path_faces - 1:
                k1 = offset
            else:
                k1 = k0 + n_pts
            for j in range(n_pts - 1):
                faces.append((k1 + j, k1 + j + 1, k0 + j + 1, k0 + j))
            # close profile
            faces.append((k1 + n_pts - 1, k1, k0, k0 + n_pts - 1))
        return faces

    def _faces_open(self, n_path_faces, offset):
        faces = []
        n_pts = self.n_pts
        for i in range(n_path_faces):
            k0 = offset + i * n_pts
            if self.closed_path and i == n_path_faces - 1:
                k1 = offset
            else:
                k1 = k0 + n_pts
            for j in range(n_pts - 1):
                faces.append((k1 + j, k1 + j + 1, k0 + j + 1, k0 + j))
        return faces

    def _faces_side(self, faces, n_path_verts, start, reverse, offset):
        n_pts = self.n_pts
        vf = [offset + start + n_pts * f for f in range(n_path_verts)]
        if reverse:
            faces.append(tuple(reversed(vf)))
        else:
            faces.append(tuple(vf))

    def faces(self, steps, offset=0, path_type='ROUND'):
        n_path_verts, n_path_faces = self.path_sections(steps, path_type)
        if self.closed_shape:
            faces = self._faces_closed(n_path_faces, offset)
        else:
            faces = self._faces_open(n_path_faces, offset)
        if self.side_cap_front > -1:
            self._faces_side(faces, n_path_verts, self.side_cap_front, False, offset)
        if self.side_cap_back > -1:
            self._faces_side(faces, n_path_verts, self.side_cap_back, True, offset)
        self._faces_cap(faces, n_path_verts, offset)
        return faces

    ############################
    # Uvmaps
    ############################

    def uv(self, steps, center, origin, size, radius, angle_y, pivot, x, x_cap, path_type='ROUND'):
        uvs = []
        n_path_verts, n_path_faces = self.path_sections(steps, path_type)
        if path_type in ['ROUND', 'ELLIPSIS']:
            x_left = size.x / 2 * (pivot - 1) + x
            x_right = size.x / 2 * (pivot + 1) - x
            if path_type == 'ELLIPSIS':
                y0, y1, a0, da = self._intersect_arc_elliptic(center, radius, x_left, x_right)
            else:
                y0, y1, a0, da = self._intersect_arc(center, radius, x_left, x_right)
            uv_r = abs(da) * radius.x / steps
            uv_v = [uv_r for i in range(steps)]
            uv_v.insert(0, y0 - origin.y)
            uv_v.append(y1 - origin.y)
            uv_v.append(size.x)
        elif path_type == 'USER_DEFINED':
            uv_v = self.user_path_uv_v
        elif path_type == 'CIRCLE':
            uv_r = 2 * pi * radius.x / steps
            uv_v = [uv_r for i in range(steps + 1)]
        elif path_type == 'QUADRI':
            dy = 0.5 * tan(angle_y) * size.x
            uv_v = [size.y - dy, size.x, size.y + dy, size.x]
        elif path_type == 'HORIZONTAL':
            uv_v = [size.y]
        elif path_type == 'VERTICAL':
            uv_v = [size.y]
        else:
            dx = size.x / (1 + self.subdiv_x)
            dy = size.y / (1 + self.subdiv_y)
            uv_v = []
            for i in range(self.subdiv_y + 1):
                uv_v.append(dy * (i + 1))
            for i in range(self.subdiv_x + 1):
                uv_v.append(dx * (i + 1))
            for i in range(self.subdiv_y + 1):
                uv_v.append(dy * (i + 1))
            for i in range(self.subdiv_x + 1):
                uv_v.append(dx * (i + 1))
            # uv_v = [size.y, size.x, size.y, size.x]

        uv_u = self.uv_u
        if self.closed_shape:
            n_pts = self.n_pts
        else:
            n_pts = self.n_pts - 1
        v0 = 0
        # uvs parties rondes
        for i in range(n_path_faces):
            v1 = v0 + uv_v[i]
            for j in range(n_pts):
                u0 = uv_u[j]
                u1 = uv_u[j + 1]
                uvs.append([(u0, v1), (u1, v1), (u1, v0), (u0, v0)])
            v0 = v1
        if self.side_cap_back > -1 or self.side_cap_front > -1:
            if path_type == 'ROUND':
                # rectangle with top part round
                coords = self._get_arc_coords(steps, Vector((0, 0, 0)), center,
                    origin, size, Vector((radius.x - x_cap, 0)), x_cap, pivot, x_cap)
            elif path_type == 'CIRCLE':
                # full circle
                coords = self._get_circle_coords(steps, Vector((0, 0, 0)), center,
                    origin, Vector((radius.x - x_cap, 0)))
            elif path_type == 'ELLIPSIS':
                coords = self._get_ellispe_coords(steps, Vector((0, 0, 0)), center,
                    origin, size, Vector((radius.x - x_cap, radius.y - x_cap)), x_cap, pivot, x_cap)
            elif path_type == 'QUADRI':
                coords = self._get_vertical_rectangular_trapezoid_coords(Vector((0, 0, 0)), center,
                    origin, size, radius.x, x_cap, pivot)
                # coords = self._get_trapezoidal_coords(0, origin, size, angle_y, x_cap, pivot, x_cap)
            else:
                coords = self._get_rectangular_coords(Vector((0, 0, 0)), size, x_cap, pivot, 0)
            if self.side_cap_front > -1:
                uvs.append(list(coords))
            if self.side_cap_back > -1:
                uvs.append(list(reversed(coords)))

        if self.closed_shape and not self.closed_path:
            coords = [(self.x[self.index[i]], y) for i, y in enumerate(self.y)]
            uvs.append(coords)
            uvs.append(list(reversed(coords)))
        return uvs

    ############################
    # Material indexes
    ############################

    def mat(self, steps, cap_front_id, cap_back_id, path_type='ROUND'):
        n_path_verts, n_path_faces = self.path_sections(steps, path_type)
        n_profil_faces = self.profil_faces
        idmat = []
        for i in range(n_path_faces):
            for f in range(n_profil_faces):
                idmat.append(self.idmat[f])
        if self.side_cap_front > -1:
            idmat.append(cap_front_id)
        if self.side_cap_back > -1:
            idmat.append(cap_back_id)
        if self.closed_shape and not self.closed_path:
            idmat.append(self.idmat[0])
            idmat.append(self.idmat[0])
        return idmat
