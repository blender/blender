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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180


class WorldButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    def poll(self, context):
        rd = context.scene.render_data
        return (context.world) and (not rd.use_game_engine) and (rd.engine in self.COMPAT_ENGINES)


class WORLD_PT_preview(WorldButtonsPanel):
    bl_label = "Preview"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        self.layout.template_preview(context.world)


class WORLD_PT_context_world(WorldButtonsPanel):
    bl_label = ""
    bl_show_header = False
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def poll(self, context):
        rd = context.scene.render_data
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


class WORLD_PT_world(WorldButtonsPanel):
    bl_label = "World"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

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
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.mist, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        world = context.world

        layout.active = world.mist.enabled

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
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.stars, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        world = context.world

        layout.active = world.stars.enabled

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
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.ambient_occlusion, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        wide_ui = context.region.width > narrowui
        ao = context.world.ambient_occlusion

        layout.active = ao.enabled

        layout.prop(ao, "gather_method", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Attenuation:")
        if ao.gather_method == 'RAYTRACE':
            col.prop(ao, "distance")
        col.prop(ao, "falloff")
        sub = col.row()
        sub.active = ao.falloff
        sub.prop(ao, "falloff_strength", text="Strength")

        if ao.gather_method == 'RAYTRACE':
            if wide_ui:
                col = split.column()

            col.label(text="Sampling:")
            col.prop(ao, "sample_method", text="")

            sub = col.column()
            sub.prop(ao, "samples")

            if ao.sample_method == 'ADAPTIVE_QMC':
                sub.prop(ao, "threshold")
                sub.prop(ao, "adapt_to_speed", slider=True)
            elif ao.sample_method == 'CONSTANT_JITTERED':
                sub.prop(ao, "bias")

        if ao.gather_method == 'APPROXIMATE':
            if wide_ui:
                col = split.column()

            col.label(text="Sampling:")
            col.prop(ao, "passes")
            col.prop(ao, "error_tolerance", text="Error")
            col.prop(ao, "pixel_cache")
            col.prop(ao, "correction")

        col = layout.column()
        col.label(text="Influence:")

        col.row().prop(ao, "blend_mode", expand=True)

        split = layout.split()

        col = split.column()
        col.prop(ao, "energy")
        col.prop(ao, "indirect_energy")

        if wide_ui:
            col = split.column()
        col.prop(ao, "color")

bpy.types.register(WORLD_PT_context_world)
bpy.types.register(WORLD_PT_preview)
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)
