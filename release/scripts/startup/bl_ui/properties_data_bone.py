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

        ob = context.object
        bone = context.bone

        if bone and ob:
            pchan = ob.pose.bones[bone.name]

            row = layout.row()
            col = row.column()
            col.prop(pchan, "location")
            col.active = not (bone.parent and bone.use_connect)

            col = row.column()
            if pchan.rotation_mode == 'QUATERNION':
                col.prop(pchan, "rotation_quaternion", text="Rotation")
            elif pchan.rotation_mode == 'AXIS_ANGLE':
                #col.label(text="Rotation")
                #col.prop(pchan, "rotation_angle", text="Angle")
                #col.prop(pchan, "rotation_axis", text="Axis")
                col.prop(pchan, "rotation_axis_angle", text="Rotation")
            else:
                col.prop(pchan, "rotation_euler", text="Rotation")

            row.column().prop(pchan, "scale")

            layout.prop(pchan, "rotation_mode")

        elif context.edit_bone:
            bone = context.edit_bone
            row = layout.row()
            row.column().prop(bone, "head")
            row.column().prop(bone, "tail")

            col = row.column()
            sub = col.column(align=True)
            sub.label(text="Roll:")
            sub.prop(bone, "roll", text="")
            sub.label()
            sub.prop(bone, "lock")


class BONE_PT_transform_locks(BoneButtonsPanel, Panel):
    bl_label = "Transform Locks"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.mode == 'POSE' and context.bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        pchan = ob.pose.bones[bone.name]

        split = layout.split(percentage=0.1)

        col = split.column(align=True)
        col.label(text="")
        col.label(text="X:")
        col.label(text="Y:")
        col.label(text="Z:")

        col = split.column()
        col.active = not (bone.parent and bone.use_connect)
        col.prop(pchan, "lock_location", text="Location")

        col = split.column()
        col.prop(pchan, "lock_rotation", text="Rotation")

        col = split.column()
        col.prop(pchan, "lock_scale", text="Scale")

        if pchan.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            row = layout.row()
            row.prop(pchan, "lock_rotations_4d", text="Lock Rotation")

            sub = row.row()
            sub.active = pchan.lock_rotations_4d
            sub.prop(pchan, "lock_rotation_w", text="W")


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
        layout.prop(bone, "bbone_segments", text="Segments")

        col = layout.column()
        col.active = bone.bbone_segments > 1

        row = col.row()
        sub = row.column(align=True)
        sub.label(text="Curve XY Offsets:")
        sub.prop(bbone, "bbone_curveinx", text="In X")
        sub.prop(bbone, "bbone_curveoutx", text="Out X")
        sub.prop(bbone, "bbone_curveiny", text="In Y")
        sub.prop(bbone, "bbone_curveouty", text="Out Y")

        sub = row.column(align=True)
        sub.label("Roll:")
        sub.prop(bbone, "bbone_rollin", text="In")
        sub.prop(bbone, "bbone_rollout", text="Out")
        sub.prop(bone, "use_endroll_as_inroll")

        row = col.row()
        sub = row.column(align=True)
        sub.label(text="Scale:")
        sub.prop(bbone, "bbone_scalein", text="Scale In")
        sub.prop(bbone, "bbone_scaleout", text="Scale Out")

        sub = row.column(align=True)
        sub.label("Easing:")
        if pchan:
            # XXX: have these also be an overlay?
            sub.prop(bbone.bone, "bbone_in", text="Ease In")
            sub.prop(bbone.bone, "bbone_out", text="Ease Out")
        else:
            sub.prop(bone, "bbone_in", text="Ease In")
            sub.prop(bone, "bbone_out", text="Ease Out")

        if pchan:
            layout.separator()

            col = layout.column()
            col.prop(pchan, "use_bbone_custom_handles")

            row = col.row()
            row.active = pchan.use_bbone_custom_handles

            sub = row.column(align=True)
            sub.label(text="In:")
            sub.prop_search(pchan, "bbone_custom_handle_start", ob.pose, "bones", text="")
            sub.prop(pchan, "use_bbone_relative_start_handle", text="Relative")

            sub = row.column(align=True)
            sub.label(text="Out:")
            sub.prop_search(pchan, "bbone_custom_handle_end", ob.pose, "bones", text="")
            sub.prop(pchan, "use_bbone_relative_end_handle", text="Relative")


