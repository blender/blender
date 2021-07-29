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
        EnumProperty,
        FloatProperty,
        )
from . import muv_common


class MUV_UnwrapConstraint(bpy.types.Operator):
    """
    Operation class: Unwrap with constrain UV coordinate
    """

    bl_idname = "uv.muv_unwrap_constraint"
    bl_label = "Unwrap Constraint"
    bl_description = "Unwrap while keeping uv coordinate"
    bl_options = {'REGISTER', 'UNDO'}

    # property for original unwrap
    method = EnumProperty(
        name="Method",
        description="Unwrapping method",
        items=[
            ('ANGLE_BASED', 'Angle Based', 'Angle Based'),
            ('CONFORMAL', 'Conformal', 'Conformal')
        ],
        default='ANGLE_BASED')
    fill_holes = BoolProperty(
        name="Fill Holes",
        description="Virtual fill holes in meshes before unwrapping",
        default=True)
    correct_aspect = BoolProperty(
        name="Correct Aspect",
        description="Map UVs taking image aspect ratio into account",
        default=True)
    use_subsurf_data = BoolProperty(
        name="Use Subsurf Modifier",
        description="""Map UVs taking vertex position after subsurf
                       into account""",
        default=False)
    margin = FloatProperty(
        name="Margin",
        description="Space between islands",
        max=1.0,
        min=0.0,
        default=0.001)

    # property for this operation
    u_const = BoolProperty(
        name="U-Constraint",
        description="Keep UV U-axis coordinate",
        default=False)
    v_const = BoolProperty(
        name="V-Constraint",
        description="Keep UV V-axis coordinate",
        default=False)

    def execute(self, _):
        obj = bpy.context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}
        uv_layer = bm.loops.layers.uv.verify()

        # get original UV coordinate
        faces = [f for f in bm.faces if f.select]
        uv_list = []
        for f in faces:
            uvs = [l[uv_layer].uv.copy() for l in f.loops]
            uv_list.append(uvs)

        # unwrap
        bpy.ops.uv.unwrap(
            method=self.method,
            fill_holes=self.fill_holes,
            correct_aspect=self.correct_aspect,
            use_subsurf_data=self.use_subsurf_data,
            margin=self.margin)

        # when U/V-Constraint is checked, revert original coordinate
        for f, uvs in zip(faces, uv_list):
            for l, uv in zip(f.loops, uvs):
                if self.u_const:
                    l[uv_layer].uv.x = uv.x
                if self.v_const:
                    l[uv_layer].uv.y = uv.y

        # update mesh
        bmesh.update_edit_mesh(obj.data)

        return {'FINISHED'}
