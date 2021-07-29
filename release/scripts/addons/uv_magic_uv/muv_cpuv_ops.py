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

__author__ = "Nutti <nutti.metro@gmail.com>, Jace Priester"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"

import bpy
import bmesh
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        EnumProperty,
        )
from . import muv_common


def memorize_view_3d_mode(fn):
    def __memorize_view_3d_mode(self, context):
        mode_orig = bpy.context.object.mode
        result = fn(self, context)
        bpy.ops.object.mode_set(mode=mode_orig)
        return result
    return __memorize_view_3d_mode


class MUV_CPUVCopyUV(bpy.types.Operator):
    """
    Operation class: Copy UV coordinate
    """

    bl_idname = "uv.muv_cpuv_copy_uv"
    bl_label = "Copy UV (Operation)"
    bl_description = "Copy UV coordinate (Operation)"
    bl_options = {'REGISTER', 'UNDO'}

    uv_map = StringProperty(options={'HIDDEN'})

    def execute(self, context):
        props = context.scene.muv_props.cpuv
        if self.uv_map == "":
            self.report({'INFO'}, "Copy UV coordinate")
        else:
            self.report(
                {'INFO'}, "Copy UV coordinate (UV map:%s)" % (self.uv_map))
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV layer
        if self.uv_map == "":
            if not bm.loops.layers.uv:
                self.report(
                    {'WARNING'}, "Object must have more than one UV map")
                return {'CANCELLED'}
            uv_layer = bm.loops.layers.uv.verify()
        else:
            uv_layer = bm.loops.layers.uv[self.uv_map]

        # get selected face
        props.src_uvs = []
        props.src_pin_uvs = []
        props.src_seams = []
        for face in bm.faces:
            if face.select:
                uvs = [l[uv_layer].uv.copy() for l in face.loops]
                pin_uvs = [l[uv_layer].pin_uv for l in face.loops]
                seams = [l.edge.seam  for l in face.loops]
                props.src_uvs.append(uvs)
                props.src_pin_uvs.append(pin_uvs)
                props.src_seams.append(seams)
        if len(props.src_uvs) == 0 or len(props.src_pin_uvs) == 0:
            self.report({'WARNING'}, "No faces are selected")
            return {'CANCELLED'}
        self.report({'INFO'}, "%d face(s) are selected" % len(props.src_uvs))

        return {'FINISHED'}


class MUV_CPUVCopyUVMenu(bpy.types.Menu):
    """
    Menu class: Copy UV coordinate
    """

    bl_idname = "uv.muv_cpuv_copy_uv_menu"
    bl_label = "Copy UV"
    bl_description = "Copy UV coordinate"

    def draw(self, context):
        layout = self.layout
        # create sub menu
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        uv_maps = bm.loops.layers.uv.keys()
        layout.operator(
            MUV_CPUVCopyUV.bl_idname,
            text="[Default]",
            icon="IMAGE_COL"
        ).uv_map = ""
        for m in uv_maps:
            layout.operator(
                MUV_CPUVCopyUV.bl_idname,
                text=m,
                icon="IMAGE_COL"
            ).uv_map = m


