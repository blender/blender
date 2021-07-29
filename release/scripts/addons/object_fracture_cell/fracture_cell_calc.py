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

# Script copyright (C) Blender Foundation 2012


def points_as_bmesh_cells(verts,
                          points,
                          points_scale=None,
                          margin_bounds=0.05,
                          margin_cell=0.0):
    from math import sqrt
    import mathutils
    from mathutils import Vector

    cells = []

    points_sorted_current = [p for p in points]
    plane_indices = []
    vertices = []

    if points_scale is not None:
        points_scale = tuple(points_scale)
    if points_scale == (1.0, 1.0, 1.0):
        points_scale = None

    # there are many ways we could get planes - convex hull for eg
    # but it ends up fastest if we just use bounding box
    if 1:
        xa = [v[0] for v in verts]
        ya = [v[1] for v in verts]
        za = [v[2] for v in verts]

        xmin, xmax = min(xa) - margin_bounds, max(xa) + margin_bounds
        ymin, ymax = min(ya) - margin_bounds, max(ya) + margin_bounds
        zmin, zmax = min(za) - margin_bounds, max(za) + margin_bounds
        convexPlanes = [
            Vector((+1.0, 0.0, 0.0, -xmax)),
            Vector((-1.0, 0.0, 0.0, +xmin)),
            Vector((0.0, +1.0, 0.0, -ymax)),
            Vector((0.0, -1.0, 0.0, +ymin)),
            Vector((0.0, 0.0, +1.0, -zmax)),
            Vector((0.0, 0.0, -1.0, +zmin)),
            ]

    for i, point_cell_current in enumerate(points):
        planes = [None] * len(convexPlanes)
        for j in range(len(convexPlanes)):
            planes[j] = convexPlanes[j].copy()
            planes[j][3] += planes[j].xyz.dot(point_cell_current)
        distance_max = 10000000000.0  # a big value!

        points_sorted_current.sort(key=lambda p: (p - point_cell_current).length_squared)

        for j in range(1, len(points)):
            normal = points_sorted_current[j] - point_cell_current
            nlength = normal.length

            if points_scale is not None:
                normal_alt = normal.copy()
                normal_alt.x *= points_scale[0]
                normal_alt.y *= points_scale[1]
                normal_alt.z *= points_scale[2]

                # rotate plane to new distance
                # should always be positive!! - but abs incase
                scalar = normal_alt.normalized().dot(normal.normalized())
                # assert(scalar >= 0.0)
                nlength *= scalar
                normal = normal_alt

            if nlength > distance_max:
                break

            plane = normal.normalized()
            plane.resize_4d()
            plane[3] = (-nlength / 2.0) + margin_cell
            planes.append(plane)

            vertices[:], plane_indices[:] = mathutils.geometry.points_in_planes(planes)
            if len(vertices) == 0:
                break

            if len(plane_indices) != len(planes):
                planes[:] = [planes[k] for k in plane_indices]

            # for comparisons use length_squared and delay
            # converting to a real length until the end.
            distance_max = 10000000000.0  # a big value!
            for v in vertices:
                distance = v.length_squared
                if distance_max < distance:
                    distance_max = distance
            distance_max = sqrt(distance_max)  # make real length
            distance_max *= 2.0

        if len(vertices) == 0:
            continue

        cells.append((point_cell_current, vertices[:]))
        del vertices[:]

    return cells
