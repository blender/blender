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

__author__ = "McBuff, Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"


import bpy
import bmesh
from . import muv_common


def calc_edge_scale(uv_layer, loop0, loop1):
    v0 = loop0.vert.co
    v1 = loop1.vert.co
    uv0 = loop0[uv_layer].uv.copy()
    uv1 = loop1[uv_layer].uv.copy()

    dv = v1 - v0
    duv = uv1 - uv0

    scale = 0.0
    if dv.magnitude > 0.00000001:
        scale = duv.magnitude / dv.magnitude

    return scale


def calc_face_scale(uv_layer, face):
    es = 0.0
    for i, l in enumerate(face.loops[1:]):
        es = es + calc_edge_scale(uv_layer, face.loops[i], l)

    return es


class MUV_WSUVMeasure(bpy.types.Operator):
    """
    Operation class: Measure face size
    """

    bl_idname = "uv.muv_wsuv_measure"
    bl_label = "Measure"
    bl_description = "Measure face size for scale calculation"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.wsuv
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

        sel_faces = [f for f in bm.faces if f.select]

        # measure average face size
        scale = 0.0
        for f in sel_faces:
            scale = scale + calc_face_scale(uv_layer, f)

        props.ref_scale = scale / len(sel_faces)

        return {'FINISHED'}


class MUV_WSUVApply(bpy.types.Operator):
    """
    Operation class: Apply scaled UV
    """

    bl_idname = "uv.muv_wsuv_apply"
    bl_label = "Apply"
    bl_description = "Apply scaled UV based on scale calculation"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.wsuv
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

        sel_faces = [f for f in bm.faces if f.select]

        # measure average face size
        scale = 0.0
        for f in sel_faces:
            scale = scale + calc_face_scale(uv_layer, f)
        scale = scale / len(sel_faces)

        ratio = props.ref_scale / scale

        orig_area = bpy.context.area.type
        bpy.context.area.type = 'IMAGE_EDITOR'

        # select all UV related to the selected faces
        bpy.ops.uv.select_all(action='SELECT')

        # apply scaled UV
        bpy.ops.transform.resize(
            value=(ratio, ratio, ratio),
            constraint_axis=(False, False, False),
            constraint_orientation='GLOBAL',
            mirror=False,
            proportional='DISABLED',
            proportional_edit_falloff='SMOOTH',
            proportional_size=1)

        bpy.context.area.type = orig_area

        bmesh.update_edit_mesh(obj.data)

        return {'FINISHED'}
