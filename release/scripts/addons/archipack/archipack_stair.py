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
# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
from bpy.types import Operator, PropertyGroup, Mesh, Panel
from bpy.props import (
    FloatProperty, BoolProperty, IntProperty, CollectionProperty,
    StringProperty, EnumProperty, FloatVectorProperty
    )
from .bmesh_utils import BmeshEdit as bmed
from .panel import Panel as Lofter
from mathutils import Vector, Matrix
from math import sin, cos, pi, floor, acos
from .archipack_manipulator import Manipulable, archipack_manipulator
from .archipack_2d import Line, Arc
from .archipack_preset import ArchipackPreset, PresetMenuOperator
from .archipack_object import ArchipackCreateTool, ArchipackObject


class Stair():
    def __init__(self, left_offset, right_offset, steps_type, nose_type, z_mode, nose_z, bottom_z):
        self.steps_type = steps_type
        self.nose_type = nose_type
        self.l_shape = None
        self.r_shape = None
        self.next_type = 'NONE'
        self.last_type = 'NONE'
        self.z_mode = z_mode
        # depth of open step
        self.nose_z = nose_z
        # size under the step on bottom
        self.bottom_z = bottom_z
        self.left_offset = left_offset
        self.right_offset = right_offset
        self.last_height = 0

    def set_matids(self, matids):
        self.idmat_top, self.idmat_step_front, self.idmat_raise, \
        self.idmat_side, self.idmat_bottom, self.idmat_step_side = matids

    def set_height(self, step_height, z0):
        self.step_height = step_height
        self.z0 = z0

    @property
    def height(self):
        return self.n_step * self.step_height

    @property
    def top_offset(self):
        return self.t_step / self.step_depth

    @property
    def top(self):
        return self.z0 + self.height

    @property
    def left_length(self):
        return self.get_length("LEFT")

    @property
    def right_length(self):
        return self.get_length("RIGHT")

    def step_size(self, step_depth):
        t_step, n_step = self.steps(step_depth)
        self.n_step = n_step
        self.t_step = t_step
        self.step_depth = step_depth
        return n_step

    def p3d_left(self, verts, p2d, i, t, landing=False):
        x, y = p2d
        nose_z = min(self.step_height, self.nose_z)
        zl = self.z0 + t * self.height
        zs = self.z0 + i * self.step_height
        if self.z_mode == 'LINEAR':
            z0 = max(0, zl)
            z1 = z0 - self.bottom_z
            verts.extend([(x, y, z0), (x, y, z1)])
        else:
            if "FULL" in self.steps_type:
                z0 = 0
            else:
                z0 = max(0, zl - nose_z - self.bottom_z)
            z3 = zs + max(0, self.step_height - nose_z)
            z4 = zs + self.step_height
            if landing:
                if "FULL" in self.steps_type:
                    z2 = 0
                    z1 = 0
                else:
                    z2 = max(0, min(z3, z3 - self.bottom_z))
                    z1 = z2
            else:
                z1 = min(z3, max(z0, zl - nose_z))
                z2 = min(z3, max(z1, zl))
            verts.extend([(x, y, z0),
                        (x, y, z1),
                        (x, y, z2),
                        (x, y, z3),
                        (x, y, z4)])

    def p3d_right(self, verts, p2d, i, t, landing=False):
        x, y = p2d
        nose_z = min(self.step_height, self.nose_z)
        zl = self.z0 + t * self.height
        zs = self.z0 + i * self.step_height
        if self.z_mode == 'LINEAR':
            z0 = max(0, zl)
            z1 = z0 - self.bottom_z
            verts.extend([(x, y, z1), (x, y, z0)])
        else:
            if "FULL" in self.steps_type:
                z0 = 0
            else:
                z0 = max(0, zl - nose_z - self.bottom_z)
            z3 = zs + max(0, self.step_height - nose_z)
            z4 = zs + self.step_height
            if landing:
                if "FULL" in self.steps_type:
                    z2 = 0
                    z1 = 0
                else:
                    z2 = max(0, min(z3, z3 - self.bottom_z))
                    z1 = z2
            else:
                z1 = min(z3, max(z0, zl - nose_z))
                z2 = min(z3, max(z1, zl))
            verts.extend([(x, y, z4),
                          (x, y, z3),
                          (x, y, z2),
                          (x, y, z1),
                          (x, y, z0)])

    def p3d_cstep_left(self, verts, p2d, i, t):
        x, y = p2d
        nose_z = min(self.step_height, self.nose_z)
        zs = self.z0 + i * self.step_height
        z3 = zs + max(0, self.step_height - nose_z)
        z1 = min(z3, zs - nose_z)
        verts.append((x, y, z1))
        verts.append((x, y, z3))

    def p3d_cstep_right(self, verts, p2d, i, t):
        x, y = p2d
        nose_z = min(self.step_height, self.nose_z)
        zs = self.z0 + i * self.step_height
        z3 = zs + max(0, self.step_height - nose_z)
        z1 = min(z3, zs - nose_z)
        verts.append((x, y, z3))
        verts.append((x, y, z1))

    def straight_stair(self, length):
        self.next_type = 'STAIR'
        s = self.straight(length)
        return StraightStair(s.p, s.v, self.left_offset, self.right_offset, self.steps_type,
                self.nose_type, self.z_mode, self.nose_z, self.bottom_z)

    def straight_landing(self, length, last_type='STAIR'):
        self.next_type = 'LANDING'
        s = self.straight(length)
        return StraightLanding(s.p, s.v, self.left_offset, self.right_offset, self.steps_type,
                self.nose_type, self.z_mode, self.nose_z, self.bottom_z, last_type=last_type)

    def curved_stair(self, da, radius, left_shape, right_shape, double_limit=pi):
        self.next_type = 'STAIR'
        n = self.normal(1)
        n.v = radius * n.v.normalized()
        if da < 0:
            n.v = -n.v
        a0 = n.angle
        c = n.p - n.v
        return CurvedStair(c, radius, a0, da, self.left_offset, self.right_offset,
                self.steps_type, self.nose_type, self.z_mode, self.nose_z, self.bottom_z,
                left_shape, right_shape, double_limit=double_limit)

    def curved_landing(self, da, radius, left_shape, right_shape, double_limit=pi, last_type='STAIR'):
        self.next_type = 'LANDING'
        n = self.normal(1)
        n.v = radius * n.v.normalized()
        if da < 0:
            n.v = -n.v
        a0 = n.angle
        c = n.p - n.v
        return CurvedLanding(c, radius, a0, da, self.left_offset, self.right_offset,
                self.steps_type, self.nose_type, self.z_mode, self.nose_z, self.bottom_z,
                left_shape, right_shape, double_limit=double_limit, last_type=last_type)

    def get_z(self, t, mode):
        if mode == 'LINEAR':
            return self.z0 + t * self.height
        else:
            step = 1 + floor(t / self.t_step)
            return self.z0 + step * self.step_height

    def make_profile(self, t, side, profile, verts, faces, matids, next=None, tnext=0):
        z0 = self.get_z(t, 'LINEAR')
        dz1 = 0
        t, part, dz0, shape = self.get_part(t, side)
        if next is not None:
            tnext, next, dz1, shape1 = next.get_part(tnext, side)
        xy, s = part.proj_xy(t, next)
        v_xy = s * xy.to_3d()
        z, s = part.proj_z(t, dz0, next, dz1)
        v_z = s * Vector((-xy.y * z.x, xy.x * z.x, z.y))
        x, y = part.lerp(t)
        verts += [Vector((x, y, z0)) + v.x * v_xy + v.y * v_z for v in profile]

    def project_uv(self, rM, uvs, verts, indexes, up_axis='Z'):
        if up_axis == 'Z':
            uvs.append([(rM * Vector(verts[i])).to_2d() for i in indexes])
        elif up_axis == 'Y':
            uvs.append([(x, z) for x, y, z in [(rM * Vector(verts[i])) for i in indexes]])
        else:
            uvs.append([(y, z) for x, y, z in [(rM * Vector(verts[i])) for i in indexes]])

    def get_proj_matrix(self, part, t, nose_y):
        # a matrix to project verts
        # into uv space for horizontal parts of this step
        # so uv = (rM * vertex).to_2d()
        tl = t - nose_y / self.get_length("LEFT")
        tr = t - nose_y / self.get_length("RIGHT")
        t2, part, dz, shape = self.get_part(tl, "LEFT")
        p0 = part.lerp(t2)
        t2, part, dz, shape = self.get_part(tr, "RIGHT")
        p1 = part.lerp(t2)
        v = (p1 - p0).normalized()
        return Matrix([
            [-v.y, v.x, 0, p0.x],
            [v.x, v.y, 0, p0.y],
            [0, 0, 1, 0],
            [0, 0, 0, 1]
        ]).inverted()

    def _make_nose(self, i, s, verts, faces, matids, uvs, nose_y):

        t = self.t_step * i

        # a matrix to project verts
        # into uv space for horizontal parts of this step
        # so uv = (rM * vertex).to_2d()
        rM = self.get_proj_matrix(self, t, nose_y)

        if self.z_mode == 'LINEAR':
            return rM

        f = len(verts)

        tl = t - nose_y / self.get_length("LEFT")
        tr = t - nose_y / self.get_length("RIGHT")

        t2, part, dz, shape = self.get_part(tl, "LEFT")
        p0 = part.lerp(t2)
        self.p3d_left(verts, p0, s, t2)

        t2, part, dz, shape = self.get_part(tr, "RIGHT")
        p1 = part.lerp(t2)
        self.p3d_right(verts, p1, s, t2)

        start = 3
        end = 6
        offset = 10

        # left, top, right
        matids.extend([self.idmat_step_side,
             self.idmat_top,
             self.idmat_step_side])

        faces += [(f + j, f + j + 1, f + j + offset + 1, f + j + offset) for j in range(start, end)]

        u = nose_y
        v = (p1 - p0).length
        w = verts[f + 2][2] - verts[f + 3][2]
        s = int((end - start) / 2)

        uvs += [[(u, verts[f + j][2]), (u, verts[f + j + 1][2]),
            (0, verts[f + j + 1][2]), (0, verts[f + j][2])] for j in range(start, start + s)]

        uvs.append([(0, 0), (0, v), (u, v), (u, 0)])

        uvs += [[(u, verts[f + j][2]), (u, verts[f + j + 1][2]),
            (0, verts[f + j + 1][2]), (0, verts[f + j][2])] for j in range(start + s + 1, end)]

        if 'STRAIGHT' in self.nose_type or 'OPEN' in self.steps_type:
            # face bottom
            matids.append(self.idmat_bottom)
            faces.append((f + end, f + start, f + offset + start, f + offset + end))
            uvs.append([(u, v), (u, 0), (0, 0), (0, v)])

        if self.steps_type != 'OPEN':
            if 'STRAIGHT' in self.nose_type:
                # front face bottom straight
                matids.append(self.idmat_raise)
                faces.append((f + 12, f + 17, f + 16, f + 13))
                uvs.append([(0, w), (v, w), (v, 0), (0, 0)])

            elif 'OBLIQUE' in self.nose_type:
                # front face bottom oblique
                matids.append(self.idmat_raise)
                faces.append((f + 12, f + 17, f + 6, f + 3))

                uvs.append([(0, w), (v, w), (v, 0), (0, 0)])

                matids.append(self.idmat_side)
                faces.append((f + 3, f + 13, f + 12))
                uvs.append([(0, 0), (u, 0), (u, w)])

                matids.append(self.idmat_side)
                faces.append((f + 6, f + 17, f + 16))
                uvs.append([(0, 0), (u, w), (u, 0)])

        # front face top
        w = verts[f + 3][2] - verts[f + 4][2]
        matids.append(self.idmat_step_front)
        faces.append((f + 4, f + 3, f + 6, f + 5))
        uvs.append([(0, 0), (0, w), (v, w), (v, 0)])
        return rM

    def make_faces(self, f, rM, verts, faces, matids, uvs):

        if self.z_mode == 'LINEAR':
            start = 0
            end = 3
            offset = 4
            matids.extend([self.idmat_side,
                 self.idmat_top,
                 self.idmat_side,
                 self.idmat_bottom])
        elif "OPEN" in self.steps_type:
            # faces dessus-dessous-lateral marches fermees
            start = 3
            end = 6
            offset = 10
            matids.extend([self.idmat_step_side,
                 self.idmat_top,
                 self.idmat_step_side,
                 self.idmat_bottom])
        else:
            # faces dessus-dessous-lateral marches fermees
            start = 0
            end = 9
            offset = 10
            matids.extend([self.idmat_side,
                 self.idmat_side,
                 self.idmat_side,
                 self.idmat_step_side,
                 self.idmat_top,
                 self.idmat_step_side,
                 self.idmat_side,
                 self.idmat_side,
                 self.idmat_side,
                 self.idmat_bottom])

        u_l0 = 0
        u_l1 = self.t_step * self.left_length
        u_r0 = 0
        u_r1 = self.t_step * self.right_length

        s = int((end - start) / 2)
        uvs += [[(u_l0, verts[f + j][2]), (u_l0, verts[f + j + 1][2]),
            (u_l1, verts[f + j + offset + 1][2]), (u_l1, verts[f + j + offset][2])] for j in range(start, start + s)]

        self.project_uv(rM, uvs, verts, [f + start + s, f + start + s + 1,
            f + start + s + offset + 1, f + start + s + offset])

        uvs += [[(u_r0, verts[f + j][2]), (u_r0, verts[f + j + 1][2]),
            (u_r1, verts[f + j + offset + 1][2]), (u_r1, verts[f + j + offset][2])] for j in range(start + s + 1, end)]

        self.project_uv(rM, uvs, verts, [f + end, f + start, f + offset + start, f + offset + end])

        faces += [(f + j, f + j + 1, f + j + offset + 1, f + j + offset) for j in range(start, end)]
        faces.append((f + end, f + start, f + offset + start, f + offset + end))


