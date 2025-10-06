# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Context, Panel, UILayout
from bpy.app.translations import contexts as i18n_contexts


class GlobalTransformPanelMixin:
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Animation"


class VIEW3D_PT_copy_global_transform(GlobalTransformPanelMixin, Panel):
    bl_label = "Global Transform"

    def draw(self, context: Context) -> None:
        layout = self.layout
        scene = context.scene

        # No need to put "Global Transform" in the operator text, given that it's already in the panel title.
        layout.operator("object.copy_global_transform", text="Copy", icon='COPYDOWN')

        paste_col = layout.column(align=True)

        paste_row = paste_col.row(align=True)
        paste_props = paste_row.operator("object.paste_transform", text="Paste", icon='PASTEDOWN')
        paste_props.method = 'CURRENT'
        paste_props.use_mirror = False
        paste_props = paste_row.operator("object.paste_transform", text="Mirrored", icon='PASTEFLIPDOWN')
        paste_props.method = 'CURRENT'
        paste_props.use_mirror = True

        wants_autokey_col = paste_col.column(align=False)
        has_autokey = scene.tool_settings.use_keyframe_insert_auto
        wants_autokey_col.enabled = has_autokey
        if not has_autokey:
            wants_autokey_col.label(text="These require auto-key:")

        paste_col = wants_autokey_col.column(align=True)
        paste_col.operator(
            "object.paste_transform",
            text="Paste to Selected Keys",
            icon='PASTEDOWN',
        ).method = 'EXISTING_KEYS'
        paste_col.operator(
            "object.paste_transform",
            text="Paste and Bake",
            icon='PASTEDOWN',
        ).method = 'BAKE'


class VIEW3D_PT_copy_global_transform_fix_to_camera(GlobalTransformPanelMixin, Panel):
    bl_label = "Fix to Camera"
    bl_parent_id = "VIEW3D_PT_copy_global_transform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context: Context) -> None:
        from bpy_extras.anim_utils import AutoKeying

        layout = self.layout
        scene = context.scene

        # Fix to Scene Camera:
        layout.use_property_split = True
        props_box = layout.column(heading="Fix", heading_ctxt=i18n_contexts.id_camera, align=True)
        props_box.prop(scene.tool_settings, "anim_fix_to_cam_use_loc", text="Location")
        props_box.prop(scene.tool_settings, "anim_fix_to_cam_use_rot", text="Rotation")
        props_box.prop(scene.tool_settings, "anim_fix_to_cam_use_scale", text="Scale")

        keyingset = AutoKeying.active_keyingset(context)
        if keyingset:
            # Show an explicit message here, even though the keying set affects
            # the other operators as well. Fix to Camera is treated as a special
            # case because it also has options for selecting what to key. The
            # logical AND of the settings is used, so a property is only keyed
            # when the keying set AND the above checkboxes say it's ok.
            props_box.label(text="Keying set is active, which may")
            props_box.label(text="reduce the effect of the above options")

        row = layout.row(align=True)
        props = row.operator("object.fix_to_camera")
        props.use_location = scene.tool_settings.anim_fix_to_cam_use_loc
        props.use_rotation = scene.tool_settings.anim_fix_to_cam_use_rot
        props.use_scale = scene.tool_settings.anim_fix_to_cam_use_scale
        row.operator("object.delete_fix_to_camera_keys", text="", icon='TRASH')


class VIEW3D_PT_copy_global_transform_mirror(GlobalTransformPanelMixin, Panel):
    bl_label = "Mirror"
    bl_parent_id = "VIEW3D_PT_copy_global_transform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context: Context) -> None:
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene

        col = layout.column(align=True)
        col.prop(scene.tool_settings, "anim_mirror_object", text="Object")

        mirror_ob = scene.tool_settings.anim_mirror_object
        if mirror_ob is None:
            # No explicit mirror object means "the current armature", so then the bone name should be editable.
            if context.object and context.object.type == 'ARMATURE':
                self._bone_search(col, scene, context.object)
            else:
                self._bone_entry(layout, scene)
        elif mirror_ob.type == 'ARMATURE':
            self._bone_search(col, scene, mirror_ob)

    def _bone_search(self, layout: UILayout, scene: bpy.types.Scene, armature_ob: bpy.types.Object) -> None:
        """Search within the bones of the given armature."""
        assert armature_ob and armature_ob.type == 'ARMATURE'

        layout.prop_search(
            scene.tool_settings,
            "anim_mirror_bone",
            armature_ob.data,
            "edit_bones" if armature_ob.mode == 'EDIT' else "bones",
            text="Bone",
        )

    def _bone_entry(self, layout: UILayout, scene: bpy.types.Scene) -> None:
        """Allow manual entry of a bone name."""
        layout.prop(scene.tool_settings, "anim_mirror_bone", text="Bone")


class VIEW3D_PT_copy_global_transform_relative(GlobalTransformPanelMixin, Panel):
    bl_label = "Relative"
    bl_parent_id = "VIEW3D_PT_copy_global_transform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context: Context) -> None:
        from bl_operators.copy_global_transform import get_relative_ob

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene

        # Copy/Paste relative to some object:
        copy_paste_sub = layout.column(align=False)
        has_relative_ob = bool(get_relative_ob(context))
        copy_paste_sub.prop(scene.tool_settings, 'anim_relative_object', text="Object")
        if not scene.tool_settings.anim_relative_object:
            copy_paste_sub.label(text="Using Active Scene Camera")

        button_sub = copy_paste_sub.row(align=True)
        button_sub.enabled = has_relative_ob
        button_sub.operator("object.copy_relative_transform", text="Copy", icon='COPYDOWN')

        paste_props = button_sub.operator("object.paste_transform", text="Paste", icon='PASTEDOWN')
        paste_props.method = 'CURRENT'
        paste_props.use_mirror = False
        paste_props.use_relative = True

        # It is unknown whether this combination of options is in any way
        # sensible or usable, and of so, in which order the mirroring and
        # relative'ing-to should happen. That's why, for now, it's disabled.
        #
        # paste_props = paste_row.operator("object.paste_transform", text="Mirrored", icon='PASTEFLIPDOWN')
        # paste_props.method = 'CURRENT'
        # paste_props.use_mirror = True
        # paste_props.use_relative = True


classes = (
    VIEW3D_PT_copy_global_transform,
    VIEW3D_PT_copy_global_transform_mirror,
    VIEW3D_PT_copy_global_transform_relative,
    VIEW3D_PT_copy_global_transform_fix_to_camera,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
