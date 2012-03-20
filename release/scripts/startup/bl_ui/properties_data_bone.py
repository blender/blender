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


class BoneButtonsPanel():
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

        row = layout.row()
        col = row.column()
        col.prop(pchan, "lock_location")
        col.active = not (bone.parent and bone.use_connect)

        col = row.column()
        if pchan.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            col.prop(pchan, "lock_rotations_4d", text="Lock Rotation")
            if pchan.lock_rotations_4d:
                col.prop(pchan, "lock_rotation_w", text="W")
            col.prop(pchan, "lock_rotation", text="")
        else:
            col.prop(pchan, "lock_rotation", text="Rotation")

        row.column().prop(pchan, "lock_scale")


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

        col = split.column()
        col.label(text="Parent:")
        if context.bone:
            col.prop(bone, "parent", text="")
        else:
            col.prop_search(bone, "parent", arm, "edit_bones", text="")

        sub = col.column()
        sub.active = (bone.parent is not None)
        sub.prop(bone, "use_connect")
        sub.prop(bone, "use_inherit_rotation", text="Inherit Rotation")
        sub.prop(bone, "use_inherit_scale", text="Inherit Scale")
        sub = col.column()
        sub.active = (not bone.parent or not bone.use_connect)
        sub.prop(bone, "use_local_location", text="Local Location")


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
            sub.active = bool(pchan.custom_shape)
            sub.prop(bone, "show_wire", text="Wireframe")

            if pchan:
                col = split.column()

                col.label(text="Custom Shape:")
                col.prop(pchan, "custom_shape", text="")
                if pchan.custom_shape:
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
        row.prop(ob.pose, "ik_solver")

        split = layout.split(percentage=0.25)
        split.prop(pchan, "lock_ik_x", icon='LOCKED' if pchan.lock_ik_x else 'UNLOCKED', text="X")
        split.active = pchan.is_in_ik_chain
        row = split.row()
        row.prop(pchan, "ik_stiffness_x", text="Stiffness", slider=True)
        row.active = pchan.lock_ik_x == False and pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        sub = split.row()

        sub.prop(pchan, "use_ik_limit_x", text="Limit")
        sub.active = pchan.lock_ik_x == False and pchan.is_in_ik_chain
        sub = split.row(align=True)
        sub.prop(pchan, "ik_min_x", text="")
        sub.prop(pchan, "ik_max_x", text="")
        sub.active = pchan.lock_ik_x == False and pchan.use_ik_limit_x and pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        split.prop(pchan, "lock_ik_y", icon='LOCKED' if pchan.lock_ik_y else 'UNLOCKED', text="Y")
        split.active = pchan.is_in_ik_chain
        row = split.row()
        row.prop(pchan, "ik_stiffness_y", text="Stiffness", slider=True)
        row.active = pchan.lock_ik_y == False and pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        sub = split.row()

        sub.prop(pchan, "use_ik_limit_y", text="Limit")
        sub.active = pchan.lock_ik_y == False and pchan.is_in_ik_chain

        sub = split.row(align=True)
        sub.prop(pchan, "ik_min_y", text="")
        sub.prop(pchan, "ik_max_y", text="")
        sub.active = pchan.lock_ik_y == False and pchan.use_ik_limit_y and pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        split.prop(pchan, "lock_ik_z", icon='LOCKED' if pchan.lock_ik_z else 'UNLOCKED', text="Z")
        split.active = pchan.is_in_ik_chain
        sub = split.row()
        sub.prop(pchan, "ik_stiffness_z", text="Stiffness", slider=True)
        sub.active = pchan.lock_ik_z == False and pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        sub = split.row()

        sub.prop(pchan, "use_ik_limit_z", text="Limit")
        sub.active = pchan.lock_ik_z == False and pchan.is_in_ik_chain
        sub = split.row(align=True)
        sub.prop(pchan, "ik_min_z", text="")
        sub.prop(pchan, "ik_max_z", text="")
        sub.active = pchan.lock_ik_z == False and pchan.use_ik_limit_z and pchan.is_in_ik_chain

        split = layout.split(percentage=0.25)
        split.label(text="Stretch:")
        sub = split.row()
        sub.prop(pchan, "ik_stretch", text="", slider=True)
        sub.active = pchan.is_in_ik_chain

        if ob.pose.ik_solver == 'ITASC':
            split = layout.split()
            col = split.column()
            col.prop(pchan, "use_ik_rotation_control", text="Control Rotation")
            col.active = pchan.is_in_ik_chain
            col = split.column()
            col.prop(pchan, "ik_rotation_weight", text="Weight", slider=True)
            col.active = pchan.is_in_ik_chain
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

        split = layout.split()

        col = split.column()
        col.label(text="Envelope:")

        sub = col.column(align=True)
        sub.prop(bone, "envelope_distance", text="Distance")
        sub.prop(bone, "envelope_weight", text="Weight")
        col.prop(bone, "use_envelope_multiply", text="Multiply")

        sub = col.column(align=True)
        sub.label(text="Radius:")
        sub.prop(bone, "head_radius", text="Head")
        sub.prop(bone, "tail_radius", text="Tail")

        col = split.column()
        col.label(text="Curved Bones:")

        sub = col.column(align=True)
        sub.prop(bone, "bbone_segments", text="Segments")
        sub.prop(bone, "bbone_in", text="Ease In")
        sub.prop(bone, "bbone_out", text="Ease Out")

        col.label(text="Offset:")
        col.prop(bone, "use_cyclic_offset")


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