class MUV_CPUVPasteUV(bpy.types.Operator):
    """
    Operation class: Paste UV coordinate
    """

    bl_idname = "uv.muv_cpuv_paste_uv"
    bl_label = "Paste UV (Operation)"
    bl_description = "Paste UV coordinate (Operation)"
    bl_options = {'REGISTER', 'UNDO'}

    uv_map = StringProperty(options={'HIDDEN'})
    strategy = EnumProperty(
        name="Strategy",
        description="Paste Strategy",
        items=[
            ('N_N', 'N:N', 'Number of faces must be equal to source'),
            ('N_M', 'N:M', 'Number of faces must not be equal to source')
        ],
        default="N_M"
    )
    flip_copied_uv = BoolProperty(
        name="Flip Copied UV",
        description="Flip Copied UV...",
        default=False
    )
    rotate_copied_uv = IntProperty(
        default=0,
        name="Rotate Copied UV",
        min=0,
        max=30
    )
    copy_seams = BoolProperty(
        name="Copy Seams",
        description="Copy Seams",
        default=True
    )

    def execute(self, context):
        props = context.scene.muv_props.cpuv
        if len(props.src_uvs) == 0 or len(props.src_pin_uvs) == 0:
            self.report({'WARNING'}, "Need copy UV at first")
            return {'CANCELLED'}
        if self.uv_map == "":
            self.report({'INFO'}, "Paste UV coordinate")
        else:
            self.report(
                {'INFO'}, "Paste UV coordinate (UV map:%s)" % (self.uv_map))
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV layer
        if self.uv_map == "":
            if not bm.loops.layers.uv:
                self.report(
                    {'WARNING'}, "Object must have more than one UV map")
                return {'CANCELLED'}
            uv_layer = bm.loops.layers.uv.verify()
        else:
            uv_layer = bm.loops.layers.uv[self.uv_map]

        # get selected face
        dest_uvs = []
        dest_pin_uvs = []
        dest_seams = []
        dest_face_indices = []
        for face in bm.faces:
            if face.select:
                dest_face_indices.append(face.index)
                uvs = [l[uv_layer].uv.copy() for l in face.loops]
                pin_uvs = [l[uv_layer].pin_uv for l in face.loops]
                seams = [l.edge.seam for l in face.loops]
                dest_uvs.append(uvs)
                dest_pin_uvs.append(pin_uvs)
                dest_seams.append(seams)
        if len(dest_uvs) == 0 or len(dest_pin_uvs) == 0:
            self.report({'WARNING'}, "No faces are selected")
            return {'CANCELLED'}
        if self.strategy == 'N_N' and len(props.src_uvs) != len(dest_uvs):
            self.report(
                {'WARNING'},
                "Number of selected faces is different from copied" +
                "(src:%d, dest:%d)" %
                (len(props.src_uvs), len(dest_uvs)))
            return {'CANCELLED'}

        # paste
        for i, idx in enumerate(dest_face_indices):
            suv = None
            spuv = None
            ss = None
            duv = None
            if self.strategy == 'N_N':
                suv = props.src_uvs[i]
                spuv = props.src_pin_uvs[i]
                ss = props.src_seams[i]
                duv = dest_uvs[i]
            elif self.strategy == 'N_M':
                suv = props.src_uvs[i % len(props.src_uvs)]
                spuv = props.src_pin_uvs[i % len(props.src_pin_uvs)]
                ss = props.src_seams[i % len(props.src_seams)]
                duv = dest_uvs[i]
            if len(suv) != len(duv):
                self.report({'WARNING'}, "Some faces are different size")
                return {'CANCELLED'}
            suvs_fr = [uv for uv in suv]
            spuvs_fr = [pin_uv for pin_uv in spuv]
            ss_fr = [s for s in ss]
            # flip UVs
            if self.flip_copied_uv is True:
                suvs_fr.reverse()
                spuvs_fr.reverse()
                ss_fr.reverse()
            # rotate UVs
            for _ in range(self.rotate_copied_uv):
                uv = suvs_fr.pop()
                pin_uv = spuvs_fr.pop()
                s = ss_fr.pop()
                suvs_fr.insert(0, uv)
                spuvs_fr.insert(0, pin_uv)
                ss_fr.insert(0, s)
            # paste UVs
            for l, suv, spuv, ss in zip(bm.faces[idx].loops, suvs_fr, spuvs_fr, ss_fr):
                l[uv_layer].uv = suv
                l[uv_layer].pin_uv = spuv
                if self.copy_seams is True:
                    l.edge.seam = ss
        self.report({'INFO'}, "%d face(s) are copied" % len(dest_uvs))

        bmesh.update_edit_mesh(obj.data)
        if self.copy_seams is True:
            obj.data.show_edge_seams = True

        return {'FINISHED'}


