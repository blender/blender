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
import bpy
import bmesh

import time

from bpy.types import Operator, PropertyGroup, Mesh, Panel
from bpy.props import (
    FloatProperty, BoolProperty, IntProperty, StringProperty,
    FloatVectorProperty, CollectionProperty, EnumProperty
)
from .bmesh_utils import BmeshEdit as bmed
from mathutils import Vector, Matrix
from mathutils.geometry import (
    interpolate_bezier
    )
from math import sin, cos, pi, atan2
from .archipack_manipulator import (
    Manipulable, archipack_manipulator,
    GlPolygon, GlPolyline,
    GlLine, GlText, FeedbackPanel
    )
from .archipack_object import ArchipackObject, ArchipackCreateTool, ArchpackDrawTool
from .archipack_2d import Line, Arc
from .archipack_snap import snap_point
from .archipack_keymaps import Keymaps


class Wall():
    def __init__(self, wall_z, z, t, flip):
        self.z = z
        self.wall_z = wall_z
        self.t = t
        self.flip = flip
        self.z_step = len(z)

    def set_offset(self, offset, last=None):
        """
            Offset line and compute intersection point
            between segments
        """
        self.line = self.make_offset(offset, last)

    def get_z(self, t):
        t0 = self.t[0]
        z0 = self.z[0]
        for i in range(1, self.z_step):
            t1 = self.t[i]
            z1 = self.z[i]
            if t <= t1:
                return z0 + (t - t0) / (t1 - t0) * (z1 - z0)
            t0, z0 = t1, z1
        return self.z[-1]

    def make_faces(self, i, f, faces):
        if i < self.n_step:
            # 1 3   5 7
            # 0 2   4 6
            if self.flip:
                faces.append((f + 2, f, f + 1, f + 3))
            else:
                faces.append((f, f + 2, f + 3, f + 1))

    def p3d(self, verts, t):
        x, y = self.lerp(t)
        z = self.wall_z + self.get_z(t)
        verts.append((x, y, 0))
        verts.append((x, y, z))

    def make_wall(self, i, verts, faces):
        t = self.t_step[i]
        f = len(verts)
        self.p3d(verts, t)
        self.make_faces(i, f, faces)

    def make_hole(self, i, verts, z0):
        t = self.t_step[i]
        x, y = self.line.lerp(t)
        verts.append((x, y, z0))

    def straight_wall(self, a0, length, wall_z, z, t):
        r = self.straight(length).rotate(a0)
        return StraightWall(r.p, r.v, wall_z, z, t, self.flip)

    def curved_wall(self, a0, da, radius, wall_z, z, t):
        n = self.normal(1).rotate(a0).scale(radius)
        if da < 0:
            n.v = -n.v
        a0 = n.angle
        c = n.p - n.v
        return CurvedWall(c, radius, a0, da, wall_z, z, t, self.flip)


class StraightWall(Wall, Line):
    def __init__(self, p, v, wall_z, z, t, flip):
        Line.__init__(self, p, v)
        Wall.__init__(self, wall_z, z, t, flip)

    def param_t(self, step_angle):
        self.t_step = self.t
        self.n_step = len(self.t) - 1


class CurvedWall(Wall, Arc):
    def __init__(self, c, radius, a0, da, wall_z, z, t, flip):
        Arc.__init__(self, c, radius, a0, da)
        Wall.__init__(self, wall_z, z, t, flip)

    def param_t(self, step_angle):
        t_step, n_step = self.steps_by_angle(step_angle)
        self.t_step = list(sorted([i * t_step for i in range(1, n_step)] + self.t))
        self.n_step = len(self.t_step) - 1


class WallGenerator():
    def __init__(self, parts):
        self.last_type = 'NONE'
        self.segs = []
        self.parts = parts
        self.faces_type = 'NONE'
        self.closed = False

    def set_offset(self, offset):
        last = None
        for i, seg in enumerate(self.segs):
            seg.set_offset(offset, last)
            last = seg.line

        if self.closed:
            w = self.segs[-1]
            if len(self.segs) > 1:
                w.line = w.make_offset(offset, self.segs[-2].line)

            p1 = self.segs[0].line.p1
            self.segs[0].line = self.segs[0].make_offset(offset, w.line)
            self.segs[0].line.p1 = p1

    def add_part(self, part, wall_z, flip):

        # TODO:
        # refactor this part (height manipulators)
        manip_index = []
        if len(self.segs) < 1:
            s = None
            z = [part.z[0]]
            manip_index.append(0)
        else:
            s = self.segs[-1]
            z = [s.z[-1]]

        t_cur = 0
        z_last = part.n_splits - 1
        t = [0]

        for i in range(part.n_splits):
            t_try = t[-1] + part.t[i]
            if t_try == t_cur:
                continue
            if t_try <= 1:
                t_cur = t_try
                t.append(t_cur)
                z.append(part.z[i])
                manip_index.append(i)
            else:
                z_last = i
                break

        if t_cur < 1:
            t.append(1)
            manip_index.append(z_last)
            z.append(part.z[z_last])

        # start a new wall
        if s is None:
            if part.type == 'S_WALL':
                p = Vector((0, 0))
                v = part.length * Vector((cos(part.a0), sin(part.a0)))
                s = StraightWall(p, v, wall_z, z, t, flip)
            elif part.type == 'C_WALL':
                c = -part.radius * Vector((cos(part.a0), sin(part.a0)))
                s = CurvedWall(c, part.radius, part.a0, part.da, wall_z, z, t, flip)
        else:
            if part.type == 'S_WALL':
                s = s.straight_wall(part.a0, part.length, wall_z, z, t)
            elif part.type == 'C_WALL':
                s = s.curved_wall(part.a0, part.da, part.radius, wall_z, z, t)

        self.segs.append(s)
        self.last_type = part.type

        return manip_index

    def close(self, closed):
        # Make last segment implicit closing one
        if closed:
            part = self.parts[-1]
            w = self.segs[-1]
            dp = self.segs[0].p0 - self.segs[-1].p0
            if "C_" in part.type:
                dw = (w.p1 - w.p0)
                w.r = part.radius / dw.length * dp.length
                # angle pt - p0        - angle p0 p1
                da = atan2(dp.y, dp.x) - atan2(dw.y, dw.x)
                a0 = w.a0 + da
                if a0 > pi:
                    a0 -= 2 * pi
                if a0 < -pi:
                    a0 += 2 * pi
                w.a0 = a0
            else:
                w.v = dp

    def locate_manipulators(self, side):

        for i, wall in enumerate(self.segs):

            manipulators = self.parts[i].manipulators

            p0 = wall.p0.to_3d()
            p1 = wall.p1.to_3d()

            # angle from last to current segment
            if i > 0:

                if i < len(self.segs) - 1:
                    manipulators[0].type_key = 'ANGLE'
                else:
                    manipulators[0].type_key = 'DUMB_ANGLE'

                v0 = self.segs[i - 1].straight(-side, 1).v.to_3d()
                v1 = wall.straight(side, 0).v.to_3d()
                manipulators[0].set_pts([p0, v0, v1])

            if type(wall).__name__ == "StraightWall":
                # segment length
                manipulators[1].type_key = 'SIZE'
                manipulators[1].prop1_name = "length"
                manipulators[1].set_pts([p0, p1, (side, 0, 0)])
            else:
                # segment radius + angle
                # scale to fix overlap with drag
                v0 = side * (wall.p0 - wall.c).to_3d()
                v1 = side * (wall.p1 - wall.c).to_3d()
                scale = 1.0 + (0.5 / v0.length)
                manipulators[1].type_key = 'ARC_ANGLE_RADIUS'
                manipulators[1].prop1_name = "da"
                manipulators[1].prop2_name = "radius"
                manipulators[1].set_pts([wall.c.to_3d(), scale * v0, scale * v1])

            # snap manipulator, dont change index !
            manipulators[2].set_pts([p0, p1, (1, 0, 0)])

            # dumb, segment index
            z = Vector((0, 0, 0.75 * wall.wall_z))
            manipulators[3].set_pts([p0 + z, p1 + z, (1, 0, 0)])

    def make_wall(self, step_angle, flip, closed, verts, faces):

        # swap manipulators so they always face outside
        side = 1
        if flip:
            side = -1

        # Make last segment implicit closing one

        nb_segs = len(self.segs) - 1
        if closed:
            nb_segs += 1

        for i, wall in enumerate(self.segs):

            wall.param_t(step_angle)
            if i < nb_segs:
                for j in range(wall.n_step + 1):
                    wall.make_wall(j, verts, faces)
            else:
                # last segment
                for j in range(wall.n_step):
                    continue
                    # print("%s" % (wall.n_step))
                    # wall.make_wall(j, verts, faces)

        self.locate_manipulators(side)

    def rotate(self, idx_from, a):
        """
            apply rotation to all following segs
        """
        self.segs[idx_from].rotate(a)
        ca = cos(a)
        sa = sin(a)
        rM = Matrix([
            [ca, -sa],
            [sa, ca]
            ])
        # rotation center
        p0 = self.segs[idx_from].p0
        for i in range(idx_from + 1, len(self.segs)):
            seg = self.segs[i]
            seg.rotate(a)
            dp = rM * (seg.p0 - p0)
            seg.translate(dp)

    def translate(self, idx_from, dp):
        """
            apply translation to all following segs
        """
        self.segs[idx_from].p1 += dp
        for i in range(idx_from + 1, len(self.segs)):
            self.segs[i].translate(dp)

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

    def draw(self, context):
        for seg in self.segs:
            seg.draw(context, render=False)

    def debug(self, verts):
        for wall in self.segs:
            for i in range(33):
                x, y = wall.lerp(i / 32)
                verts.append((x, y, 0))

    def make_surface(self, o, verts, height):
        bm = bmesh.new()
        for v in verts:
            bm.verts.new(v)
        bm.verts.ensure_lookup_table()
        for i in range(1, len(verts)):
            bm.edges.new((bm.verts[i - 1], bm.verts[i]))
        bm.edges.new((bm.verts[-1], bm.verts[0]))
        bm.edges.ensure_lookup_table()
        bmesh.ops.contextual_create(bm, geom=bm.edges)
        geom = bm.faces[:]
        bmesh.ops.solidify(bm, geom=geom, thickness=height)
        bm.to_mesh(o.data)
        bm.free()

    def make_hole(self, context, hole_obj, d):

        offset = -0.5 * (1 - d.x_offset) * d.width

        z0 = 0.1
        self.set_offset(offset)

        nb_segs = len(self.segs) - 1
        if d.closed:
            nb_segs += 1

        verts = []
        for i, wall in enumerate(self.segs):
            wall.param_t(d.step_angle)
            if i < nb_segs:
                for j in range(wall.n_step + 1):
                    wall.make_hole(j, verts, -z0)

        self.make_surface(hole_obj, verts, d.z + z0)


