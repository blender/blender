# -*- coding: utf-8 -*-

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

# based completely on addon by zmj100
# added some distance limits to prevent overlap - max12345


import bpy
import bmesh
from bpy.types import Operator
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        EnumProperty,
        )
from math import (
        sin, cos, tan,
        degrees, radians,
        )
from mathutils import Matrix


def edit_mode_out():
    bpy.ops.object.mode_set(mode='OBJECT')


def edit_mode_in():
    bpy.ops.object.mode_set(mode='EDIT')


def angle_rotation(rp, q, axis, angle):
    # returns the vector made by the rotation of the vector q
    # rp by angle around axis and then adds rp

    return (Matrix.Rotation(angle, 3, axis) * (q - rp)) + rp


def face_inset_fillet(bme, face_index_list, inset_amount, distance,
                      number_of_sides, out, radius, type_enum, kp):
    list_del = []

    for faceindex in face_index_list:

        bme.faces.ensure_lookup_table()
        # loops through the faces...
        f = bme.faces[faceindex]
        f.select_set(0)
        list_del.append(f)
        f.normal_update()
        vertex_index_list = [v.index for v in f.verts]
        dict_0 = {}
        orientation_vertex_list = []
        n = len(vertex_index_list)
        for i in range(n):
            # loops through the vertices
            dict_0[i] = []
            bme.verts.ensure_lookup_table()
            p = (bme.verts[vertex_index_list[i]].co).copy()
            p1 = (bme.verts[vertex_index_list[(i - 1) % n]].co).copy()
            p2 = (bme.verts[vertex_index_list[(i + 1) % n]].co).copy()
            # copies some vert coordinates, always the 3 around i
            dict_0[i].append(bme.verts[vertex_index_list[i]])
            # appends the bmesh vert of the appropriate index to the dict
            vec1 = p - p1
            vec2 = p - p2
            # vectors for the other corner points to the cornerpoint
            # corresponding to i / p
            angle = vec1.angle(vec2)

            adj = inset_amount / tan(angle * 0.5)
            h = (adj ** 2 + inset_amount ** 2) ** 0.5
            if round(degrees(angle)) == 180 or round(degrees(angle)) == 0.0:
                # if the corner is a straight line...
                # I think this creates some new points...
                if out is True:
                    val = ((f.normal).normalized() * inset_amount)
                else:
                    val = -((f.normal).normalized() * inset_amount)
                p6 = angle_rotation(p, p + val, vec1, radians(90))
            else:
                # if the corner is an actual corner
                val = ((f.normal).normalized() * h)
                if out is True:
                    # this -(p - (vec2.normalized() * adj))) is just the freaking axis afaik...
                    p6 = angle_rotation(
                                p, p + val,
                                -(p - (vec2.normalized() * adj)),
                                -radians(90)
                                )
                else:
                    p6 = angle_rotation(
                                p, p - val,
                                ((p - (vec1.normalized() * adj)) - (p - (vec2.normalized() * adj))),
                                -radians(90)
                                )

                orientation_vertex_list.append(p6)

        new_inner_face = []
        orientation_vertex_list_length = len(orientation_vertex_list)
        ovll = orientation_vertex_list_length

        for j in range(ovll):
            q = orientation_vertex_list[j]
            q1 = orientation_vertex_list[(j - 1) % ovll]
            q2 = orientation_vertex_list[(j + 1) % ovll]
            # again, these are just vectors between somewhat displaced corner vertices
            vec1_ = q - q1
            vec2_ = q - q2
            ang_ = vec1_.angle(vec2_)

            # the angle between them
            if round(degrees(ang_)) == 180 or round(degrees(ang_)) == 0.0:
                # again... if it's really a line...
                v = bme.verts.new(q)
                new_inner_face.append(v)
                dict_0[j].append(v)
            else:
                # s.a.
                if radius is False:
                    h_ = distance * (1 / cos(ang_ * 0.5))
                    d = distance
                elif radius is True:
                    h_ = distance / sin(ang_ * 0.5)
                    d = distance / tan(ang_ * 0.5)
                # max(d) is vec1_.magnitude * 0.5
                # or vec2_.magnitude * 0.5 respectively

                # only functional difference v
                if d > vec1_.magnitude * 0.5:
                    d = vec1_.magnitude * 0.5

                if d > vec2_.magnitude * 0.5:
                    d = vec2_.magnitude * 0.5
                # only functional difference ^

                q3 = q - (vec1_.normalized() * d)
                q4 = q - (vec2_.normalized() * d)
                # these are new verts somewhat offset from the corners
                rp_ = q - ((q - ((q3 + q4) * 0.5)).normalized() * h_)
                # reference point inside the curvature
                axis_ = vec1_.cross(vec2_)
                # this should really be just the face normal
                vec3_ = rp_ - q3
                vec4_ = rp_ - q4
                rot_ang = vec3_.angle(vec4_)
                cornerverts = []

                for o in range(number_of_sides + 1):
                    # this calculates the actual new vertices
                    q5 = angle_rotation(rp_, q4, axis_, rot_ang * o / number_of_sides)
                    v = bme.verts.new(q5)

                    # creates new bmesh vertices from it
                    bme.verts.index_update()

                    dict_0[j].append(v)
                    cornerverts.append(v)

                cornerverts.reverse()
                new_inner_face.extend(cornerverts)

        if out is False:
            f = bme.faces.new(new_inner_face)
            f.select_set(True)
        elif out is True and kp is True:
            f = bme.faces.new(new_inner_face)
            f.select_set(True)

        n2_ = len(dict_0)
        # these are the new side faces, those that don't depend on cornertype
        for o in range(n2_):
            list_a = dict_0[o]
            list_b = dict_0[(o + 1) % n2_]
            bme.faces.new([list_a[0], list_b[0], list_b[-1], list_a[1]])
            bme.faces.index_update()
        # cornertype 1 - ngon faces
        if type_enum == 'opt0':
            for k in dict_0:
                if len(dict_0[k]) > 2:
                    bme.faces.new(dict_0[k])
                    bme.faces.index_update()
        # cornertype 2 - triangulated faces
        if type_enum == 'opt1':
            for k_ in dict_0:
                q_ = dict_0[k_][0]
                dict_0[k_].pop(0)
                n3_ = len(dict_0[k_])
                for kk in range(n3_ - 1):
                    bme.faces.new([dict_0[k_][kk], dict_0[k_][(kk + 1) % n3_], q_])
                    bme.faces.index_update()

    del_ = [bme.faces.remove(f) for f in list_del]

    if del_:
        del del_


