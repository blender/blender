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

# <pep8 compliant>

import bpy
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class BoneButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "bone"

    @classmethod
    def poll(cls, context):
        return (context.bone or context.edit_bone)


class BONE_PT_context_bone(BoneButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        bone = context.bone
        if not bone:
            bone = context.edit_bone

        row = layout.row()
        row.label(text="", icon='BONE_DATA')
        row.prop(bone, "name", text="")


class BONE_PT_transform(BoneButtonsPanel, Panel):
    bl_label = "Transform"

    @classmethod
    def poll(cls, context):
        if context.edit_bone:
            return True

        ob = context.object
        return ob and ob.mode == 'POSE' and context.bone

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        bone = context.bone

        col = layout.column()

        if bone and ob:
            pchan = ob.pose.bones[bone.name]
            col.active = not (bone.parent and bone.use_connect)

            sub = col.row(align=True)
            sub.prop(pchan, "location")
            sub.prop(pchan, "lock_location", text="")

            col = layout.column()
            if pchan.rotation_mode == 'QUATERNION':
                sub = col.row(align=True)
                sub.prop(pchan, "rotation_quaternion", text="Rotation")
                subsub = sub.column(align=True)
                subsub.prop(pchan, "lock_rotation_w", text="")
                subsub.prop(pchan, "lock_rotation", text="")
            elif pchan.rotation_mode == 'AXIS_ANGLE':
                # col.label(text="Rotation")
                #col.prop(pchan, "rotation_angle", text="Angle")
                #col.prop(pchan, "rotation_axis", text="Axis")
                sub = col.row(align=True)
                sub.prop(pchan, "rotation_axis_angle", text="Rotation")
                subsub = sub.column(align=True)
                subsub.prop(pchan, "lock_rotation_w", text="")
                subsub.prop(pchan, "lock_rotation", text="")
            else:
                sub = col.row(align=True)
                sub.prop(pchan, "rotation_euler", text="Rotation")
                sub.prop(pchan, "lock_rotation", text="")

            col = layout.column()
            sub = col.row(align=True)
            sub.prop(pchan, "scale")
            sub.prop(pchan, "lock_scale", text="")

            col = layout.column()
            col.prop(pchan, "rotation_mode")

        elif context.edit_bone:
            bone = context.edit_bone
            col = layout.column()
            col.prop(bone, "head")
            col.prop(bone, "tail")

            col = layout.column()
            col.prop(bone, "roll")
            col.prop(bone, "lock")

            col = layout.column()
            col.prop(bone, "tail_radius")
            col.prop(bone, "envelope_distance")


class BONE_PT_curved(BoneButtonsPanel, Panel):
    bl_label = "Bendy Bones"
    #bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        ob = context.object
        bone = context.bone
        # arm = context.armature
        pchan = None

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
            bbone = pchan
        elif bone is None:
            bone = context.edit_bone
            bbone = bone
        else:
            bbone = bone

        layout = self.layout
        layout.use_property_split = True

        layout.prop(bone, "bbone_segments", text="Segments")

        col = layout.column()
        col.active = bone.bbone_segments > 1

        col = layout.column(align=True)
        col.prop(bbone, "bbone_curveinx", text="Curve In X")
        col.prop(bbone, "bbone_curveiny", text="In Y")

        col = layout.column(align=True)
        col.prop(bbone, "bbone_curveoutx", text="Curve Out X")
        col.prop(bbone, "bbone_curveouty", text="Out Y")

        col = layout.column(align=True)
        col.prop(bbone, "bbone_rollin", text="Roll In")
        col.prop(bbone, "bbone_rollout", text="Out")
        col.prop(bone, "use_endroll_as_inroll")

        col = layout.column(align=True)
        col.prop(bbone, "bbone_scalein", text="Scale In")
        col.prop(bbone, "bbone_scaleout", text="Out")

        col = layout.column(align=True)
        col.prop(bbone, "bbone_easein", text="Ease In")
        col.prop(bbone, "bbone_easeout", text="Out")

        if pchan:
            layout.separator()

            col = layout.column()
            col.use_property_split = False
            col.prop(pchan, "use_bbone_custom_handles")

            col = layout.column(align=True)
            col.active = pchan.use_bbone_custom_handles
            col.use_property_split = True

            sub = col.column()
            sub.prop_search(pchan, "bbone_custom_handle_start", ob.pose, "bones", text="Custom Handle Start")
            sub.prop_search(pchan, "bbone_custom_handle_end", ob.pose, "bones", text="End")

            sub = col.column(align=True)
            sub.prop(pchan, "use_bbone_relative_start_handle", text="Relative Handle Start")
            sub.prop(pchan, "use_bbone_relative_end_handle", text="End")


class BONE_PT_relations(BoneButtonsPanel, Panel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        bone = context.bone
        arm = context.armature
        pchan = None

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
        elif bone is None:
            bone = context.edit_bone

        col = layout.column()
        col.use_property_split = False
        col.prop(bone, "layers", text="")
        col.use_property_split = True
        col = layout.column()

        col.separator()

        if context.bone:
            col.prop(bone, "parent")
        else:
            col.prop_search(bone, "parent", arm, "edit_bones")

        if ob and pchan:
            col.prop(bone, "use_relative_parent")
            col.prop_search(pchan, "bone_group", ob.pose, "bone_groups", text="Bone Group")

        sub = col.column()
        sub.active = (bone.parent is not None)
        sub.prop(bone, "use_connect")
        sub.prop(bone, "use_inherit_rotation")
        sub.prop(bone, "use_inherit_scale")
        sub = col.column()
        sub.active = (not bone.parent or not bone.use_connect)
        sub.prop(bone, "use_local_location")


class BONE_PT_display(BoneButtonsPanel, Panel):
    bl_label = "Display"

    @classmethod
    def poll(cls, context):
        return context.bone

    def draw(self, context):
        # note. this works ok in edit-mode but isn't
        # all that useful so disabling for now.
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        bone = context.bone
        pchan = None

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
        elif bone is None:
            bone = context.edit_bone

        if bone:

            col = layout.column()
            col.prop(bone, "hide", text="Hide")
            sub = col.column()
            sub.active = bool(pchan and pchan.custom_shape)
            sub.prop(bone, "show_wire", text="Wireframe")

            if pchan:
                col = layout.column()
                col.prop(pchan, "custom_shape")
                if pchan.custom_shape:
                    col.prop(pchan, "use_custom_shape_bone_size", text="Bone Size")
                    col.prop(pchan, "custom_shape_scale", text="Scale")
                    col.prop_search(pchan, "custom_shape_transform", ob.pose, "bones")


class BONE_PT_inverse_kinematics(BoneButtonsPanel, Panel):
    bl_label = "Inverse Kinematics"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.mode == 'POSE' and context.bone

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        bone = context.bone
        pchan = ob.pose.bones[bone.name]

        active = pchan.is_in_ik_chain

        col = layout.column()
        col.prop(pchan, "ik_stretch", slider=True)
        col.active = active

        layout.separator()

        col = layout.column(align=True)

        col.prop(pchan, "lock_ik_x", text="Lock IK X")
        col.prop(pchan, "lock_ik_y", text="Y")
        col.prop(pchan, "lock_ik_z", text="Z")

        col = layout.column(align=True)

        sub = col.column(align=True)
        sub.active = pchan.lock_ik_x is False and active
        sub.prop(pchan, "ik_stiffness_x", text="Stiffness X", slider=True)
        sub = col.column(align=True)
        sub.active = pchan.lock_ik_y is False and active
        sub.prop(pchan, "ik_stiffness_y", text="Y", slider=True)
        sub = col.column(align=True)
        sub.active = pchan.lock_ik_z is False and active
        sub.prop(pchan, "ik_stiffness_z", text="Z", slider=True)

        col = layout.column(align=True)

        sub = col.column()
        sub.active = pchan.lock_ik_x is False and active
        sub.prop(pchan, "use_ik_limit_x", text="Limit X")

        sub = col.column(align=True)
        sub.active = pchan.lock_ik_x is False and pchan.use_ik_limit_x and active
        sub.prop(pchan, "ik_min_x", text="Min")
        sub.prop(pchan, "ik_max_x", text="Max")

        col.separator()

        sub = col.column()
        sub.active = pchan.lock_ik_y is False and active
        sub.prop(pchan, "use_ik_limit_y", text="Limit Y")

        sub = col.column(align=True)
        sub.active = pchan.lock_ik_y is False and pchan.use_ik_limit_y and active
        sub.prop(pchan, "ik_min_y", text="Min")
        sub.prop(pchan, "ik_max_y", text="Max")

        col.separator()

        sub = col.column()
        sub.active = pchan.lock_ik_z is False and active
        sub.prop(pchan, "use_ik_limit_z", text="Limit Z")

        sub = col.column(align=True)
        sub.active = pchan.lock_ik_z is False and pchan.use_ik_limit_z and active
        sub.prop(pchan, "ik_min_z", text="Min")
        sub.prop(pchan, "ik_max_z", text="Max")

        col.separator()

        if ob.pose.ik_solver == 'ITASC':

            col = layout.column()
            col.prop(pchan, "use_ik_rotation_control", text="Control Rotation")
            col.active = active

            col = layout.column()

            col.prop(pchan, "ik_rotation_weight", text="IK Rotation Weight", slider=True)
            col.active = active
            # not supported yet
            #row = layout.row()
            #row.prop(pchan, "use_ik_linear_control", text="Joint Size")
            #row.prop(pchan, "ik_linear_weight", text="Weight", slider=True)


class BONE_PT_deform(BoneButtonsPanel, Panel):
    bl_label = "Deform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        bone = context.bone

        if not bone:
            bone = context.edit_bone

        self.layout.prop(bone, "use_deform", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        bone = context.bone

        if not bone:
            bone = context.edit_bone

        layout.active = bone.use_deform

        col = layout.column()
        col.prop(bone, "envelope_distance", text="Envelope Distance")
        col.prop(bone, "envelope_weight", text="Envelope Weight")
        col.prop(bone, "use_envelope_multiply", text="Envelope Multiply")

        col.separator()

        col = layout.column(align=True)
        col.prop(bone, "head_radius", text="Radius Head")
        col.prop(bone, "tail_radius", text="Tail")


class BONE_PT_custom_props(BoneButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    _property_type = bpy.types.Bone, bpy.types.EditBone, bpy.types.PoseBone

    @property
    def _context_path(self):
        obj = bpy.context.object
        if obj and obj.mode == 'POSE':
            return "active_pose_bone"
        else:
            return "active_bone"


classes = (
    BONE_PT_context_bone,
    BONE_PT_transform,
    BONE_PT_curved,
    BONE_PT_relations,
    BONE_PT_display,
    BONE_PT_inverse_kinematics,
    BONE_PT_deform,
    BONE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
