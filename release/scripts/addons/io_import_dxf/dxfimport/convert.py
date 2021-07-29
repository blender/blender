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

import itertools
from . import is_
from .fake_entities import ArcEntity
from mathutils import Vector, Matrix, Euler, Color
from math import pi, radians, floor, ceil, degrees
from copy import deepcopy


class ShortVec(Vector):
    def __str__(self):
        return "Vec" + str((round(self.x, 2), round(self.y, 2), round(self.z, 2)))

    def __repr__(self):
        return self.__str__()


def bspline_to_cubic(do, en, curve, errors=None):
    """
    do: an instance of Do()
    en: a DXF entity
    curve: Blender geometry data of type "CURVE"
    inserts knots until every knot has multiplicity of 3; returns new spline controlpoints
    if degree of the spline is > 3 "None" is returned
    """
    def clean_knots():
        start = knots[:degree + 1]
        end = knots[-degree - 1:]

        if start.count(start[0]) < degree + 1:
            maxa = max(start)
            for i in range(degree + 1):
                knots[i] = maxa

        if end.count(end[0]) < degree + 1:
            mina = min(end)
            lenk = len(knots)
            for i in range(lenk - degree - 1, lenk):
                knots[i] = mina

    def insert_knot(t, k, p):
        """ http://www.cs.mtu.edu/~shene/COURSES/cs3621/NOTES/spline/NURBS-knot-insert.html """
        def a(t, ui, uip):
            if uip == ui:
                print("zero!")
                return 0
            return (t - ui) / (uip - ui)

        new_spline = spline.copy()
        for pp in range(p, 1, -1):
            i = k - pp + 1
            ai = a(t, knots[i], knots[i + p])
            new_spline[i] = (1 - ai) * spline[i - 1] + ai * spline[i]

        ai = a(t, knots[k], knots[k + p])
        new_spline.insert(k, (1 - ai) * spline[k - 1] + ai * spline[k % len(spline)])
        knots.insert(k, t)

        return new_spline

    knots = list(en.knots)
    spline = [ShortVec(cp) for cp in en.control_points]
    degree = len(knots) - len(spline) - 1
    if degree <= 3:
        clean_knots()
        k = 1
        st = 1
        while k < len(knots) - 1:
            t = knots[k]
            multilen = knots[st:-st].count(t)
            if multilen < degree:
                before = multilen
                while multilen < degree:
                    spline = insert_knot(t, k, degree)
                    multilen += 1
                    k += 1
                k += before
            else:
                k += degree

        if degree <= 2:
            return quad_to_cube(spline)

        # the ugly truth
        if len(spline) % 3 == 0:
            spline.append([spline[-1]])
            errors.add("Cubic spline: Something went wrong with knot insertion")
        return spline


def quad_to_cube(spline):
    """
    spline: list of (x,y,z)-tuples)
    Converts quad bezier to cubic bezier curve.
    """
    s = []
    for i, p in enumerate(spline):
        if i % 2 == 1:
            before = Vector(spline[i - 1])
            after = Vector(spline[(i + 1) % len(spline)])
            s.append(before + 2 / 3 * (Vector(p) - before))
            s.append(after + 2 / 3 * (Vector(p) - after))
        else:
            s.append(p)

    # degree == 1
    if len(spline) == 2:
        s.append(spline[-1])
    return s


def bulge_to_arc(point, next, bulge):
    """
    point: start point of segment in lwpolyline
    next: end point of segment in lwpolyline
    bulge: number between 0 and 1
    Converts a bulge of lwpolyline to an arc with a bulge describing the amount of how much a straight segment should
    be bended to an arc. With the bulge one can find the center point of the arc that replaces the segment.
    """

    rot = Matrix(((0, -1, 0), (1, 0, 0), (0, 0, 1)))
    section = next - point
    section_length = section.length / 2
    direction = -bulge / abs(bulge)
    correction = 1
    sagitta_len = section_length * abs(bulge)
    radius = (sagitta_len**2 + section_length**2) / (2 * sagitta_len)
    if sagitta_len < radius:
        cosagitta_len = radius - sagitta_len
    else:
        cosagitta_len = sagitta_len - radius
        direction *= -1
        correction *= -1
    center = point + section / 2 + section.normalized() * cosagitta_len * rot * direction
    cp = point - center
    cn = next - center
    cr = cp.to_3d().cross(cn.to_3d()) * correction
    start = Vector((1, 0))
    if cr[2] > 0:
        angdir = 0
        startangle = -start.angle_signed(cp.to_2d())
        endangle = -start.angle_signed(cn.to_2d())
    else:
        angdir = 1
        startangle = start.angle_signed(cp.to_2d())
        endangle = start.angle_signed(cn.to_2d())
    return ArcEntity(startangle, endangle, center.to_3d(), radius, angdir)