class StraightStair(Stair, Line):
    def __init__(self, p, v, left_offset, right_offset, steps_type, nose_type, z_mode, nose_z, bottom_z):
        Stair.__init__(self, left_offset, right_offset, steps_type, nose_type, z_mode, nose_z, bottom_z)
        Line.__init__(self, p, v)
        self.l_line = self.offset(-left_offset)
        self.r_line = self.offset(right_offset)

    def make_step(self, i, verts, faces, matids, uvs, nose_y=0):

        rM = self._make_nose(i, i, verts, faces, matids, uvs, nose_y)

        t0 = self.t_step * i

        f = len(verts)

        p = self.l_line.lerp(t0)
        self.p3d_left(verts, p, i, t0)
        p = self.r_line.lerp(t0)
        self.p3d_right(verts, p, i, t0)

        t1 = t0 + self.t_step

        p = self.l_line.lerp(t1)
        self.p3d_left(verts, p, i, t1)
        p = self.r_line.lerp(t1)
        self.p3d_right(verts, p, i, t1)

        self.make_faces(f, rM, verts, faces, matids, uvs)

        if "OPEN" in self.steps_type:
            faces.append((f + 13, f + 14, f + 15, f + 16))
            matids.append(self.idmat_step_front)
            uvs.append([(0, 0), (0, 1), (1, 1), (1, 0)])

    def get_length(self, side):
        return self.length

    def get_lerp_vect(self, posts, side, i, t_step, respect_edges, z_offset=0, t0_abs=None):
        if t0_abs is not None:
            t0 = t0_abs
        else:
            t0 = i * t_step
        t, part, dz, shape = self.get_part(t0, side)
        dz /= part.length
        n = part.normal(t)
        z0 = self.get_z(t0, 'STEP')
        z1 = self.get_z(t0, 'LINEAR')
        posts.append((n, dz, z0, z1 + t0 * z_offset))
        return [t0]

    def n_posts(self, post_spacing, side, respect_edges):
        return self.steps(post_spacing)

    def get_part(self, t, side):
        if side == 'LEFT':
            part = self.l_line
        else:
            part = self.r_line
        return t, part, self.height, 'LINE'


class CurvedStair(Stair, Arc):
    def __init__(self, c, radius, a0, da, left_offset, right_offset, steps_type, nose_type,
        z_mode, nose_z, bottom_z, left_shape, right_shape, double_limit=pi):

        Stair.__init__(self, left_offset, right_offset, steps_type, nose_type, z_mode, nose_z, bottom_z)
        Arc.__init__(self, c, radius, a0, da)
        self.l_shape = left_shape
        self.r_shape = right_shape
        self.edges_multiples = round(abs(da), 6) > double_limit
        # left arc, tangeant at start and end
        self.l_arc, self.l_t0, self.l_t1, self.l_tc = self.set_offset(-left_offset, left_shape)
        self.r_arc, self.r_t0, self.r_t1, self.r_tc = self.set_offset(right_offset, right_shape)

    def set_offset(self, offset, shape):
        arc = self.offset(offset)
        t0 = arc.tangeant(0, 1)
        t1 = arc.tangeant(1, 1)
        tc = arc.tangeant(0.5, 1)
        if self.edges_multiples:
            i, p, t = t0.intersect(tc)
            tc.v *= 2 * t
            tc.p = p
            i, p, t2 = tc.intersect(t1)
        else:
            i, p, t = t0.intersect(t1)
        t0.v *= t
        t1.p = p
        t1.v *= t
        return arc, t0, t1, tc

    def get_length(self, side):
        if side == 'RIGHT':
            arc = self.r_arc
            shape = self.r_shape
            t0 = self.r_t0
        else:
            arc = self.l_arc
            shape = self.l_shape
            t0 = self.l_t0
        if shape == 'CIRCLE':
            return arc.length
        else:
            if self.edges_multiples:
                # two edges
                return t0.length * 4
            else:
                return t0.length * 2

    def _make_step(self, t_step, i, s, verts, landing=False):

        tb = t_step * i

        f = len(verts)

        t, part, dz, shape = self.get_part(tb, "LEFT")
        p = part.lerp(t)
        self.p3d_left(verts, p, s, tb, landing)

        t, part, dz, shape = self.get_part(tb, "RIGHT")
        p = part.lerp(t)
        self.p3d_right(verts, p, s, tb, landing)
        return f

    def _make_edge(self, t_step, i, j, f, rM, verts, faces, matids, uvs):
        tb = t_step * i
        # make edges verts after regular ones
        if self.l_shape != 'CIRCLE' or self.r_shape != 'CIRCLE':
            if self.edges_multiples:
                # edge 1
                if tb < 0.25 and tb + t_step > 0.25:
                    f0 = f
                    f = len(verts)
                    if self.l_shape == 'CIRCLE':
                        self.p3d_left(verts, self.l_arc.lerp(0.25), j, 0.25)
                    else:
                        self.p3d_left(verts, self.l_tc.p, j, 0.25)
                    if self.r_shape == 'CIRCLE':
                        self.p3d_right(verts, self.r_arc.lerp(0.25), j, 0.25)
                    else:
                        self.p3d_right(verts, self.r_tc.p, j, 0.25)
                    self.make_faces(f0, rM, verts, faces, matids, uvs)
                # edge 2
                if tb < 0.75 and tb + t_step > 0.75:
                    f0 = f
                    f = len(verts)
                    if self.l_shape == 'CIRCLE':
                        self.p3d_left(verts, self.l_arc.lerp(0.75), j, 0.75)
                    else:
                        self.p3d_left(verts, self.l_t1.p, j, 0.75)
                    if self.r_shape == 'CIRCLE':
                        self.p3d_right(verts, self.r_arc.lerp(0.75), j, 0.75)
                    else:
                        self.p3d_right(verts, self.r_t1.p, j, 0.75)
                    self.make_faces(f0, rM, verts, faces, matids, uvs)
            else:
                if tb < 0.5 and tb + t_step > 0.5:
                    f0 = f
                    f = len(verts)
                    # the step goes through the edge
                    if self.l_shape == 'CIRCLE':
                        self.p3d_left(verts, self.l_arc.lerp(0.5), j, 0.5)
                    else:
                        self.p3d_left(verts, self.l_t1.p, j, 0.5)
                    if self.r_shape == 'CIRCLE':
                        self.p3d_right(verts, self.r_arc.lerp(0.5), j, 0.5)
                    else:
                        self.p3d_right(verts, self.r_t1.p, j, 0.5)
                    self.make_faces(f0, rM, verts, faces, matids, uvs)
        return f

    def make_step(self, i, verts, faces, matids, uvs, nose_y=0):

        # open stair with closed face

        # step nose
        rM = self._make_nose(i, i, verts, faces, matids, uvs, nose_y)
        f = 0
        if self.l_shape == 'CIRCLE' or self.r_shape == 'CIRCLE':
            # every 6 degree
            n_subs = max(1, int(abs(self.da) / pi * 30 / self.n_step))
            t_step = self.t_step / n_subs
            for j in range(n_subs):
                f0 = f
                f = self._make_step(t_step, n_subs * i + j, i, verts)
                if j > 0:
                    self.make_faces(f0, rM, verts, faces, matids, uvs)
                f = self._make_edge(t_step, n_subs * i + j, i, f, rM, verts, faces, matids, uvs)
        else:
            f = self._make_step(self.t_step, i, i, verts)
            f = self._make_edge(self.t_step, i, i, f, rM, verts, faces, matids, uvs)

        self._make_step(self.t_step, i + 1, i, verts)
        self.make_faces(f, rM, verts, faces, matids, uvs)

        if "OPEN" in self.steps_type and self.z_mode != 'LINEAR':
            # back face top
            faces.append((f + 13, f + 14, f + 15, f + 16))
            matids.append(self.idmat_step_front)
            uvs.append([(0, 0), (0, 1), (1, 1), (1, 0)])

    def get_part(self, t, side):
        if side == 'RIGHT':
            arc = self.r_arc
            shape = self.r_shape
            t0, t1, tc = self.r_t0, self.r_t1, self.r_tc
        else:
            arc = self.l_arc
            shape = self.l_shape
            t0, t1, tc = self.l_t0, self.l_t1, self.l_tc
        if shape == 'CIRCLE':
            return t, arc, self.height, shape
        else:
            if self.edges_multiples:
                # two edges
                if t <= 0.25:
                    return 4 * t, t0, 0.25 * self.height, shape
                elif t <= 0.75:
                    return 2 * (t - 0.25), tc, 0.5 * self.height, shape
                else:
                    return 4 * (t - 0.75), t1, 0.25 * self.height, shape
            else:
                if t <= 0.5:
                    return 2 * t, t0, 0.5 * self.height, shape
                else:
                    return 2 * (t - 0.5), t1, 0.5 * self.height, shape

    def get_lerp_vect(self, posts, side, i, t_step, respect_edges, z_offset=0, t0_abs=None):
        if t0_abs is not None:
            t0 = t0_abs
        else:
            t0 = i * t_step
        res = [t0]
        t1 = t0 + t_step
        zs = self.get_z(t0, 'STEP')
        zl = self.get_z(t0, 'LINEAR')

        # vect normal
        t, part, dz, shape = self.get_part(t0, side)
        n = part.normal(t)
        dz /= part.length
        posts.append((n, dz, zs, zl + t0 * z_offset))

        if shape != 'CIRCLE' and respect_edges:
            if self.edges_multiples:
                if t0 < 0.25 and t1 > 0.25:
                    zs = self.get_z(0.25, 'STEP')
                    zl = self.get_z(0.25, 'LINEAR')
                    t, part, dz, shape = self.get_part(0.25, side)
                    n = part.normal(1)
                    posts.append((n, dz, zs, zl + 0.25 * z_offset))
                    res.append(0.25)
                if t0 < 0.75 and t1 > 0.75:
                    zs = self.get_z(0.75, 'STEP')
                    zl = self.get_z(0.75, 'LINEAR')
                    t, part, dz, shape = self.get_part(0.75, side)
                    n = part.normal(1)
                    posts.append((n, dz, zs, zl + 0.75 * z_offset))
                    res.append(0.75)
            elif t0 < 0.5 and t1 > 0.5:
                    zs = self.get_z(0.5, 'STEP')
                    zl = self.get_z(0.5, 'LINEAR')
                    t, part, dz, shape = self.get_part(0.5, side)
                    n = part.normal(1)
                    posts.append((n, dz, zs, zl + 0.5 * z_offset))
                    res.append(0.5)
        return res

    def n_posts(self, post_spacing, side, respect_edges):
        if side == 'LEFT':
            arc, t0, shape = self.l_arc, self.l_t0, self.l_shape
        else:
            arc, t0, shape = self.r_arc, self.r_t0, self.r_shape
        step_factor = 1
        if shape == 'CIRCLE':
            length = arc.length
        else:
            if self.edges_multiples:
                if respect_edges:
                    step_factor = 2
                length = 4 * t0.length
            else:
                length = 2 * t0.length
        steps = step_factor * max(1, round(length / post_spacing, 0))
        # print("respect_edges:%s t_step:%s n_step:%s" % (respect_edges, 1.0 / steps, int(steps)))
        return 1.0 / steps, int(steps)