def update(self, context):
    self.update(context)


def update_childs(self, context):
    self.update(context, update_childs=True, manipulable_refresh=True)


def update_manipulators(self, context):
    self.update(context, manipulable_refresh=True)


def update_t_part(self, context):
    """
        Make this wall a T child of parent wall
        orient child so y points inside wall and x follow wall segment
        set child a0 according
    """
    o = self.find_in_selection(context)
    if o is not None:

        # w is parent wall
        w = context.scene.objects.get(self.t_part)
        wd = archipack_wall2.datablock(w)

        if wd is not None:
            og = self.get_generator()
            self.setup_childs(o, og)

            bpy.ops.object.select_all(action="DESELECT")

            # 5 cases here:
            # 1 No parents at all
            # 2 o has parent
            # 3 w has parent
            # 4 o and w share same parent allready
            # 5 o and w dosent share parent
            link_to_parent = False

            # when both walls do have a reference point, we may delete one of them
            to_delete = None

            # select childs and make parent reference point active
            if w.parent is None:
                # Either link to o.parent or create new parent
                link_to_parent = True
                if o.parent is None:
                    # create a reference point and make it active
                    x, y, z = w.bound_box[0]
                    context.scene.cursor_location = w.matrix_world * Vector((x, y, z))
                    # fix issue #9
                    context.scene.objects.active = o
                    bpy.ops.archipack.reference_point()
                    o.select = True
                else:
                    context.scene.objects.active = o.parent
                w.select = True
            else:
                # w has parent
                if o.parent is not w.parent:
                    link_to_parent = True
                    context.scene.objects.active = w.parent
                    o.select = True
                    if o.parent is not None:
                        # store o.parent to delete it
                        to_delete = o.parent
                        for c in o.parent.children:
                            if c is not o:
                                c.hide_select = False
                                c.select = True

            parent = context.active_object

            dmax = 2 * wd.width

            wg = wd.get_generator()

            otM = o.matrix_world
            orM = Matrix([
                otM[0].to_2d(),
                otM[1].to_2d()
                ])

            wtM = w.matrix_world
            wrM = Matrix([
                wtM[0].to_2d(),
                wtM[1].to_2d()
                ])

            # dir in absolute world coordsys
            dir = orM * og.segs[0].straight(1, 0).v

            # pt in w coordsys
            pos = otM.translation
            pt = (wtM.inverted() * pos).to_2d()

            for wall_idx, wall in enumerate(wg.segs):
                res, dist, t = wall.point_sur_segment(pt)
                # outside is on the right side of the wall
                #  p1
                #  |-- x
                #  p0

                # NOTE:
                # rotation here is wrong when w has not parent while o has parent

                if res and t > 0 and t < 1 and abs(dist) < dmax:
                    x = wrM * wall.straight(1, t).v
                    y = wrM * wall.normal(t).v.normalized()
                    self.parts[0].a0 = dir.angle_signed(x)
                    o.matrix_world = Matrix([
                        [x.x, -y.x, 0, pos.x],
                        [x.y, -y.y, 0, pos.y],
                        [0, 0, 1, pos.z],
                        [0, 0, 0, 1]
                    ])
                    break

            if link_to_parent and bpy.ops.archipack.parent_to_reference.poll():
                bpy.ops.archipack.parent_to_reference('INVOKE_DEFAULT')

            # update generator to take new rotation in account
            # use this to relocate windows on wall after reparenting
            g = self.get_generator()
            self.relocate_childs(context, o, g)

            # hide holes from select
            for c in parent.children:
                if "archipack_hybridhole" in c:
                    c.hide_select = True

            # delete unneeded reference point
            if to_delete is not None:
                bpy.ops.object.select_all(action="DESELECT")
                to_delete.select = True
                context.scene.objects.active = to_delete
                if bpy.ops.object.delete.poll():
                    bpy.ops.object.delete(use_global=False)

        elif self.t_part != "":
            self.t_part = ""

    self.restore_context(context)


def set_splits(self, value):
    if self.n_splits != value:
        self.auto_update = False
        self._set_t(value)
        self.auto_update = True
        self.n_splits = value
    return None


def get_splits(self):
    return self.n_splits