def bulgepoly_to_cubic(do, lwpolyline):
    """
    do: instance of Do()
    lwpolyline: DXF entity of type polyline
    Bulges define how much a straight segment of a polyline should be transformed to an arc. Hence do.arc() is called
    for segments with a bulge and all segments are being connected to a cubic bezier curve in the end.
    Reference: http://www.afralisp.net/archive/lisp/Bulges1.htm
    """
    def handle_segment(last, point, bulge):
        if bulge != 0 and not ((point - last).length == 0 or point == last):
            arc = bulge_to_arc(last, point, bulge)
            cubic_bezier = do.arc(arc, None, aunits=1, angdir=arc.angdir,  angbase=0)
        else:
            la = last.to_3d()
            po = point.to_3d()
            section = point - last
            cubic_bezier = [la, la + section * 1 / 3, la + section * 2 / 3, po]
        return cubic_bezier

    points = lwpolyline.points
    bulges = lwpolyline.bulge
    lenpo = len(points)
    spline = []
    for i in range(1, lenpo):
        spline += handle_segment(Vector(points[i - 1]), Vector(points[i]), bulges[i - 1])[:-1]

    if lwpolyline.is_closed:
        spline += handle_segment(Vector(points[-1]), Vector(points[0]), bulges[-1])
    else:
        spline.append(points[-1])
    return spline


def bulgepoly_to_lenlist(lwpolyline):
    """
    returns a list with the segment lengths of a lwpolyline
    """
    def handle_segment(last, point, bulge):
        seglen = (point - last).length
        if bulge != 0 and seglen != 0:
            arc = bulge_to_arc(last, point, bulge)
            if arc.startangle > arc.endangle:
                arc.endangle += 2 * pi
            angle = arc.endangle - arc.startangle
            lenlist.append(abs(arc.radius * angle))
        else:
            lenlist.append(seglen)

    points = lwpolyline.points
    bulges = lwpolyline.bulge
    lenpo = len(points)
    lenlist = []
    for i in range(1, lenpo):
        handle_segment(Vector(points[i - 1][:2]), Vector(points[i][:2]), bulges[i - 1])

    if lwpolyline.is_closed:
        handle_segment(Vector(points[-1][:2]), Vector(points[0][:2]), bulges[-1])

    return lenlist


def extrusion_to_matrix(entity):
    """
    Converts an extrusion vector to a rotation matrix that denotes the transformation between world coordinate system
    and the entity's own coordinate system (described by the extrusion vector).
    """
    def arbitrary_x_axis(extrusion_normal):
        world_y = Vector((0, 1, 0))
        world_z = Vector((0, 0, 1))
        if abs(extrusion_normal[0]) < 1 / 64 and abs(extrusion_normal[1]) < 1 / 64:
            a_x = world_y.cross(extrusion_normal)
        else:
            a_x = world_z.cross(extrusion_normal)
        a_x.normalize()
        return a_x, extrusion_normal.cross(a_x)

    az = Vector(entity.extrusion)
    ax, ay = arbitrary_x_axis(az)
    ax4 = ax.to_4d()
    ay4 = ay.to_4d()
    az4 = az.to_4d()
    ax4[3] = 0
    ay4[3] = 0
    az4[3] = 0
    translation = Vector((0, 0, 0, 1))
    if hasattr(entity, "elevation"):
        if type(entity.elevation) is tuple:
            translation = Vector(entity.elevation).to_4d()
        else:
            translation = (az * entity.elevation).to_4d()
    return Matrix((ax4, ay4, az4, translation)).transposed()


def split_by_width(entity):
    """
    Used to split a curve (polyline, lwpolyline) into smaller segments if their width is varying in the overall curve.
    """
    class WidthTuple:
        def __init__(self, w):
            self.w1 = w[0]
            self.w2 = w[1]

        def __eq__(self, other):
            return self.w1 == other.w1 and self.w2 == other.w2 and self.w1 == self.w2

    if is_.varying_width(entity):
        entities = []
        en_template = deepcopy(entity)
        en_template.points = []
        en_template.bulge = []
        en_template.width = []
        en_template.tangents = []
        
        # is_closed is an attrib only on polyline 
        if en_template.dxftype == 'POLYLINE':
            en_template.is_closed = False
        else:
            # disable closed flag (0x01) when is_closed is a @property
            en_template.flags ^= 1
            
        i = 0
        for pair, same_width in itertools.groupby(entity.width, key=lambda w: WidthTuple(w)):
            en = deepcopy(en_template)
            for segment in same_width:
                en.points.append(entity.points[i])
                en.points.append(entity.points[(i + 1) % len(entity.points)])
                en.bulge.append(entity.bulge[i])
                en.width.append(entity.width[i])
                i += 1
            entities.append(en)

        if not entity.is_closed:
            entities.pop(-1)
        return entities

    else:
        return [entity]
