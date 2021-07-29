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
# Cutter / CutAble shared by roof, slab, and floor
# ----------------------------------------------------------
from mathutils import Vector, Matrix
from mathutils.geometry import interpolate_bezier
from math import cos, sin, pi, atan2
import bmesh
from random import uniform
from bpy.props import (
    FloatProperty, IntProperty, BoolProperty,
    StringProperty, EnumProperty
    )
from .archipack_2d import Line


class CutterSegment(Line):

    def __init__(self, p, v, type='DEFAULT'):
        Line.__init__(self, p, v)
        self.type = type
        self.is_hole = True

    @property
    def copy(self):
        return CutterSegment(self.p.copy(), self.v.copy(), self.type)

    def straight(self, length, t=1):
        s = self.copy
        s.p = self.lerp(t)
        s.v = self.v.normalized() * length
        return s

    def set_offset(self, offset, last=None):
        """
            Offset line and compute intersection point
            between segments
        """
        self.line = self.make_offset(offset, last)

    def offset(self, offset):
        s = self.copy
        s.p += self.sized_normal(0, offset).v
        return s

    @property
    def oposite(self):
        s = self.copy
        s.p += s.v
        s.v = -s.v
        return s


class CutterGenerator():

    def __init__(self, d):
        self.parts = d.parts
        self.operation = d.operation
        self.segs = []

    def add_part(self, part):

        if len(self.segs) < 1:
            s = None
        else:
            s = self.segs[-1]

        # start a new Cutter
        if s is None:
            v = part.length * Vector((cos(part.a0), sin(part.a0)))
            s = CutterSegment(Vector((0, 0)), v, part.type)
        else:
            s = s.straight(part.length).rotate(part.a0)
            s.type = part.type

        self.segs.append(s)

    def set_offset(self):
        last = None
        for i, seg in enumerate(self.segs):
            seg.set_offset(self.parts[i].offset, last)
            last = seg.line

    def close(self):
        # Make last segment implicit closing one
        s0 = self.segs[-1]
        s1 = self.segs[0]
        dp = s1.p0 - s0.p0
        s0.v = dp

        if len(self.segs) > 1:
            s0.line = s0.make_offset(self.parts[-1].offset, self.segs[-2].line)

        p1 = s1.line.p1
        s1.line = s1.make_offset(self.parts[0].offset, s0.line)
        s1.line.p1 = p1

    def locate_manipulators(self):
        if self.operation == 'DIFFERENCE':
            side = -1
        else:
            side = 1
        for i, f in enumerate(self.segs):

            manipulators = self.parts[i].manipulators
            p0 = f.p0.to_3d()
            p1 = f.p1.to_3d()
            # angle from last to current segment
            if i > 0:

                if i < len(self.segs) - 1:
                    manipulators[0].type_key = 'ANGLE'
                else:
                    manipulators[0].type_key = 'DUMB_ANGLE'

                v0 = self.segs[i - 1].straight(-side, 1).v.to_3d()
                v1 = f.straight(side, 0).v.to_3d()
                manipulators[0].set_pts([p0, v0, v1])

            # segment length
            manipulators[1].type_key = 'SIZE'
            manipulators[1].prop1_name = "length"
            manipulators[1].set_pts([p0, p1, (side, 0, 0)])

            # snap manipulator, dont change index !
            manipulators[2].set_pts([p0, p1, (side, 0, 0)])
            # dumb segment id
            manipulators[3].set_pts([p0, p1, (side, 0, 0)])

            # offset
            manipulators[4].set_pts([
                p0,
                p0 + f.sized_normal(0, max(0.0001, self.parts[i].offset)).v.to_3d(),
                (0.5, 0, 0)
            ])

    def change_coordsys(self, fromTM, toTM):
        """
            move shape fromTM into toTM coordsys
        """
        dp = (toTM.inverted() * fromTM.translation).to_2d()
        da = toTM.row[1].to_2d().angle_signed(fromTM.row[1].to_2d())
        ca = cos(da)
        sa = sin(da)
        rM = Matrix([
            [ca, -sa],
            [sa, ca]
            ])
        for s in self.segs:
            tp = (rM * s.p0) - s.p0 + dp
            s.rotate(da)
            s.translate(tp)

    def get_index(self, index):
        n_segs = len(self.segs)
        if index >= n_segs:
            index -= n_segs
        return index

    def next_seg(self, index):
        idx = self.get_index(index + 1)
        return self.segs[idx]

    def last_seg(self, index):
        return self.segs[index - 1]

    def get_verts(self, verts, edges):

        n_segs = len(self.segs) - 1

        for s in self.segs:
            verts.append(s.line.p0.to_3d())

        for i in range(n_segs):
            edges.append([i, i + 1])