def update_type(self, context):

    d = self.find_datablock_in_selection(context)

    if d is not None and d.auto_update:

        d.auto_update = False
        idx = 0
        for i, part in enumerate(d.parts):
            if part == self:
                idx = i
                break
        a0 = 0
        if idx > 0:
            g = d.get_generator()
            w0 = g.segs[idx - 1]
            a0 = w0.straight(1).angle
            if "C_" in self.type:
                w = w0.straight_wall(self.a0, self.length, d.z, self.z, self.t)
            else:
                w = w0.curved_wall(self.a0, self.da, self.radius, d.z, self.z, self.t)
        else:
            if "C_" in self.type:
                p = Vector((0, 0))
                v = self.length * Vector((cos(self.a0), sin(self.a0)))
                w = StraightWall(p, v, d.z, self.z, self.t, d.flip)
                a0 = pi / 2
            else:
                c = -self.radius * Vector((cos(self.a0), sin(self.a0)))
                w = CurvedWall(c, self.radius, self.a0, pi, d.z, self.z, self.t, d.flip)

        # w0 - w - w1
        if d.closed and idx == d.n_parts:
            dp = - w.p0
        else:
            dp = w.p1 - w.p0

        if "C_" in self.type:
            self.radius = 0.5 * dp.length
            self.da = pi
            a0 = atan2(dp.y, dp.x) - pi / 2 - a0
        else:
            self.length = dp.length
            a0 = atan2(dp.y, dp.x) - a0

        if a0 > pi:
            a0 -= 2 * pi
        if a0 < -pi:
            a0 += 2 * pi
        self.a0 = a0

        if idx + 1 < d.n_parts:
            # adjust rotation of next part
            part1 = d.parts[idx + 1]
            if "C_" in self.type:
                a0 = part1.a0 - pi / 2
            else:
                a0 = part1.a0 + w.straight(1).angle - atan2(dp.y, dp.x)

            if a0 > pi:
                a0 -= 2 * pi
            if a0 < -pi:
                a0 += 2 * pi
            part1.a0 = a0

        d.auto_update = True


