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
from mathutils import Vector, Matrix
from math import sin, cos, pi, atan2, sqrt, acos
import bpy
# allow to draw parts with gl for debug puropses
from .archipack_gl import GlBaseLine


class Projection(GlBaseLine):

    def __init__(self):
        GlBaseLine.__init__(self)

    def proj_xy(self, t, next=None):
        """
            length of projection of sections at crossing line / circle intersections
            deformation unit vector for profil in xy axis
            so f(x_profile) = position of point in xy plane
        """
        if next is None:
            return self.normal(t).v.normalized(), 1
        v0 = self.normal(1).v.normalized()
        v1 = next.normal(0).v.normalized()
        direction = v0 + v1
        adj = (v0 * self.length) * (v1 * next.length)
        hyp = (self.length * next.length)
        c = min(1, max(-1, adj / hyp))
        size = 1 / cos(0.5 * acos(c))
        return direction.normalized(), min(3, size)

    def proj_z(self, t, dz0, next=None, dz1=0):
        """
            length of projection along crossing line / circle
            deformation unit vector for profil in z axis at line / line intersection
            so f(y) = position of point in yz plane
        """
        return Vector((0, 1)), 1
        """
            NOTE (to myself):
              In theory this is how it has to be done so sections follow path,
              but in real world results are better when sections are z-up.
              So return a dumb 1 so f(y) = y
        """
        if next is None:
            dz = dz0 / self.length
        else:
            dz = (dz1 + dz0) / (self.length + next.length)
        return Vector((0, 1)), sqrt(1 + dz * dz)
        # 1 / sqrt(1 + (dz0 / self.length) * (dz0 / self.length))
        if next is None:
            return Vector((-dz0, self.length)).normalized(), 1
        v0 = Vector((self.length, dz0))
        v1 = Vector((next.length, dz1))
        direction = Vector((-dz0, self.length)).normalized() + Vector((-dz1, next.length)).normalized()
        adj = v0 * v1
        hyp = (v0.length * v1.length)
        c = min(1, max(-1, adj / hyp))
        size = -cos(pi - 0.5 * acos(c))
        return direction.normalized(), size