class StraightLanding(StraightStair):
    def __init__(self, p, v, left_offset, right_offset, steps_type,
            nose_type, z_mode, nose_z, bottom_z, last_type='STAIR'):

        StraightStair.__init__(self, p, v, left_offset, right_offset, steps_type,
            nose_type, z_mode, nose_z, bottom_z)

        self.last_type = last_type

    @property
    def height(self):
        return 0

    @property
    def top_offset(self):
        return self.t_step / self.v.length

    @property
    def top(self):
        if self.next_type == 'LANDING':
            return self.z0
        else:
            return self.z0 + self.step_height

    def step_size(self, step_depth):
        self.n_step = 1
        self.t_step = 1
        self.step_depth = step_depth
        if self.last_type == 'LANDING':
            return 0
        else:
            return 1

    def make_step(self, i, verts, faces, matids, uvs, nose_y=0):

        if i == 0 and self.last_type != 'LANDING':
            rM = self._make_nose(i, 0, verts, faces, matids, uvs, nose_y)
        else:
            rM = self.get_proj_matrix(self.l_line, self.t_step * i, nose_y)

        f = len(verts)
        j = 0
        t0 = self.t_step * i

        p = self.l_line.lerp(t0)
        self.p3d_left(verts, p, j, t0)

        p = self.r_line.lerp(t0)
        self.p3d_right(verts, p, j, t0)

        t1 = t0 + self.t_step
        p = self.l_line.lerp(t1)
        self.p3d_left(verts, p, j, t1, self.next_type != 'LANDING')

        p = self.r_line.lerp(t1)
        self.p3d_right(verts, p, j, t1, self.next_type != 'LANDING')

        self.make_faces(f, rM, verts, faces, matids, uvs)

        if "OPEN" in self.steps_type and self.next_type != 'LANDING':
            faces.append((f + 13, f + 14, f + 15, f + 16))
            matids.append(self.idmat_step_front)
            uvs.append([(0, 0), (0, 1), (1, 1), (1, 0)])

    def straight_landing(self, length):
        return Stair.straight_landing(self, length, last_type='LANDING')

    def curved_landing(self, da, radius, left_shape, right_shape, double_limit=pi):
        return Stair.curved_landing(self, da, radius, left_shape,
            right_shape, double_limit=double_limit, last_type='LANDING')

    def get_z(self, t, mode):
        if mode == 'STEP':
            return self.z0 + self.step_height
        else:
            return self.z0


class CurvedLanding(CurvedStair):
    def __init__(self, c, radius, a0, da, left_offset, right_offset, steps_type,
        nose_type, z_mode, nose_z, bottom_z, left_shape, right_shape, double_limit=pi, last_type='STAIR'):

        CurvedStair.__init__(self, c, radius, a0, da, left_offset, right_offset, steps_type,
            nose_type, z_mode, nose_z, bottom_z, left_shape, right_shape, double_limit=double_limit)

        self.last_type = last_type

    @property
    def top_offset(self):
        if self.l_shape == 'CIRCLE' or self.r_shape == 'CIRCLE':
            return self.t_step / self.step_depth
        else:
            if self.edges_multiples:
                return 0.5 / self.length
            else:
                return 1 / self.length

    @property
    def height(self):
        return 0

    @property
    def top(self):
        if self.next_type == 'LANDING':
            return self.z0
        else:
            return self.z0 + self.step_height

    def step_size(self, step_depth):
        if self.l_shape == 'CIRCLE' or self.r_shape == 'CIRCLE':
            t_step, n_step = self.steps(step_depth)
        else:
            if self.edges_multiples:
                t_step, n_step = 0.5, 2
            else:
                t_step, n_step = 1, 1
        self.n_step = n_step
        self.t_step = t_step
        self.step_depth = step_depth
        if self.last_type == 'LANDING':
            return 0
        else:
            return 1

    def make_step(self, i, verts, faces, matids, uvs, nose_y=0):

        if i == 0 and 'LANDING' not in self.last_type:
            rM = self._make_nose(i, 0, verts, faces, matids, uvs, nose_y)
        else:
            rM = self.get_proj_matrix(self.l_arc, self.t_step * i, nose_y)

        f = len(verts)

        if self.l_shape == 'CIRCLE' or self.r_shape == 'CIRCLE':
            n_subs = max(1, int(abs(self.da / pi * 30 / self.n_step)))
            t_step = self.t_step / n_subs
            for j in range(n_subs):
                f0 = f
                f = self._make_step(t_step, n_subs * i + j, 0, verts)
                if j > 0:
                    self.make_faces(f0, rM, verts, faces, matids, uvs)
                f = self._make_edge(t_step, n_subs * i + j, 0, f, rM, verts, faces, matids, uvs)
        else:
            f = self._make_step(self.t_step, i, 0, verts)
            f = self._make_edge(self.t_step, i, 0, f, rM, verts, faces, matids, uvs)

        self._make_step(self.t_step, i + 1, 0, verts, i == self.n_step - 1 and 'LANDING' not in self.next_type)
        self.make_faces(f, rM, verts, faces, matids, uvs)

        if "OPEN" in self.steps_type and 'LANDING' not in self.next_type:
            faces.append((f + 13, f + 14, f + 15, f + 16))
            matids.append(self.idmat_step_front)
            uvs.append([(0, 0), (0, 1), (1, 1), (1, 0)])

    def straight_landing(self, length):
        return Stair.straight_landing(self, length, last_type='LANDING')

    def curved_landing(self, da, radius, left_shape, right_shape, double_limit=pi):
        return Stair.curved_landing(self, da, radius, left_shape,
            right_shape, double_limit=double_limit, last_type='LANDING')

    def get_z(self, t, mode):
        if mode == 'STEP':
            return self.z0 + self.step_height
        else:
            return self.z0


