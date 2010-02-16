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
from rna_prop_ui import PropertyPanel

narrowui = 180


class BoneButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "bone"

    def poll(self, context):
        return (context.bone or context.edit_bone)


class BONE_PT_context_bone(BoneButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        bone = context.bone
        if not bone:
            bone = context.edit_bone

        row = layout.row()
        row.label(text="", icon='BONE_DATA')
        row.prop(bone, "name", text="")


class BONE_PT_custom_props(BoneButtonsPanel, PropertyPanel):

    @property
    def _context_path(self):
        obj = bpy.context.object
        if obj and obj.mode == 'POSE':
            return "active_pose_bone"
        else:
            return "active_bone"


class BONE_PT_transform(BoneButtonsPanel):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        wide_ui = context.region.width > narrowui

        if not bone:
            bone = context.edit_bone
            if wide_ui:
                row = layout.row()
                row.column().prop(bone, "head")
                row.column().prop(bone, "tail")

                col = row.column()
                sub = col.column(align=True)
                sub.label(text="Roll:")
                sub.prop(bone, "roll", text="")
                sub.label()
                sub.prop(bone, "locked")
            else:
                col = layout.column()
                col.prop(bone, "head")
                col.prop(bone, "tail")
                col.prop(bone, "roll")
                col.prop(bone, "locked")

        else:
            pchan = ob.pose.bones[context.bone.name]

            if wide_ui:
                row = layout.row()
                col = row.column()
                col.prop(pchan, "location")
                col.active = not (bone.parent and bone.connected)

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
            else:
                col = layout.column()
                sub = col.column()
                sub.active = not (bone.parent and bone.connected)
                sub.prop(pchan, "location")
                col.label(text="Rotation:")
                col.prop(pchan, "rotation_mode", text="")
                if pchan.rotation_mode == 'QUATERNION':
                    col.prop(pchan, "rotation_quaternion", text="")
                elif pchan.rotation_mode == 'AXIS_ANGLE':
                    col.prop(pchan, "rotation_axis_angle", text="")
                else:
                    col.prop(pchan, "rotation_euler", text="")
                col.prop(pchan, "scale")


class BONE_PT_transform_locks(BoneButtonsPanel):
    bl_label = "Transform Locks"
    bl_default_closed = True

    def poll(self, context):
        return context.bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        pchan = ob.pose.bones[context.bone.name]

        row = layout.row()
        col = row.column()
        col.prop(pchan, "lock_location")
        col.active = not (bone.parent and bone.connected)

        col = row.column()
        if pchan.rotation_mode in ('QUATERNION', 'AXIS_ANGLE'):
            col.prop(pchan, "lock_rotations_4d", text="Lock Rotation")
            if pchan.lock_rotations_4d:
                col.prop(pchan, "lock_rotation_w", text="W")
            col.prop(pchan, "lock_rotation", text="")
        else:
            col.prop(pchan, "lock_rotation", text="Rotation")

        row.column().prop(pchan, "lock_scale")


class BONE_PT_relations(BoneButtonsPanel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        arm = context.armature
        wide_ui = context.region.width > narrowui

        if not bone:
            bone = context.edit_bone
            pchan = None
        else:
            pchan = ob.pose.bones[context.bone.name]

        split = layout.split()

        col = split.column()
        col.label(text="Layers:")
        col.prop(bone, "layer", text="")

        col.separator()

        if ob and pchan:
            col.label(text="Bone Group:")
            col.prop_object(pchan, "bone_group", ob.pose, "bone_groups", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Parent:")
        if context.bone:
            col.prop(bone, "parent", text="")
        else:
            col.prop_object(bone, "parent", arm, "edit_bones", text="")

        sub = col.column()
        sub.active = (bone.parent is not None)
        sub.prop(bone, "connected")
        sub.prop(bone, "hinge", text="Inherit Rotation")
        sub.prop(bone, "inherit_scale", text="Inherit Scale")
        sub = col.column()
        sub.active = (not bone.parent or not bone.connected)
        sub.prop(bone, "local_location", text="Local Location")


class BONE_PT_display(BoneButtonsPanel):
    bl_label = "Display"

    def poll(self, context):
        return context.bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        wide_ui = context.region.width > narrowui

        if not bone:
            bone = context.edit_bone
            pchan = None
        else:
            pchan = ob.pose.bones[context.bone.name]

        if ob and pchan:

            split = layout.split()

            col = split.column()
            col.prop(bone, "draw_wire", text="Wireframe")
            col.prop(bone, "hidden", text="Hide")

            if wide_ui:
                col = split.column()

            col.label(text="Custom Shape:")
            col.prop(pchan, "custom_shape", text="")
            if pchan.custom_shape:
                col.prop_object(pchan, "custom_shape_transform", ob.pose, "bones", text="At")


class BONE_PT_inverse_kinematics(BoneButtonsPanel):
    bl_label = "Inverse Kinematics"
    bl_default_closed = True

    def poll(self, context):
        return context.active_pose_bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        pchan = ob.pose.bones[bone.name]
        wide_ui = context.region.width > narrowui

        split = layout.split(percentage=0.25)
        split.prop(pchan, "ik_dof_x", text="X")
        split.active = pchan.has_ik
        row = split.row()
        row.prop(pchan, "ik_stiffness_x", text="Stiffness", slider=True)
        row.active = pchan.ik_dof_x and pchan.has_ik

        if wide_ui:
            split = layout.split(percentage=0.25)
            sub = split.row()
        else:
            sub = layout.column(align=True)
        sub.prop(pchan, "ik_limit_x", text="Limit")
        sub.active = pchan.ik_dof_x and pchan.has_ik
        if wide_ui:
            sub = split.row(align=True)
        sub.prop(pchan, "ik_min_x", text="")
        sub.prop(pchan, "ik_max_x", text="")
        sub.active = pchan.ik_dof_x and pchan.ik_limit_x and pchan.has_ik

        split = layout.split(percentage=0.25)
        split.prop(pchan, "ik_dof_y", text="Y")
        split.active = pchan.has_ik and pchan.has_ik
        row = split.row()
        row.prop(pchan, "ik_stiffness_y", text="Stiffness", slider=True)
        row.active = pchan.ik_dof_y and pchan.has_ik

        if wide_ui:
            split = layout.split(percentage=0.25)
            sub = split.row()
        else:
            sub = layout.column(align=True)
        sub.prop(pchan, "ik_limit_y", text="Limit")
        sub.active = pchan.ik_dof_y and pchan.has_ik
        if wide_ui:
            sub = split.row(align=True)
        sub.prop(pchan, "ik_min_y", text="")
        sub.prop(pchan, "ik_max_y", text="")
        sub.active = pchan.ik_dof_y and pchan.ik_limit_y and pchan.has_ik

        split = layout.split(percentage=0.25)
        split.prop(pchan, "ik_dof_z", text="Z")
        split.active = pchan.has_ik and pchan.has_ik
        sub = split.row()
        sub.prop(pchan, "ik_stiffness_z", text="Stiffness", slider=True)
        sub.active = pchan.ik_dof_z and pchan.has_ik

        if wide_ui:
            split = layout.split(percentage=0.25)
            sub = split.row()
        else:
            sub = layout.column(align=True)
        sub.prop(pchan, "ik_limit_z", text="Limit")
        sub.active = pchan.ik_dof_z and pchan.has_ik
        if wide_ui:
            sub = split.row(align=True)
        sub.prop(pchan, "ik_min_z", text="")
        sub.prop(pchan, "ik_max_z", text="")
        sub.active = pchan.ik_dof_z and pchan.ik_limit_z and pchan.has_ik
        split = layout.split()
        split.prop(pchan, "ik_stretch", text="Stretch", slider=True)
        if wide_ui:
            split.label()
        split.active = pchan.has_ik

        if ob.pose.ik_solver == 'ITASC':
            split = layout.split()
            col = split.column()
            col.prop(pchan, "ik_rot_control", text="Control Rotation")
            col.active = pchan.has_ik
            if wide_ui:
                col = split.column()
            col.prop(pchan, "ik_rot_weight", text="Weight", slider=True)
            col.active = pchan.has_ik
            # not supported yet
            #row = layout.row()
            #row.prop(pchan, "ik_lin_control", text="Joint Size")
            #row.prop(pchan, "ik_lin_weight", text="Weight", slider=True)


class BONE_PT_deform(BoneButtonsPanel):
    bl_label = "Deform"
    bl_default_closed = True

    def draw_header(self, context):
        bone = context.bone

        if not bone:
            bone = context.edit_bone

        self.layout.prop(bone, "deform", text="")

    def draw(self, context):
        layout = self.layout

        bone = context.bone
        wide_ui = context.region.width > narrowui

        if not bone:
            bone = context.edit_bone

        layout.active = bone.deform

        split = layout.split()

        col = split.column()
        col.label(text="Envelope:")

        sub = col.column(align=True)
        sub.prop(bone, "envelope_distance", text="Distance")
        sub.prop(bone, "envelope_weight", text="Weight")
        col.prop(bone, "multiply_vertexgroup_with_envelope", text="Multiply")

        sub = col.column(align=True)
        sub.label(text="Radius:")
        sub.prop(bone, "head_radius", text="Head")
        sub.prop(bone, "tail_radius", text="Tail")

        if wide_ui:
            col = split.column()
        col.label(text="Curved Bones:")

        sub = col.column(align=True)
        sub.prop(bone, "bbone_segments", text="Segments")
        sub.prop(bone, "bbone_in", text="Ease In")
        sub.prop(bone, "bbone_out", text="Ease Out")

        col.label(text="Offset:")
        col.prop(bone, "cyclic_offset")

classes = [
    BONE_PT_context_bone,
    BONE_PT_transform,
    BONE_PT_transform_locks,
    BONE_PT_relations,
    BONE_PT_display,
    BONE_PT_inverse_kinematics,
    BONE_PT_deform,

    BONE_PT_custom_props]

def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)

def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()

