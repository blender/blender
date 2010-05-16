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

narrowui = bpy.context.user_preferences.view.properties_width_check


class WorldButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    def poll(self, context):
        rd = context.scene.render
        return (context.world) and (not rd.use_game_engine) and (rd.engine in self.COMPAT_ENGINES)


class WORLD_PT_preview(WorldButtonsPanel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        self.layout.template_preview(context.world)


class WORLD_PT_context_world(WorldButtonsPanel):
    bl_label = ""
    bl_show_header = False
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        rd = context.scene.render
        return (not rd.use_game_engine) and (rd.engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data
        wide_ui = context.region.width > narrowui


        if wide_ui:
            split = layout.split(percentage=0.65)
            if scene:
                split.template_ID(scene, "world", new="world.new")
            elif world:
                split.template_ID(space, "pin_id")
        else:
            layout.template_ID(scene, "world", new="world.new")


class WORLD_PT_custom_props(WorldButtonsPanel, PropertyPanel):
    COMPAT_ENGINES = {'BLENDER_RENDER'}
    _context_path = "world"


class WORLD_PT_world(WorldButtonsPanel):
    bl_label = "World"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        world = context.world

        if wide_ui:
            row = layout.row()
            row.prop(world, "paper_sky")
            row.prop(world, "blend_sky")
            row.prop(world, "real_sky")
        else:
            col = layout.column()
            col.prop(world, "paper_sky")
            col.prop(world, "blend_sky")
            col.prop(world, "real_sky")

        row = layout.row()
        row.column().prop(world, "horizon_color")
        col = row.column()
        col.prop(world, "zenith_color")
        col.active = world.blend_sky
        row.column().prop(world, "ambient_color")


class WORLD_PT_mist(WorldButtonsPanel):
    bl_label = "Mist"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.mist, "use_mist", text="")

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        world = context.world

        layout.active = world.mist.use_mist

        split = layout.split()

        col = split.column()
        col.prop(world.mist, "intensity", slider=True)
        col.prop(world.mist, "start")

        if wide_ui:
            col = split.column()
        col.prop(world.mist, "depth")
        col.prop(world.mist, "height")

        layout.prop(world.mist, "falloff")


class WORLD_PT_stars(WorldButtonsPanel):
    bl_label = "Stars"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.stars, "use_stars", text="")

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        world = context.world

        layout.active = world.stars.use_stars

        split = layout.split()

        col = split.column()
        col.prop(world.stars, "size")
        col.prop(world.stars, "color_randomization", text="Colors")

        if wide_ui:
            col = split.column()
        col.prop(world.stars, "min_distance", text="Min. Dist")
        col.prop(world.stars, "average_separation", text="Separation")


class WORLD_PT_ambient_occlusion(WorldButtonsPanel):
    bl_label = "Ambient Occlusion"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        light = context.world.lighting
        self.layout.prop(light, "use_ambient_occlusion", text="")

    def draw(self, context):
        layout = self.layout
        light = context.world.lighting

        layout.active = light.use_ambient_occlusion

        split = layout.split()
        split.prop(light, "ao_factor", text="Factor")
        split.prop(light, "ao_blend_mode", text="")


class WORLD_PT_environment_lighting(WorldButtonsPanel):
    bl_label = "Environment Lighting"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        light = context.world.lighting
        self.layout.prop(light, "use_environment_lighting", text="")

    def draw(self, context):
        layout = self.layout
        light = context.world.lighting

        layout.active = light.use_environment_lighting

        split = layout.split()
        split.prop(light, "environment_energy", text="Energy")
        split.prop(light, "environment_color", text="")


class WORLD_PT_indirect_lighting(WorldButtonsPanel):
    bl_label = "Indirect Lighting"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        light = context.world.lighting
        self.layout.prop(light, "use_indirect_lighting", text="")

    def draw(self, context):
        layout = self.layout
        light = context.world.lighting

        layout.active = light.use_indirect_lighting

        split = layout.split()
        split.prop(light, "indirect_factor", text="Factor")
        split.prop(light, "indirect_bounces", text="Bounces")


class WORLD_PT_gather(WorldButtonsPanel):
    bl_label = "Gather"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        light = context.world.lighting

        layout.active = light.use_ambient_occlusion or light.use_environment_lighting or light.use_indirect_lighting

        layout.prop(light, "gather_method", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Attenuation:")
        if light.gather_method == 'RAYTRACE':
            col.prop(light, "distance")
        col.prop(light, "falloff")
        sub = col.row()
        sub.active = light.falloff
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
            col.prop(light, "error_tolerance", text="Error")
            col.prop(light, "pixel_cache")
            col.prop(light, "correction")


classes = [
    WORLD_PT_context_world,
    WORLD_PT_preview,
    WORLD_PT_world,
    WORLD_PT_ambient_occlusion,
    WORLD_PT_environment_lighting,
    WORLD_PT_indirect_lighting,
    WORLD_PT_gather,
    WORLD_PT_mist,
    WORLD_PT_stars,

    WORLD_PT_custom_props]


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