# Operator

class MESH_OT_face_inset_fillet(Operator):
    bl_idname = "mesh.face_inset_fillet"
    bl_label = "Face Inset Fillet"
    bl_description = ("Inset selected and Fillet (make round) the corners \n"
                     "of the newly created Faces")
    bl_options = {"REGISTER", "UNDO"}

    # inset amount
    inset_amount = FloatProperty(
            name="Inset amount",
            description="Define the size of the Inset relative to the selection",
            default=0.04,
            min=0, max=100.0,
            step=1,
            precision=3
            )
    # number of sides
    number_of_sides = IntProperty(
            name="Number of sides",
            description="Define the roundness of the corners by specifying\n"
                        "the subdivision count",
            default=4,
            min=1, max=100,
            step=1
            )
    distance = FloatProperty(
            name="",
            description="Use distance or radius for corners' size calculation",
            default=0.04,
            min=0.00001, max=100.0,
            step=1,
            precision=3
            )
    out = BoolProperty(
            name="Outside",
            description="Inset the Faces outwards in relation to the selection\n"
                        "Note: depending on the geometry, can give unsatisfactory results",
            default=False
            )
    radius = BoolProperty(
            name="Radius",
            description="Use radius for corners' size calculation",
            default=False
            )
    type_enum = EnumProperty(
            items=(('opt0', "N-gon", "N-gon corners - Keep the corner Faces uncut"),
                   ('opt1', "Triangle", "Triangulate corners")),
            name="Corner Type",
            default="opt0"
            )
    kp = BoolProperty(
            name="Keep faces",
            description="Do not delete the inside Faces\n"
                        "Only available if the Out option is checked",
            default=False
            )

    def draw(self, context):
        layout = self.layout

        layout.label("Corner Type:")

        row = layout.row()
        row.prop(self, "type_enum", text="")

        row = layout.row(align=True)
        row.prop(self, "out")

        if self.out is True:
            row.prop(self, "kp")

        row = layout.row()
        row.prop(self, "inset_amount")

        row = layout.row()
        row.prop(self, "number_of_sides")

        row = layout.row()
        row.prop(self, "radius")

        row = layout.row()
        dist_rad = "Radius" if self.radius else "Distance"
        row.prop(self, "distance", text=dist_rad)

    def execute(self, context):
        # this really just prepares everything for the main function
        inset_amount = self.inset_amount
        number_of_sides = self.number_of_sides
        distance = self.distance
        out = self.out
        radius = self.radius
        type_enum = self.type_enum
        kp = self.kp

        edit_mode_out()
        ob_act = context.active_object
        bme = bmesh.new()
        bme.from_mesh(ob_act.data)
        # this
        face_index_list = [f.index for f in bme.faces if f.select and f.is_valid]

        if len(face_index_list) == 0:
            self.report({'WARNING'},
                        "No suitable Face selection found. Operation cancelled")
            edit_mode_in()

            return {'CANCELLED'}

        elif len(face_index_list) != 0:
            face_inset_fillet(bme, face_index_list,
                              inset_amount, distance, number_of_sides,
                              out, radius, type_enum, kp)

        bme.to_mesh(ob_act.data)
        edit_mode_in()

        return {'FINISHED'}
