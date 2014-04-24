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
from bpy.types import Panel, UIList
from rna_prop_ui import PropertyPanel

from bl_ui.properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
    )


class SCENE_UL_keying_set_paths(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.KeyingSetPath)
        kspath = item
        icon = layout.enum_item_icon(kspath, "id_type", kspath.id_type)
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            # Do not make this one editable in uiList for now...
            layout.label(text=kspath.data_path, translate=False, icon_value=icon)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class SceneButtonsPanel():
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
        layout.prop(scene, "active_clip", text="Active Clip")


class SCENE_PT_unit(SceneButtonsPanel, Panel):
    bl_label = "Units"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        unit = context.scene.unit_settings

        col = layout.column()
        col.row().prop(unit, "system", expand=True)
        col.row().prop(unit, "system_rotation", expand=True)

        if unit.system != 'NONE':
            row = layout.row()
            row.prop(unit, "scale_length", text="Scale")
            row.prop(unit, "use_separate")


class SCENE_PT_keying_sets(SceneButtonsPanel, Panel):
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
            col.prop(ks, "bl_options")


class SCENE_PT_keying_set_paths(SceneButtonsPanel, Panel):
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
            col.prop(ksp, "bl_options")


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
        col.label("Listener:")
        col.prop(scene, "audio_distance_model", text="")
        col.prop(scene, "audio_doppler_speed", text="Speed")
        col.prop(scene, "audio_doppler_factor", text="Doppler")

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
        col.prop(rd, "simplify_subdivision", text="Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")

        col.prop(rd, "use_simplify_triangulate")

        col = split.column()
        col.prop(rd, "simplify_shadow_samples", text="Shadow Samples")
        col.prop(rd, "simplify_ao_sss", text="AO and SSS")


class SCENE_PT_custom_props(SceneButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "scene"
    _property_type = bpy.types.Scene

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