class StairGenerator():
    def __init__(self, parts):
        self.parts = parts
        self.last_type = 'NONE'
        self.stairs = []
        self.steps_type = 'NONE'
        self.sum_da = 0
        self.user_defined_post = None
        self.user_defined_uvs = None
        self.user_defined_mat = None

    def add_part(self, type, steps_type, nose_type, z_mode, nose_z, bottom_z, center,
            radius, da, width_left, width_right, length, left_shape, right_shape):

        self.steps_type = steps_type
        if len(self.stairs) < 1:
            s = None
        else:
            s = self.stairs[-1]

        if "S_" not in type:
            self.sum_da += da

        # start a new stair
        if s is None:
            if type == 'S_STAIR':
                p = Vector((0, 0))
                v = Vector((0, length))
                s = StraightStair(p, v, width_left, width_right, steps_type, nose_type, z_mode, nose_z, bottom_z)
            elif type == 'C_STAIR':
                if da < 0:
                    c = Vector((radius, 0))
                else:
                    c = Vector((-radius, 0))
                s = CurvedStair(c, radius, 0, da, width_left, width_right, steps_type,
                        nose_type, z_mode, nose_z, bottom_z, left_shape, right_shape)
            elif type == 'D_STAIR':
                if da < 0:
                    c = Vector((radius, 0))
                else:
                    c = Vector((-radius, 0))
                s = CurvedStair(c, radius, 0, da, width_left, width_right, steps_type,
                        nose_type, z_mode, nose_z, bottom_z, left_shape, right_shape, double_limit=0)
            elif type == 'S_LANDING':
                p = Vector((0, 0))
                v = Vector((0, length))
                s = StraightLanding(p, v, width_left, width_right, steps_type, nose_type, z_mode, nose_z, bottom_z)
            elif type == 'C_LANDING':
                if da < 0:
                    c = Vector((radius, 0))
                else:
                    c = Vector((-radius, 0))
                s = CurvedLanding(c, radius, 0, da, width_left, width_right, steps_type,
                        nose_type, z_mode, nose_z, bottom_z, left_shape, right_shape)
            elif type == 'D_LANDING':
                if da < 0:
                    c = Vector((radius, 0))
                else:
                    c = Vector((-radius, 0))
                s = CurvedLanding(c, radius, 0, da, width_left, width_right, steps_type,
                        nose_type, z_mode, nose_z, bottom_z, left_shape, right_shape, double_limit=0)
        else:
            if type == 'S_STAIR':
                s = s.straight_stair(length)
            elif type == 'C_STAIR':
                s = s.curved_stair(da, radius, left_shape, right_shape)
            elif type == 'D_STAIR':
                s = s.curved_stair(da, radius, left_shape, right_shape, double_limit=0)
            elif type == 'S_LANDING':
                s = s.straight_landing(length)
            elif type == 'C_LANDING':
                s = s.curved_landing(da, radius, left_shape, right_shape)
            elif type == 'D_LANDING':
                s = s.curved_landing(da, radius, left_shape, right_shape, double_limit=0)
        self.stairs.append(s)
        self.last_type = type

    def n_steps(self, step_depth):
        n_steps = 0
        for stair in self.stairs:
            n_steps += stair.step_size(step_depth)
        return n_steps

    def set_height(self, step_height):
        z = 0
        for stair in self.stairs:
            stair.set_height(step_height, z)
            z = stair.top

    def make_stair(self, height, step_depth, verts, faces, matids, uvs, nose_y=0):
        n_steps = self.n_steps(step_depth)
        self.set_height(height / n_steps)

        for s, stair in enumerate(self.stairs):
            if s < len(self.parts):
                manipulator = self.parts[s].manipulators[0]
                # Store Gl Points for manipulators
                if 'Curved' in type(stair).__name__:
                    c = stair.c
                    p0 = (stair.p0 - c).to_3d()
                    p1 = (stair.p1 - c).to_3d()
                    manipulator.set_pts([(c.x, c.y, stair.top), p0, p1])
                    manipulator.type_key = 'ARC_ANGLE_RADIUS'
                    manipulator.prop1_name = 'da'
                    manipulator.prop2_name = 'radius'
                else:
                    if self.sum_da > 0:
                        side = 1
                    else:
                        side = -1
                    v0 = stair.p0
                    v1 = stair.p1
                    manipulator.set_pts([(v0.x, v0.y, stair.top), (v1.x, v1.y, stair.top), (side, 0, 0)])
                    manipulator.type_key = 'SIZE'
                    manipulator.prop1_name = 'length'

            for i in range(stair.n_step):
                stair.make_step(i, verts, faces, matids, uvs, nose_y=nose_y)
                if s < len(self.stairs) - 1 and self.steps_type != 'OPEN' and \
                        'Landing' in type(stair).__name__ and stair.next_type != "LANDING":
                    f = len(verts) - 10
                    faces.append((f, f + 1, f + 8, f + 9))
                    matids.append(self.stairs[-1].idmat_bottom)
                    u = verts[f + 1][2] - verts[f][2]
                    v = (Vector(verts[f]) - Vector(verts[f + 9])).length
                    uvs.append([(0, 0), (0, u), (v, u), (v, 0)])

        if self.steps_type != 'OPEN' and len(self.stairs) > 0:
            f = len(verts) - 10
            faces.append((f, f + 1, f + 2, f + 3, f + 4, f + 5, f + 6, f + 7, f + 8, f + 9))
            matids.append(self.stairs[-1].idmat_bottom)
            uvs.append([(0, 0), (.1, 0), (.2, 0), (.3, 0), (.4, 0), (.4, 1), (.3, 1), (.2, 1), (.1, 1), (0, 1)])

    def setup_user_defined_post(self, o, post_x, post_y, post_z):
        self.user_defined_post = o
        x = o.bound_box[6][0] - o.bound_box[0][0]
        y = o.bound_box[6][1] - o.bound_box[0][1]
        z = o.bound_box[6][2] - o.bound_box[0][2]
        self.user_defined_post_scale = Vector((post_x / x, post_y / -y, post_z / z))
        m = o.data
        # create vertex group lookup dictionary for names
        vgroup_names = {vgroup.index: vgroup.name for vgroup in o.vertex_groups}
        # create dictionary of vertex group assignments per vertex
        self.vertex_groups = [[vgroup_names[g.group] for g in v.groups] for v in m.vertices]
        # uvs
        uv_act = m.uv_layers.active
        if uv_act is not None:
            uv_layer = uv_act.data
            self.user_defined_uvs = [[uv_layer[li].uv for li in p.loop_indices] for p in m.polygons]
        else:
            self.user_defined_uvs = [[(0, 0) for i in p.vertices] for p in m.polygons]
        # material ids
        self.user_defined_mat = [p.material_index for p in m.polygons]

    def get_user_defined_post(self, tM, z0, z1, z2, slope, post_z, verts, faces, matids, uvs):
        f = len(verts)
        m = self.user_defined_post.data
        for i, g in enumerate(self.vertex_groups):
            co = m.vertices[i].co.copy()
            co.x *= self.user_defined_post_scale.x
            co.y *= self.user_defined_post_scale.y
            co.z *= self.user_defined_post_scale.z
            if 'Top' in g:
                co.z += z2
            elif 'Bottom' in g:
                co.z += 0
            else:
                co.z += z1
            if 'Slope' in g:
                co.z += co.y * slope
            verts.append(tM * co)
        matids += self.user_defined_mat
        faces += [tuple([i + f for i in p.vertices]) for p in m.polygons]
        uvs += self.user_defined_uvs

    def get_post(self, post, post_x, post_y, post_z, post_alt, sub_offset_x,
            id_mat, verts, faces, matids, uvs, bottom="STEP"):

        n, dz, zs, zl = post
        slope = dz * post_y

        if self.user_defined_post is not None:
            if bottom == "STEP":
                z0 = zs
            else:
                z0 = zl
            z1 = zl - z0
            z2 = zl - z0
            x, y = -n.v.normalized()
            tM = Matrix([
                [x, y, 0, n.p.x],
                [y, -x, 0, n.p.y],
                [0, 0, 1, z0 + post_alt],
                [0, 0, 0, 1]
            ])
            self.get_user_defined_post(tM, z0, z1, z2, dz, post_z, verts, faces, matids, uvs)
            return

        z3 = zl + post_z + post_alt - slope
        z4 = zl + post_z + post_alt + slope
        if bottom == "STEP":
            z0 = zs + post_alt
            z1 = zs + post_alt
        else:
            z0 = zl + post_alt - slope
            z1 = zl + post_alt + slope
        vn = n.v.normalized()
        dx = post_x * vn
        dy = post_y * Vector((vn.y, -vn.x))
        oy = sub_offset_x * vn
        x0, y0 = n.p - dx + dy + oy
        x1, y1 = n.p - dx - dy + oy
        x2, y2 = n.p + dx - dy + oy
        x3, y3 = n.p + dx + dy + oy
        f = len(verts)
        verts.extend([(x0, y0, z0), (x0, y0, z3),
                    (x1, y1, z1), (x1, y1, z4),
                    (x2, y2, z1), (x2, y2, z4),
                    (x3, y3, z0), (x3, y3, z3)])
        faces.extend([(f, f + 1, f + 3, f + 2),
                    (f + 2, f + 3, f + 5, f + 4),
                    (f + 4, f + 5, f + 7, f + 6),
                    (f + 6, f + 7, f + 1, f),
                    (f, f + 2, f + 4, f + 6),
                    (f + 7, f + 5, f + 3, f + 1)])
        matids.extend([id_mat, id_mat, id_mat, id_mat, id_mat, id_mat])
        x = [(0, 0), (0, post_z), (post_x, post_z), (post_x, 0)]
        y = [(0, 0), (0, post_z), (post_y, post_z), (post_y, 0)]
        z = [(0, 0), (post_x, 0), (post_x, post_y), (0, post_y)]
        uvs.extend([x, y, x, y, z, z])

    def get_panel(self, subs, altitude, panel_x, panel_z, sub_offset_x, idmat, verts, faces, matids, uvs):
        n_subs = len(subs)
        if n_subs < 1:
            return
        f = len(verts)
        x0 = sub_offset_x - 0.5 * panel_x
        x1 = sub_offset_x + 0.5 * panel_x
        z0 = 0
        z1 = panel_z
        profile = [Vector((x0, z0)), Vector((x1, z0)), Vector((x1, z1)), Vector((x0, z1))]
        user_path_uv_v = []
        n_sections = n_subs - 1
        n, dz, zs, zl = subs[0]
        p0 = n.p
        v0 = n.v.normalized()
        for s, section in enumerate(subs):
            n, dz, zs, zl = section
            p1 = n.p
            if s < n_sections:
                v1 = subs[s + 1][0].v.normalized()
            dir = (v0 + v1).normalized()
            scale = 1 / cos(0.5 * acos(min(1, max(-1, v0 * v1))))
            for p in profile:
                x, y = n.p + scale * p.x * dir
                z = zl + p.y + altitude
                verts.append((x, y, z))
            if s > 0:
                user_path_uv_v.append((p1 - p0).length)
            p0 = p1
            v0 = v1

        # build faces using Panel
        lofter = Lofter(
            # closed_shape, index, x, y, idmat
            True,
            [i for i in range(len(profile))],
            [p.x for p in profile],
            [p.y for p in profile],
            [idmat for i in range(len(profile))],
            closed_path=False,
            user_path_uv_v=user_path_uv_v,
            user_path_verts=n_subs
            )
        faces += lofter.faces(16, offset=f, path_type='USER_DEFINED')
        matids += lofter.mat(16, idmat, idmat, path_type='USER_DEFINED')
        v = Vector((0, 0))
        uvs += lofter.uv(16, v, v, v, v, 0, v, 0, 0, path_type='USER_DEFINED')

    def reset_shapes(self):
        for s, stair in enumerate(self.stairs):
            if 'Curved' in type(stair).__name__:
                stair.l_shape = self.parts[s].left_shape
                stair.r_shape = self.parts[s].right_shape

    def make_subs(self, height, step_depth, x, y, z, post_y, altitude, bottom, side, slice,
            post_spacing, sub_spacing, respect_edges, move_x, x_offset, sub_offset_x, mat,
            verts, faces, matids, uvs):

        n_steps = self.n_steps(step_depth)
        self.set_height(height / n_steps)
        n_stairs = len(self.stairs) - 1
        subs = []

        if side == "LEFT":
            offset = move_x - x_offset
            # offset_sub = offset - sub_offset_x
        else:
            offset = move_x + x_offset
            # offset_sub = offset + sub_offset_x

        for s, stair in enumerate(self.stairs):
            if 'Curved' in type(stair).__name__:
                if side == "LEFT":
                    part = stair.l_arc
                    shape = stair.l_shape
                else:
                    part = stair.r_arc
                    shape = stair.r_shape
                # Note: use left part as reference for post distances
                # use right part as reference for panels
                stair.l_arc, stair.l_t0, stair.l_t1, stair.l_tc = stair.set_offset(offset, shape)
                stair.r_arc, stair.r_t0, stair.r_t1, stair.r_tc = stair.set_offset(offset, shape)
            else:
                stair.l_line = stair.offset(offset)
                stair.r_line = stair.offset(offset)
                part = stair.l_line

            lerp_z = 0
            edge_t = 1
            edge_size = 0
            # interpolate z near end landing
            if 'Landing' in type(stair).__name__ and stair.next_type == 'STAIR':
                if not slice:
                    line = stair.normal(1).offset(self.stairs[s + 1].step_depth)
                    res, p, t_part = part.intersect(line)
                    # does perpendicular line intersects circle ?
                    if res:
                        edge_size = self.stairs[s + 1].step_depth / stair.get_length(side)
                        edge_t = 1 - edge_size
                    else:
                        # in this case, lerp z over one step
                        lerp_z = stair.step_height

            t_step, n_step = stair.n_posts(post_spacing, side, respect_edges)

            # space between posts
            sp = stair.get_length(side)
            # post size
            t_post = post_y / sp

            if s == n_stairs:
                n_step += 1
            for i in range(n_step):
                res_t = stair.get_lerp_vect([], side, i, t_step, respect_edges)
                # subs
                if s < n_stairs or i < n_step - 1:
                    res_t.append((i + 1) * t_step)
                for j in range(len(res_t) - 1):
                    t0 = res_t[j] + t_post
                    t1 = res_t[j + 1] - t_post
                    dt = t1 - t0
                    n_subs = int(sp * dt / sub_spacing)
                    if n_subs > 0:
                        t_subs = dt / n_subs
                        for k in range(1, n_subs):
                            t = t0 + k * t_subs
                            stair.get_lerp_vect(subs, side, 1, t0 + k * t_subs, False)
                            if t > edge_t:
                                n, dz, z0, z1 = subs[-1]
                                subs[-1] = n, dz, z0, z1 + (t - edge_t) / edge_size * stair.step_height
                            if lerp_z > 0:
                                n, dz, z0, z1 = subs[-1]
                                subs[-1] = n, dz, z0, z1 + t * stair.step_height

        for i, post in enumerate(subs):
            self.get_post(post, x, y, z, altitude, sub_offset_x, mat, verts, faces, matids, uvs, bottom=bottom)

    def make_post(self, height, step_depth, x, y, z, altitude, side, post_spacing, respect_edges, move_x, x_offset, mat,
        verts, faces, matids, uvs):
        n_steps = self.n_steps(step_depth)
        self.set_height(height / n_steps)
        l_posts = []
        n_stairs = len(self.stairs) - 1

        for s, stair in enumerate(self.stairs):
            if type(stair).__name__ in ['CurvedStair', 'CurvedLanding']:
                stair.l_arc, stair.l_t0, stair.l_t1, stair.l_tc = stair.set_offset(move_x - x_offset, stair.l_shape)
                stair.r_arc, stair.r_t0, stair.r_t1, stair.r_tc = stair.set_offset(move_x + x_offset, stair.r_shape)
            else:
                stair.l_line = stair.offset(move_x - x_offset)
                stair.r_line = stair.offset(move_x + x_offset)

            t_step, n_step = stair.n_posts(post_spacing, side, respect_edges)

            if s == n_stairs:
                n_step += 1
            for i in range(n_step):
                stair.get_lerp_vect(l_posts, side, i, t_step, respect_edges)

                if s == n_stairs and i == n_step - 1:
                    n, dz, z0, z1 = l_posts[-1]
                    l_posts[-1] = (n, dz, z0 - stair.step_height, z1)

        for i, post in enumerate(l_posts):
            self.get_post(post, x, y, z, altitude, 0, mat, verts, faces, matids, uvs)

    def make_panels(self, height, step_depth, x, z, post_y, altitude, side, post_spacing,
            panel_dist, respect_edges, move_x, x_offset, sub_offset_x, mat, verts, faces, matids, uvs):

        n_steps = self.n_steps(step_depth)
        self.set_height(height / n_steps)
        subs = []
        n_stairs = len(self.stairs) - 1

        if side == "LEFT":
            offset = move_x - x_offset
        else:
            offset = move_x + x_offset

        for s, stair in enumerate(self.stairs):

            is_circle = False
            if 'Curved' in type(stair).__name__:
                if side == "LEFT":
                    is_circle = stair.l_shape == "CIRCLE"
                    shape = stair.l_shape
                else:
                    is_circle = stair.r_shape == "CIRCLE"
                    shape = stair.r_shape
                stair.l_arc, stair.l_t0, stair.l_t1, stair.l_tc = stair.set_offset(offset, shape)
                stair.r_arc, stair.r_t0, stair.r_t1, stair.r_tc = stair.set_offset(offset, shape)
            else:
                stair.l_line = stair.offset(offset)
                stair.r_line = stair.offset(offset)

            # space between posts
            sp = stair.get_length(side)

            t_step, n_step = stair.n_posts(post_spacing, side, respect_edges)

            if is_circle and 'Curved' in type(stair).__name__:
                panel_da = abs(stair.da) / pi * 180 / n_step
                panel_step = max(1, int(panel_da / 6))
            else:
                panel_step = 1

            # post size
            t_post = (post_y + panel_dist) / sp

            if s == n_stairs:
                n_step += 1
            for i in range(n_step):
                res_t = stair.get_lerp_vect([], side, i, t_step, respect_edges)
                # subs
                if s < n_stairs or i < n_step - 1:
                    res_t.append((i + 1) * t_step)
                for j in range(len(res_t) - 1):
                    t0 = res_t[j] + t_post
                    t1 = res_t[j + 1] - t_post
                    dt = t1 - t0
                    t_curve = dt / panel_step
                    if dt > 0:
                        panel = []
                        for k in range(panel_step):
                            stair.get_lerp_vect(panel, side, 1, t_curve, True, t0_abs=t0 + k * t_curve)
                        stair.get_lerp_vect(panel, side, 1, t1, False)
                        subs.append(panel)
        for sub in subs:
            self.get_panel(sub, altitude, x, z, sub_offset_x, mat, verts, faces, matids, uvs)

    def make_part(self, height, step_depth, part_x, part_z, x_move, x_offset,
            z_offset, z_mode, steps_type, verts, faces, matids, uvs):

        params = [(stair.z_mode, stair.l_shape, stair.r_shape,
            stair.bottom_z, stair.steps_type) for stair in self.stairs]

        for stair in self.stairs:
            if x_offset > 0:
                stair.l_shape = stair.r_shape
            else:
                stair.r_shape = stair.l_shape
            stair.steps_type = steps_type
            stair.z_mode = "LINEAR"
            stair.bottom_z = part_z
            if 'Curved' in type(stair).__name__:
                stair.l_arc, stair.l_t0, stair.l_t1, stair.l_tc = \
                    stair.set_offset(x_move + x_offset + 0.5 * part_x, stair.l_shape)
                stair.r_arc, stair.r_t0, stair.r_t1, stair.r_tc = \
                    stair.set_offset(x_move + x_offset - 0.5 * part_x, stair.r_shape)
            else:
                stair.l_line = stair.offset(x_move + x_offset + 0.5 * part_x)
                stair.r_line = stair.offset(x_move + x_offset - 0.5 * part_x)
        n_steps = self.n_steps(step_depth)
        self.set_height(height / n_steps)
        for j, stair in enumerate(self.stairs):
            stair.z0 += z_offset + part_z
            stair.n_step *= 2
            stair.t_step /= 2
            stair.step_height /= 2
            for i in range(stair.n_step):
                stair.make_step(i, verts, faces, matids, uvs, nose_y=0)
            stair.n_step /= 2
            stair.t_step *= 2
            stair.step_height *= 2
            stair.z_mode = params[j][0]
            stair.l_shape = params[j][1]
            stair.r_shape = params[j][2]
            stair.bottom_z = params[j][3]
            stair.steps_type = params[j][4]
            stair.z0 -= z_offset + part_z

    def make_profile(self, profile, idmat, side, slice, height, step_depth,
            x_offset, z_offset, extend, verts, faces, matids, uvs):

        for stair in self.stairs:
            if 'Curved' in type(stair).__name__:
                stair.l_arc, stair.l_t0, stair.l_t1, stair.l_tc = stair.set_offset(-x_offset, stair.l_shape)
                stair.r_arc, stair.r_t0, stair.r_t1, stair.r_tc = stair.set_offset(x_offset, stair.r_shape)
            else:
                stair.l_line = stair.offset(-x_offset)
                stair.r_line = stair.offset(x_offset)

        n_steps = self.n_steps(step_depth)
        self.set_height(height / n_steps)

        n_stairs = len(self.stairs) - 1

        if n_stairs < 0:
            return

        sections = []
        sections.append([])

        # first step
        if extend != 0:
            t = -extend / self.stairs[0].length
            self.stairs[0].get_lerp_vect(sections[-1], side, 1, t, True)

        for s, stair in enumerate(self.stairs):
            n_step = 1
            is_circle = False

            if 'Curved' in type(stair).__name__:
                if side == "LEFT":
                    part = stair.l_arc
                    is_circle = stair.l_shape == "CIRCLE"
                else:
                    part = stair.r_arc
                    is_circle = stair.r_shape == "CIRCLE"
            else:
                if side == "LEFT":
                    part = stair.l_line
                else:
                    part = stair.r_line

            if is_circle:
                n_step = 3 * stair.n_step

            t_step = 1 / n_step

            last_t = 1.0
            do_last = True
            lerp_z = 0
            # last section 1 step before stair
            if 'Landing' in type(stair).__name__ and stair.next_type == 'STAIR':
                if not slice:
                    line = stair.normal(1).offset(self.stairs[s + 1].step_depth)
                    res, p, t_part = part.intersect(line)
                    # does perpendicular line intersects circle ?
                    if res:
                        last_t = 1 - self.stairs[s + 1].step_depth / stair.get_length(side)
                        if last_t < 0:
                            do_last = False
                    else:
                        # in this case, lerp z over one step
                        do_last = False
                        lerp_z = stair.step_height

            if s == n_stairs:
                n_step += 1

            for i in range(n_step):
                res_t = stair.get_lerp_vect(sections[-1], side, i, t_step, True, z_offset=lerp_z)
                # remove corner section
                for cur_t in res_t:
                    if cur_t > 0 and cur_t > last_t:
                        sections[-1] = sections[-1][:-1]

            # last section 1 step before next stair start
            if 'Landing' in type(stair).__name__ and stair.next_type == 'STAIR':
                if do_last:
                    stair.get_lerp_vect(sections[-1], side, 1, last_t, False)
                if slice:
                    sections.append([])
                    if extend > 0:
                        t = -extend / self.stairs[s + 1].length
                        self.stairs[s + 1].get_lerp_vect(sections[-1], side, 1, t, True)

        t = 1 + extend / self.stairs[-1].length
        self.stairs[-1].get_lerp_vect(sections[-1], side, 1, t, True)

        for cur_sect in sections:
            user_path_verts = len(cur_sect)
            f = len(verts)
            if user_path_verts > 0:
                user_path_uv_v = []
                n, dz, z0, z1 = cur_sect[-1]
                cur_sect[-1] = (n, dz, z0 - stair.step_height, z1)
                n_sections = user_path_verts - 1
                n, dz, zs, zl = cur_sect[0]
                p0 = n.p
                v0 = n.v.normalized()
                for s, section in enumerate(cur_sect):
                    n, dz, zs, zl = section
                    p1 = n.p
                    if s < n_sections:
                        v1 = cur_sect[s + 1][0].v.normalized()
                    dir = (v0 + v1).normalized()
                    scale = 1 / cos(0.5 * acos(min(1, max(-1, v0 * v1))))
                    for p in profile:
                        x, y = n.p + scale * p.x * dir
                        z = zl + p.y + z_offset
                        verts.append((x, y, z))
                    if s > 0:
                        user_path_uv_v.append((p1 - p0).length)
                    p0 = p1
                    v0 = v1

                # build faces using Panel
                lofter = Lofter(
                    # closed_shape, index, x, y, idmat
                    True,
                    [i for i in range(len(profile))],
                    [p.x for p in profile],
                    [p.y for p in profile],
                    [idmat for i in range(len(profile))],
                    closed_path=False,
                    user_path_uv_v=user_path_uv_v,
                    user_path_verts=user_path_verts
                    )
                faces += lofter.faces(16, offset=f, path_type='USER_DEFINED')
                matids += lofter.mat(16, idmat, idmat, path_type='USER_DEFINED')
                v = Vector((0, 0))
                uvs += lofter.uv(16, v, v, v, v, 0, v, 0, 0, path_type='USER_DEFINED')

    def set_matids(self, id_materials):
        for stair in self.stairs:
            stair.set_matids(id_materials)


