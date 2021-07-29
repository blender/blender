from math import sqrt, cos, sin, acos, degrees, radians, pi

import bpy
import mathutils
from mathutils import Vector
from mathutils.geometry import interpolate_bezier


def get_points(spline, clean=True):
    '''
    usage:
    spline = bpy.data.curves[0].splines[0]
    points = get_points(spline)
    '''

    knots = spline.bezier_points
    if len(knots) < 2:
        return

    # verts per segment
    r = spline.resolution_u + 1

    # segments in spline
    segments = len(knots)

    if not spline.use_cyclic_u:
        segments -= 1

    master_point_list = []
    for i in range(segments):
        inext = (i + 1) % len(knots)

        knot1 = knots[i].co
        handle1 = knots[i].handle_right
        handle2 = knots[inext].handle_left
        knot2 = knots[inext].co

        bezier = knot1, handle1, handle2, knot2, r
        points = interpolate_bezier(*bezier)
        master_point_list.extend(points)

    # some clean up to remove consecutive doubles, this could be smarter...
    if clean:
        old = master_point_list
        good = [v for i, v in enumerate(old[:-1]) if not old[i] == old[i + 1]]
        good.append(old[-1])
        return good

    # makes edge keys, ensure cyclic
    Edges = [[i, i + 1] for i in range(n_verts - 1)]
    if spline.use_cyclic_u:
        Edges.append([i, 0])

    return master_point_list, Edges


class Arc(object):

    '''
    Arc class for SVG reading is taken from:
    https://github.com/regebro/svg.path
    license: CC0 1.0 Universal
    '''

    def __init__(self, start, radius, rotation, arc, sweep, end):
        """radius is complex, rotation is in degrees,
           large and sweep are 1 or 0 (True/False also work)"""

        self.start = start
        self.radius = radius
        self.rotation = rotation
        self.arc = bool(arc)
        self.sweep = bool(sweep)
        self.end = end

        self._parameterize()

    def _parameterize(self):
        # Conversion from endpoint to center parameterization
        # http://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes

        cosr = cos(radians(self.rotation))
        sinr = sin(radians(self.rotation))
        dx = (self.start.real - self.end.real) / 2
        dy = (self.start.imag - self.end.imag) / 2
        x1prim = cosr * dx + sinr * dy
        x1prim_sq = x1prim * x1prim
        y1prim = -sinr * dx + cosr * dy
        y1prim_sq = y1prim * y1prim

        rx = self.radius.real
        rx_sq = rx * rx
        ry = self.radius.imag
        ry_sq = ry * ry

        # Correct out of range radii
        radius_check = (x1prim_sq / rx_sq) + (y1prim_sq / ry_sq)
        if radius_check > 1:
            rx *= sqrt(radius_check)
            ry *= sqrt(radius_check)
            rx_sq = rx * rx
            ry_sq = ry * ry

        t1 = rx_sq * y1prim_sq
        t2 = ry_sq * x1prim_sq
        c = sqrt(abs((rx_sq * ry_sq - t1 - t2) / (t1 + t2)))

        if self.arc == self.sweep:
            c = -c
        cxprim = c * rx * y1prim / ry
        cyprim = -c * ry * x1prim / rx

        self.center = complex((cosr * cxprim - sinr * cyprim) +
                              ((self.start.real + self.end.real) / 2),
                              (sinr * cxprim + cosr * cyprim) +
                              ((self.start.imag + self.end.imag) / 2))

        ux = (x1prim - cxprim) / rx
        uy = (y1prim - cyprim) / ry
        vx = (-x1prim - cxprim) / rx
        vy = (-y1prim - cyprim) / ry
        n = sqrt(ux * ux + uy * uy)
        p = ux
        theta = degrees(acos(p / n))
        if uy < 0:
            theta = -theta
        self.theta = theta % 360

        n = sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy))
        p = ux * vx + uy * vy
        if p == 0:
            delta = degrees(acos(0))
        else:
            delta = degrees(acos(p / n))
        if (ux * vy - uy * vx) < 0:
            delta = -delta
        self.delta = delta % 360
        if not self.sweep:
            self.delta -= 360

    def point(self, pos):
        angle = radians(self.theta + (self.delta * pos))
        cosr = cos(radians(self.rotation))
        sinr = sin(radians(self.rotation))

        x = cosr * cos(angle) * self.radius.real - sinr * \
            sin(angle) * self.radius.imag + self.center.real
        y = sinr * cos(angle) * self.radius.real + cosr * \
            sin(angle) * self.radius.imag + self.center.imag
        return [x, y]
