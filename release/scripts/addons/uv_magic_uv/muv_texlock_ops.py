# <pep8-80 compliant>

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

__author__ = "Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"

import math
from math import (
        atan2, cos,
        sqrt, sin, fabs,
        )

import bpy
import bmesh
from mathutils import Vector
from bpy.props import BoolProperty
from . import muv_common


def get_vco(verts_orig, loop):
    """
    Get vertex original coordinate from loop
    """
    for vo in verts_orig:
        if vo["vidx"] == loop.vert.index and vo["moved"] is False:
            return vo["vco"]
    return loop.vert.co


def get_link_loops(vert):
    """
    Get loop linked to vertex
    """
    link_loops = []
    for f in vert.link_faces:
        adj_loops = []
        for loop in f.loops:
            # self loop
            if loop.vert == vert:
                l = loop
            # linked loop
            else:
                for e in loop.vert.link_edges:
                    if e.other_vert(loop.vert) == vert:
                        adj_loops.append(loop)
        if len(adj_loops) < 2:
            return None

        link_loops.append({"l": l, "l0": adj_loops[0], "l1": adj_loops[1]})
    return link_loops


def get_ini_geom(link_loop, uv_layer, verts_orig, v_orig):
    """
    Get initial geometory
    (Get interior angle of face in vertex/UV space)
    """
    u = link_loop["l"][uv_layer].uv
    v0 = get_vco(verts_orig, link_loop["l0"])
    u0 = link_loop["l0"][uv_layer].uv
    v1 = get_vco(verts_orig, link_loop["l1"])
    u1 = link_loop["l1"][uv_layer].uv

    # get interior angle of face in vertex space
    v0v1 = v1 - v0
    v0v = v_orig["vco"] - v0
    v1v = v_orig["vco"] - v1
    theta0 = v0v1.angle(v0v)
    theta1 = v0v1.angle(-v1v)
    if (theta0 + theta1) > math.pi:
        theta0 = v0v1.angle(-v0v)
        theta1 = v0v1.angle(v1v)

    # get interior angle of face in UV space
    u0u1 = u1 - u0
    u0u = u - u0
    u1u = u - u1
    phi0 = u0u1.angle(u0u)
    phi1 = u0u1.angle(-u1u)
    if (phi0 + phi1) > math.pi:
        phi0 = u0u1.angle(-u0u)
        phi1 = u0u1.angle(u1u)

    # get direction of linked UV coordinate
    # this will be used to judge whether angle is more or less than 180 degree
    dir0 = u0u1.cross(u0u) > 0
    dir1 = u0u1.cross(u1u) > 0

    return {
        "theta0": theta0,
        "theta1": theta1,
        "phi0": phi0,
        "phi1": phi1,
        "dir0": dir0,
        "dir1": dir1}


def get_target_uv(link_loop, uv_layer, verts_orig, v, ini_geom):
    """
    Get target UV coordinate
    """
    v0 = get_vco(verts_orig, link_loop["l0"])
    lo0 = link_loop["l0"]
    v1 = get_vco(verts_orig, link_loop["l1"])
    lo1 = link_loop["l1"]

    # get interior angle of face in vertex space
    v0v1 = v1 - v0
    v0v = v.co - v0
    v1v = v.co - v1
    theta0 = v0v1.angle(v0v)
    theta1 = v0v1.angle(-v1v)
    if (theta0 + theta1) > math.pi:
        theta0 = v0v1.angle(-v0v)
        theta1 = v0v1.angle(v1v)

    # calculate target interior angle in UV space
    phi0 = theta0 * ini_geom["phi0"] / ini_geom["theta0"]
    phi1 = theta1 * ini_geom["phi1"] / ini_geom["theta1"]

    uv0 = lo0[uv_layer].uv
    uv1 = lo1[uv_layer].uv

    # calculate target vertex coordinate from target interior angle
    tuv0, tuv1 = calc_tri_vert(uv0, uv1, phi0, phi1)

    # target UV coordinate depends on direction, so judge using direction of
    # linked UV coordinate
    u0u1 = uv1 - uv0
    u0u = tuv0 - uv0
    u1u = tuv0 - uv1
    dir0 = u0u1.cross(u0u) > 0
    dir1 = u0u1.cross(u1u) > 0
    if (ini_geom["dir0"] != dir0) or (ini_geom["dir1"] != dir1):
        return tuv1

    return tuv0