def update(self, context):
    self.update(context)


def update_manipulators(self, context):
    self.update(context, manipulable_refresh=True)


def update_preset(self, context):
    auto_update = self.auto_update
    self.auto_update = False
    if self.presets == 'STAIR_I':
        self.n_parts = 1
        self.update_parts()
        self.parts[0].type = 'S_STAIR'
    elif self.presets == 'STAIR_L':
        self.n_parts = 3
        self.update_parts()
        self.parts[0].type = 'S_STAIR'
        self.parts[1].type = 'C_STAIR'
        self.parts[2].type = 'S_STAIR'
        self.da = pi / 2
    elif self.presets == 'STAIR_U':
        self.n_parts = 3
        self.update_parts()
        self.parts[0].type = 'S_STAIR'
        self.parts[1].type = 'D_STAIR'
        self.parts[2].type = 'S_STAIR'
        self.da = pi
    elif self.presets == 'STAIR_O':
        self.n_parts = 2
        self.update_parts()
        self.parts[0].type = 'D_STAIR'
        self.parts[1].type = 'D_STAIR'
        self.da = pi
    # keep auto_update state same
    # prevent unwanted load_preset update
    self.auto_update = auto_update


materials_enum = (
            ('0', 'Ceiling', '', 0),
            ('1', 'White', '', 1),
            ('2', 'Concrete', '', 2),
            ('3', 'Wood', '', 3),
            ('4', 'Metal', '', 4),
            ('5', 'Glass', '', 5)
            )


class archipack_stair_material(PropertyGroup):
    index = EnumProperty(
        items=materials_enum,
        default='4',
        update=update
        )

    def find_datablock_in_selection(self, context):
        """
            find witch selected object this instance belongs to
            provide support for "copy to selected"
        """
        selected = [o for o in context.selected_objects]
        for o in selected:
            props = archipack_stair.datablock(o)
            if props:
                for part in props.rail_mat:
                    if part == self:
                        return props
        return None

    def update(self, context):
        props = self.find_datablock_in_selection(context)
        if props is not None:
            props.update(context)


