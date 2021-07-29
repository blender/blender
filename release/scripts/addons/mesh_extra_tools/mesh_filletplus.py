# -*- coding: utf-8 -*-

# ##### END GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

bl_info = {
    "name": "FilletPlus",
    "author": "Gert De Roost - original by zmj100",
    "version": (0, 4, 3),
    "blender": (2, 61, 0),
    "location": "View3D > Tool Shelf",
    "description": "",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        )
from bpy.types import Operator
import bmesh
from mathutils import Matrix
from math import (
        cos, pi, sin,
        degrees, tan,
        )


def list_clear_(l):
    if l:
        del l[:]
    return l


def get_adj_v_(list_):
    tmp = {}
    for i in list_:
        try:
            tmp[i[0]].append(i[1])
        except KeyError:
            tmp[i[0]] = [i[1]]
        try:
            tmp[i[1]].append(i[0])
        except KeyError:
            tmp[i[1]] = [i[0]]
    return tmp


class f_buf():
    # one of the angles was not 0 or 180
    check = False


def fillets(list_0, startv, vertlist, face, adj, n, out, flip, radius):
    try:
        dict_0 = get_adj_v_(list_0)
        list_1 = [[dict_0[i][0], i, dict_0[i][1]] for i in dict_0 if (len(dict_0[i]) == 2)][0]
        list_3 = []
        for elem in list_1:
            list_3.append(bm.verts[elem])
        list_2 = []

        p_ = list_3[1]
        p = (list_3[1].co).copy()
        p1 = (list_3[0].co).copy()
        p2 = (list_3[2].co).copy()

        vec1 = p - p1
        vec2 = p - p2

        ang = vec1.angle(vec2, any)
        check_angle = round(degrees(ang))

        if check_angle == 180 or check_angle == 0.0:
            return False
        else:
            f_buf.check = True

        opp = adj

        if radius is False:
            h = adj * (1 / cos(ang * 0.5))
            adj_ = adj
        elif radius is True:
            h = opp / sin(ang * 0.5)
            adj_ = opp / tan(ang * 0.5)

        p3 = p - (vec1.normalized() * adj_)
        p4 = p - (vec2.normalized() * adj_)
        rp = p - ((p - ((p3 + p4) * 0.5)).normalized() * h)

        vec3 = rp - p3
        vec4 = rp - p4

        axis = vec1.cross(vec2)

        if out is False:
            if flip is False:
                rot_ang = vec3.angle(vec4)
            elif flip is True:
                rot_ang = vec1.angle(vec2)
        elif out is True:
            rot_ang = (2 * pi) - vec1.angle(vec2)

        for j in range(n + 1):
            new_angle = rot_ang * j / n
            mtrx = Matrix.Rotation(new_angle, 3, axis)
            if out is False:
                if flip is False:
                    tmp = p4 - rp
                    tmp1 = mtrx * tmp
                    tmp2 = tmp1 + rp
                elif flip is True:
                    p3 = p - (vec1.normalized() * opp)
                    tmp = p3 - p
                    tmp1 = mtrx * tmp
                    tmp2 = tmp1 + p
            elif out is True:
                p4 = p - (vec2.normalized() * opp)
                tmp = p4 - p
                tmp1 = mtrx * tmp
                tmp2 = tmp1 + p

            v = bm.verts.new(tmp2)
            list_2.append(v)

        if flip is True:
            list_3[1:2] = list_2
        else:
            list_2.reverse()
            list_3[1:2] = list_2

        list_clear_(list_2)

        n1 = len(list_3)

        for t in range(n1 - 1):
            bm.edges.new([list_3[t], list_3[(t + 1) % n1]])

            v = bm.verts.new(p)
            bm.edges.new([v, p_])

        bm.edges.ensure_lookup_table()

        if face is not None:
            for l in face.loops:
                if l.vert == list_3[0]:
                    startl = l
                    break
            vertlist2 = []

            if startl.link_loop_next.vert == startv:
                l = startl.link_loop_prev
                while len(vertlist) > 0:
                    vertlist2.insert(0, l.vert)
                    vertlist.pop(vertlist.index(l.vert))
                    l = l.link_loop_prev
            else:
                l = startl.link_loop_next
                while len(vertlist) > 0:
                    vertlist2.insert(0, l.vert)
                    vertlist.pop(vertlist.index(l.vert))
                    l = l.link_loop_next

            for v in list_3:
                vertlist2.append(v)
            bm.faces.new(vertlist2)
        if startv.is_valid:
            bm.verts.remove(startv)
        else:
            print("\n[Function fillets Error]\n"
                  "Starting vertex (startv var) couldn't be removed\n")
            return False
        bm.verts.ensure_lookup_table()
        bm.edges.ensure_lookup_table()
        bm.faces.ensure_lookup_table()
        list_3[1].select = 1
        list_3[-2].select = 1
        bm.edges.get([list_3[0], list_3[1]]).select = 1
        bm.edges.get([list_3[-1], list_3[-2]]).select = 1
        bm.verts.index_update()
        bm.edges.index_update()
        bm.faces.index_update()

        me.update(calc_edges=True, calc_tessface=True)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

    except Exception as e:
        print("\n[Function fillets Error]\n{}\n".format(e))
        return False