class Line(Projection):
    """
        2d Line
        Internally stored as p: origin and v:size and direction
        moving p will move both ends of line
        moving p0 or p1 move only one end of line
            p1
            ^
            | v
            p0 == p
    """
    def __init__(self, p=None, v=None, p0=None, p1=None):
        """
            Init by either
            p: Vector or tuple origin
            v: Vector or tuple size and direction
            or
            p0: Vector or tuple 1 point location
            p1: Vector or tuple 2 point location
            Will convert any into Vector 2d
            both optionnals
        """
        Projection.__init__(self)
        if p is not None and v is not None:
            self.p = Vector(p).to_2d()
            self.v = Vector(v).to_2d()
        elif p0 is not None and p1 is not None:
            self.p = Vector(p0).to_2d()
            self.v = Vector(p1).to_2d() - self.p
        else:
            self.p = Vector((0, 0))
            self.v = Vector((0, 0))
        self.line = None

    @property
    def copy(self):
        return Line(self.p.copy(), self.v.copy())

    @property
    def p0(self):
        return self.p

    @property
    def p1(self):
        return self.p + self.v

    @p0.setter
    def p0(self, p0):
        """
            Note: setting p0
            move p0 only
        """
        p1 = self.p1
        self.p = Vector(p0).to_2d()
        self.v = p1 - p0

    @p1.setter
    def p1(self, p1):
        """
            Note: setting p1
            move p1 only
        """
        self.v = Vector(p1).to_2d() - self.p

    @property
    def length(self):
        """
            3d length
        """
        return self.v.length

    @property
    def angle(self):
        """
            2d angle on xy plane
        """
        return atan2(self.v.y, self.v.x)

    @property
    def a0(self):
        return self.angle

    @property
    def angle_normal(self):
        """
            2d angle of perpendicular
            lie on the right side
            p1
            |--x
            p0
        """
        return atan2(-self.v.x, self.v.y)

    @property
    def reversed(self):
        return Line(self.p, -self.v)

    @property
    def oposite(self):
        return Line(self.p + self.v, -self.v)

    @property
    def cross_z(self):
        """
            2d Vector perpendicular on plane xy
            lie on the right side
            p1
            |--x
            p0
        """
        return Vector((self.v.y, -self.v.x))

    @property
    def cross(self):
        return Vector((self.v.y, -self.v.x))

    def signed_angle(self, u, v):
        """
            signed angle between two vectors range [-pi, pi]
        """
        return atan2(u.x * v.y - u.y * v.x, u.x * v.x + u.y * v.y)

    def delta_angle(self, last):
        """
            signed delta angle between end of line and start of this one
            this value is object's a0 for segment = self
        """
        if last is None:
            return self.angle
        return self.signed_angle(last.straight(1, 1).v, self.straight(1, 0).v)

    def normal(self, t=0):
        """
            2d Line perpendicular on plane xy
            at position t in current segment
            lie on the right side
            p1
            |--x
            p0
        """
        return Line(self.lerp(t), self.cross_z)

    def sized_normal(self, t, size):
        """
            2d Line perpendicular on plane xy
            at position t in current segment
            and of given length
            lie on the right side when size > 0
            p1
            |--x
            p0
        """
        return Line(self.lerp(t), size * self.cross_z.normalized())

    def lerp(self, t):
        """
            3d interpolation
        """
        return self.p + self.v * t

    def intersect(self, line):
        """
            2d intersection on plane xy
            return
            True if intersect
            p: point of intersection
            t: param t of intersection on current line
        """
        c = line.cross_z
        d = self.v * c
        if d == 0:
            return False, 0, 0
        t = (c * (line.p - self.p)) / d
        return True, self.lerp(t), t

    def intersect_ext(self, line):
        """
            same as intersect, but return param t on both lines
        """
        c = line.cross_z
        d = self.v * c
        if d == 0:
            return False, 0, 0, 0
        dp = line.p - self.p
        c2 = self.cross_z
        u = (c * dp) / d
        v = (c2 * dp) / d
        return u > 0 and v > 0 and u < 1 and v < 1, self.lerp(u), u, v

    def point_sur_segment(self, pt):
        """ _point_sur_segment
            point: Vector 2d
            t: param t de l'intersection sur le segment courant
            d: distance laterale perpendiculaire positif a droite
        """
        dp = pt - self.p
        dl = self.length
        if dl == 0:
            return dp.length < 0.00001, 0, 0
        d = (self.v.x * dp.y - self.v.y * dp.x) / dl
        t = (self.v * dp) / (dl * dl)
        return t > 0 and t < 1, d, t

    def steps(self, len):
        steps = max(1, round(self.length / len, 0))
        return 1 / steps, int(steps)

    def in_place_offset(self, offset):
        """
            Offset current line
            offset > 0 on the right part
        """
        self.p += offset * self.cross_z.normalized()

    def offset(self, offset):
        """
            Return a new line
            offset > 0 on the right part
        """
        return Line(self.p + offset * self.cross_z.normalized(), self.v)

    def tangeant(self, t, da, radius):
        p = self.lerp(t)
        if da < 0:
            c = p + radius * self.cross_z.normalized()
        else:
            c = p - radius * self.cross_z.normalized()
        return Arc(c, radius, self.angle_normal, da)

    def straight(self, length, t=1):
        return Line(self.lerp(t), self.v.normalized() * length)

    def translate(self, dp):
        self.p += dp

    def rotate(self, a):
        """
            Rotate segment ccw arroud p0
        """
        ca = cos(a)
        sa = sin(a)
        self.v = Matrix([
            [ca, -sa],
            [sa, ca]
            ]) * self.v
        return self

    def scale(self, length):
        self.v = length * self.v.normalized()
        return self

    def tangeant_unit_vector(self, t):
        return self.v.normalized()

    def as_curve(self, context):
        """
            Draw Line with open gl in screen space
            aka: coords are in pixels
        """
        curve = bpy.data.curves.new('LINE', type='CURVE')
        curve.dimensions = '2D'
        spline = curve.splines.new('POLY')
        spline.use_endpoint_u = False
        spline.use_cyclic_u = False
        pts = self.pts
        spline.points.add(len(pts) - 1)
        for i, p in enumerate(pts):
            x, y, z = p
            spline.points[i].co = (x, y, 0, 1)
        curve_obj = bpy.data.objects.new('LINE', curve)
        context.scene.objects.link(curve_obj)
        curve_obj.select = True

    def make_offset(self, offset, last=None):
        """
            Return offset between last and self.
            Adjust last and self start to match
            intersection point
        """
        line = self.offset(offset)
        if last is None:
            return line

        if hasattr(last, "r"):
            res, d, t = line.point_sur_segment(last.c)
            c = (last.r * last.r) - (d * d)
            # print("t:%s" % t)
            if c <= 0:
                # no intersection !
                p0 = line.lerp(t)
            else:
                # center is past start of line
                if t > 0:
                    p0 = line.lerp(t) - line.v.normalized() * sqrt(c)
                else:
                    p0 = line.lerp(t) + line.v.normalized() * sqrt(c)
            # compute da of arc
            u = last.p0 - last.c
            v = p0 - last.c
            da = self.signed_angle(u, v)
            # da is ccw
            if last.ccw:
                # da is cw
                if da < 0:
                    # so take inverse
                    da = 2 * pi + da
            elif da > 0:
                # da is ccw
                da = 2 * pi - da
            last.da = da
            line.p0 = p0
        else:
            # intersect line / line
            # 1 line -> 2 line
            c = line.cross_z
            d = last.v * c
            if d == 0:
                return line
            v = line.p - last.p
            t = (c * v) / d
            c2 = last.cross_z
            u = (c2 * v) / d
            # intersect past this segment end
            # or before last segment start
            # print("u:%s t:%s" % (u, t))
            if u > 1 or t < 0:
                return line
            p = last.lerp(t)
            line.p0 = p
            last.p1 = p

        return line

    @property
    def pts(self):
        return [self.p0.to_3d(), self.p1.to_3d()]