class archipack_stair_part(PropertyGroup):
    type = EnumProperty(
            items=(
                ('S_STAIR', 'Straight stair', '', 0),
                ('C_STAIR', 'Curved stair', '', 1),
                ('D_STAIR', 'Dual Curved stair', '', 2),
                ('S_LANDING', 'Straight landing', '', 3),
                ('C_LANDING', 'Curved landing', '', 4),
                ('D_LANDING', 'Dual Curved landing', '', 5)
                ),
            default='S_STAIR',
            update=update_manipulators
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
            min=0.01,
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
    left_shape = EnumProperty(
            items=(
                ('RECTANGLE', 'Straight', '', 0),
                ('CIRCLE', 'Curved ', '', 1)
                ),
            default='RECTANGLE',
            update=update
            )
    right_shape = EnumProperty(
            items=(
                ('RECTANGLE', 'Straight', '', 0),
                ('CIRCLE', 'Curved ', '', 1)
                ),
            default='RECTANGLE',
            update=update
            )
    manipulators = CollectionProperty(type=archipack_manipulator)

    def find_datablock_in_selection(self, context):
        """
            find witch selected object this instance belongs to
            provide support for "copy to selected"
        """
        selected = [o for o in context.selected_objects]
        for o in selected:
            props = archipack_stair.datablock(o)
            if props:
                for part in props.parts:
                    if part == self:
                        return props
        return None

    def update(self, context, manipulable_refresh=False):
        props = self.find_datablock_in_selection(context)
        if props is not None:
            props.update(context, manipulable_refresh)

    def draw(self, layout, context, index, user_mode):
        if user_mode:
            box = layout.box()
            row = box.row()
            row.prop(self, "type", text=str(index + 1))
            if self.type in ['C_STAIR', 'C_LANDING', 'D_STAIR', 'D_LANDING']:
                row = box.row()
                row.prop(self, "radius")
                row = box.row()
                row.prop(self, "da")
            else:
                row = box.row()
                row.prop(self, "length")
            if self.type in ['C_STAIR', 'C_LANDING', 'D_STAIR', 'D_LANDING']:
                row = box.row(align=True)
                row.prop(self, "left_shape", text="")
                row.prop(self, "right_shape", text="")
        else:
            if self.type in ['S_STAIR', 'S_LANDING']:
                box = layout.box()
                row = box.row()
                row.prop(self, "length")


class archipack_stair(ArchipackObject, Manipulable, PropertyGroup):

    parts = CollectionProperty(type=archipack_stair_part)
    n_parts = IntProperty(
            name="Parts",
            min=1,
            max=32,
            default=1, update=update_manipulators
            )
    step_depth = FloatProperty(
            name="Going",
            min=0.2,
            default=0.25,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    width = FloatProperty(
            name="Width",
            min=0.01,
            default=1.2,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    height = FloatProperty(
            name="Height",
            min=0.1,
            default=2.4, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    nose_y = FloatProperty(
            name="Depth",
            min=0.0,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    x_offset = FloatProperty(
            name="Offset",
            default=0.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    nose_z = FloatProperty(
            name="Height",
            min=0.001,
            default=0.03, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    bottom_z = FloatProperty(
            name="Thickness",
            min=0.001,
            default=0.03, precision=2, step=1,
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
    da = FloatProperty(
            name="Angle",
            min=-pi,
            max=pi,
            default=pi / 2,
            subtype='ANGLE', unit='ROTATION',
            update=update
            )
    total_angle = FloatProperty(
            name="Angle",
            min=-50 * pi,
            max=50 * pi,
            default=2 * pi,
            subtype='ANGLE', unit='ROTATION',
            update=update
            )
    steps_type = EnumProperty(
            name="Steps",
            items=(
                ('CLOSED', 'Closed', '', 0),
                ('FULL', 'Full height', '', 1),
                ('OPEN', 'Open ', '', 2)
                ),
            default='CLOSED',
            update=update
            )
    nose_type = EnumProperty(
            name="Nosing",
            items=(
                ('STRAIGHT', 'Straight', '', 0),
                ('OBLIQUE', 'Oblique', '', 1),
                ),
            default='STRAIGHT',
            update=update
            )
    left_shape = EnumProperty(
            items=(
                ('RECTANGLE', 'Straight', '', 0),
                ('CIRCLE', 'Curved ', '', 1)
                ),
            default='RECTANGLE',
            update=update
            )
    right_shape = EnumProperty(
            items=(
                ('RECTANGLE', 'Straight', '', 0),
                ('CIRCLE', 'Curved ', '', 1)
                ),
            default='RECTANGLE',
            update=update
            )
    z_mode = EnumProperty(
            name="Interp z",
            items=(
                ('STANDARD', 'Standard', '', 0),
                ('LINEAR', 'Bottom Linear', '', 1),
                ('LINEAR_TOP', 'All Linear', '', 2)
                ),
            default='STANDARD',
            update=update
            )
    presets = EnumProperty(
            items=(
                ('STAIR_I', 'I stair', '', 0),
                ('STAIR_L', 'L stair', '', 1),
                ('STAIR_U', 'U stair', '', 2),
                ('STAIR_O', 'O stair', '', 3),
                ('STAIR_USER', 'User defined stair', '', 4),
                ),
            default='STAIR_I', update=update_preset
            )
    left_post = BoolProperty(
            name='left',
            default=True,
            update=update
            )
    right_post = BoolProperty(
            name='right',
            default=True,
            update=update
            )
    post_spacing = FloatProperty(
            name="Spacing",
            min=0.1,
            default=1.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    post_x = FloatProperty(
            name="Width",
            min=0.001,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    post_y = FloatProperty(
            name="Length",
            min=0.001,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    post_z = FloatProperty(
            name="Height",
            min=0.001,
            default=1, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    post_alt = FloatProperty(
            name="Altitude",
            min=-100,
            default=0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    post_offset_x = FloatProperty(
            name="Offset",
            min=-100.0, max=100,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    post_corners = BoolProperty(
            name="Only on edges",
            update=update,
            default=False
            )
    user_defined_post_enable = BoolProperty(
            name="User",
            update=update,
            default=True
            )
    user_defined_post = StringProperty(
            name="User defined",
            update=update
            )
    idmat_post = EnumProperty(
            name="Post",
            items=materials_enum,
            default='4',
            update=update
            )
    left_subs = BoolProperty(
            name='left',
            default=False,
            update=update
            )
    right_subs = BoolProperty(
            name='right',
            default=False,
            update=update
            )
    subs_spacing = FloatProperty(
            name="Spacing",
            min=0.05,
            default=0.10, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    subs_x = FloatProperty(
            name="Width",
            min=0.001,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    subs_y = FloatProperty(
            name="Length",
            min=0.001,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    subs_z = FloatProperty(
            name="Height",
            min=0.001,
            default=1, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    subs_alt = FloatProperty(
            name="Altitude",
            min=-100,
            default=0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    subs_offset_x = FloatProperty(
            name="Offset",
            min=-100.0, max=100,
            default=0.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    subs_bottom = EnumProperty(
            name="Bottom",
            items=(
                ('STEP', 'Follow step', '', 0),
                ('LINEAR', 'Linear', '', 1),
                ),
            default='STEP',
            update=update
            )
    user_defined_subs_enable = BoolProperty(
            name="User",
            update=update,
            default=True
            )
    user_defined_subs = StringProperty(
            name="User defined",
            update=update
            )
    idmat_subs = EnumProperty(
            name="Subs",
            items=materials_enum,
            default='4',
            update=update
            )
    left_panel = BoolProperty(
            name='left',
            default=True,
            update=update
            )
    right_panel = BoolProperty(
            name='right',
            default=True,
            update=update
            )
    panel_alt = FloatProperty(
            name="Altitude",
            default=0.25, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    panel_x = FloatProperty(
            name="Width",
            min=0.001,
            default=0.01, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    panel_z = FloatProperty(
            name="Height",
            min=0.001,
            default=0.6, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    panel_dist = FloatProperty(
            name="Spacing",
            min=0.001,
            default=0.05, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    panel_offset_x = FloatProperty(
            name="Offset",
            default=0.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    idmat_panel = EnumProperty(
            name="Panels",
            items=materials_enum,
            default='5',
            update=update
            )
    left_rail = BoolProperty(
            name="left",
            update=update,
            default=False
            )
    right_rail = BoolProperty(
            name="right",
            update=update,
            default=False
            )
    rail_n = IntProperty(
            name="#",
            default=1,
            min=0,
            max=31,
            update=update
            )
    rail_x = FloatVectorProperty(
            name="Width",
            default=[
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05
            ],
            size=31,
            min=0.001,
            precision=2, step=1,
            unit='LENGTH',
            update=update
            )
    rail_z = FloatVectorProperty(
            name="Height",
            default=[
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05,
                0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05
            ],
            size=31,
            min=0.001,
            precision=2, step=1,
            unit='LENGTH',
            update=update
            )
    rail_offset = FloatVectorProperty(
            name="Offset",
            default=[
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0
            ],
            size=31,
            precision=2, step=1,
            unit='LENGTH',
            update=update
            )
    rail_alt = FloatVectorProperty(
            name="Altitude",
            default=[
                1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
            ],
            size=31,
            precision=2, step=1,
            unit='LENGTH',
            update=update
            )
    rail_mat = CollectionProperty(type=archipack_stair_material)

    left_handrail = BoolProperty(
            name="left",
            update=update,
            default=True
            )
    right_handrail = BoolProperty(
            name="right",
            update=update,
            default=True
            )
    handrail_offset = FloatProperty(
            name="Offset",
            default=0.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    handrail_alt = FloatProperty(
            name="Altitude",
            default=1.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    handrail_extend = FloatProperty(
            name="Extend",
            default=0.1, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    handrail_slice_left = BoolProperty(
            name='Slice',
            default=True,
            update=update
            )
    handrail_slice_right = BoolProperty(
            name='Slice',
            default=True,
            update=update
            )
    handrail_profil = EnumProperty(
            name="Profil",
            items=(
                ('SQUARE', 'Square', '', 0),
                ('CIRCLE', 'Circle', '', 1),
                ('COMPLEX', 'Circle over square', '', 2)
                ),
            default='SQUARE',
            update=update
            )
    handrail_x = FloatProperty(
            name="Width",
            min=0.001,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    handrail_y = FloatProperty(
            name="Height",
            min=0.001,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    handrail_radius = FloatProperty(
            name="Radius",
            min=0.001,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )

    left_string = BoolProperty(
            name="left",
            update=update,
            default=False
            )
    right_string = BoolProperty(
            name="right",
            update=update,
            default=False
            )
    string_x = FloatProperty(
            name="Width",
            min=-100.0,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    string_z = FloatProperty(
            name="Height",
            default=0.3, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    string_offset = FloatProperty(
            name="Offset",
            default=0.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    string_alt = FloatProperty(
            name="Altitude",
            default=-0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )

    idmat_bottom = EnumProperty(
            name="Bottom",
            items=materials_enum,
            default='1',
            update=update
            )
    idmat_raise = EnumProperty(
            name="Raise",
            items=materials_enum,
            default='1',
            update=update
            )
    idmat_step_front = EnumProperty(
            name="Step front",
            items=materials_enum,
            default='3',
            update=update
            )
    idmat_top = EnumProperty(
            name="Top",
            items=materials_enum,
            default='3',
            update=update
            )
    idmat_side = EnumProperty(
            name="Side",
            items=materials_enum,
            default='1',
            update=update
            )
    idmat_step_side = EnumProperty(
            name="Step Side",
            items=materials_enum,
            default='3',
            update=update
            )
    idmat_handrail = EnumProperty(
            name="Handrail",
            items=materials_enum,
            default='3',
            update=update
            )
    idmat_string = EnumProperty(
            name="String",
            items=materials_enum,
            default='3',
            update=update
            )

    # UI layout related
    parts_expand = BoolProperty(
            default=False
            )
    steps_expand = BoolProperty(
            default=False
            )
    rail_expand = BoolProperty(
            default=False
            )
    idmats_expand = BoolProperty(
            default=False
            )
    handrail_expand = BoolProperty(
            default=False
            )
    string_expand = BoolProperty(
            default=False
            )
    post_expand = BoolProperty(
            default=False
            )
    panel_expand = BoolProperty(
            default=False
            )
    subs_expand = BoolProperty(
            default=False
            )

    auto_update = BoolProperty(
            options={'SKIP_SAVE'},
            default=True,
            update=update_manipulators
            )

    def setup_manipulators(self):

        if len(self.manipulators) == 0:
            s = self.manipulators.add()
            s.prop1_name = "width"
            s = self.manipulators.add()
            s.prop1_name = "height"
            s.normal = Vector((0, 1, 0))

        for i in range(self.n_parts):
            p = self.parts[i]
            n_manips = len(p.manipulators)
            if n_manips < 1:
                m = p.manipulators.add()
                m.type_key = 'SIZE'
                m.prop1_name = 'length'

    def update_parts(self):

        # remove rails materials
        for i in range(len(self.rail_mat), self.rail_n, -1):
            self.rail_mat.remove(i - 1)

        # add rails
        for i in range(len(self.rail_mat), self.rail_n):
            self.rail_mat.add()

        # remove parts
        for i in range(len(self.parts), self.n_parts, -1):
            self.parts.remove(i - 1)

        # add parts
        for i in range(len(self.parts), self.n_parts):
            self.parts.add()

        self.setup_manipulators()

    def update(self, context, manipulable_refresh=False):

        o = self.find_in_selection(context, self.auto_update)

        if o is None:
            return

        # clean up manipulators before any data model change
        if manipulable_refresh:
            self.manipulable_disable(context)

        self.update_parts()

        center = Vector((0, 0))
        verts = []
        faces = []
        matids = []
        uvs = []
        id_materials = [int(self.idmat_top), int(self.idmat_step_front), int(self.idmat_raise),
                        int(self.idmat_side), int(self.idmat_bottom), int(self.idmat_step_side)]

        # depth at bottom
        bottom_z = self.bottom_z
        if self.steps_type == 'OPEN':
            # depth at front
            bottom_z = self.nose_z

        width_left = 0.5 * self.width - self.x_offset
        width_right = 0.5 * self.width + self.x_offset

        self.manipulators[0].set_pts([(-width_left, 0, 0), (width_right, 0, 0), (1, 0, 0)])
        self.manipulators[1].set_pts([(0, 0, 0), (0, 0, self.height), (1, 0, 0)])

        g = StairGenerator(self.parts)
        if self.presets == 'STAIR_USER':
            for part in self.parts:
                g.add_part(part.type, self.steps_type, self.nose_type, self.z_mode, self.nose_z,
                        bottom_z, center, max(width_left + 0.01, width_right + 0.01, part.radius), part.da,
                        width_left, width_right, part.length, part.left_shape, part.right_shape)

        elif self.presets == 'STAIR_O':
            n_parts = max(1, int(round(abs(self.total_angle) / pi, 0)))
            if self.total_angle > 0:
                dir = 1
            else:
                dir = -1
            last_da = self.total_angle - dir * (n_parts - 1) * pi
            if dir * last_da > pi:
                n_parts += 1
                last_da -= dir * pi
            abs_last = dir * last_da

            for part in range(n_parts - 1):
                g.add_part('D_STAIR', self.steps_type, self.nose_type, self.z_mode, self.nose_z,
                            bottom_z, center, max(width_left + 0.01, width_right + 0.01, self.radius), dir * pi,
                            width_left, width_right, 1.0, self.left_shape, self.right_shape)
            if round(abs_last, 2) > 0:
                if abs_last > pi / 2:
                    g.add_part('C_STAIR', self.steps_type, self.nose_type, self.z_mode, self.nose_z,
                            bottom_z, center, max(width_left + 0.01, width_right + 0.01, self.radius),
                            dir * pi / 2,
                            width_left, width_right, 1.0, self.left_shape, self.right_shape)
                    g.add_part('C_STAIR', self.steps_type, self.nose_type, self.z_mode, self.nose_z,
                            bottom_z, center, max(width_left + 0.01, width_right + 0.01, self.radius),
                            last_da - dir * pi / 2,
                            width_left, width_right, 1.0, self.left_shape, self.right_shape)
                else:
                    g.add_part('C_STAIR', self.steps_type, self.nose_type, self.z_mode, self.nose_z,
                            bottom_z, center, max(width_left + 0.01, width_right + 0.01, self.radius), last_da,
                            width_left, width_right, 1.0, self.left_shape, self.right_shape)
        else:
            # STAIR_L STAIR_I STAIR_U
            for part in self.parts:
                g.add_part(part.type, self.steps_type, self.nose_type, self.z_mode, self.nose_z,
                            bottom_z, center, max(width_left + 0.01, width_right + 0.01, self.radius), self.da,
                            width_left, width_right, part.length, self.left_shape, self.right_shape)

        # Stair basis
        g.set_matids(id_materials)
        g.make_stair(self.height, self.step_depth, verts, faces, matids, uvs, nose_y=self.nose_y)

        # Ladder
        offset_x = 0.5 * self.width - self.post_offset_x
        post_spacing = self.post_spacing
        if self.post_corners:
            post_spacing = 10000

        if self.user_defined_post_enable:
            # user defined posts
            user_def_post = context.scene.objects.get(self.user_defined_post)
            if user_def_post is not None and user_def_post.type == 'MESH':
                g.setup_user_defined_post(user_def_post, self.post_x, self.post_y, self.post_z)

        if self.left_post:
            g.make_post(self.height, self.step_depth, 0.5 * self.post_x, 0.5 * self.post_y,
                    self.post_z, self.post_alt, 'LEFT', post_spacing, self.post_corners,
                    self.x_offset, offset_x, int(self.idmat_post), verts, faces, matids, uvs)

        if self.right_post:
            g.make_post(self.height, self.step_depth, 0.5 * self.post_x, 0.5 * self.post_y,
                    self.post_z, self.post_alt, 'RIGHT', post_spacing, self.post_corners,
                    self.x_offset, offset_x, int(self.idmat_post), verts, faces, matids, uvs)

        # reset user def posts
        g.user_defined_post = None

        # user defined subs
        if self.user_defined_subs_enable:
            user_def_subs = context.scene.objects.get(self.user_defined_subs)
            if user_def_subs is not None and user_def_subs.type == 'MESH':
                g.setup_user_defined_post(user_def_subs, self.subs_x, self.subs_y, self.subs_z)

        if self.left_subs:
            g.make_subs(self.height, self.step_depth, 0.5 * self.subs_x, 0.5 * self.subs_y,
                    self.subs_z, 0.5 * self.post_y, self.subs_alt, self.subs_bottom, 'LEFT',
                    self.handrail_slice_left, post_spacing, self.subs_spacing, self.post_corners,
                    self.x_offset, offset_x, -self.subs_offset_x, int(self.idmat_subs), verts, faces, matids, uvs)

        if self.right_subs:
            g.make_subs(self.height, self.step_depth, 0.5 * self.subs_x, 0.5 * self.subs_y,
                    self.subs_z, 0.5 * self.post_y, self.subs_alt, self.subs_bottom, 'RIGHT',
                    self.handrail_slice_right, post_spacing, self.subs_spacing, self.post_corners,
                    self.x_offset, offset_x, self.subs_offset_x, int(self.idmat_subs), verts, faces, matids, uvs)

        g.user_defined_post = None

        if self.left_panel:
            g.make_panels(self.height, self.step_depth, 0.5 * self.panel_x, self.panel_z, 0.5 * self.post_y,
                    self.panel_alt, 'LEFT', post_spacing, self.panel_dist, self.post_corners,
                    self.x_offset, offset_x, -self.panel_offset_x, int(self.idmat_panel), verts, faces, matids, uvs)

        if self.right_panel:
            g.make_panels(self.height, self.step_depth, 0.5 * self.panel_x, self.panel_z, 0.5 * self.post_y,
                    self.panel_alt, 'RIGHT', post_spacing, self.panel_dist, self.post_corners,
                    self.x_offset, offset_x, self.panel_offset_x, int(self.idmat_panel), verts, faces, matids, uvs)

        if self.right_rail:
            for i in range(self.rail_n):
                id_materials = [int(self.rail_mat[i].index) for j in range(6)]
                g.set_matids(id_materials)
                g.make_part(self.height, self.step_depth, self.rail_x[i], self.rail_z[i],
                        self.x_offset, offset_x + self.rail_offset[i],
                        self.rail_alt[i], 'LINEAR', 'CLOSED', verts, faces, matids, uvs)

        if self.left_rail:
            for i in range(self.rail_n):
                id_materials = [int(self.rail_mat[i].index) for j in range(6)]
                g.set_matids(id_materials)
                g.make_part(self.height, self.step_depth, self.rail_x[i], self.rail_z[i],
                        self.x_offset, -offset_x - self.rail_offset[i],
                        self.rail_alt[i], 'LINEAR', 'CLOSED', verts, faces, matids, uvs)

        if self.handrail_profil == 'COMPLEX':
            sx = self.handrail_x
            sy = self.handrail_y
            handrail = [Vector((sx * x, sy * y)) for x, y in [
            (-0.28, 1.83), (-0.355, 1.77), (-0.415, 1.695), (-0.46, 1.605), (-0.49, 1.51), (-0.5, 1.415),
            (-0.49, 1.315), (-0.46, 1.225), (-0.415, 1.135), (-0.355, 1.06), (-0.28, 1.0), (-0.255, 0.925),
            (-0.33, 0.855), (-0.5, 0.855), (-0.5, 0.0), (0.5, 0.0), (0.5, 0.855), (0.33, 0.855), (0.255, 0.925),
            (0.28, 1.0), (0.355, 1.06), (0.415, 1.135), (0.46, 1.225), (0.49, 1.315), (0.5, 1.415),
            (0.49, 1.51), (0.46, 1.605), (0.415, 1.695), (0.355, 1.77), (0.28, 1.83), (0.19, 1.875),
            (0.1, 1.905), (0.0, 1.915), (-0.095, 1.905), (-0.19, 1.875)]]

        elif self.handrail_profil == 'SQUARE':
            x = 0.5 * self.handrail_x
            y = self.handrail_y
            handrail = [Vector((-x, y)), Vector((-x, 0)), Vector((x, 0)), Vector((x, y))]
        elif self.handrail_profil == 'CIRCLE':
            r = self.handrail_radius
            handrail = [Vector((r * sin(0.1 * -a * pi), r * (0.5 + cos(0.1 * -a * pi)))) for a in range(0, 20)]

        if self.right_handrail:
            g.make_profile(handrail, int(self.idmat_handrail), "RIGHT", self.handrail_slice_right,
                self.height, self.step_depth, self.x_offset + offset_x + self.handrail_offset,
                self.handrail_alt, self.handrail_extend, verts, faces, matids, uvs)

        if self.left_handrail:
            g.make_profile(handrail, int(self.idmat_handrail), "LEFT", self.handrail_slice_left,
                self.height, self.step_depth, -self.x_offset + offset_x + self.handrail_offset,
                self.handrail_alt, self.handrail_extend, verts, faces, matids, uvs)

        w = 0.5 * self.string_x
        h = self.string_z
        string = [Vector((-w, 0)), Vector((w, 0)), Vector((w, h)), Vector((-w, h))]

        if self.right_string:
            g.make_profile(string, int(self.idmat_string), "RIGHT", False, self.height, self.step_depth,
                self.x_offset + 0.5 * self.width + self.string_offset,
                self.string_alt, 0, verts, faces, matids, uvs)

        if self.left_string:
            g.make_profile(string, int(self.idmat_string), "LEFT", False, self.height, self.step_depth,
                -self.x_offset + 0.5 * self.width + self.string_offset,
                self.string_alt, 0, verts, faces, matids, uvs)

        bmed.buildmesh(context, o, verts, faces, matids=matids, uvs=uvs, weld=True, clean=True)

        # enable manipulators rebuild
        if manipulable_refresh:
            self.manipulable_refresh = True

        self.restore_context(context)

    def manipulable_setup(self, context):
        """
            TODO: Implement the setup part as per parent object basis

            self.manipulable_disable(context)
            o = context.active_object
            for m in self.manipulators:
                self.manip_stack.append(m.setup(context, o, self))

        """
        self.manipulable_disable(context)
        o = context.active_object

        self.setup_manipulators()

        if self.presets is not 'STAIR_O':
            for i, part in enumerate(self.parts):
                if i >= self.n_parts:
                    break
                if "S_" in part.type or self.presets in ['STAIR_USER']:
                    for j, m in enumerate(part.manipulators):
                        self.manip_stack.append(m.setup(context, o, part))

        if self.presets in ['STAIR_U', 'STAIR_L']:
            self.manip_stack.append(self.parts[1].manipulators[0].setup(context, o, self))

        for m in self.manipulators:
            self.manip_stack.append(m.setup(context, o, self))


class ARCHIPACK_PT_stair(Panel):
    bl_idname = "ARCHIPACK_PT_stair"
    bl_label = "Stair"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    # bl_context = 'object'
    bl_category = 'ArchiPack'

    @classmethod
    def poll(cls, context):
        return archipack_stair.filter(context.active_object)

    def draw(self, context):
        prop = archipack_stair.datablock(context.active_object)
        if prop is None:
            return
        scene = context.scene
        layout = self.layout
        row = layout.row(align=True)
        row.operator('archipack.stair_manipulate', icon='HAND')
        row = layout.row(align=True)
        row.prop(prop, 'presets', text="")
        box = layout.box()
        # box.label(text="Styles")
        row = box.row(align=True)
        # row.menu("ARCHIPACK_MT_stair_preset", text=bpy.types.ARCHIPACK_MT_stair_preset.bl_label)
        row.operator("archipack.stair_preset_menu", text=bpy.types.ARCHIPACK_OT_stair_preset_menu.bl_label)
        row.operator("archipack.stair_preset", text="", icon='ZOOMIN')
        row.operator("archipack.stair_preset", text="", icon='ZOOMOUT').remove_active = True
        box = layout.box()
        box.prop(prop, 'width')
        box.prop(prop, 'height')
        box.prop(prop, 'bottom_z')
        box.prop(prop, 'x_offset')
        # box.prop(prop, 'z_mode')
        box = layout.box()
        row = box.row()
        if prop.parts_expand:
            row.prop(prop, 'parts_expand', icon="TRIA_DOWN", icon_only=True, text="Parts", emboss=False)
            if prop.presets == 'STAIR_USER':
                box.prop(prop, 'n_parts')
            if prop.presets != 'STAIR_USER':
                row = box.row(align=True)
                row.prop(prop, "left_shape", text="")
                row.prop(prop, "right_shape", text="")
                row = box.row()
                row.prop(prop, "radius")
                row = box.row()
                if prop.presets == 'STAIR_O':
                    row.prop(prop, 'total_angle')
                else:
                    row.prop(prop, 'da')
            if prop.presets != 'STAIR_O':
                for i, part in enumerate(prop.parts):
                    part.draw(layout, context, i, prop.presets == 'STAIR_USER')
        else:
            row.prop(prop, 'parts_expand', icon="TRIA_RIGHT", icon_only=True, text="Parts", emboss=False)

        box = layout.box()
        row = box.row()
        if prop.steps_expand:
            row.prop(prop, 'steps_expand', icon="TRIA_DOWN", icon_only=True, text="Steps", emboss=False)
            box.prop(prop, 'steps_type')
            box.prop(prop, 'step_depth')
            box.prop(prop, 'nose_type')
            box.prop(prop, 'nose_z')
            box.prop(prop, 'nose_y')
        else:
            row.prop(prop, 'steps_expand', icon="TRIA_RIGHT", icon_only=True, text="Steps", emboss=False)

        box = layout.box()
        row = box.row(align=True)
        if prop.handrail_expand:
            row.prop(prop, 'handrail_expand', icon="TRIA_DOWN", icon_only=True, text="Handrail", emboss=False)
        else:
            row.prop(prop, 'handrail_expand', icon="TRIA_RIGHT", icon_only=True, text="Handrail", emboss=False)

        row.prop(prop, 'left_handrail')
        row.prop(prop, 'right_handrail')

        if prop.handrail_expand:
            box.prop(prop, 'handrail_alt')
            box.prop(prop, 'handrail_offset')
            box.prop(prop, 'handrail_extend')
            box.prop(prop, 'handrail_profil')
            if prop.handrail_profil != 'CIRCLE':
                box.prop(prop, 'handrail_x')
                box.prop(prop, 'handrail_y')
            else:
                box.prop(prop, 'handrail_radius')
            row = box.row(align=True)
            row.prop(prop, 'handrail_slice_left')
            row.prop(prop, 'handrail_slice_right')

        box = layout.box()
        row = box.row(align=True)
        if prop.string_expand:
            row.prop(prop, 'string_expand', icon="TRIA_DOWN", icon_only=True, text="String", emboss=False)
        else:
            row.prop(prop, 'string_expand', icon="TRIA_RIGHT", icon_only=True, text="String", emboss=False)
        row.prop(prop, 'left_string')
        row.prop(prop, 'right_string')
        if prop.string_expand:
            box.prop(prop, 'string_x')
            box.prop(prop, 'string_z')
            box.prop(prop, 'string_alt')
            box.prop(prop, 'string_offset')

        box = layout.box()
        row = box.row(align=True)
        if prop.post_expand:
            row.prop(prop, 'post_expand', icon="TRIA_DOWN", icon_only=True, text="Post", emboss=False)
        else:
            row.prop(prop, 'post_expand', icon="TRIA_RIGHT", icon_only=True, text="Post", emboss=False)
        row.prop(prop, 'left_post')
        row.prop(prop, 'right_post')
        if prop.post_expand:
            box.prop(prop, 'post_corners')
            if not prop.post_corners:
                box.prop(prop, 'post_spacing')
            box.prop(prop, 'post_x')
            box.prop(prop, 'post_y')
            box.prop(prop, 'post_z')
            box.prop(prop, 'post_alt')
            box.prop(prop, 'post_offset_x')
            row = box.row(align=True)
            row.prop(prop, 'user_defined_post_enable', text="")
            row.prop_search(prop, "user_defined_post", scene, "objects", text="")

        box = layout.box()
        row = box.row(align=True)
        if prop.subs_expand:
            row.prop(prop, 'subs_expand', icon="TRIA_DOWN", icon_only=True, text="Subs", emboss=False)
        else:
            row.prop(prop, 'subs_expand', icon="TRIA_RIGHT", icon_only=True, text="Subs", emboss=False)

        row.prop(prop, 'left_subs')
        row.prop(prop, 'right_subs')
        if prop.subs_expand:
            box.prop(prop, 'subs_spacing')
            box.prop(prop, 'subs_x')
            box.prop(prop, 'subs_y')
            box.prop(prop, 'subs_z')
            box.prop(prop, 'subs_alt')
            box.prop(prop, 'subs_offset_x')
            box.prop(prop, 'subs_bottom')
            row = box.row(align=True)
            row.prop(prop, 'user_defined_subs_enable', text="")
            row.prop_search(prop, "user_defined_subs", scene, "objects", text="")

        box = layout.box()
        row = box.row(align=True)
        if prop.panel_expand:
            row.prop(prop, 'panel_expand', icon="TRIA_DOWN", icon_only=True, text="Panels", emboss=False)
        else:
            row.prop(prop, 'panel_expand', icon="TRIA_RIGHT", icon_only=True, text="Panels", emboss=False)
        row.prop(prop, 'left_panel')
        row.prop(prop, 'right_panel')
        if prop.panel_expand:
            box.prop(prop, 'panel_dist')
            box.prop(prop, 'panel_x')
            box.prop(prop, 'panel_z')
            box.prop(prop, 'panel_alt')
            box.prop(prop, 'panel_offset_x')

        box = layout.box()
        row = box.row(align=True)
        if prop.rail_expand:
            row.prop(prop, 'rail_expand', icon="TRIA_DOWN", icon_only=True, text="Rails", emboss=False)
        else:
            row.prop(prop, 'rail_expand', icon="TRIA_RIGHT", icon_only=True, text="Rails", emboss=False)
        row.prop(prop, 'left_rail')
        row.prop(prop, 'right_rail')
        if prop.rail_expand:
            box.prop(prop, 'rail_n')
            for i in range(prop.rail_n):
                box = layout.box()
                box.label(text="Rail " + str(i + 1))
                box.prop(prop, 'rail_x', index=i)
                box.prop(prop, 'rail_z', index=i)
                box.prop(prop, 'rail_alt', index=i)
                box.prop(prop, 'rail_offset', index=i)
                box.prop(prop.rail_mat[i], 'index', text="")

        box = layout.box()
        row = box.row()

        if prop.idmats_expand:
            row.prop(prop, 'idmats_expand', icon="TRIA_DOWN", icon_only=True, text="Materials", emboss=False)
            box.prop(prop, 'idmat_top')
            box.prop(prop, 'idmat_side')
            box.prop(prop, 'idmat_bottom')
            box.prop(prop, 'idmat_step_side')
            box.prop(prop, 'idmat_step_front')
            box.prop(prop, 'idmat_raise')
            box.prop(prop, 'idmat_handrail')
            box.prop(prop, 'idmat_panel')
            box.prop(prop, 'idmat_post')
            box.prop(prop, 'idmat_subs')
            box.prop(prop, 'idmat_string')
        else:
            row.prop(prop, 'idmats_expand', icon="TRIA_RIGHT", icon_only=True, text="Materials", emboss=False)


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------


class ARCHIPACK_OT_stair(ArchipackCreateTool, Operator):
    bl_idname = "archipack.stair"
    bl_label = "Stair"
    bl_description = "Create a Stair"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    def create(self, context):
        m = bpy.data.meshes.new("Stair")
        o = bpy.data.objects.new("Stair", m)
        d = m.archipack_stair.add()
        context.scene.objects.link(o)
        o.select = True
        context.scene.objects.active = o
        self.load_preset(d)
        self.add_material(o)
        m.auto_smooth_angle = 0.20944
        return o

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    def execute(self, context):
        if context.mode == "OBJECT":
            bpy.ops.object.select_all(action="DESELECT")
            o = self.create(context)
            o.location = context.scene.cursor_location
            o.select = True
            context.scene.objects.active = o
            self.manipulate()
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}

# ------------------------------------------------------------------
# Define operator class to manipulate object
# ------------------------------------------------------------------


class ARCHIPACK_OT_stair_manipulate(Operator):
    bl_idname = "archipack.stair_manipulate"
    bl_label = "Manipulate"
    bl_description = "Manipulate"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        return archipack_stair.filter(context.active_object)

    def invoke(self, context, event):
        d = archipack_stair.datablock(context.active_object)
        d.manipulable_invoke(context)
        return {'FINISHED'}


# ------------------------------------------------------------------
# Define operator class to load / save presets
# ------------------------------------------------------------------


class ARCHIPACK_OT_stair_preset_menu(PresetMenuOperator, Operator):
    bl_description = "Show Stair Presets"
    bl_idname = "archipack.stair_preset_menu"
    bl_label = "Stair style"
    preset_subdir = "archipack_stair"


class ARCHIPACK_OT_stair_preset(ArchipackPreset, Operator):
    """Add a Stair Preset"""
    bl_idname = "archipack.stair_preset"
    bl_label = "Add Stair Style"
    preset_menu = "ARCHIPACK_OT_stair_preset_menu"

    @property
    def blacklist(self):
        return ['manipulators']


def register():
    bpy.utils.register_class(archipack_stair_material)
    bpy.utils.register_class(archipack_stair_part)
    bpy.utils.register_class(archipack_stair)
    Mesh.archipack_stair = CollectionProperty(type=archipack_stair)
    bpy.utils.register_class(ARCHIPACK_PT_stair)
    bpy.utils.register_class(ARCHIPACK_OT_stair)
    bpy.utils.register_class(ARCHIPACK_OT_stair_preset_menu)
    bpy.utils.register_class(ARCHIPACK_OT_stair_preset)
    bpy.utils.register_class(ARCHIPACK_OT_stair_manipulate)


def unregister():
    bpy.utils.unregister_class(archipack_stair_material)
    bpy.utils.unregister_class(archipack_stair_part)
    bpy.utils.unregister_class(archipack_stair)
    del Mesh.archipack_stair
    bpy.utils.unregister_class(ARCHIPACK_PT_stair)
    bpy.utils.unregister_class(ARCHIPACK_OT_stair)
    bpy.utils.unregister_class(ARCHIPACK_OT_stair_preset_menu)
    bpy.utils.unregister_class(ARCHIPACK_OT_stair_preset)
    bpy.utils.unregister_class(ARCHIPACK_OT_stair_manipulate)