class CutAblePolygon():
    """
        Simple boolean operations
        Cutable generator / polygon
        Object MUST have properties
        - segs
        - holes
        - convex
    """
    def as_lines(self, step_angle=0.104):
        """
            Convert curved segments to straight lines
        """
        segs = []
        for s in self.segs:
            if "Curved" in type(s).__name__:
                dt, steps = s.steps_by_angle(step_angle)
                segs.extend(s.as_lines(steps))
            else:
                segs.append(s)
        self.segs = segs

    def inside(self, pt, segs=None):
        """
            Point inside poly (raycast method)
            support concave polygons
            TODO:
            make s1 angle different than all othr segs
        """
        s1 = Line(pt, Vector((min(10000, 100 * self.xsize), uniform(-0.5, 0.5))))
        counter = 0
        if segs is None:
            segs = self.segs
        for s in segs:
            res, p, t, u = s.intersect_ext(s1)
            if res:
                counter += 1
        return counter % 2 == 1

    def get_index(self, index):
        n_segs = len(self.segs)
        if index >= n_segs:
            index -= n_segs
        return index

    def is_convex(self):
        n_segs = len(self.segs)
        self.convex = True
        sign = False
        s0 = self.segs[-1]
        for i in range(n_segs):
            s1 = self.segs[i]
            if "Curved" in type(s1).__name__:
                self.convex = False
                return
            c = s0.v.cross(s1.v)
            if i == 0:
                sign = (c > 0)
            elif sign != (c > 0):
                self.convex = False
                return
            s0 = s1

    def get_intersections(self, border, cutter, s_start, segs, start_by_hole):
        """
            Detect all intersections
            for boundary: store intersection point, t, idx of segment, idx of cutter
            sort by t
        """
        s_segs = border.segs
        b_segs = cutter.segs
        s_nsegs = len(s_segs)
        b_nsegs = len(b_segs)
        inter = []

        # find all intersections
        for idx in range(s_nsegs):
            s_idx = border.get_index(s_start + idx)
            s = s_segs[s_idx]
            for b_idx, b in enumerate(b_segs):
                res, p, u, v = s.intersect_ext(b)
                if res:
                    inter.append((s_idx, u, b_idx, v, p))

        # print("%s" % (self.side))
        # print("%s" % (inter))

        if len(inter) < 1:
            return True

        # sort by seg and param t of seg
        inter.sort()

        # reorder so we realy start from s_start
        for i, it in enumerate(inter):
            if it[0] >= s_start:
                order = i
                break

        inter = inter[order:] + inter[:order]

        # print("%s" % (inter))
        p0 = border.segs[s_start].p0

        n_inter = len(inter) - 1

        for i in range(n_inter):
            s_end, u, b_start, v, p = inter[i]
            s_idx = border.get_index(s_start)
            s = s_segs[s_idx].copy
            s.is_hole = not start_by_hole
            segs.append(s)
            idx = s_idx
            max_iter = s_nsegs
            # walk through s_segs until intersection
            while s_idx != s_end and max_iter > 0:
                idx += 1
                s_idx = border.get_index(idx)
                s = s_segs[s_idx].copy
                s.is_hole = not start_by_hole
                segs.append(s)
                max_iter -= 1
            segs[-1].p1 = p

            s_start, u, b_end, v, p = inter[i + 1]
            b_idx = cutter.get_index(b_start)
            s = b_segs[b_idx].copy
            s.is_hole = start_by_hole
            segs.append(s)
            idx = b_idx
            max_iter = b_nsegs
            # walk through b_segs until intersection
            while b_idx != b_end and max_iter > 0:
                idx += 1
                b_idx = cutter.get_index(idx)
                s = b_segs[b_idx].copy
                s.is_hole = start_by_hole
                segs.append(s)
                max_iter -= 1
            segs[-1].p1 = p

        # add part between last intersection and start point
        idx = s_start
        s_idx = border.get_index(s_start)
        s = s_segs[s_idx].copy
        s.is_hole = not start_by_hole
        segs.append(s)
        max_iter = s_nsegs
        # go until end of segment is near start of first one
        while (s_segs[s_idx].p1 - p0).length > 0.0001 and max_iter > 0:
            idx += 1
            s_idx = border.get_index(idx)
            s = s_segs[s_idx].copy
            s.is_hole = not start_by_hole
            segs.append(s)
            max_iter -= 1

        if len(segs) > s_nsegs + b_nsegs + 1:
            # print("slice failed found:%s of:%s" % (len(segs), s_nsegs + b_nsegs))
            return False

        for i, s in enumerate(segs):
            s.p0 = segs[i - 1].p1

        return True

    def slice(self, cutter):
        """
            Simple 2d Boolean between boundary and roof part
            Dosen't handle slicing roof into multiple parts

            4 cases:
            1 pitch has point in boundary -> start from this point
            2 boundary has point in pitch -> start from this point
            3 no points inside -> find first crossing segment
            4 not points inside and no crossing segments
        """
        # print("************")

        # keep inside or cut inside
        # keep inside must be CCW
        # cut inside must be CW
        keep_inside = (cutter.operation == 'INTERSECTION')

        start = -1

        f_segs = self.segs
        c_segs = cutter.segs
        store = []

        slice_res = True
        is_inside = False

        # find if either a cutter or
        # cutter intersects
        # (at least one point of any must be inside other one)

        # find a point of this pitch inside cutter
        for i, s in enumerate(f_segs):
            res = self.inside(s.p0, c_segs)
            if res:
                is_inside = True
            if res == keep_inside:
                start = i
                # print("pitch pt %sside f_start:%s %s" % (in_out, start, self.side))
                slice_res = self.get_intersections(self, cutter, start, store, True)
                break

        # seek for point of cutter inside pitch
        for i, s in enumerate(c_segs):
            res = self.inside(s.p0)
            if res:
                is_inside = True
            # no pitch point found inside cutter
            if start < 0 and res == keep_inside:
                start = i
                # print("cutter pt %sside c_start:%s %s" % (in_out, start, self.side))
                # swap cutter / pitch so we start from cutter
                slice_res = self.get_intersections(cutter, self, start, store, False)
                break

        # no points found at all
        if start < 0:
            # print("no pt inside")
            return not keep_inside

        if not slice_res:
            # print("slice fails")
            # found more segments than input
            # cutter made more than one loop
            return True

        if len(store) < 1:
            if is_inside:
                # print("not touching, add as hole")
                if keep_inside:
                    self.segs = cutter.segs
                else:
                    self.holes.append(cutter)

            return True

        self.segs = store
        self.is_convex()

        return True