def calc_tri_vert(v0, v1, angle0, angle1):
    """
    Calculate rest coordinate from other coordinates and angle of end
    """
    angle = math.pi - angle0 - angle1

    alpha = atan2(v1.y - v0.y, v1.x - v0.x)
    d = (v1.x - v0.x) / cos(alpha)
    a = d * sin(angle0) / sin(angle)
    b = d * sin(angle1) / sin(angle)
    s = (a + b + d) / 2.0
    if fabs(d) < 0.0000001:
        xd = 0
        yd = 0
    else:
        xd = (b * b - a * a + d * d) / (2 * d)
        yd = 2 * sqrt(s * (s - a) * (s - b) * (s - d)) / d
    x1 = xd * cos(alpha) - yd * sin(alpha) + v0.x
    y1 = xd * sin(alpha) + yd * cos(alpha) + v0.y
    x2 = xd * cos(alpha) + yd * sin(alpha) + v0.x
    y2 = xd * sin(alpha) - yd * cos(alpha) + v0.y

    return Vector((x1, y1)), Vector((x2, y2))


class MUV_TexLockStart(bpy.types.Operator):
    """
    Operation class: Start Texture Lock
    """

    bl_idname = "uv.muv_texlock_start"
    bl_label = "Start"
    bl_description = "Start Texture Lock"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.texlock
        obj = bpy.context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()
            bm.faces.ensure_lookup_table()

        if not bm.loops.layers.uv:
            self.report(
                {'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}

        props.verts_orig = [
            {"vidx": v.index, "vco": v.co.copy(), "moved": False}
            for v in bm.verts if v.select]

        return {'FINISHED'}


class MUV_TexLockStop(bpy.types.Operator):
    """
    Operation class: Stop Texture Lock
    """

    bl_idname = "uv.muv_texlock_stop"
    bl_label = "Stop"
    bl_description = "Stop Texture Lock"
    bl_options = {'REGISTER', 'UNDO'}

    connect = BoolProperty(
        name="Connect UV",
        default=True)

    def execute(self, context):
        props = context.scene.muv_props.texlock
        obj = bpy.context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()
            bm.faces.ensure_lookup_table()

        if not bm.loops.layers.uv:
            self.report(
                {'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}
        uv_layer = bm.loops.layers.uv.verify()

        verts = [v.index for v in bm.verts if v.select]
        verts_orig = props.verts_orig

        # move UV followed by vertex coordinate
        for vidx, v_orig in zip(verts, verts_orig):
            if vidx != v_orig["vidx"]:
                self.report({'ERROR'}, "Internal Error")
                return {"CANCELLED"}

            v = bm.verts[vidx]
            link_loops = get_link_loops(v)

            result = []

            for ll in link_loops:
                ini_geom = get_ini_geom(ll, uv_layer, verts_orig, v_orig)
                target_uv = get_target_uv(
                    ll, uv_layer, verts_orig, v, ini_geom)
                result.append({"l": ll["l"], "uv": target_uv})

            # connect other face's UV
            if self.connect:
                ave = Vector((0.0, 0.0))
                for r in result:
                    ave = ave + r["uv"]
                ave = ave / len(result)
                for r in result:
                    r["l"][uv_layer].uv = ave
            else:
                for r in result:
                    r["l"][uv_layer].uv = r["uv"]
            v_orig["moved"] = True
            bmesh.update_edit_mesh(obj.data)

        return {'FINISHED'}


class MUV_TexLockUpdater(bpy.types.Operator):
    """
    Operation class: Texture locking updater
    """

    bl_idname = "uv.muv_texlock_updater"
    bl_label = "Texture Lock Updater"
    bl_description = "Texture Lock Updater"

    def __init__(self):
        self.__timer = None

    def __update_uv(self, context):
        """
        Update UV when vertex coordinates are changed
        """
        props = context.scene.muv_props.texlock
        obj = bpy.context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()
            bm.faces.ensure_lookup_table()

        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}
        uv_layer = bm.loops.layers.uv.verify()

        verts = [v.index for v in bm.verts if v.select]
        verts_orig = props.intr_verts_orig

        for vidx, v_orig in zip(verts, verts_orig):
            if vidx != v_orig["vidx"]:
                self.report({'ERROR'}, "Internal Error")
                return {"CANCELLED"}

            v = bm.verts[vidx]
            link_loops = get_link_loops(v)

            result = []
            for ll in link_loops:
                ini_geom = get_ini_geom(ll, uv_layer, verts_orig, v_orig)
                target_uv = get_target_uv(
                    ll, uv_layer, verts_orig, v, ini_geom)
                result.append({"l": ll["l"], "uv": target_uv})

            # UV connect option is always true, because it raises
            # unexpected behavior
            ave = Vector((0.0, 0.0))
            for r in result:
                ave = ave + r["uv"]
            ave = ave / len(result)
            for r in result:
                r["l"][uv_layer].uv = ave
            v_orig["moved"] = True
            bmesh.update_edit_mesh(obj.data)

        muv_common.redraw_all_areas()
        props.intr_verts_orig = [
            {"vidx": v.index, "vco": v.co.copy(), "moved": False}
            for v in bm.verts if v.select]

    def modal(self, context, event):
        props = context.scene.muv_props.texlock
        if context.area:
            context.area.tag_redraw()
        if props.intr_running is False:
            self.__handle_remove(context)
            return {'FINISHED'}
        if event.type == 'TIMER':
            self.__update_uv(context)

        return {'PASS_THROUGH'}

    def __handle_add(self, context):
        if self.__timer is None:
            self.__timer = context.window_manager.event_timer_add(
                0.10, context.window)
            context.window_manager.modal_handler_add(self)

    def __handle_remove(self, context):
        if self.__timer is not None:
            context.window_manager.event_timer_remove(self.__timer)
            self.__timer = None

    def execute(self, context):
        props = context.scene.muv_props.texlock
        if props.intr_running is False:
            self.__handle_add(context)
            props.intr_running = True
            return {'RUNNING_MODAL'}
        else:
            props.intr_running = False
        if context.area:
            context.area.tag_redraw()

        return {'FINISHED'}


class MUV_TexLockIntrStart(bpy.types.Operator):
    """
    Operation class: Start texture locking (Interactive mode)
    """

    bl_idname = "uv.muv_texlock_intr_start"
    bl_label = "Texture Lock Start (Interactive mode)"
    bl_description = "Texture Lock Start (Realtime UV update)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.texlock
        if props.intr_running is True:
            return {'CANCELLED'}

        obj = bpy.context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()
            bm.faces.ensure_lookup_table()

        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}

        props.intr_verts_orig = [
            {"vidx": v.index, "vco": v.co.copy(), "moved": False}
            for v in bm.verts if v.select]

        bpy.ops.uv.muv_texlock_updater()

        return {'FINISHED'}


# Texture lock (Stop, Interactive mode)
class MUV_TexLockIntrStop(bpy.types.Operator):
    """
    Operation class: Stop texture locking (interactive mode)
    """

    bl_idname = "uv.muv_texlock_intr_stop"
    bl_label = "Texture Lock Stop (Interactive mode)"
    bl_description = "Texture Lock Stop (Realtime UV update)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.texlock
        if props.intr_running is False:
            return {'CANCELLED'}

        bpy.ops.uv.muv_texlock_updater()

        return {'FINISHED'}