class Circle(Projection):
    def __init__(self, c, radius):
        Projection.__init__(self)
        self.r = radius
        self.r2 = radius * radius
        self.c = c

    def intersect(self, line):
        v = line.p - self.c
        A = line.v * line.v
        B = 2 * v * line.v
        C = v * v - self.r2
        d = B * B - 4 * A * C
        if A <= 0.0000001 or d < 0:
            # dosent intersect, find closest point of line
            res, d, t = line.point_sur_segment(self.c)
            return False, line.lerp(t), t
        elif d == 0:
            t = -B / 2 * A
            return True, line.lerp(t), t
        else:
            AA = 2 * A
            dsq = sqrt(d)
            t0 = (-B + dsq) / AA
            t1 = (-B - dsq) / AA
            if abs(t0) < abs(t1):
                return True, line.lerp(t0), t0
            else:
                return True, line.lerp(t1), t1

    def translate(self, dp):
        self.c += dp


class Arc(Circle):
    """
        Represent a 2d Arc
        TODO:
            make it possible to define an arc by start point end point and center
    """
    def __init__(self, c, radius, a0, da):
        """
            a0 and da arguments are in radians
            c Vector 2d center
            radius float radius
            a0 radians start angle
            da radians delta angle from start to end
            a0 = 0   on the right side
            a0 = pi on the left side
            da > 0 CCW contrary-clockwise
            da < 0 CW  clockwise
            stored internally as radians
        """
        Circle.__init__(self, Vector(c).to_2d(), radius)
        self.line = None
        self.a0 = a0
        self.da = da

    @property
    def angle(self):
        """
            angle of vector p0 p1
        """
        v = self.p1 - self.p0
        return atan2(v.y, v.x)

    @property
    def ccw(self):
        return self.da > 0

    def signed_angle(self, u, v):
        """
            signed angle between two vectors
        """
        return atan2(u.x * v.y - u.y * v.x, u.x * v.x + u.y * v.y)

    def delta_angle(self, last):
        """
            signed delta angle between end of line and start of this one
            this value is object's a0 for segment = self
        """
        if last is None:
            return self.a0
        return self.signed_angle(last.straight(1, 1).v, self.straight(1, 0).v)

    def scale_rot_matrix(self, u, v):
        """
            given vector u and v (from and to p0 p1)
            apply scale factor to radius and
            return a matrix to rotate and scale
            the center around u origin so
            arc fit v
        """
        # signed angle old new vectors (rotation)
        a = self.signed_angle(u, v)
        # scale factor
        scale = v.length / u.length
        ca = scale * cos(a)
        sa = scale * sin(a)
        return scale, Matrix([
            [ca, -sa],
            [sa, ca]
            ])

    @property
    def p0(self):
        """
            start point of arc
        """
        return self.lerp(0)

    @property
    def p1(self):
        """
            end point of arc
        """
        return self.lerp(1)

    @p0.setter
    def p0(self, p0):
        """
            rotate and scale arc so it intersect p0 p1
            da is not affected
        """
        u = self.p0 - self.p1
        v = p0 - self.p1
        scale, rM = self.scale_rot_matrix(u, v)
        self.c = self.p1 + rM * (self.c - self.p1)
        self.r *= scale
        self.r2 = self.r * self.r
        dp = p0 - self.c
        self.a0 = atan2(dp.y, dp.x)

    @p1.setter
    def p1(self, p1):
        """
            rotate and scale arc so it intersect p0 p1
            da is not affected
        """
        p0 = self.p0
        u = self.p1 - p0
        v = p1 - p0

        scale, rM = self.scale_rot_matrix(u, v)
        self.c = p0 + rM * (self.c - p0)
        self.r *= scale
        self.r2 = self.r * self.r
        dp = p0 - self.c
        self.a0 = atan2(dp.y, dp.x)

    @property
    def length(self):
        """
            arc length
        """
        return self.r * abs(self.da)

    @property
    def oposite(self):
        a0 = self.a0 + self.da
        if a0 > pi:
            a0 -= 2 * pi
        if a0 < -pi:
            a0 += 2 * pi
        return Arc(self.c, self.r, a0, -self.da)

    def normal(self, t=0):
        """
            Perpendicular line starting at t
            always on the right side
        """
        p = self.lerp(t)
        if self.da < 0:
            return Line(p, self.c - p)
        else:
            return Line(p, p - self.c)

    def sized_normal(self, t, size):
        """
            Perpendicular line starting at t and of a length size
            on the right side when size > 0
        """
        p = self.lerp(t)
        if self.da < 0:
            v = self.c - p
        else:
            v = p - self.c
        return Line(p, size * v.normalized())

    def lerp(self, t):
        """
            Interpolate along segment
            t parameter [0, 1] where 0 is start of arc and 1 is end
        """
        a = self.a0 + t * self.da
        return self.c + Vector((self.r * cos(a), self.r * sin(a)))

    def steps(self, length):
        """
            Compute step count given desired step length
        """
        steps = max(1, round(self.length / length, 0))
        return 1.0 / steps, int(steps)

    def intersect_ext(self, line):
        """
            same as intersect, but return param t on both lines
        """
        res, p, v = self.intersect(line)
        v0 = self.p0 - self.c
        v1 = p - self.c
        u = self.signed_angle(v0, v1) / self.da
        return res and u > 0 and v > 0 and u < 1 and v < 1, p, u, v

    # this is for wall
    def steps_by_angle(self, step_angle):
        steps = max(1, round(abs(self.da) / step_angle, 0))
        return 1.0 / steps, int(steps)

    def as_lines(self, steps):
        """
            convert Arc to lines
        """
        res = []
        p0 = self.lerp(0)
        for step in range(steps):
            p1 = self.lerp((step + 1) / steps)
            s = Line(p0=p0, p1=p1)
            res.append(s)
            p0 = p1

        if self.line is not None:
            p0 = self.line.lerp(0)
            for step in range(steps):
                p1 = self.line.lerp((step + 1) / steps)
                res[step].line = Line(p0=p0, p1=p1)
                p0 = p1
        return res

    def offset(self, offset):
        """
            Offset circle
            offset > 0 on the right part
        """
        if self.da > 0:
            radius = self.r + offset
        else:
            radius = self.r - offset
        return Arc(self.c, radius, self.a0, self.da)

    def tangeant(self, t, length):
        """
            Tangeant line so we are able to chain Circle and lines
            Beware, counterpart on Line does return an Arc !
        """
        a = self.a0 + t * self.da
        ca = cos(a)
        sa = sin(a)
        p = self.c + Vector((self.r * ca, self.r * sa))
        v = Vector((length * sa, -length * ca))
        if self.da > 0:
            v = -v
        return Line(p, v)

    def tangeant_unit_vector(self, t):
        """
            Return Tangeant vector of length 1
        """
        a = self.a0 + t * self.da
        ca = cos(a)
        sa = sin(a)
        v = Vector((sa, -ca))
        if self.da > 0:
            v = -v
        return v

    def straight(self, length, t=1):
        """
            Return a tangeant Line
            Counterpart on Line also return a Line
        """
        return self.tangeant(t, length)

    def point_sur_segment(self, pt):
        """
            Point pt lie on arc ?
            return
            True when pt lie on segment
            t [0, 1] where it lie (normalized between start and end)
            d distance from arc
        """
        dp = pt - self.c
        d = dp.length - self.r
        a = atan2(dp.y, dp.x)
        t = (a - self.a0) / self.da
        return t > 0 and t < 1, d, t

    def rotate(self, a):
        """
            Rotate center so we rotate ccw arround p0
        """
        ca = cos(a)
        sa = sin(a)
        rM = Matrix([
            [ca, -sa],
            [sa, ca]
            ])
        p0 = self.p0
        self.c = p0 + rM * (self.c - p0)
        dp = p0 - self.c
        self.a0 = atan2(dp.y, dp.x)
        return self

    # make offset for line / arc, arc / arc
    def make_offset(self, offset, last=None):

        line = self.offset(offset)

        if last is None:
            return line

        if hasattr(last, "v"):
            # intersect line / arc
            # 1 line -> 2 arc
            res, d, t = last.point_sur_segment(line.c)
            c = line.r2 - (d * d)
            if c <= 0:
                # no intersection !
                p0 = last.lerp(t)
            else:

                # center is past end of line
                if t > 1:
                    # Arc take precedence
                    p0 = last.lerp(t) - last.v.normalized() * sqrt(c)
                else:
                    # line take precedence
                    p0 = last.lerp(t) + last.v.normalized() * sqrt(c)

            # compute a0 and da of arc
            u = p0 - line.c
            v = line.p1 - line.c
            line.a0 = atan2(u.y, u.x)
            da = self.signed_angle(u, v)
            # da is ccw
            if self.ccw:
                # da is cw
                if da < 0:
                    # so take inverse
                    da = 2 * pi + da
            elif da > 0:
                # da is ccw
                da = 2 * pi - da
            line.da = da
            last.p1 = p0
        else:
            # intersect arc / arc x1 = self x0 = last
            # rule to determine right side ->
            # same side of d as p0 of self
            dc = line.c - last.c
            tmp = Line(last.c, dc)
            res, d, t = tmp.point_sur_segment(self.p0)
            r = line.r + last.r
            dist = dc.length
            if dist > r or \
                dist < abs(last.r - self.r):
                # no intersection
                return line
            if dist == r:
                # 1 solution
                p0 = dc * -last.r / r + self.c
            else:
                # 2 solutions
                a = (last.r2 - line.r2 + dist * dist) / (2.0 * dist)
                v2 = last.c + dc * a / dist
                h = sqrt(last.r2 - a * a)
                r = Vector((-dc.y, dc.x)) * (h / dist)
                p0 = v2 + r
                res, d1, t = tmp.point_sur_segment(p0)
                # take other point if we are not on the same side
                if d1 > 0:
                    if d < 0:
                        p0 = v2 - r
                elif d > 0:
                    p0 = v2 - r

            # compute da of last
            u = last.p0 - last.c
            v = p0 - last.c
            last.da = self.signed_angle(u, v)

            # compute a0 and da of current
            u, v = v, line.p1 - line.c
            line.a0 = atan2(u.y, u.x)
            line.da = self.signed_angle(u, v)
        return line

    # DEBUG
    @property
    def pts(self):
        n_pts = max(1, int(round(abs(self.da) / pi * 30, 0)))
        t_step = 1 / n_pts
        return [self.lerp(i * t_step).to_3d() for i in range(n_pts + 1)]

    def as_curve(self, context):
        """
            Draw 2d arc with open gl in screen space
            aka: coords are in pixels
        """
        curve = bpy.data.curves.new('ARC', type='CURVE')
        curve.dimensions = '2D'
        spline = curve.splines.new('POLY')
        spline.use_endpoint_u = False
        spline.use_cyclic_u = False
        pts = self.pts
        spline.points.add(len(pts) - 1)
        for i, p in enumerate(pts):
            x, y = p
            spline.points[i].co = (x, y, 0, 1)
        curve_obj = bpy.data.objects.new('ARC', curve)
        context.scene.objects.link(curve_obj)
        curve_obj.select = True