class CutAbleGenerator():

    def bissect(self, bm,
            plane_co,
            plane_no,
            dist=0.001,
            use_snap_center=False,
            clear_outer=True,
            clear_inner=False
            ):
        geom = bm.verts[:]
        geom.extend(bm.edges[:])
        geom.extend(bm.faces[:])

        bmesh.ops.bisect_plane(bm,
            geom=geom,
            dist=dist,
            plane_co=plane_co,
            plane_no=plane_no,
            use_snap_center=False,
            clear_outer=clear_outer,
            clear_inner=clear_inner
            )

    def cut_holes(self, bm, cutable, offset={'DEFAULT': 0}):
        o_keys = offset.keys()
        has_offset = len(o_keys) > 1 or offset['DEFAULT'] != 0
        # cut holes
        for hole in cutable.holes:

            if has_offset:

                for s in hole.segs:
                    if s.length > 0:
                        if s.type in o_keys:
                            of = offset[s.type]
                        else:
                            of = offset['DEFAULT']
                        n = s.sized_normal(0, 1).v
                        p0 = s.p0 + n * of
                        self.bissect(bm, p0.to_3d(), n.to_3d(), clear_outer=False)

                # compute boundary with offset
                new_s = None
                segs = []
                for s in hole.segs:
                    if s.length > 0:
                        if s.type in o_keys:
                            of = offset[s.type]
                        else:
                            of = offset['DEFAULT']
                        new_s = s.make_offset(of, new_s)
                        segs.append(new_s)
                # last / first intersection
                if len(segs) > 0:
                    res, p0, t = segs[0].intersect(segs[-1])
                    if res:
                        segs[0].p0 = p0
                        segs[-1].p1 = p0

            else:
                for s in hole.segs:
                    if s.length > 0:
                        n = s.sized_normal(0, 1).v
                        self.bissect(bm, s.p0.to_3d(), n.to_3d(), clear_outer=False)
                # use hole boundary
                segs = hole.segs
            if len(segs) > 0:
                # when hole segs are found clear parts inside hole
                f_geom = [f for f in bm.faces
                    if cutable.inside(
                        f.calc_center_median().to_2d(),
                        segs=segs)]
                if len(f_geom) > 0:
                    bmesh.ops.delete(bm, geom=f_geom, context=5)

    def cut_boundary(self, bm, cutable, offset={'DEFAULT': 0}):
        o_keys = offset.keys()
        has_offset = len(o_keys) > 1 or offset['DEFAULT'] != 0
        # cut outside parts
        if has_offset:
            for s in cutable.segs:
                if s.length > 0:
                    if s.type in o_keys:
                        of = offset[s.type]
                    else:
                        of = offset['DEFAULT']
                    n = s.sized_normal(0, 1).v
                    p0 = s.p0 + n * of
                    self.bissect(bm, p0.to_3d(), n.to_3d(), clear_outer=cutable.convex)
        else:
            for s in cutable.segs:
                if s.length > 0:
                    n = s.sized_normal(0, 1).v
                    self.bissect(bm, s.p0.to_3d(), n.to_3d(), clear_outer=cutable.convex)

        if not cutable.convex:
            f_geom = [f for f in bm.faces
                if not cutable.inside(f.calc_center_median().to_2d())]
            if len(f_geom) > 0:
                bmesh.ops.delete(bm, geom=f_geom, context=5)


