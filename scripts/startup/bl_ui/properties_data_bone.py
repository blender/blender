# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel
import rna_prop_ui

from bpy.app.translations import contexts as i18n_contexts


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

            row = col.row(align=True)
            row.prop(pchan, "location")
            row.use_property_decorate = False
            row.prop(pchan, "lock_location", text="", emboss=False, icon='DECORATE_UNLOCKED')

            rotation_mode = pchan.rotation_mode
            if rotation_mode == 'QUATERNION':
                col = layout.column()
                row = col.row(align=True)
                row.prop(pchan, "rotation_quaternion", text="Rotation")
                sub = row.column(align=True)
                sub.use_property_decorate = False
                sub.prop(pchan, "lock_rotation_w", text="", emboss=False, icon='DECORATE_UNLOCKED')
                sub.prop(pchan, "lock_rotation", text="", emboss=False, icon='DECORATE_UNLOCKED')
            elif rotation_mode == 'AXIS_ANGLE':
                col = layout.column()
                row = col.row(align=True)
                row.prop(pchan, "rotation_axis_angle", text="Rotation")

                sub = row.column(align=True)
                sub.use_property_decorate = False
                sub.prop(pchan, "lock_rotation_w", text="", emboss=False, icon='DECORATE_UNLOCKED')
                sub.prop(pchan, "lock_rotation", text="", emboss=False, icon='DECORATE_UNLOCKED')
            else:
                col = layout.column()
                row = col.row(align=True)
                row.prop(pchan, "rotation_euler", text="Rotation")
                row.use_property_decorate = False
                row.prop(pchan, "lock_rotation", text="", emboss=False, icon='DECORATE_UNLOCKED')
            row = layout.row(align=True)
            row.prop(pchan, "rotation_mode", text="Mode")
            row.label(text="", icon='BLANK1')

            col = layout.column()
            row = col.row(align=True)
            row.prop(pchan, "scale")
            row.use_property_decorate = False
            row.prop(pchan, "lock_scale", text="", emboss=False, icon='DECORATE_UNLOCKED')

        elif context.edit_bone:
            bone = context.edit_bone
            col = layout.column()
            col.prop(bone, "head")
            col.prop(bone, "tail")

            col = layout.column()
            col.prop(bone, "roll")
            col.prop(bone, "length")
            col.prop(bone, "lock")


class BONE_PT_curved(BoneButtonsPanel, Panel):
    bl_label = "Bendy Bones"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        ob = context.object
        bone = context.bone
        arm = context.armature
        bone_list = "bones"

        if ob and bone:
            bbone = ob.pose.bones[bone.name]
        elif bone is None:
            bone = context.edit_bone
            bbone = bone
            bone_list = "edit_bones"
        else:
            bbone = bone

        layout = self.layout
        layout.use_property_split = True

        layout.prop(bone, "bbone_segments", text="Segments")

        col = layout.column(align=True)
        col.prop(bone, "bbone_x", text="Display Size X")
        col.prop(bone, "bbone_z", text="Z")

        topcol = layout.column()
        topcol.active = bone.bbone_segments > 1

        topcol.prop(bone, "bbone_mapping_mode", text="Vertex Mapping")

        col = topcol.column(align=True)
        col.prop(bbone, "bbone_curveinx", text="Curve In X")
        col.prop(bbone, "bbone_curveinz", text="Z")

        col = topcol.column(align=True)
        col.prop(bbone, "bbone_curveoutx", text="Curve Out X")
        col.prop(bbone, "bbone_curveoutz", text="Z")

        col = topcol.column(align=True)
        col.prop(bbone, "bbone_rollin", text="Roll In")
        col.prop(bbone, "bbone_rollout", text="Out", text_ctxt=i18n_contexts.id_armature)
        col.prop(bone, "use_endroll_as_inroll")

        col = topcol.column(align=True)
        col.prop(bbone, "bbone_scalein", text="Scale In")

        col = topcol.column(align=True)
        col.prop(bbone, "bbone_scaleout", text="Scale Out")

        col = topcol.column(align=True)
        col.prop(bbone, "bbone_easein", text="Ease In", text_ctxt=i18n_contexts.id_armature)
        col.prop(bbone, "bbone_easeout", text="Out", text_ctxt=i18n_contexts.id_armature)
        col.prop(bone, "use_scale_easing")

        col = topcol.column(align=True)
        col.prop(bone, "bbone_handle_type_start", text="Start Handle")

        col2 = col.column(align=True)
        col2.active = (bone.bbone_handle_type_start != 'AUTO')
        col2.prop_search(bone, "bbone_custom_handle_start", arm, bone_list, text="Custom")

        row = col.row(align=True)
        row.use_property_split = False
        split = row.split(factor=0.4)
        split.alignment = 'RIGHT'
        split.label(text="Scale")
        split2 = split.split(factor=0.7)
        row2 = split2.row(align=True)
        row2.prop(bone, "bbone_handle_use_scale_start", index=0, text="X", toggle=True)
        row2.prop(bone, "bbone_handle_use_scale_start", index=1, text="Y", toggle=True)
        row2.prop(bone, "bbone_handle_use_scale_start", index=2, text="Z", toggle=True)
        split2.prop(bone, "bbone_handle_use_ease_start", text="Ease", text_ctxt=i18n_contexts.id_armature, toggle=True)
        row.label(icon='BLANK1')

        col = topcol.column(align=True)
        col.prop(bone, "bbone_handle_type_end", text="End Handle")

        col2 = col.column(align=True)
        col2.active = (bone.bbone_handle_type_end != 'AUTO')
        col2.prop_search(bone, "bbone_custom_handle_end", arm, bone_list, text="Custom")

        row = col.row(align=True)
        row.use_property_split = False
        split = row.split(factor=0.4)
        split.alignment = 'RIGHT'
        split.label(text="Scale")
        split2 = split.split(factor=0.7)
        row2 = split2.row(align=True)
        row2.prop(bone, "bbone_handle_use_scale_end", index=0, text="X", toggle=True)
        row2.prop(bone, "bbone_handle_use_scale_end", index=1, text="Y", toggle=True)
        row2.prop(bone, "bbone_handle_use_scale_end", index=2, text="Z", toggle=True)
        split2.prop(bone, "bbone_handle_use_ease_end", text="Ease", text_ctxt=i18n_contexts.id_armature, toggle=True)
        row.label(icon='BLANK1')