def do_filletplus(self, pair):
    is_finished = True
    try:
        startv = None
        global inaction
        global flip
        list_0 = [list([e.verts[0].index, e.verts[1].index]) for e in pair]

        vertset = set([])
        bm.verts.ensure_lookup_table()
        bm.edges.ensure_lookup_table()
        bm.faces.ensure_lookup_table()
        vertset.add(bm.verts[list_0[0][0]])
        vertset.add(bm.verts[list_0[0][1]])
        vertset.add(bm.verts[list_0[1][0]])
        vertset.add(bm.verts[list_0[1][1]])

        v1, v2, v3 = vertset

        if len(list_0) != 2:
            self.report({'WARNING'}, "Two adjacent edges must be selected")
            is_finished = False
        else:
            inaction = 1
            vertlist = []
            found = 0
            for f in v1.link_faces:
                if v2 in f.verts and v3 in f.verts:
                    found = 1
            if not found:
                for v in [v1, v2, v3]:
                    if v.index in list_0[0] and v.index in list_0[1]:
                        startv = v
                face = None
            else:
                for f in v1.link_faces:
                    if v2 in f.verts and v3 in f.verts:
                        for v in f.verts:
                            if not(v in vertset):
                                vertlist.append(v)
                            if (v in vertset and v.link_loops[0].link_loop_prev.vert in vertset and
                               v.link_loops[0].link_loop_next.vert in vertset):
                                startv = v
                        face = f
            if out is True:
                flip = False
            if startv:
                fills = fillets(list_0, startv, vertlist, face, adj, n, out, flip, radius)
                if not fills:
                    is_finished = False
            else:
                is_finished = False
    except Exception as e:
        print("\n[Function do_filletplus Error]\n{}\n".format(e))
        is_finished = False
    return is_finished


def check_is_not_coplanar(bm_data):
    from mathutils import Vector
    check = False
    angles, norm_angle = 0, 0
    z_vec = Vector((0, 0, 1))
    try:
        bm_data.faces.ensure_lookup_table()

        for f in bm_data.faces:
            norm_angle = f.normal.angle(z_vec)
            if angles == 0:
                angles = norm_angle
            if angles != norm_angle:
                check = True
                break
    except Exception as e:
        print("\n[Function check_is_not_coplanar Error]\n{}\n".format(e))
        check = True
    return check


#  Operator

class MESH_OT_fillet_plus(Operator):
    bl_idname = "mesh.fillet_plus"
    bl_label = "Fillet Plus"
    bl_description = ("Fillet adjoining edges\n"
                      "Note: Works on a mesh whose all faces share the same normal")
    bl_options = {"REGISTER", "UNDO"}

    adj = FloatProperty(
            name="",
            description="Size of the filleted corners",
            default=0.1,
            min=0.00001, max=100.0,
            step=1,
            precision=3
            )
    n = IntProperty(
            name="",
            description="Subdivision of the filleted corners",
            default=3,
            min=1, max=50,
            step=1
            )
    out = BoolProperty(
            name="Outside",
            description="Fillet towards outside",
            default=False
            )
    flip = BoolProperty(
            name="Flip",
            description="Flip the direction of the Fillet\n"
                        "Only available if Outside option is not active",
            default=False
            )
    radius = BoolProperty(
            name="Radius",
            description="Use radius for the size of the filleted corners",
            default=False
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and context.mode == 'EDIT_MESH')

    def draw(self, context):
        layout = self.layout

        if f_buf.check is False:
            layout.label(text="Angle is equal to 0 or 180", icon="INFO")
            layout.label(text="Can not fillet", icon="BLANK1")
        else:
            layout.prop(self, "radius")
            if self.radius is True:
                layout.label("Radius:")
            elif self.radius is False:
                layout.label("Distance:")
            layout.prop(self, "adj")
            layout.label("Number of sides:")
            layout.prop(self, "n")

            if self.n > 1:
                row = layout.row(align=False)
                row.prop(self, "out")
                if self.out is False:
                    row.prop(self, "flip")

    def execute(self, context):
        global inaction
        global bm, me, adj, n, out, flip, radius

        adj = self.adj
        n = self.n
        out = self.out
        flip = self.flip
        radius = self.radius

        inaction = 0
        f_buf.check = False

        ob_act = context.active_object
        try:
            me = ob_act.data
            bm = bmesh.from_edit_mesh(me)
            warn_obj = bool(check_is_not_coplanar(bm))
            if warn_obj is False:
                tempset = set([])
                bm.verts.ensure_lookup_table()
                bm.edges.ensure_lookup_table()
                bm.faces.ensure_lookup_table()
                for v in bm.verts:
                    if v.select and v.is_boundary:
                        tempset.add(v)
                for v in tempset:
                    edgeset = set([])
                    for e in v.link_edges:
                        if e.select and e.is_boundary:
                            edgeset.add(e)
                        if len(edgeset) == 2:
                            is_finished = do_filletplus(self, edgeset)
                            if not is_finished:
                                break

                if inaction == 1:
                    bpy.ops.mesh.select_all(action="DESELECT")
                    for v in bm.verts:
                        if len(v.link_edges) == 0:
                            bm.verts.remove(v)
                    bpy.ops.object.editmode_toggle()
                    bpy.ops.object.editmode_toggle()
                else:
                    self.report({'WARNING'}, "Filletplus operation could not be performed")
                    return {'CANCELLED'}
            else:
                self.report({'WARNING'}, "Mesh is not a coplanar surface. Operation cancelled")
                return {'CANCELLED'}
        except:
            self.report({'WARNING'}, "Filletplus operation could not be performed")
            return {'CANCELLED'}

        return {'FINISHED'}
