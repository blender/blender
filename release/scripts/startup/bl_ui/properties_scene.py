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


class SCENE_PT_audio(SceneButtonsPanel, Panel):
    bl_label = "Audio"
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

        layout.operator("sound.mixdown")


class SCENE_PT_unit(SceneButtonsPanel, Panel):
    bl_label = "Units"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout
        unit = context.scene.unit_settings

        col = layout.column()
        col.row().prop(unit, "system", expand=True)
        col.row().prop(unit, "system_rotation", expand=True)

        row = layout.row()
        row.active = (unit.system != 'NONE')
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
        col.template_list(scene, "keying_sets", scene.keying_sets, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("anim.keying_set_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_remove", icon='ZOOMOUT', text="")

        ks = scene.keying_sets.active
        if ks and ks.is_path_absolute:
            row = layout.row()

            col = row.column()
            col.prop(ks, "name")

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
        col.template_list(ks, "paths", ks.paths, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("anim.keying_set_path_add", icon='ZOOMIN', text="")
        col.operator("anim.keying_set_path_remove", icon='ZOOMOUT', text="")

        ksp = ks.paths.active
        if ksp:
            col = layout.column()
            col.label(text="Target:")
            col.template_any_ID(ksp, "id", "id_type")
            col.template_path_builder(ksp, "data_path", ksp.id)

            row = layout.row()

            col = row.column()
            col.label(text="Array Target:")
            col.prop(ksp, "use_entire_array")
            if ksp.use_entire_array is False:
                col.prop(ksp, "array_index")

            col = row.column()
            col.label(text="F-Curve Grouping:")
            col.prop(ksp, "group_method")
            if ksp.group_method == 'NAMED':
                col.prop(ksp, "group")

            col.prop(ksp, "bl_options")


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