class Line3d(Line):
    """
        3d Line
        mostly a gl enabled for future use in manipulators
        coords are in world space
    """
    def __init__(self, p=None, v=None, p0=None, p1=None, z_axis=None):
        """
            Init by either
            p: Vector or tuple origin
            v: Vector or tuple size and direction
            or
            p0: Vector or tuple 1 point location
            p1: Vector or tuple 2 point location
            Will convert any into Vector 3d
            both optionnals
        """
        if p is not None and v is not None:
            self.p = Vector(p).to_3d()
            self.v = Vector(v).to_3d()
        elif p0 is not None and p1 is not None:
            self.p = Vector(p0).to_3d()
            self.v = Vector(p1).to_3d() - self.p
        else:
            self.p = Vector((0, 0, 0))
            self.v = Vector((0, 0, 0))
        if z_axis is not None:
            self.z_axis = z_axis
        else:
            self.z_axis = Vector((0, 0, 1))

    @property
    def p0(self):
        return self.p

    @property
    def p1(self):
        return self.p + self.v

    @p0.setter
    def p0(self, p0):
        """
            Note: setting p0
            move p0 only
        """
        p1 = self.p1
        self.p = Vector(p0).to_3d()
        self.v = p1 - p0

    @p1.setter
    def p1(self, p1):
        """
            Note: setting p1
            move p1 only
        """
        self.v = Vector(p1).to_3d() - self.p

    @property
    def cross_z(self):
        """
            3d Vector perpendicular on plane xy
            lie on the right side
            p1
            |--x
            p0
        """
        return self.v.cross(Vector((0, 0, 1)))

    @property
    def cross(self):
        """
            3d Vector perpendicular on plane defined by z_axis
            lie on the right side
            p1
            |--x
            p0
        """
        return self.v.cross(self.z_axis)

    def normal(self, t=0):
        """
            3d Vector perpendicular on plane defined by z_axis
            lie on the right side
            p1
            |--x
            p0
        """
        n = Line3d()
        n.p = self.lerp(t)
        n.v = self.cross
        return n

    def sized_normal(self, t, size):
        """
            3d Line perpendicular on plane defined by z_axis and of given size
            positionned at t in current line
            lie on the right side
            p1
            |--x
            p0
        """
        p = self.lerp(t)
        v = size * self.cross.normalized()
        return Line3d(p, v, z_axis=self.z_axis)

    def offset(self, offset):
        """
            offset > 0 on the right part
        """
        return Line3d(self.p + offset * self.cross.normalized(), self.v)

    # unless override, 2d methods should raise NotImplementedError
    def intersect(self, line):
        raise NotImplementedError

    def point_sur_segment(self, pt):
        raise NotImplementedError

    def tangeant(self, t, da, radius):
        raise NotImplementedError
