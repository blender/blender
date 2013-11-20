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


class WorldButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.world and context.scene.render.engine in cls.COMPAT_ENGINES)


class WORLD_PT_context_world(WorldButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (not rd.use_game_engine) and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data
        rd = context.scene.render

        texture_count = world and len(world.texture_slots.keys())

        split = layout.split(percentage=0.85)
        if scene:
            split.template_ID(scene, "world", new="world.new")
        elif world:
            split.template_ID(space, "pin_id")

        if texture_count and rd.engine != 'CYCLES':
            split.label(text=str(texture_count), icon='TEXTURE')


class WORLD_PT_preview(WorldButtonsPanel, Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.world) and (not rd.use_game_engine) and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        self.layout.template_preview(context.world)


class WORLD_PT_world(WorldButtonsPanel, Panel):
    bl_label = "World"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        world = context.world

        row = layout.row()
        row.prop(world, "use_sky_paper")
        row.prop(world, "use_sky_blend")
        row.prop(world, "use_sky_real")

        row = layout.row()
        row.column().prop(world, "horizon_color")
        col = row.column()
        col.prop(world, "zenith_color")
        col.active = world.use_sky_blend
        row.column().prop(world, "ambient_color")

        row = layout.row()
        row.prop(world, "exposure")
        row.prop(world, "color_range")


class WORLD_PT_ambient_occlusion(WorldButtonsPanel, Panel):
    bl_label = "Ambient Occlusion"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        light = context.world.light_settings
        self.layout.prop(light, "use_ambient_occlusion", text="")

    def draw(self, context):
        layout = self.layout

        light = context.world.light_settings

        layout.active = light.use_ambient_occlusion

        split = layout.split()
        split.prop(light, "ao_factor", text="Factor")
        split.prop(light, "ao_blend_type", text="")


class WORLD_PT_environment_lighting(WorldButtonsPanel, Panel):
    bl_label = "Environment Lighting"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        light = context.world.light_settings
        self.layout.prop(light, "use_environment_light", text="")

    def draw(self, context):
        layout = self.layout

        light = context.world.light_settings

        layout.active = light.use_environment_light

        split = layout.split()
        split.prop(light, "environment_energy", text="Energy")
        split.prop(light, "environment_color", text="")


class WORLD_PT_indirect_lighting(WorldButtonsPanel, Panel):
    bl_label = "Indirect Lighting"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        light = context.world.light_settings
        self.layout.prop(light, "use_indirect_light", text="")

    def draw(self, context):
        layout = self.layout

        light = context.world.light_settings

        layout.active = light.use_indirect_light and light.gather_method == 'APPROXIMATE'

        split = layout.split()
        split.prop(light, "indirect_factor", text="Factor")
        split.prop(light, "indirect_bounces", text="Bounces")

        if light.gather_method == 'RAYTRACE':
            layout.label(text="Only works with Approximate gather method")


class WORLD_PT_gather(WorldButtonsPanel, Panel):
    bl_label = "Gather"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        light = context.world.light_settings

        layout.active = light.use_ambient_occlusion or light.use_environment_light or light.use_indirect_light

        layout.prop(light, "gather_method", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Attenuation:")
        if light.gather_method == 'RAYTRACE':
            col.prop(light, "distance")
        col.prop(light, "use_falloff")
        sub = col.row()
        sub.active = light.use_falloff
        sub.prop(light, "falloff_strength", text="Strength")

        if light.gather_method == 'RAYTRACE':
            col = split.column()

            col.label(text="Sampling:")
            col.prop(light, "sample_method", text="")

            sub = col.column()
            sub.prop(light, "samples")

            if light.sample_method == 'ADAPTIVE_QMC':
                sub.prop(light, "threshold")
                sub.prop(light, "adapt_to_speed", slider=True)
            elif light.sample_method == 'CONSTANT_JITTERED':
                sub.prop(light, "bias")

        if light.gather_method == 'APPROXIMATE':
            col = split.column()

            col.label(text="Sampling:")
            col.prop(light, "passes")
            col.prop(light, "error_threshold", text="Error")
            col.prop(light, "use_cache")
            col.prop(light, "correction")


class WORLD_PT_mist(WorldButtonsPanel, Panel):
    bl_label = "Mist"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.mist_settings, "use_mist", text="")

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.active = world.mist_settings.use_mist

        split = layout.split()

        col = split.column()
        col.prop(world.mist_settings, "intensity")
        col.prop(world.mist_settings, "start")

        col = split.column()
        col.prop(world.mist_settings, "depth")
        col.prop(world.mist_settings, "height")

        layout.prop(world.mist_settings, "falloff")


class WORLD_PT_custom_props(WorldButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "world"
    _property_type = bpy.types.World

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
