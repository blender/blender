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

import bpy
import bmesh
from bpy.props import (
        BoolProperty,
        IntProperty,
        )
from . import muv_common


class MUV_FlipRot(bpy.types.Operator):
    """
    Operation class: Flip and Rotate UV coordinate
    """

    bl_idname = "uv.muv_fliprot"
    bl_label = "Flip/Rotate UV"
    bl_description = "Flip/Rotate UV coordinate"
    bl_options = {'REGISTER', 'UNDO'}

    flip = BoolProperty(
        name="Flip UV",
        description="Flip UV...",
        default=False
    )
    rotate = IntProperty(
        default=0,
        name="Rotate UV",
        min=0,
        max=30
    )
    seams = BoolProperty(
        name="Seams",
        description="Seams",
        default=True
    )

    def execute(self, context):
        self.report({'INFO'}, "Flip/Rotate UV")
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV layer
        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}
        uv_layer = bm.loops.layers.uv.verify()

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
        self.report({'INFO'}, "%d face(s) are selected" % len(dest_uvs))

        # paste
        for idx, duvs, dpuvs, dss in zip(dest_face_indices, dest_uvs, dest_pin_uvs, dest_seams):
            duvs_fr = [uv for uv in duvs]
            dpuvs_fr = [pin_uv for pin_uv in dpuvs]
            dss_fr = [s for s in dss]
            # flip UVs
            if self.flip is True:
                duvs_fr.reverse()
                dpuvs_fr.reverse()
                dss_fr.reverse()
            # rotate UVs
            for _ in range(self.rotate):
                uv = duvs_fr.pop()
                pin_uv = dpuvs_fr.pop()
                s = dss_fr.pop()
                duvs_fr.insert(0, uv)
                dpuvs_fr.insert(0, pin_uv)
                dss_fr.insert(0, s)
            # paste UVs
            for l, duv, dpuv, ds in zip(
                    bm.faces[idx].loops, duvs_fr, dpuvs_fr, dss_fr):
                l[uv_layer].uv = duv
                l[uv_layer].pin_uv = dpuv
                if self.seams is True:
                    l.edge.seam = ds

        self.report({'INFO'}, "%d face(s) are flipped/rotated" % len(dest_uvs))

        bmesh.update_edit_mesh(obj.data)
        if self.seams is True:
            obj.data.show_edge_seams = True

        return {'FINISHED'}