def update_hole(self, context):
    # update parent's only when manipulated
    self.update(context, update_parent=True)


class ArchipackCutterPart():
    """
        Cutter segment PropertyGroup

        Childs MUST implements
        -find_in_selection
        Childs MUST define
        -type EnumProperty
    """
    length = FloatProperty(
            name="Length",
            min=0.01,
            max=1000.0,
            default=2.0,
            update=update_hole
            )
    a0 = FloatProperty(
            name="Angle",
            min=-2 * pi,
            max=2 * pi,
            default=0,
            subtype='ANGLE', unit='ROTATION',
            update=update_hole
            )
    offset = FloatProperty(
            name="Offset",
            min=0,
            default=0,
            update=update_hole
            )

    def find_in_selection(self, context):
        raise NotImplementedError

    def draw(self, layout, context, index):
        box = layout.box()
        box.prop(self, "type", text=str(index + 1))
        box.prop(self, "length")
        # box.prop(self, "offset")
        box.prop(self, "a0")

    def update(self, context, update_parent=False):
        props = self.find_in_selection(context)
        if props is not None:
            props.update(context, update_parent=update_parent)


def update_operation(self, context):
    self.reverse(context, make_ccw=(self.operation == 'INTERSECTION'))


def update_path(self, context):
    self.update_path(context)


def update(self, context):
    self.update(context)


def update_manipulators(self, context):
    self.update(context, manipulable_refresh=True)