class archipack_wall2_part(PropertyGroup):
    type = EnumProperty(
            items=(
                ('S_WALL', 'Straight', '', 0),
                ('C_WALL', 'Curved', '', 1)
                ),
            default='S_WALL',
            update=update_type
            )
    length = FloatProperty(
            name="Length",
            min=0.01,
            default=2.0,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    radius = FloatProperty(
            name="Radius",
            min=0.5,
            default=0.7,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    a0 = FloatProperty(
            name="Start angle",
            min=-pi,
            max=pi,
            default=pi / 2,
            subtype='ANGLE', unit='ROTATION',
            update=update
            )
    da = FloatProperty(
            name="Angle",
            min=-pi,
            max=pi,
            default=pi / 2,
            subtype='ANGLE', unit='ROTATION',
            update=update
            )
    z = FloatVectorProperty(
            name="Height",
            default=[
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0
            ],
            size=31,
            update=update
            )
    t = FloatVectorProperty(
            name="Position",
            min=0,
            max=1,
            default=[
                1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1, 1
            ],
            size=31,
            update=update
            )
    splits = IntProperty(
            name="Splits",
            default=1,
            min=1,
            max=31,
            get=get_splits, set=set_splits
            )
    n_splits = IntProperty(
            name="Splits",
            default=1,
            min=1,
            max=31,
            update=update
            )
    auto_update = BoolProperty(default=True)
    manipulators = CollectionProperty(type=archipack_manipulator)
    # ui related
    expand = BoolProperty(default=False)

    def _set_t(self, splits):
        t = 1 / splits
        for i in range(splits):
            self.t[i] = t

    def find_datablock_in_selection(self, context):
        """
            find witch selected object this instance belongs to
            provide support for "copy to selected"
        """
        selected = [o for o in context.selected_objects]
        for o in selected:
            props = archipack_wall2.datablock(o)
            if props:
                for part in props.parts:
                    if part == self:
                        return props
        return None

    def update(self, context, manipulable_refresh=False):
        if not self.auto_update:
            return
        props = self.find_datablock_in_selection(context)
        if props is not None:
            props.update(context, manipulable_refresh)

    def draw(self, layout, context, index):

        row = layout.row(align=True)
        if self.expand:
            row.prop(self, 'expand', icon="TRIA_DOWN", icon_only=True, text="Part " + str(index + 1), emboss=False)
        else:
            row.prop(self, 'expand', icon="TRIA_RIGHT", icon_only=True, text="Part " + str(index + 1), emboss=False)

        row.prop(self, "type", text="")

        if self.expand:
            row = layout.row(align=True)
            row.operator("archipack.wall2_insert", text="Split").index = index
            row.operator("archipack.wall2_remove", text="Remove").index = index
            if self.type == 'C_WALL':
                row = layout.row()
                row.prop(self, "radius")
                row = layout.row()
                row.prop(self, "da")
            else:
                row = layout.row()
                row.prop(self, "length")
            row = layout.row()
            row.prop(self, "a0")
            row = layout.row()
            row.prop(self, "splits")
            for split in range(self.n_splits):
                row = layout.row()
                row.prop(self, "z", text="alt", index=split)
                row.prop(self, "t", text="pos", index=split)


class archipack_wall2_child(PropertyGroup):
    # Size  Loc
    # Delta Loc
    manipulators = CollectionProperty(type=archipack_manipulator)
    child_name = StringProperty()
    wall_idx = IntProperty()
    pos = FloatVectorProperty(subtype='XYZ')
    flip = BoolProperty(default=False)

    def get_child(self, context):
        d = None
        child = context.scene.objects.get(self.child_name)
        if child is not None and child.data is not None:
            cd = child.data
            if 'archipack_window' in cd:
                d = cd.archipack_window[0]
            elif 'archipack_door' in cd:
                d = cd.archipack_door[0]
        return child, d


class archipack_wall2(ArchipackObject, Manipulable, PropertyGroup):
    parts = CollectionProperty(type=archipack_wall2_part)
    n_parts = IntProperty(
            name="Parts",
            min=1,
            max=1024,
            default=1, update=update_manipulators
            )
    step_angle = FloatProperty(
            description="Curved parts segmentation",
            name="Step angle",
            min=1 / 180 * pi,
            max=pi,
            default=6 / 180 * pi,
            subtype='ANGLE', unit='ROTATION',
            update=update
            )
    width = FloatProperty(
            name="Width",
            min=0.01,
            default=0.2,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    z = FloatProperty(
            name='Height',
            min=0.1,
            default=2.7, precision=2,
            unit='LENGTH', subtype='DISTANCE',
            description='height', update=update,
            )
    x_offset = FloatProperty(
            name="Offset",
            min=-1, max=1,
            default=-1, precision=2, step=1,
            update=update
            )
    radius = FloatProperty(
            name="Radius",
            min=0.5,
            default=0.7,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    da = FloatProperty(
            name="Angle",
            min=-pi,
            max=pi,
            default=pi / 2,
            subtype='ANGLE', unit='ROTATION',
            update=update
            )
    flip = BoolProperty(
            name="Flip",
            default=False,
            update=update_childs
            )
    closed = BoolProperty(
            default=False,
            name="Close",
            update=update_manipulators
            )
    auto_update = BoolProperty(
            options={'SKIP_SAVE'},
            default=True,
            update=update_manipulators
            )
    realtime = BoolProperty(
            options={'SKIP_SAVE'},
            default=True,
            name="Real Time",
            description="Relocate childs in realtime"
            )
    # dumb manipulators to show sizes between childs
    childs_manipulators = CollectionProperty(type=archipack_manipulator)
    # store to manipulate windows and doors
    childs = CollectionProperty(type=archipack_wall2_child)
    t_part = StringProperty(
            name="Parent wall",
            description="This part will follow parent when set",
            default="",
            update=update_t_part
            )

    def insert_part(self, context, o, where):
        self.manipulable_disable(context)
        self.auto_update = False
        # the part we do split
        part_0 = self.parts[where]
        part_0.length /= 2
        part_0.da /= 2
        self.parts.add()
        part_1 = self.parts[len(self.parts) - 1]
        part_1.type = part_0.type
        part_1.length = part_0.length
        part_1.da = part_0.da
        part_1.a0 = 0
        # move after current one
        self.parts.move(len(self.parts) - 1, where + 1)
        self.n_parts += 1
        # re-eval childs location
        g = self.get_generator()
        self.setup_childs(o, g)

        self.setup_manipulators()
        self.auto_update = True

    def add_part(self, context, length):
        self.manipulable_disable(context)
        self.auto_update = False
        p = self.parts.add()
        p.length = length
        self.parts.move(len(self.parts) - 1, self.n_parts)
        self.n_parts += 1
        self.setup_manipulators()
        self.auto_update = True
        return self.parts[self.n_parts - 1]

    def remove_part(self, context, o, where):
        self.manipulable_disable(context)
        self.auto_update = False
        # preserve shape
        # using generator
        if where > 0:
            g = self.get_generator()
            w = g.segs[where - 1]
            w.p1 = g.segs[where].p1

            if where + 1 < self.n_parts:
                self.parts[where + 1].a0 = g.segs[where + 1].delta_angle(w)

            part = self.parts[where - 1]

            if "C_" in part.type:
                part.radius = w.r
            else:
                part.length = w.length

            if where > 1:
                part.a0 = w.delta_angle(g.segs[where - 2])
            else:
                part.a0 = w.straight(1, 0).angle

        self.parts.remove(where)
        self.n_parts -= 1

        # re-eval child location
        g = self.get_generator()
        self.setup_childs(o, g)

        # fix snap manipulators index
        self.setup_manipulators()
        self.auto_update = True

    def get_generator(self):
        # print("get_generator")
        g = WallGenerator(self.parts)
        for part in self.parts:
            g.add_part(part, self.z, self.flip)
        g.close(self.closed)
        return g

    def update_parts(self, o, update_childs=False):
        # print("update_parts")
        # remove rows
        # NOTE:
        # n_parts+1
        # as last one is end point of last segment or closing one
        row_change = False
        for i in range(len(self.parts), self.n_parts + 1, -1):
            row_change = True
            self.parts.remove(i - 1)

        # add rows
        for i in range(len(self.parts), self.n_parts + 1):
            row_change = True
            self.parts.add()

        self.setup_manipulators()

        g = self.get_generator()

        if o is not None and (row_change or update_childs):
            self.setup_childs(o, g)

        return g

    def setup_manipulators(self):

        if len(self.manipulators) == 0:
            # make manipulators selectable
            s = self.manipulators.add()
            s.prop1_name = "width"
            s = self.manipulators.add()
            s.prop1_name = "n_parts"
            s.type_key = 'COUNTER'
            s = self.manipulators.add()
            s.prop1_name = "z"
            s.normal = (0, 1, 0)

        if self.t_part != "" and len(self.manipulators) < 4:
            s = self.manipulators.add()
            s.prop1_name = "x"
            s.type_key = 'DELTA_LOC'

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
            p.manipulators[2].prop1_name = str(i)
            p.manipulators[3].prop1_name = str(i + 1)

    def interpolate_bezier(self, pts, wM, p0, p1, resolution):
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

    def from_spline(self, wM, resolution, spline):
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

        if self.is_cw(pts):
            pts = list(reversed(pts))

        self.auto_update = False
        self.from_points(pts, spline.use_cyclic_u)
        self.auto_update = True

    def from_points(self, pts, closed):

        self.n_parts = len(pts) - 1

        if closed:
            self.n_parts -= 1

        self.update_parts(None)

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
                print("Too many pts for parts")
                break
            p = self.parts[i]
            p.length = dp.to_2d().length
            p.dz = dp.z
            p.a0 = da
            a0 += da
            p0 = p1

        self.closed = closed

    def reverse(self, context, o):

        g = self.get_generator()

        self.auto_update = False

        pts = [seg.p0.to_3d() for seg in g.segs]

        if not self.closed:
            g.segs.pop()

        g_segs = list(reversed(g.segs))

        last_seg = None

        for i, seg in enumerate(g_segs):

            s = seg.oposite
            if "Curved" in type(seg).__name__:
                self.parts[i].type = "C_WALL"
                self.parts[i].radius = s.r
                self.parts[i].da = s.da
            else:
                self.parts[i].type = "S_WALL"
                self.parts[i].length = s.length

            self.parts[i].a0 = s.delta_angle(last_seg)

            last_seg = s

        if self.closed:
            pts.append(pts[0])

        pts = list(reversed(pts))

        # location wont change for closed walls
        if not self.closed:
            dp = pts[0] - pts[-1]
            # pre-translate as dp is in local coordsys
            o.matrix_world = o.matrix_world * Matrix([
                [1, 0, 0, dp.x],
                [0, 1, 0, dp.y],
                [0, 0, 1, 0],
                [0, 0, 0, 1],
                ])

        # self.from_points(pts, self.closed)

        g = self.get_generator()

        self.setup_childs(o, g)
        self.auto_update = True

        # flip does trigger relocate and keep childs orientation
        self.flip = not self.flip

    def update(self, context, manipulable_refresh=False, update_childs=False):

        o = self.find_in_selection(context, self.auto_update)

        if o is None:
            return

        if manipulable_refresh:
            # prevent crash by removing all manipulators refs to datablock before changes
            self.manipulable_disable(context)

        verts = []
        faces = []

        g = self.update_parts(o, update_childs)
        # print("make_wall")
        g.make_wall(self.step_angle, self.flip, self.closed, verts, faces)

        if self.closed:
            f = len(verts)
            if self.flip:
                faces.append((0, f - 2, f - 1, 1))
            else:
                faces.append((f - 2, 0, 1, f - 1))

        # print("buildmesh")
        bmed.buildmesh(context, o, verts, faces, matids=None, uvs=None, weld=True)

        side = 1
        if self.flip:
            side = -1
        # Width
        offset = side * (0.5 * self.x_offset) * self.width
        self.manipulators[0].set_pts([
            g.segs[0].sized_normal(0, offset + 0.5 * side * self.width).v.to_3d(),
            g.segs[0].sized_normal(0, offset - 0.5 * side * self.width).v.to_3d(),
            (-side, 0, 0)
            ])

        # Parts COUNTER
        self.manipulators[1].set_pts([g.segs[-2].lerp(1.1).to_3d(),
            g.segs[-2].lerp(1.1 + 0.5 / g.segs[-2].length).to_3d(),
            (-side, 0, 0)
            ])

        # Height
        self.manipulators[2].set_pts([
            (0, 0, 0),
            (0, 0, self.z),
            (-1, 0, 0)
            ], normal=g.segs[0].straight(side, 0).v.to_3d())

        if self.t_part != "":
            t = 0.3 / g.segs[0].length
            self.manipulators[3].set_pts([
                g.segs[0].sized_normal(t, 0.1).p1.to_3d(),
                g.segs[0].sized_normal(t, -0.1).p1.to_3d(),
                (1, 0, 0)
                ])

        if self.realtime:
            # update child location and size
            self.relocate_childs(context, o, g)
            # store gl points
            self.update_childs(context, o, g)
        else:
            bpy.ops.archipack.wall2_throttle_update(name=o.name)

        modif = o.modifiers.get('Wall')
        if modif is None:
            modif = o.modifiers.new('Wall', 'SOLIDIFY')
            modif.use_quality_normals = True
            modif.use_even_offset = True
            modif.material_offset_rim = 2
            modif.material_offset = 1

        modif.thickness = self.width
        modif.offset = self.x_offset

        if manipulable_refresh:
            # print("manipulable_refresh=True")
            self.manipulable_refresh = True

        self.restore_context(context)

    # manipulable children objects like windows and doors
    def child_partition(self, array, begin, end):
        pivot = begin
        for i in range(begin + 1, end + 1):
            # wall idx
            if array[i][1] < array[begin][1]:
                pivot += 1
                array[i], array[pivot] = array[pivot], array[i]
            # param t on the wall
            elif array[i][1] == array[begin][1] and array[i][4] <= array[begin][4]:
                pivot += 1
                array[i], array[pivot] = array[pivot], array[i]
        array[pivot], array[begin] = array[begin], array[pivot]
        return pivot

    def sort_child(self, array, begin=0, end=None):
        # print("sort_child")
        if end is None:
            end = len(array) - 1

        def _quicksort(array, begin, end):
            if begin >= end:
                return
            pivot = self.child_partition(array, begin, end)
            _quicksort(array, begin, pivot - 1)
            _quicksort(array, pivot + 1, end)
        return _quicksort(array, begin, end)

    def add_child(self, name, wall_idx, pos, flip):
        # print("add_child %s %s" % (name, wall_idx))
        c = self.childs.add()
        c.child_name = name
        c.wall_idx = wall_idx
        c.pos = pos
        c.flip = flip
        m = c.manipulators.add()
        m.type_key = 'DELTA_LOC'
        m.prop1_name = "x"
        m = c.manipulators.add()
        m.type_key = 'SNAP_SIZE_LOC'
        m.prop1_name = "x"
        m.prop2_name = "x"

    def setup_childs(self, o, g):
        """
            Store childs
            create manipulators
            call after a boolean oop
        """
        # tim = time.time()
        self.childs.clear()
        self.childs_manipulators.clear()
        if o.parent is None:
            return
        wall_with_childs = [0 for i in range(self.n_parts + 1)]
        relocate = []
        dmax = 2 * self.width

        wtM = o.matrix_world
        wrM = Matrix([
            wtM[0].to_2d(),
            wtM[1].to_2d()
            ])
        witM = wtM.inverted()

        for child in o.parent.children:
            # filter allowed childs
            cd = child.data
            wd = archipack_wall2.datablock(child)
            if (child != o and cd is not None and (
                    'archipack_window' in cd or
                    'archipack_door' in cd or (
                        wd is not None and
                        o.name in wd.t_part
                        )
                    )):

                # setup on T linked walls
                if wd is not None:
                    wg = wd.get_generator()
                    wd.setup_childs(child, wg)

                ctM = child.matrix_world
                crM = Matrix([
                    ctM[0].to_2d(),
                    ctM[1].to_2d()
                ])

                # pt in w coordsys
                pos = ctM.translation
                pt = (witM * pos).to_2d()

                for wall_idx, wall in enumerate(g.segs):
                    # may be optimized with a bound check
                    res, dist, t = wall.point_sur_segment(pt)
                    # outside is on the right side of the wall
                    #  p1
                    #  |-- x
                    #  p0
                    if res and t > 0 and t < 1 and abs(dist) < dmax:
                        # dir in world coordsys
                        dir = wrM * wall.sized_normal(t, 1).v
                        wall_with_childs[wall_idx] = 1
                        m = self.childs_manipulators.add()
                        m.type_key = 'DUMB_SIZE'
                        # always make window points outside
                        if "archipack_window" in cd:
                            flip = self.flip
                        else:
                            dir_y = crM * Vector((0, -1))
                            # let door orient where user want
                            flip = (dir_y - dir).length > 0.5
                        # store z in wall space
                        relocate.append((
                            child.name,
                            wall_idx,
                            (t * wall.length, dist, (witM * pos).z),
                            flip,
                            t))
                        break

        self.sort_child(relocate)
        for child in relocate:
            name, wall_idx, pos, flip, t = child
            self.add_child(name, wall_idx, pos, flip)

        # add a dumb size from last child to end of wall segment
        for i in range(sum(wall_with_childs)):
            m = self.childs_manipulators.add()
            m.type_key = 'DUMB_SIZE'
        # print("setup_childs:%1.4f" % (time.time()-tim))

    def relocate_childs(self, context, o, g):
        """
            Move and resize childs after wall edition
        """
        # print("relocate_childs")
        # tim = time.time()
        w = -self.x_offset * self.width
        if self.flip:
            w = -w
        tM = o.matrix_world
        for child in self.childs:
            c, d = child.get_child(context)
            if c is None:
                continue
            t = child.pos.x / g.segs[child.wall_idx].length
            n = g.segs[child.wall_idx].sized_normal(t, 1)
            rx, ry = -n.v
            rx, ry = ry, -rx
            if child.flip:
                rx, ry = -rx, -ry

            if d is not None:
                # print("change flip:%s width:%s" % (d.flip != child.flip, d.y != self.width))
                if d.y != self.width or d.flip != child.flip:
                    c.select = True
                    d.auto_update = False
                    d.flip = child.flip
                    d.y = self.width
                    d.auto_update = True
                    c.select = False
                x, y = n.p - (0.5 * w * n.v)
            else:
                x, y = n.p - (child.pos.y * n.v)

            context.scene.objects.active = o
            # preTranslate
            c.matrix_world = tM * Matrix([
                [rx, -ry, 0, x],
                [ry, rx, 0, y],
                [0, 0, 1, child.pos.z],
                [0, 0, 0, 1]
            ])

            # Update T linked wall's childs
            if archipack_wall2.filter(c):
                d = archipack_wall2.datablock(c)
                cg = d.get_generator()
                d.relocate_childs(context, c, cg)

        # print("relocate_childs:%1.4f" % (time.time()-tim))

    def update_childs(self, context, o, g):
        """
            setup gl points for childs
        """
        # print("update_childs")

        if o.parent is None:
            return

        # swap manipulators so they always face outside
        manip_side = 1
        if self.flip:
            manip_side = -1

        itM = o.matrix_world.inverted()
        m_idx = 0
        for wall_idx, wall in enumerate(g.segs):
            p0 = wall.lerp(0)
            wall_has_childs = False
            for child in self.childs:
                if child.wall_idx == wall_idx:
                    c, d = child.get_child(context)
                    if d is not None:
                        # child is either a window or a door
                        wall_has_childs = True
                        dt = 0.5 * d.x / wall.length
                        pt = (itM * c.matrix_world.translation).to_2d()
                        res, y, t = wall.point_sur_segment(pt)
                        child.pos = (wall.length * t, y, child.pos.z)
                        p1 = wall.lerp(t - dt)
                        # dumb size between childs
                        self.childs_manipulators[m_idx].set_pts([
                            (p0.x, p0.y, 0),
                            (p1.x, p1.y, 0),
                            (manip_side * 0.5, 0, 0)])
                        m_idx += 1
                        x, y = 0.5 * d.x, -self.x_offset * 0.5 * d.y

                        if child.flip:
                            side = -manip_side
                        else:
                            side = manip_side

                        # delta loc
                        child.manipulators[0].set_pts([(-x, side * -y, 0), (x, side * -y, 0), (side, 0, 0)])
                        # loc size
                        child.manipulators[1].set_pts([
                            (-x, side * -y, 0),
                            (x, side * -y, 0),
                            (0.5 * side, 0, 0)])
                        p0 = wall.lerp(t + dt)
            p1 = wall.lerp(1)
            if wall_has_childs:
                # dub size after all childs
                self.childs_manipulators[m_idx].set_pts([
                    (p0.x, p0.y, 0),
                    (p1.x, p1.y, 0),
                    (manip_side * 0.5, 0, 0)])
                m_idx += 1

    def manipulate_childs(self, context):
        """
            setup child manipulators
        """
        # print("manipulate_childs")
        n_parts = self.n_parts
        if self.closed:
            n_parts += 1

        for wall_idx in range(n_parts):
            for child in self.childs:
                if child.wall_idx == wall_idx:
                    c, d = child.get_child(context)
                    if d is not None:
                        # delta loc
                        self.manip_stack.append(child.manipulators[0].setup(context, c, d, self.manipulate_callback))
                        # loc size
                        self.manip_stack.append(child.manipulators[1].setup(context, c, d, self.manipulate_callback))

    def manipulate_callback(self, context, o=None, manipulator=None):
        found = False
        if o.parent is not None:
            for c in o.parent.children:
                if (archipack_wall2.datablock(c) == self):
                    context.scene.objects.active = c
                    found = True
                    break
        if found:
            self.manipulable_manipulate(context, manipulator=manipulator)

    def manipulable_manipulate(self, context, event=None, manipulator=None):
        type_name = type(manipulator).__name__
        # print("manipulable_manipulate %s" % (type_name))
        if type_name in [
                'DeltaLocationManipulator',
                'SizeLocationManipulator',
                'SnapSizeLocationManipulator'
                ]:
            # update manipulators pos of childs
            o = context.active_object
            if o.parent is None:
                return
            g = self.get_generator()
            itM = o.matrix_world.inverted() * o.parent.matrix_world
            for child in self.childs:
                c, d = child.get_child(context)
                if d is not None:
                    wall = g.segs[child.wall_idx]
                    pt = (itM * c.location).to_2d()
                    res, d, t = wall.point_sur_segment(pt)
                    child.pos = (t * wall.length, d, child.pos.z)
            # update childs manipulators
            self.update_childs(context, o, g)

    def manipulable_move_t_part(self, context, o=None, manipulator=None):
        """
            Callback for t_parts childs
        """
        type_name = type(manipulator).__name__
        # print("manipulable_manipulate %s" % (type_name))
        if type_name in [
                'DeltaLocationManipulator'
                ]:
            # update manipulators pos of childs
            if archipack_wall2.datablock(o) != self:
                return
            g = self.get_generator()
            # update childs
            self.relocate_childs(context, o, g)

    def manipulable_release(self, context):
        """
            Override with action to do on mouse release
            eg: big update
        """
        return

    def manipulable_setup(self, context):
        # print("manipulable_setup")
        self.manipulable_disable(context)
        o = context.active_object

        # setup childs manipulators
        self.manipulate_childs(context)
        n_parts = self.n_parts
        if self.closed:
            n_parts += 1

        # update manipulators on version change
        self.setup_manipulators()

        for i, part in enumerate(self.parts):

            if i < n_parts:
                if i > 0:
                    # start angle
                    self.manip_stack.append(part.manipulators[0].setup(context, o, part))

                # length / radius + angle
                self.manip_stack.append(part.manipulators[1].setup(context, o, part))
                # segment index
                self.manip_stack.append(part.manipulators[3].setup(context, o, self))

            # snap point
            self.manip_stack.append(part.manipulators[2].setup(context, o, self))

        # height as per segment will be here when done

        # width + counter
        for m in self.manipulators:
            self.manip_stack.append(m.setup(context, o, self, self.manipulable_move_t_part))

        # dumb between childs
        for m in self.childs_manipulators:
            self.manip_stack.append(m.setup(context, o, self))

    def manipulable_exit(self, context):
        """
            Override with action to do when modal exit
        """
        return

    def manipulable_invoke(self, context):
        """
            call this in operator invoke()
        """
        # print("manipulable_invoke")
        if self.manipulate_mode:
            self.manipulable_disable(context)
            return False

        # self.manip_stack = []
        o = context.active_object
        g = self.get_generator()
        # setup childs manipulators
        self.setup_childs(o, g)
        # store gl points
        self.update_childs(context, o, g)
        # dont do anything ..
        # self.manipulable_release(context)
        # self.manipulate_mode = True
        self.manipulable_setup(context)
        self.manipulate_mode = True

        self._manipulable_invoke(context)

        return True

    def find_roof(self, context, o, g):
        tM = o.matrix_world
        up = Vector((0, 0, 1))
        for seg in g.segs:
            p = tM * seg.p0.to_3d()
            p.z = 0.01
            # prevent self intersect
            o.hide = True
            res, pos, normal, face_index, r, matrix_world = context.scene.ray_cast(
                p,
                up)
            o.hide = False
            # print("res:%s" % res)
            if res and r.data is not None and "archipack_roof" in r.data:
                return r, r.data.archipack_roof[0]

        return None, None


# Update throttle (hack)
# use 2 globals to store a timer and state of update_action
# Use to update floor boolean on edit
update_timer = None
update_timer_updating = False
throttle_delay = 0.5
throttle_start = 0


class ARCHIPACK_OT_wall2_throttle_update(Operator):
    bl_idname = "archipack.wall2_throttle_update"
    bl_label = "Update childs with a delay"

    name = StringProperty()

    def modal(self, context, event):
        global update_timer_updating
        if event.type == 'TIMER' and not update_timer_updating:
            # cant rely on TIMER event as another timer may run
            if time.time() - throttle_start > throttle_delay:
                update_timer_updating = True
                o = context.scene.objects.get(self.name)
                if o is not None:
                    m = o.modifiers.get("AutoBoolean")
                    if m is not None:
                        o.hide = False
                        # o.draw_type = 'TEXTURED'
                        # m.show_viewport = True

                    return self.cancel(context)
        return {'PASS_THROUGH'}

    def execute(self, context):
        global update_timer
        global update_timer_updating
        global throttle_delay
        global throttle_start
        if update_timer is not None:
            context.window_manager.event_timer_remove(update_timer)
            if update_timer_updating:
                return {'CANCELLED'}
            # reset update_timer so it only occurs once 0.1s after last action
            throttle_start = time.time()
            update_timer = context.window_manager.event_timer_add(throttle_delay, context.window)
            return {'CANCELLED'}
        throttle_start = time.time()
        update_timer_updating = False
        context.window_manager.modal_handler_add(self)
        update_timer = context.window_manager.event_timer_add(throttle_delay, context.window)
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        global update_timer
        context.window_manager.event_timer_remove(update_timer)
        update_timer = None
        return {'CANCELLED'}


class ARCHIPACK_PT_wall2(Panel):
    bl_idname = "ARCHIPACK_PT_wall2"
    bl_label = "Wall"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'ArchiPack'

    def draw(self, context):
        prop = archipack_wall2.datablock(context.object)
        if prop is None:
            return
        layout = self.layout
        row = layout.row(align=True)
        row.operator("archipack.wall2_manipulate", icon='HAND')
        # row = layout.row(align=True)
        # row.prop(prop, 'realtime')
        box = layout.box()
        box.prop(prop, 'n_parts')
        box.prop(prop, 'step_angle')
        box.prop(prop, 'width')
        box.prop(prop, 'z')
        box.prop(prop, 'flip')
        box.prop(prop, 'x_offset')
        row = layout.row()
        row.prop(prop, "closed")
        row = layout.row()
        row.prop_search(prop, "t_part", context.scene, "objects", text="T link", icon='OBJECT_DATAMODE')
        layout.operator("archipack.wall2_reverse", icon='FILE_REFRESH')
        row = layout.row(align=True)
        row.operator("archipack.wall2_fit_roof")
        # row.operator("archipack.wall2_fit_roof", text="Inside").inside = True
        n_parts = prop.n_parts
        if prop.closed:
            n_parts += 1
        for i, part in enumerate(prop.parts):
            if i < n_parts:
                box = layout.box()
                part.draw(box, context, i)

    @classmethod
    def poll(cls, context):
        return archipack_wall2.filter(context.active_object)


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------


class ARCHIPACK_OT_wall2(ArchipackCreateTool, Operator):
    bl_idname = "archipack.wall2"
    bl_label = "Wall"
    bl_description = "Create a Wall"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    def create(self, context):
        m = bpy.data.meshes.new("Wall")
        o = bpy.data.objects.new("Wall", m)
        d = m.archipack_wall2.add()
        d.manipulable_selectable = True
        context.scene.objects.link(o)
        o.select = True
        # around 12 degree
        m.auto_smooth_angle = 0.20944
        context.scene.objects.active = o
        self.load_preset(d)
        self.add_material(o)
        return o

    def execute(self, context):
        if context.mode == "OBJECT":
            bpy.ops.object.select_all(action="DESELECT")
            o = self.create(context)
            o.location = bpy.context.scene.cursor_location
            o.select = True
            context.scene.objects.active = o
            self.manipulate()
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_wall2_from_curve(Operator):
    bl_idname = "archipack.wall2_from_curve"
    bl_label = "Wall curve"
    bl_description = "Create a wall from a curve"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    auto_manipulate = BoolProperty(default=True)

    @classmethod
    def poll(self, context):
        return context.active_object is not None and context.active_object.type == 'CURVE'

    def create(self, context):
        curve = context.active_object
        for spline in curve.data.splines:
            bpy.ops.archipack.wall2(auto_manipulate=self.auto_manipulate)
            o = context.scene.objects.active
            d = archipack_wall2.datablock(o)
            d.from_spline(curve.matrix_world, 12, spline)
            if spline.type == 'POLY':
                pt = spline.points[0].co
            elif spline.type == 'BEZIER':
                pt = spline.bezier_points[0].co
            else:
                pt = Vector((0, 0, 0))
            # pretranslate
            o.matrix_world = curve.matrix_world * Matrix([
                [1, 0, 0, pt.x],
                [0, 1, 0, pt.y],
                [0, 0, 1, pt.z],
                [0, 0, 0, 1]
                ])
        return o

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    def execute(self, context):
        if context.mode == "OBJECT":
            bpy.ops.object.select_all(action="DESELECT")
            o = self.create(context)
            if o is not None:
                o.select = True
                context.scene.objects.active = o
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_wall2_from_slab(Operator):
    bl_idname = "archipack.wall2_from_slab"
    bl_label = "->Wall"
    bl_description = "Create a wall from a slab"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    auto_manipulate = BoolProperty(default=True)

    @classmethod
    def poll(self, context):
        o = context.active_object
        return o is not None and o.data is not None and 'archipack_slab' in o.data

    def create(self, context):
        slab = context.active_object
        wd = slab.data.archipack_slab[0]
        bpy.ops.archipack.wall2(auto_manipulate=self.auto_manipulate)
        o = context.scene.objects.active
        d = archipack_wall2.datablock(o)
        d.auto_update = False
        d.parts.clear()
        d.n_parts = wd.n_parts - 1
        d.closed = True
        for part in wd.parts:
            p = d.parts.add()
            if "S_" in part.type:
                p.type = "S_WALL"
            else:
                p.type = "C_WALL"
            p.length = part.length
            p.radius = part.radius
            p.da = part.da
            p.a0 = part.a0
        o.select = True
        context.scene.objects.active = o
        d.auto_update = True
        # pretranslate
        o.matrix_world = slab.matrix_world.copy()

        bpy.ops.object.select_all(action='DESELECT')
        # parenting childs to wall reference point
        if o.parent is None:
            x, y, z = o.bound_box[0]
            context.scene.cursor_location = o.matrix_world * Vector((x, y, z))
            # fix issue #9
            context.scene.objects.active = o
            bpy.ops.archipack.reference_point()
        else:
            o.parent.select = True
            context.scene.objects.active = o.parent
        o.select = True
        slab.select = True
        bpy.ops.archipack.parent_to_reference()
        o.parent.select = False
        return o

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    def execute(self, context):
        if context.mode == "OBJECT":
            bpy.ops.object.select_all(action="DESELECT")
            o = self.create(context)
            o.select = True
            context.scene.objects.active = o
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_wall2_fit_roof(Operator):
    bl_idname = "archipack.wall2_fit_roof"
    bl_label = "Fit roof"
    bl_description = "Fit roof"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    inside = BoolProperty(default=False)

    @classmethod
    def poll(self, context):
        return archipack_wall2.filter(context.active_object)

    def execute(self, context):
        o = context.active_object
        d = archipack_wall2.datablock(o)
        g = d.get_generator()
        r, rd = d.find_roof(context, o, g)
        if rd is not None:
            d.setup_childs(o, g)
            rd.make_wall_fit(context, r, o, self.inside)
        return {'FINISHED'}

# ------------------------------------------------------------------
# Define operator class to draw a wall
# ------------------------------------------------------------------


class ARCHIPACK_OT_wall2_draw(ArchpackDrawTool, Operator):
    bl_idname = "archipack.wall2_draw"
    bl_label = "Draw a Wall"
    bl_description = "Draw a Wall"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    o = None
    state = 'RUNNING'
    flag_create = False
    flag_next = False
    wall_part1 = None
    wall_line1 = None
    line = None
    label = None
    feedback = None
    takeloc = Vector((0, 0, 0))
    sel = []
    act = None

    # constraint to other wall and make a T child
    parent = None
    takemat = None

    @classmethod
    def poll(cls, context):
        return True

    def draw_callback(self, _self, context):
        self.feedback.draw(context)

    def sp_draw(self, sp, context):
        z = 2.7
        if self.state == 'CREATE':
            p0 = self.takeloc
        else:
            p0 = sp.takeloc

        p1 = sp.placeloc
        delta = p1 - p0
        # print("sp_draw state:%s delta:%s p0:%s p1:%s" % (self.state, delta.length, p0, p1))
        if delta.length == 0:
            return
        self.wall_part1.set_pos([p0, p1, Vector((p1.x, p1.y, p1.z + z)), Vector((p0.x, p0.y, p0.z + z))])
        self.wall_line1.set_pos([p0, p1, Vector((p1.x, p1.y, p1.z + z)), Vector((p0.x, p0.y, p0.z + z))])
        self.wall_part1.draw(context)
        self.wall_line1.draw(context)
        self.line.p = p0
        self.line.v = delta
        self.label.set_pos(context, self.line.length, self.line.lerp(0.5), self.line.v, normal=Vector((0, 0, 1)))
        self.label.draw(context)
        self.line.draw(context)

    def sp_callback(self, context, event, state, sp):
        # print("sp_callback event %s %s state:%s" % (event.type, event.value, state))

        if state == 'SUCCESS':

            if self.state == 'CREATE':
                takeloc = self.takeloc
                delta = sp.placeloc - self.takeloc
            else:
                takeloc = sp.takeloc
                delta = sp.delta

            old = context.active_object
            if self.o is None:
                bpy.ops.archipack.wall2(auto_manipulate=False)
                o = context.active_object
                o.location = takeloc
                self.o = o
                d = archipack_wall2.datablock(o)
                part = d.parts[0]
                part.length = delta.length
            else:
                o = self.o
                o.select = True
                context.scene.objects.active = o
                d = archipack_wall2.datablock(o)
                # Check for end close to start and close when applicable
                dp = sp.placeloc - o.location
                if dp.length < 0.01:
                    d.closed = True
                    self.state = 'CANCEL'
                    return

                part = d.add_part(context, delta.length)

            # print("self.o :%s" % o.name)
            rM = o.matrix_world.inverted().to_3x3()
            g = d.get_generator()
            w = g.segs[-2]
            dp = rM * delta
            da = atan2(dp.y, dp.x) - w.straight(1).angle
            a0 = part.a0 + da
            if a0 > pi:
                a0 -= 2 * pi
            if a0 < -pi:
                a0 += 2 * pi
            part.a0 = a0

            context.scene.objects.active = old
            self.flag_next = True
            context.area.tag_redraw()
            # print("feedback.on:%s" % self.feedback.on)

        self.state = state

    def sp_init(self, context, event, state, sp):
        # print("sp_init event %s %s %s" % (event.type, event.value, state))
        if state == 'SUCCESS':
            # point placed, check if a wall was under mouse
            res, tM, wall, y = self.mouse_hover_wall(context, event)
            if res:
                d = archipack_wall2.datablock(wall)
                if event.ctrl:
                    # user snap, use direction as constraint
                    tM.translation = sp.placeloc.copy()
                else:
                    # without snap, use wall's bottom
                    tM.translation -= y.normalized() * (0.5 * d.width)
                self.takeloc = tM.translation
                self.parent = wall.name
                self.takemat = tM
            else:
                self.takeloc = sp.placeloc.copy()

            self.state = 'RUNNING'
            # print("feedback.on:%s" % self.feedback.on)
        elif state == 'CANCEL':
            self.state = state
            return

    def ensure_ccw(self):
        """
            Wall to slab expect wall vertex order to be ccw
            so reverse order here when needed
        """
        d = archipack_wall2.datablock(self.o)
        g = d.get_generator()
        pts = [seg.p0.to_3d() for seg in g.segs]

        if d.closed:
            pts.append(pts[0])

        if d.is_cw(pts):
            d.x_offset = 1
            pts = list(reversed(pts))
            self.o.location += pts[0] - pts[-1]

        d.from_points(pts, d.closed)

    def modal(self, context, event):

        context.area.tag_redraw()
        # print("modal event %s %s" % (event.type, event.value))
        if event.type == 'NONE':
            return {'PASS_THROUGH'}

        if self.state == 'STARTING':
            takeloc = self.mouse_to_plane(context, event)
            # wait for takeloc being visible when button is over horizon
            rv3d = context.region_data
            viewinv = rv3d.view_matrix.inverted()

            if (takeloc * viewinv).z < 0 or not rv3d.is_perspective:
                # print("STARTING")
                snap_point(takeloc=takeloc,
                    callback=self.sp_init,
                    constraint_axis=(True, True, False),
                    release_confirm=True)
            return {'RUNNING_MODAL'}

        elif self.state == 'RUNNING':
            # print("RUNNING")
            self.state = 'CREATE'
            snap_point(takeloc=self.takeloc,
                draw=self.sp_draw,
                takemat=self.takemat,
                transform_orientation=context.space_data.transform_orientation,
                callback=self.sp_callback,
                constraint_axis=(True, True, False),
                release_confirm=self.max_style_draw_tool)
            return {'RUNNING_MODAL'}

        elif self.state != 'CANCEL' and event.type in {'LEFTMOUSE', 'RET', 'NUMPAD_ENTER', 'SPACE'}:

            # print('LEFTMOUSE %s' % (event.value))
            self.feedback.instructions(context, "Draw a wall", "Click & Drag to add a segment", [
                ('CTRL', 'Snap'),
                ('MMBTN', 'Constraint to axis'),
                ('X Y', 'Constraint to axis'),
                ('BACK_SPACE', 'Remove part'),
                ('RIGHTCLICK or ESC', 'exit')
                ])

            # press with max mode release with blender mode
            if self.max_style_draw_tool:
                evt_value = 'PRESS'
            else:
                evt_value = 'RELEASE'

            if event.value == evt_value:

                if self.flag_next:
                    self.flag_next = False
                    o = self.o
                    o.select = True
                    context.scene.objects.active = o
                    d = archipack_wall2.datablock(o)
                    g = d.get_generator()
                    p0 = g.segs[-2].p0
                    p1 = g.segs[-2].p1
                    dp = p1 - p0
                    takemat = o.matrix_world * Matrix([
                        [dp.x, dp.y, 0, p1.x],
                        [dp.y, -dp.x, 0, p1.y],
                        [0, 0, 1, 0],
                        [0, 0, 0, 1]
                    ])
                    takeloc = o.matrix_world * p1.to_3d()
                    o.select = False
                else:
                    takeloc = self.mouse_to_plane(context, event)
                    takemat = None

                snap_point(takeloc=takeloc,
                    takemat=takemat,
                    draw=self.sp_draw,
                    callback=self.sp_callback,
                    constraint_axis=(True, True, False),
                    release_confirm=self.max_style_draw_tool)

            return {'RUNNING_MODAL'}

        if self.keymap.check(event, self.keymap.undo) or (
                event.type in {'BACK_SPACE'} and event.value == 'RELEASE'
                ):
            if self.o is not None:
                o = self.o
                o.select = True
                context.scene.objects.active = o
                d = archipack_wall2.datablock(o)
                if d.n_parts > 1:
                    d.n_parts -= 1
            return {'RUNNING_MODAL'}

        if self.state == 'CANCEL' or (event.type in {'ESC', 'RIGHTMOUSE'} and
                event.value == 'RELEASE'):

            self.feedback.disable()
            bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')

            if self.o is None:
                context.scene.objects.active = self.act
                for o in self.sel:
                    o.select = True
            else:
                self.o.select = True
                context.scene.objects.active = self.o

                # remove last segment with blender mode
                d = archipack_wall2.datablock(self.o)
                if not self.max_style_draw_tool:
                    if not d.closed and d.n_parts > 1:
                        d.n_parts -= 1

                self.o.select = True
                context.scene.objects.active = self.o
                # make T child
                if self.parent is not None:
                    d.t_part = self.parent

                if bpy.ops.archipack.wall2_manipulate.poll():
                    bpy.ops.archipack.wall2_manipulate('INVOKE_DEFAULT')

            return {'FINISHED'}

        return {'PASS_THROUGH'}

    def invoke(self, context, event):

        if context.mode == "OBJECT":
            prefs = context.user_preferences.addons[__name__.split('.')[0]].preferences
            self.max_style_draw_tool = prefs.max_style_draw_tool
            self.keymap = Keymaps(context)
            self.wall_part1 = GlPolygon((0.5, 0, 0, 0.2))
            self.wall_line1 = GlPolyline((0.5, 0, 0, 0.8))
            self.line = GlLine()
            self.label = GlText()
            self.feedback = FeedbackPanel()
            self.feedback.instructions(context, "Draw a wall", "Click & Drag to start", [
                ('CTRL', 'Snap'),
                ('MMBTN', 'Constraint to axis'),
                ('X Y', 'Constraint to axis'),
                ('SHIFT+CTRL+TAB', 'Switch snap mode'),
                ('RIGHTCLICK or ESC', 'exit without change')
                ])
            self.feedback.enable()
            args = (self, context)

            self.sel = [o for o in context.selected_objects]
            self.act = context.active_object
            bpy.ops.object.select_all(action="DESELECT")

            self.state = 'STARTING'

            self._handle = bpy.types.SpaceView3D.draw_handler_add(self.draw_callback, args, 'WINDOW', 'POST_PIXEL')
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------
# Define operator class to manage parts
# ------------------------------------------------------------------


class ARCHIPACK_OT_wall2_insert(Operator):
    bl_idname = "archipack.wall2_insert"
    bl_label = "Insert"
    bl_description = "Insert part"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    index = IntProperty(default=0)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            d = archipack_wall2.datablock(o)
            if d is None:
                return {'CANCELLED'}
            d.insert_part(context, o, self.index)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_wall2_remove(Operator):
    bl_idname = "archipack.wall2_remove"
    bl_label = "Remove"
    bl_description = "Remove part"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    index = IntProperty(default=0)

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            d = archipack_wall2.datablock(o)
            if d is None:
                return {'CANCELLED'}
            d.remove_part(context, o, self.index)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_wall2_reverse(Operator):
    bl_idname = "archipack.wall2_reverse"
    bl_label = "Reverse"
    bl_description = "Reverse parts order"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.mode == "OBJECT":
            o = context.active_object
            d = archipack_wall2.datablock(o)
            if d is None:
                return {'CANCELLED'}
            d.reverse(context, o)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------
# Define operator class to manipulate object
# ------------------------------------------------------------------


class ARCHIPACK_OT_wall2_manipulate(Operator):
    bl_idname = "archipack.wall2_manipulate"
    bl_label = "Manipulate"
    bl_description = "Manipulate"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        return archipack_wall2.filter(context.active_object)

    def invoke(self, context, event):
        d = archipack_wall2.datablock(context.active_object)
        d.manipulable_invoke(context)
        return {'FINISHED'}

    def execute(self, context):
        """
            For use in boolean ops
        """
        if archipack_wall2.filter(context.active_object):
            o = context.active_object
            d = archipack_wall2.datablock(o)
            g = d.get_generator()
            d.setup_childs(o, g)
            d.update_childs(context, o, g)
            d.update(context)
            o.select = True
            context.scene.objects.active = o
        return {'FINISHED'}


def register():
    bpy.utils.register_class(archipack_wall2_part)
    bpy.utils.register_class(archipack_wall2_child)
    bpy.utils.register_class(archipack_wall2)
    Mesh.archipack_wall2 = CollectionProperty(type=archipack_wall2)
    bpy.utils.register_class(ARCHIPACK_PT_wall2)
    bpy.utils.register_class(ARCHIPACK_OT_wall2)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_draw)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_insert)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_remove)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_reverse)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_manipulate)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_from_curve)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_from_slab)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_throttle_update)
    bpy.utils.register_class(ARCHIPACK_OT_wall2_fit_roof)


def unregister():
    bpy.utils.unregister_class(archipack_wall2_part)
    bpy.utils.unregister_class(archipack_wall2_child)
    bpy.utils.unregister_class(archipack_wall2)
    del Mesh.archipack_wall2
    bpy.utils.unregister_class(ARCHIPACK_PT_wall2)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_draw)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_insert)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_remove)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_reverse)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_manipulate)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_from_curve)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_from_slab)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_throttle_update)
    bpy.utils.unregister_class(ARCHIPACK_OT_wall2_fit_roof)