class BONE_PT_relations(BoneButtonsPanel, Panel):
    bl_options = {'DEFAULT_CLOSED'}
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

        if context.bone:
            col.prop(bone, "parent")
        else:
            col.prop_search(bone, "parent", arm, "edit_bones")

        if ob and pchan:
            col.prop(bone, "use_relative_parent")

        sub = col.column()
        sub.active = (bone.parent is not None)
        sub.prop(bone, "use_connect")
        sub = col.column()
        sub.active = (not bone.parent or not bone.use_connect)
        sub.prop(bone, "use_local_location")
        sub = col.column()
        sub.active = (bone.parent is not None)
        sub.prop(bone, "use_inherit_rotation")
        sub.prop(bone, "inherit_scale")


class BONE_PT_collections(BoneButtonsPanel, Panel):
    bl_label = "Bone Collections"
    bl_parent_id = "BONE_PT_relations"

    @classmethod
    def poll(cls, context):
        return context.bone or context.edit_bone

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False

        bone = context.bone or context.edit_bone
        object = context.pose_object or context.edit_object or context.object
        if not object:
            layout.active = False
            sub = layout.column(align=True)
            sub.label(text="Cannot figure out which object this bone belongs to.")
            sub.label(text="Please file a bug report.")
            return

        armature = object.data
        is_solo_active = armature.collections.is_solo_active

        if not bone.collections:
            layout.active = False
            layout.label(text="Not assigned to any bone collection.")
            return

        box = layout.box()
        sub = box.column(align=True)
        for bcoll in bone.collections:
            bcoll_row = sub.row()
            bcoll_row.emboss = 'NONE'

            # Name & visibility of bcoll. Safe things, so aligned together.
            row = bcoll_row.row(align=True)
            row.label(text=bcoll.name)

            # Sub-layout that's dimmed when the bone collection's own visibility flag doesn't matter.
            sub_visible = row.row(align=True)
            sub_visible.active = (not is_solo_active) and bcoll.is_visible_ancestors
            sub_visible.prop(bcoll, "is_visible", text="", icon='HIDE_OFF' if bcoll.is_visible else 'HIDE_ON')

            row.prop(bcoll, "is_solo", text="", icon='SOLO_ON' if bcoll.is_solo else 'SOLO_OFF')

            # Unassign operator, less safe so with a bit of spacing.
            props = bcoll_row.operator("armature.collection_unassign_named", text="", icon='X')
            props.name = bcoll.name
            props.bone_name = bone.name