class ArchipackCutter():
    n_parts = IntProperty(
            name="Parts",
            min=1,
            default=1, update=update_manipulators
            )
    z = FloatProperty(
            name="dumb z",
            description="Dumb z for manipulator placeholder",
            default=0.01,
            options={'SKIP_SAVE'}
            )
    user_defined_path = StringProperty(
            name="User defined",
            update=update_path
            )
    user_defined_resolution = IntProperty(
            name="Resolution",
            min=1,
            max=128,
            default=12, update=update_path
            )
    operation = EnumProperty(
            items=(
                ('DIFFERENCE', 'Difference', 'Cut inside part', 0),
                ('INTERSECTION', 'Intersection', 'Keep inside part', 1)
                ),
            default='DIFFERENCE',
            update=update_operation
            )
    auto_update = BoolProperty(
            options={'SKIP_SAVE'},
            default=True,
            update=update_manipulators
            )
    # UI layout related
    parts_expand = BoolProperty(
            default=False
            )
    closed = BoolProperty(
            description="keep closed to be wall snap manipulator compatible",
            options={'SKIP_SAVE'},
            default=True
            )

    def draw(self, layout, context):
        box = layout.box()
        row = box.row()
        if self.parts_expand:
            row.prop(self, 'parts_expand', icon="TRIA_DOWN", icon_only=True, text="Parts", emboss=False)
            box.prop(self, 'n_parts')
            for i, part in enumerate(self.parts):
                part.draw(layout, context, i)
        else:
            row.prop(self, 'parts_expand', icon="TRIA_RIGHT", icon_only=True, text="Parts", emboss=False)

    def update_parts(self):
        # print("update_parts")
        # remove rows
        # NOTE:
        # n_parts+1
        # as last one is end point of last segment or closing one
        for i in range(len(self.parts), self.n_parts + 1, -1):
            self.parts.remove(i - 1)

        # add rows
        for i in range(len(self.parts), self.n_parts + 1):
            self.parts.add()

        self.setup_manipulators()

    def update_parent(self, context):
        raise NotImplementedError

    def setup_manipulators(self):
        for i in range(self.n_parts + 1):
            p = self.parts[i]
            n_manips = len(p.manipulators)
            if n_manips < 1:
                s = p.manipulators.add()
                s.type_key = "ANGLE"
                s.prop1_name = "a0"
            if n_manips < 2:
                s = p.manipulators.add()
                s.type_key = "SIZE"
                s.prop1_name = "length"
            if n_manips < 3:
                s = p.manipulators.add()
                s.type_key = 'WALL_SNAP'
                s.prop1_name = str(i)
                s.prop2_name = 'z'
            if n_manips < 4:
                s = p.manipulators.add()
                s.type_key = 'DUMB_STRING'
                s.prop1_name = str(i + 1)
            if n_manips < 5:
                s = p.manipulators.add()
                s.type_key = "SIZE"
                s.prop1_name = "offset"
            p.manipulators[2].prop1_name = str(i)
            p.manipulators[3].prop1_name = str(i + 1)

    def get_generator(self):
        g = CutterGenerator(self)
        for i, part in enumerate(self.parts):
            g.add_part(part)
        g.set_offset()
        g.close()
        return g

    def interpolate_bezier(self, pts, wM, p0, p1, resolution):
        # straight segment, worth testing here
        # since this can lower points count by a resolution factor
        # use normalized to handle non linear t
        if resolution == 0:
            pts.append(wM * p0.co.to_3d())
        else:
            v = (p1.co - p0.co).normalized()
            d1 = (p0.handle_right - p0.co).normalized()
            d2 = (p1.co - p1.handle_left).normalized()
            if d1 == v and d2 == v:
                pts.append(wM * p0.co.to_3d())
            else:
                seg = interpolate_bezier(wM * p0.co,
                    wM * p0.handle_right,
                    wM * p1.handle_left,
                    wM * p1.co,
                    resolution + 1)
                for i in range(resolution):
                    pts.append(seg[i].to_3d())

    def is_cw(self, pts):
        p0 = pts[0]
        d = 0
        for p in pts[1:]:
            d += (p.x * p0.y - p.y * p0.x)
            p0 = p
        return d > 0

    def ensure_direction(self):
        # get segs ensure they are cw or ccw depending on operation
        # whatever the user do with points
        g = self.get_generator()
        pts = [seg.p0.to_3d() for seg in g.segs]
        if self.is_cw(pts) != (self.operation == 'INTERSECTION'):
            return g
        g.segs = [s.oposite for s in reversed(g.segs)]
        return g

    def from_spline(self, context, wM, resolution, spline):
        pts = []
        if spline.type == 'POLY':
            pts = [wM * p.co.to_3d() for p in spline.points]
            if spline.use_cyclic_u:
                pts.append(pts[0])
        elif spline.type == 'BEZIER':
            points = spline.bezier_points
            for i in range(1, len(points)):
                p0 = points[i - 1]
                p1 = points[i]
                self.interpolate_bezier(pts, wM, p0, p1, resolution)
            if spline.use_cyclic_u:
                p0 = points[-1]
                p1 = points[0]
                self.interpolate_bezier(pts, wM, p0, p1, resolution)
                pts.append(pts[0])
            else:
                pts.append(wM * points[-1].co)

        if self.is_cw(pts) == (self.operation == 'INTERSECTION'):
            pts = list(reversed(pts))

        pt = wM.inverted() * pts[0]

        # pretranslate
        o = self.find_in_selection(context, self.auto_update)
        o.matrix_world = wM * Matrix([
            [1, 0, 0, pt.x],
            [0, 1, 0, pt.y],
            [0, 0, 1, pt.z],
            [0, 0, 0, 1]
            ])
        self.auto_update = False
        self.from_points(pts)
        self.auto_update = True
        self.update_parent(context, o)

    def from_points(self, pts):

        self.n_parts = len(pts) - 2

        self.update_parts()

        p0 = pts.pop(0)
        a0 = 0
        for i, p1 in enumerate(pts):
            dp = p1 - p0
            da = atan2(dp.y, dp.x) - a0
            if da > pi:
                da -= 2 * pi
            if da < -pi:
                da += 2 * pi
            if i >= len(self.parts):
                # print("Too many pts for parts")
                break
            p = self.parts[i]
            p.length = dp.to_2d().length
            p.dz = dp.z
            p.a0 = da
            a0 += da
            p0 = p1

    def reverse(self, context, make_ccw=False):

        o = self.find_in_selection(context, self.auto_update)

        g = self.get_generator()

        pts = [seg.p0.to_3d() for seg in g.segs]

        if self.is_cw(pts) != make_ccw:
            return

        types = [p.type for p in self.parts]

        pts.append(pts[0])

        pts = list(reversed(pts))
        self.auto_update = False

        self.from_points(pts)

        for i, type in enumerate(reversed(types)):
            self.parts[i].type = type
        self.auto_update = True
        self.update_parent(context, o)

    def update_path(self, context):
        user_def_path = context.scene.objects.get(self.user_defined_path)
        if user_def_path is not None and user_def_path.type == 'CURVE':
            self.from_spline(context,
                user_def_path.matrix_world,
                self.user_defined_resolution,
                user_def_path.data.splines[0])

    def make_surface(self, o, verts, edges):
        bm = bmesh.new()
        for v in verts:
            bm.verts.new(v)
        bm.verts.ensure_lookup_table()
        for ed in edges:
            bm.edges.new((bm.verts[ed[0]], bm.verts[ed[1]]))
        bm.edges.new((bm.verts[-1], bm.verts[0]))
        bm.edges.ensure_lookup_table()
        bm.to_mesh(o.data)
        bm.free()

    def update(self, context, manipulable_refresh=False, update_parent=False):

        o = self.find_in_selection(context, self.auto_update)

        if o is None:
            return

        # clean up manipulators before any data model change
        if manipulable_refresh:
            self.manipulable_disable(context)

        self.update_parts()

        verts = []
        edges = []

        g = self.get_generator()
        g.locate_manipulators()

        # vertex index in order to build axis
        g.get_verts(verts, edges)

        if len(verts) > 2:
            self.make_surface(o, verts, edges)

        # enable manipulators rebuild
        if manipulable_refresh:
            self.manipulable_refresh = True

        # update parent on direct edit
        if manipulable_refresh or update_parent:
            self.update_parent(context, o)

        # restore context
        self.restore_context(context)

    def manipulable_setup(self, context):

        self.manipulable_disable(context)
        o = context.active_object

        n_parts = self.n_parts + 1

        self.setup_manipulators()

        for i, part in enumerate(self.parts):
            if i < n_parts:

                if i > 0:
                    # start angle
                    self.manip_stack.append(part.manipulators[0].setup(context, o, part))

                # length
                self.manip_stack.append(part.manipulators[1].setup(context, o, part))
                # index
                self.manip_stack.append(part.manipulators[3].setup(context, o, self))
                # offset
                # self.manip_stack.append(part.manipulators[4].setup(context, o, part))

            # snap point
            self.manip_stack.append(part.manipulators[2].setup(context, o, self))