class MUV_CPUVPasteUVMenu(bpy.types.Menu):
    """
    Menu class: Paste UV coordinate
    """

    bl_idname = "uv.muv_cpuv_paste_uv_menu"
    bl_label = "Paste UV"
    bl_description = "Paste UV coordinate"

    def draw(self, context):
        layout = self.layout
        # create sub menu
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        uv_maps = bm.loops.layers.uv.keys()
        layout.operator(
            MUV_CPUVPasteUV.bl_idname,
            text="[Default]", icon="IMAGE_COL").uv_map = ""
        for m in uv_maps:
            layout.operator(
                MUV_CPUVPasteUV.bl_idname,
                text=m, icon="IMAGE_COL").uv_map = m


class MUV_CPUVObjCopyUV(bpy.types.Operator):
    """
    Operation class: Copy UV coordinate per object
    """

    bl_idname = "object.muv_cpuv_obj_copy_uv"
    bl_label = "Copy UV"
    bl_description = "Copy UV coordinate"
    bl_options = {'REGISTER', 'UNDO'}

    uv_map = StringProperty(options={'HIDDEN'})

    @memorize_view_3d_mode
    def execute(self, context):
        props = context.scene.muv_props.cpuv_obj
        if self.uv_map == "":
            self.report({'INFO'}, "Copy UV coordinate per object")
        else:
            self.report(
                {'INFO'},
                "Copy UV coordinate per object (UV map:%s)" % (self.uv_map))
        bpy.ops.object.mode_set(mode='EDIT')

        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV layer
        if self.uv_map == "":
            if not bm.loops.layers.uv:
                self.report(
                    {'WARNING'}, "Object must have more than one UV map")
                return {'CANCELLED'}
            uv_layer = bm.loops.layers.uv.verify()
        else:
            uv_layer = bm.loops.layers.uv[self.uv_map]

        # get selected face
        props.src_uvs = []
        props.src_pin_uvs = []
        props.src_seams = []
        for face in bm.faces:
            uvs = [l[uv_layer].uv.copy() for l in face.loops]
            pin_uvs = [l[uv_layer].pin_uv for l in face.loops]
            seams = [l.edge.seam  for l in face.loops]
            props.src_uvs.append(uvs)
            props.src_pin_uvs.append(pin_uvs)
            props.src_seams.append(seams)

        self.report({'INFO'}, "%s's UV coordinates are copied" % (obj.name))

        return {'FINISHED'}


class MUV_CPUVObjCopyUVMenu(bpy.types.Menu):
    """
    Menu class: Copy UV coordinate per object
    """

    bl_idname = "object.muv_cpuv_obj_copy_uv_menu"
    bl_label = "Copy UV"
    bl_description = "Copy UV coordinate per object"

    def draw(self, _):
        layout = self.layout
        # create sub menu
        uv_maps = bpy.context.active_object.data.uv_textures.keys()
        layout.operator(
            MUV_CPUVObjCopyUV.bl_idname,
            text="[Default]", icon="IMAGE_COL").uv_map = ""
        for m in uv_maps:
            layout.operator(
                MUV_CPUVObjCopyUV.bl_idname,
                text=m, icon="IMAGE_COL").uv_map = m