class BONE_PT_display(BoneButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.bone or context.edit_bone

    def draw(self, context):
        # NOTE: this works ok in edit-mode but isn't
        # all that useful so disabling for now.
        layout = self.layout
        layout.use_property_split = True

        if context.bone is None:
            self.draw_edit_bone(context, layout)
        else:
            self.draw_bone(context, layout)

    def draw_bone(self, context, layout):
        bone = context.bone

        col = layout.column()
        # Figure out the pose bone.
        ob = context.object
        pose_bone = ob and ob.pose.bones[bone.name]
        hide_select_sub = col.column()
        if pose_bone:
            col.prop(pose_bone, "hide", text="Hide", toggle=False)
            hide_select_sub.active = not pose_bone.hide
        hide_select_sub.prop(bone, "hide_select", invert_checkbox=True)
        col.prop(bone, "display_type", text="Display As")

        if not pose_bone:
            return

        # Allow the layout to use the space normally occupied by the 'set a key' diamond.
        layout.use_property_decorate = False

        row = layout.row(align=True)
        row.prop(bone.color, "palette", text="Bone Color")
        props = row.operator("armature.copy_bone_color_to_selected", text="", icon='UV_SYNC_SELECT')
        props.bone_type = 'EDIT'
        self.draw_bone_color_ui(layout, bone.color)

        row = layout.row(align=True)
        row.prop(pose_bone.color, "palette", text="Pose Bone Color")
        props = row.operator("armature.copy_bone_color_to_selected", text="", icon='UV_SYNC_SELECT')
        props.bone_type = 'POSE'
        self.draw_bone_color_ui(layout, pose_bone.color)

    def draw_edit_bone(self, context, layout):
        bone = context.edit_bone
        if bone is None:
            return

        col = layout.column()
        col.prop(bone, "hide", text="Hide", toggle=False)
        hide_select_sub = col.column()
        hide_select_sub.active = not bone.hide
        hide_select_sub.prop(bone, "hide_select", invert_checkbox=True)
        col.prop(bone, "display_type", text="Display As")
        layout.prop(bone.color, "palette", text="Bone Color")
        self.draw_bone_color_ui(layout, bone.color)

    def draw_bone_color_ui(self, layout, bone_color):
        if not bone_color.is_custom:
            return

        split = layout.split(factor=0.401)

        col = split.column()
        row = col.row()
        row.alignment = 'RIGHT'
        row.label(text="Custom Colors")

        col = split.column(align=True)
        row = col.row(align=True)
        row.use_property_split = False
        row.prop(bone_color.custom, "normal", text="")
        row.prop(bone_color.custom, "select", text="")
        row.prop(bone_color.custom, "active", text="")


class BONE_PT_display_custom_shape(BoneButtonsPanel, Panel):
    bl_label = "Custom Shape"
    bl_parent_id = "BONE_PT_display"

    @classmethod
    def poll(cls, context):
        return context.bone

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        bone = context.bone
        pchan = None

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
        elif bone is None:
            bone = context.edit_bone

        if bone and pchan:
            col = layout.column()
            col.prop(pchan, "custom_shape")

            sub = col.column()
            sub.active = bool(pchan and pchan.custom_shape)
            sub.separator()

            sub.prop(pchan, "custom_shape_translation", text="Translation")
            sub.prop(pchan, "custom_shape_rotation_euler", text="Rotation")
            sub.prop(pchan, "custom_shape_scale_xyz", text="Scale")

            sub.prop_search(pchan, "custom_shape_transform", ob.pose, "bones", text="Override Transform")
            subsub = sub.column()
            subsub.active = bool(pchan and pchan.custom_shape and pchan.custom_shape_transform)
            subsub.prop(pchan, "use_transform_at_custom_shape")
            subsubsub = subsub.column()
            subsubsub.active = subsub.active and pchan.use_transform_at_custom_shape
            subsubsub.prop(pchan, "use_transform_around_custom_shape")
            sub.prop(pchan, "use_custom_shape_bone_size")

            sub.separator()
            sub.prop(bone, "show_wire", text="Wireframe")
            sub.prop(pchan, "custom_shape_wire_width")


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
            # row = layout.row()
            # row.prop(pchan, "use_ik_linear_control", text="Joint Size")
            # row.prop(pchan, "ik_linear_weight", text="Weight", slider=True)


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


class BONE_PT_custom_props(BoneButtonsPanel, rna_prop_ui.PropertyPanel, Panel):
    _property_type = bpy.types.Bone, bpy.types.EditBone, bpy.types.PoseBone

    @classmethod
    def _poll(cls, context):
        context_path = cls._get_context_path(context)
        rna_item, _context_member = rna_prop_ui.rna_idprop_context_value(context, context_path, cls._property_type)
        return bool(rna_item)

    def draw(self, context):
        context_path = self._get_context_path(context)
        rna_prop_ui.draw(self.layout, context, context_path, self._property_type)

    @classmethod
    def _get_context_path(self, context):
        if context.mode == 'EDIT_ARMATURE':
            # This also accounts for pinned armatures.
            return "edit_bone"

        obj = context.object
        if not obj:
            # We have to return _something_. If there is some bone by some
            # miracle, just use it.
            return "bone"

        if obj.mode != 'POSE':
            # Outside of pose mode, active_bone is the one to use. It's either a
            # Bone or an EditBone, depending on the mode.
            return "active_bone"

        if context.active_pose_bone is not None:
            # There is an active pose bone, so use it.
            return "active_pose_bone"

        # When the active bone is hidden, `context.active_pose_bone` is None, but
        # `context.bone` still points to it. Use that to still get the pose bone.
        if context.bone is None:
            # If there is no active bone, let the rest of the code refer to the
            # also-None active pose bone, as that's more appropriate given we're
            # currently in pose mode.
            return "active_pose_bone"

        bone_path = obj.pose.bones[context.bone.name].path_from_id()
        return "object." + bone_path


classes = (
    BONE_PT_context_bone,
    BONE_PT_transform,
    BONE_PT_curved,
    BONE_PT_relations,
    BONE_PT_collections,
    BONE_PT_inverse_kinematics,
    BONE_PT_deform,
    BONE_PT_display,
    BONE_PT_display_custom_shape,
    BONE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
