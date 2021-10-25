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
from bpy.types import (
        Menu,
        Panel,
        UIList,
        )

from rna_prop_ui import PropertyPanel

from bl_ui.properties_physics_common import (
        point_cache_ui,
        effector_weights_ui,
        )


class SCENE_MT_units_length_presets(Menu):
    """Unit of measure for properties that use length values"""
    bl_label = "Unit Presets"
    preset_subdir = "units_length"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class SCENE_UL_keying_set_paths(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.KeyingSetPath)
        kspath = item
        icon = layout.enum_item_icon(kspath, "id_type", kspath.id_type)
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            # Do not make this one editable in uiList for now...
            layout.label(text=kspath.data_path, translate=False, icon_value=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class SceneButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return context.scene and (rd.engine in cls.COMPAT_ENGINES)


class SCENE_PT_scene(SceneButtonsPanel, Panel):
    bl_label = "Scene"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.prop(scene, "camera")
        layout.prop(scene, "background_set", text="Background")
        if context.scene.render.engine != 'BLENDER_GAME':
            layout.prop(scene, "active_clip", text="Active Clip")


class SCENE_PT_unit(SceneButtonsPanel, Panel):
    bl_label = "Units"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        unit = context.scene.unit_settings

        row = layout.row(align=True)
        row.menu("SCENE_MT_units_length_presets", text=SCENE_MT_units_length_presets.bl_label)
        row.operator("scene.units_length_preset_add", text="", icon='ZOOMIN')
        row.operator("scene.units_length_preset_add", text="", icon='ZOOMOUT').remove_active = True

        layout.separator()

        split = layout.split(percentage=0.35)
        split.label("Length:")
        split.prop(unit, "system", text="")
        split = layout.split(percentage=0.35)
        split.label("Angle:")
        split.prop(unit, "system_rotation", text="")

        col = layout.column()
        col.enabled = unit.system != 'NONE'
        split = col.split(percentage=0.35)
        split.label("Unit Scale:")
        split.prop(unit, "scale_length", text="")
        split = col.split(percentage=0.35)
        split.row()
        split.prop(unit, "use_separate")


class SceneKeyingSetsPanel:

    @staticmethod
    def draw_keyframing_settings(context, layout, ks, ksp):
        SceneKeyingSetsPanel._draw_keyframing_setting(
                context, layout, ks, ksp, "Needed",
                "use_insertkey_override_needed", "use_insertkey_needed",
                userpref_fallback="use_keyframe_insert_needed")

        SceneKeyingSetsPanel._draw_keyframing_setting(
                context, layout, ks, ksp, "Visual",
                "use_insertkey_override_visual", "use_insertkey_visual",
                userpref_fallback="use_visual_keying")

        SceneKeyingSetsPanel._draw_keyframing_setting(
                context, layout, ks, ksp, "XYZ to RGB",
                "use_insertkey_override_xyz_to_rgb", "use_insertkey_xyz_to_rgb")

    @staticmethod
    def _draw_keyframing_setting(context, layout, ks, ksp, label, toggle_prop, prop, userpref_fallback=None):
        if ksp:
            item = ksp

            if getattr(ks, toggle_prop):
                owner = ks
                propname = prop
            else:
                owner = context.user_preferences.edit
                if userpref_fallback:
                    propname = userpref_fallback
                else:
                    propname = prop
        else:
            item = ks

            owner = context.user_preferences.edit
            if userpref_fallback:
                propname = userpref_fallback
            else:
                propname = prop

        row = layout.row(align=True)
        row.prop(item, toggle_prop, text="", icon='STYLUS_PRESSURE', toggle=True)  # XXX: needs dedicated icon

        subrow = row.row()
        subrow.active = getattr(item, toggle_prop)
        if subrow.active:
            subrow.prop(item, prop, text=label)
        else:
            subrow.prop(owner, propname, text=label)


class SCENE_PT_keying_sets(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Keying Sets"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        row = layout.row()

        col = row.column()
        col.template_list("UI_UL_list", "keying_sets", scene, "keying_sets", scene.keying_sets, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("anim.keying_set_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_remove", icon='ZOOMOUT', text="")

        ks = scene.keying_sets.active
        if ks and ks.is_path_absolute:
            row = layout.row()

            col = row.column()
            col.prop(ks, "bl_description")

            subcol = col.column()
            subcol.operator_context = 'INVOKE_DEFAULT'
            subcol.operator("anim.keying_set_export", text="Export to File").filepath = "keyingset.py"

            col = row.column()
            col.label(text="Keyframing Settings:")
            self.draw_keyframing_settings(context, col, ks, None)


class SCENE_PT_keying_set_paths(SceneButtonsPanel, SceneKeyingSetsPanel, Panel):
    bl_label = "Active Keying Set"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ks = context.scene.keying_sets.active
        return (ks and ks.is_path_absolute)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        ks = scene.keying_sets.active

        row = layout.row()
        row.label(text="Paths:")

        row = layout.row()

        col = row.column()
        col.template_list("SCENE_UL_keying_set_paths", "", ks, "paths", ks.paths, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("anim.keying_set_path_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_path_remove", icon='ZOOMOUT', text="")

        ksp = ks.paths.active
        if ksp:
            col = layout.column()
            col.label(text="Target:")
            col.template_any_ID(ksp, "id", "id_type")
            col.template_path_builder(ksp, "data_path", ksp.id)

            row = col.row(align=True)
            row.label(text="Array Target:")
            row.prop(ksp, "use_entire_array", text="All Items")
            if ksp.use_entire_array:
                row.label(text=" ")  # padding
            else:
                row.prop(ksp, "array_index", text="Index")

            layout.separator()

            row = layout.row()
            col = row.column()
            col.label(text="F-Curve Grouping:")
            col.prop(ksp, "group_method", text="")
            if ksp.group_method == 'NAMED':
                col.prop(ksp, "group")

            col = row.column()
            col.label(text="Keyframing Settings:")
            self.draw_keyframing_settings(context, col, ks, ksp)


class SCENE_PT_color_management(SceneButtonsPanel, Panel):
    bl_label = "Color Management"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        col = layout.column()
        col.label(text="Display:")
        col.prop(scene.display_settings, "display_device")

        col = layout.column()
        col.separator()
        col.label(text="Render:")
        col.template_colormanaged_view_settings(scene, "view_settings")

        col = layout.column()
        col.separator()
        col.label(text="Sequencer:")
        col.prop(scene.sequencer_colorspace_settings, "name")


class SCENE_PT_audio(SceneButtonsPanel, Panel):
    bl_label = "Audio"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = context.scene.render
        ffmpeg = rd.ffmpeg

        layout.prop(scene, "audio_volume")
        layout.operator("sound.bake_animation")

        split = layout.split()

        col = split.column()
        col.label("Distance Model:")
        col.prop(scene, "audio_distance_model", text="")
        sub = col.column(align=True)
        sub.prop(scene, "audio_doppler_speed", text="Speed")
        sub.prop(scene, "audio_doppler_factor", text="Doppler")

        col = split.column()
        col.label("Format:")
        col.prop(ffmpeg, "audio_channels", text="")
        col.prop(ffmpeg, "audio_mixrate", text="Rate")


class SCENE_PT_physics(SceneButtonsPanel, Panel):
    bl_label = "Gravity"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        self.layout.prop(context.scene, "use_gravity", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.active = scene.use_gravity

        layout.prop(scene, "gravity", text="")


class SCENE_PT_rigid_body_world(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body World"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        rd = scene.render
        return scene and (rd.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world
        if rbw is not None:
            self.layout.prop(rbw, "enabled", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        rbw = scene.rigidbody_world

        if rbw is None:
            layout.operator("rigidbody.world_add")
        else:
            layout.operator("rigidbody.world_remove")

            col = layout.column()
            col.active = rbw.enabled

            col = col.column()
            col.prop(rbw, "group")
            col.prop(rbw, "constraints")

            split = col.split()

            col = split.column()
            col.prop(rbw, "time_scale", text="Speed")
            col.prop(rbw, "use_split_impulse")

            col = split.column()
            col.prop(rbw, "steps_per_second", text="Steps Per Second")
            col.prop(rbw, "solver_iterations", text="Solver Iterations")


class SCENE_PT_rigid_body_cache(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body Cache"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        scene = context.scene
        return scene and scene.rigidbody_world and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        point_cache_ui(self, context, rbw.point_cache, rbw.point_cache.is_baked is False and rbw.enabled, 'RIGID_BODY')


class SCENE_PT_rigid_body_field_weights(SceneButtonsPanel, Panel):
    bl_label = "Rigid Body Field Weights"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        scene = context.scene
        return scene and scene.rigidbody_world and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        scene = context.scene
        rbw = scene.rigidbody_world

        effector_weights_ui(self, context, rbw.effector_weights, 'RIGID_BODY')


class SCENE_PT_simplify(SceneButtonsPanel, Panel):
    bl_label = "Simplify"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.active = rd.use_simplify

        split = layout.split()

        col = split.column()
        col.label(text="Viewport:")
        col.prop(rd, "simplify_subdivision", text="Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")

        col = split.column()
        col.label(text="Render:")
        col.prop(rd, "simplify_subdivision_render", text="Subdivision")
        col.prop(rd, "simplify_child_particles_render", text="Child Particles")
        col.prop(rd, "simplify_shadow_samples", text="Shadow Samples")
        col.prop(rd, "simplify_ao_sss", text="AO and SSS")
        col.prop(rd, "use_simplify_triangulate")


class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "scene"
    _property_type = bpy.types.Scene


classes = (
    SCENE_MT_units_length_presets,
    SCENE_UL_keying_set_paths,
    SCENE_PT_scene,
    SCENE_PT_unit,
    SCENE_PT_keying_sets,
    SCENE_PT_keying_set_paths,
    SCENE_PT_color_management,
    SCENE_PT_audio,
    SCENE_PT_physics,
    SCENE_PT_rigid_body_world,
    SCENE_PT_rigid_body_cache,
    SCENE_PT_rigid_body_field_weights,
    SCENE_PT_simplify,
    SCENE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