class MUV_CPUVObjPasteUV(bpy.types.Operator):
    """
    Operation class: Paste UV coordinate per object
    """

    bl_idname = "object.muv_cpuv_obj_paste_uv"
    bl_label = "Paste UV"
    bl_description = "Paste UV coordinate"
    bl_options = {'REGISTER', 'UNDO'}

    uv_map = StringProperty(options={'HIDDEN'})
    copy_seams = BoolProperty(
        name="Copy Seams",
        description="Copy Seams",
        default=True
    )

    @memorize_view_3d_mode
    def execute(self, context):
        props = context.scene.muv_props.cpuv_obj
        if len(props.src_uvs) == 0 or len(props.src_pin_uvs) == 0:
            self.report({'WARNING'}, "Need copy UV at first")
            return {'CANCELLED'}

        for o in bpy.data.objects:
            if not hasattr(o.data, "uv_textures") or not o.select:
                continue

            bpy.ops.object.mode_set(mode='OBJECT')
            bpy.context.scene.objects.active = o
            bpy.ops.object.mode_set(mode='EDIT')

            obj = context.active_object
            bm = bmesh.from_edit_mesh(obj.data)
            if muv_common.check_version(2, 73, 0) >= 0:
                bm.faces.ensure_lookup_table()

            if (self.uv_map == "" or
                    self.uv_map not in bm.loops.layers.uv.keys()):
                self.report({'INFO'}, "Paste UV coordinate per object")
            else:
                self.report(
                    {'INFO'},
                    "Paste UV coordinate per object (UV map: %s)"
                    % (self.uv_map))

            # get UV layer
            if (self.uv_map == "" or
                    self.uv_map not in bm.loops.layers.uv.keys()):
                if not bm.loops.layers.uv:
                    self.report(
                        {'WARNING'}, "Object must have more than one UV map")
                    return {'CANCELLED'}
                uv_layer = bm.loops.layers.uv.verify()
            else:
                uv_layer = bm.loops.layers.uv[self.uv_map]

            # get selected face
            dest_uvs = []
            dest_pin_uvs = []
            dest_seams = []
            dest_face_indices = []
            for face in bm.faces:
                dest_face_indices.append(face.index)
                uvs = [l[uv_layer].uv.copy() for l in face.loops]
                pin_uvs = [l[uv_layer].pin_uv for l in face.loops]
                seams = [l.edge.seam for l in face.loops]
                dest_uvs.append(uvs)
                dest_pin_uvs.append(pin_uvs)
                dest_seams.append(seams)
            if len(props.src_uvs) != len(dest_uvs):
                self.report(
                    {'WARNING'},
                    "Number of faces is different from copied " +
                    "(src:%d, dest:%d)"
                    % (len(props.src_uvs), len(dest_uvs))
                )
                return {'CANCELLED'}

            # paste
            for i, idx in enumerate(dest_face_indices):
                suv = props.src_uvs[i]
                spuv = props.src_pin_uvs[i]
                ss = props.src_seams[i]
                duv = dest_uvs[i]
                if len(suv) != len(duv):
                    self.report({'WARNING'}, "Some faces are different size")
                    return {'CANCELLED'}
                suvs_fr = [uv for uv in suv]
                spuvs_fr = [pin_uv for pin_uv in spuv]
                ss_fr = [s for s in ss]
                # paste UVs
                for l, suv, spuv, ss in zip(
                        bm.faces[idx].loops, suvs_fr, spuvs_fr, ss_fr):
                    l[uv_layer].uv = suv
                    l[uv_layer].pin_uv = spuv
                if self.copy_seams is True:
                    l.edge.seam = ss

            bmesh.update_edit_mesh(obj.data)
            if self.copy_seams is True:
                obj.data.show_edge_seams = True

            self.report(
                {'INFO'}, "%s's UV coordinates are pasted" % (obj.name))

        return {'FINISHED'}


class MUV_CPUVObjPasteUVMenu(bpy.types.Menu):
    """
    Menu class: Paste UV coordinate per object
    """

    bl_idname = "object.muv_cpuv_obj_paste_uv_menu"
    bl_label = "Paste UV"
    bl_description = "Paste UV coordinate per object"

    def draw(self, _):
        layout = self.layout
        # create sub menu
        uv_maps = []
        for obj in bpy.data.objects:
            if hasattr(obj.data, "uv_textures") and obj.select:
                uv_maps.extend(obj.data.uv_textures.keys())
        uv_maps = list(set(uv_maps))
        layout.operator(
            MUV_CPUVObjPasteUV.bl_idname,
            text="[Default]", icon="IMAGE_COL").uv_map = ""
        for m in uv_maps:
            layout.operator(
                MUV_CPUVObjPasteUV.bl_idname,
                text=m, icon="IMAGE_COL").uv_map = m