class BONE_PT_relations(BoneButtonsPanel, Panel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        arm = context.armature
        pchan = None

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
        elif bone is None:
            bone = context.edit_bone

        split = layout.split()

        col = split.column()
        col.label(text="Layers:")
        col.prop(bone, "layers", text="")

        col.separator()

        if ob and pchan:
            col.label(text="Bone Group:")
            col.prop_search(pchan, "bone_group", ob.pose, "bone_groups", text="")
            col.label(text="Object Children:")
            col.prop(bone, "use_relative_parent")

        col = split.column()
        col.label(text="Parent:")
        if context.bone:
            col.prop(bone, "parent", text="")
        else:
            col.prop_search(bone, "parent", arm, "edit_bones", text="")

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

        ob = context.object
        bone = context.bone
        pchan = None

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
        elif bone is None:
            bone = context.edit_bone

        if bone:
            split = layout.split()

            col = split.column()
            col.prop(bone, "hide", text="Hide")
            sub = col.column()
            sub.active = bool(pchan and pchan.custom_shape)
            sub.prop(bone, "show_wire", text="Wireframe")

            if pchan:
                col = split.column()

                col.label(text="Custom Shape:")
                col.prop(pchan, "custom_shape", text="")
                if pchan.custom_shape:
                    col.prop(pchan, "use_custom_shape_bone_size", text="Bone Size")
                    col.prop(pchan, "custom_shape_scale", text="Scale")
                    col.prop_search(pchan, "custom_shape_transform", ob.pose, "bones", text="At")


class BONE_PT_inverse_kinematics(BoneButtonsPanel, Panel):
    bl_label = "Inverse Kinematics"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.mode == 'POSE' and context.bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        pchan = ob.pose.bones[bone.name]

        row = layout.row()

        active = pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        split.prop(pchan, "lock_ik_x", text="X")
        split.active = active
        row = split.row()
        row.prop(pchan, "ik_stiffness_x", text="Stiffness", slider=True)
        row.active = pchan.lock_ik_x is False and active

        split = layout.split(percentage=0.25)
        sub = split.row()

        sub.prop(pchan, "use_ik_limit_x", text="Limit")
        sub.active = pchan.lock_ik_x is False and active
        sub = split.row(align=True)
        sub.prop(pchan, "ik_min_x", text="")
        sub.prop(pchan, "ik_max_x", text="")
        sub.active = pchan.lock_ik_x is False and pchan.use_ik_limit_x and active

        split = layout.split(percentage=0.25)
        split.prop(pchan, "lock_ik_y", text="Y")
        split.active = active
        row = split.row()
        row.prop(pchan, "ik_stiffness_y", text="Stiffness", slider=True)
        row.active = pchan.lock_ik_y is False and active

        split = layout.split(percentage=0.25)
        sub = split.row()

        sub.prop(pchan, "use_ik_limit_y", text="Limit")
        sub.active = pchan.lock_ik_y is False and active

        sub = split.row(align=True)
        sub.prop(pchan, "ik_min_y", text="")
        sub.prop(pchan, "ik_max_y", text="")
        sub.active = pchan.lock_ik_y is False and pchan.use_ik_limit_y and active

        split = layout.split(percentage=0.25)
        split.prop(pchan, "lock_ik_z", text="Z")
        split.active = active
        sub = split.row()
        sub.prop(pchan, "ik_stiffness_z", text="Stiffness", slider=True)
        sub.active = pchan.lock_ik_z is False and active

        split = layout.split(percentage=0.25)
        sub = split.row()

        sub.prop(pchan, "use_ik_limit_z", text="Limit")
        sub.active = pchan.lock_ik_z is False and active
        sub = split.row(align=True)
        sub.prop(pchan, "ik_min_z", text="")
        sub.prop(pchan, "ik_max_z", text="")
        sub.active = pchan.lock_ik_z is False and pchan.use_ik_limit_z and active

        split = layout.split(percentage=0.25)
        split.label(text="Stretch:")
        sub = split.row()
        sub.prop(pchan, "ik_stretch", text="", slider=True)
        sub.active = active

        if ob.pose.ik_solver == 'ITASC':
            split = layout.split()
            col = split.column()
            col.prop(pchan, "use_ik_rotation_control", text="Control Rotation")
            col.active = active
            col = split.column()
            col.prop(pchan, "ik_rotation_weight", text="Weight", slider=True)
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

        bone = context.bone

        if not bone:
            bone = context.edit_bone

        layout.active = bone.use_deform

        row = layout.row()

        col = row.column(align=True)
        col.label(text="Envelope:")
        col.prop(bone, "envelope_distance", text="Distance")
        col.prop(bone, "envelope_weight", text="Weight")
        col.prop(bone, "use_envelope_multiply", text="Multiply")

        col = row.column(align=True)
        col.label(text="Envelope Radius:")
        col.prop(bone, "head_radius", text="Head")
        col.prop(bone, "tail_radius", text="Tail")


class BONE_PT_custom_props(BoneButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _property_type = bpy.types.Bone, bpy.types.EditBone, bpy.types.PoseBone

    @property
    def _context_path(self):
        obj = bpy.context.object
        if obj and obj.mode == 'POSE':
            return "active_pose_bone"
        else:
            return "active_bone"

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
