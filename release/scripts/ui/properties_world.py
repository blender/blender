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
        col2 = context.region.width > narrowui

        
        if col2:
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
        col2 = context.region.width > narrowui
        world = context.world
        
        if col2:
            row = layout.row()
            row.itemR(world, "paper_sky")
            row.itemR(world, "blend_sky")
            row.itemR(world, "real_sky")
        else:
            col = layout.column()
            col.itemR(world, "paper_sky")
            col.itemR(world, "blend_sky")
            col.itemR(world, "real_sky")

        row = layout.row()
        row.column().itemR(world, "horizon_color")
        col = row.column()
        col.itemR(world, "zenith_color")
        col.active = world.blend_sky
        row.column().itemR(world, "ambient_color")


class WORLD_PT_mist(WorldButtonsPanel):
    bl_label = "Mist"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        world = context.world

        self.layout.itemR(world.mist, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        col2 = context.region.width > narrowui
        world = context.world

        layout.active = world.mist.enabled
        
        split = layout.split()
        
        col = split.column()
        col.itemR(world.mist, "intensity", slider=True)
        col.itemR(world.mist, "start")

        if col2:
            col = split.column()
        col.itemR(world.mist, "depth")
        col.itemR(world.mist, "height")

        layout.itemR(world.mist, "falloff")


class WORLD_PT_stars(WorldButtonsPanel):
    bl_label = "Stars"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        world = context.world

        self.layout.itemR(world.stars, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        col2 = context.region.width > narrowui
        world = context.world

        layout.active = world.stars.enabled

        split = layout.split()
        
        col = split.column()
        col.itemR(world.stars, "size")
        col.itemR(world.stars, "color_randomization", text="Colors")

        if col2:
            col = split.column()
        col.itemR(world.stars, "min_distance", text="Min. Dist")
        col.itemR(world.stars, "average_separation", text="Separation")


class WORLD_PT_ambient_occlusion(WorldButtonsPanel):
    bl_label = "Ambient Occlusion"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        world = context.world

        self.layout.itemR(world.ambient_occlusion, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        col2 = context.region.width > narrowui
        ao = context.world.ambient_occlusion

        layout.active = ao.enabled

        layout.itemR(ao, "gather_method", expand=True)

        split = layout.split()

        col = split.column()
        col.itemL(text="Attenuation:")
        if ao.gather_method == 'RAYTRACE':
            col.itemR(ao, "distance")
        col.itemR(ao, "falloff")
        sub = col.row()
        sub.active = ao.falloff
        sub.itemR(ao, "falloff_strength", text="Strength")

        if ao.gather_method == 'RAYTRACE':
            if col2:
                col = split.column()

            col.itemL(text="Sampling:")
            col.itemR(ao, "sample_method", text="")

            sub = col.column()
            sub.itemR(ao, "samples")

            if ao.sample_method == 'ADAPTIVE_QMC':
                sub.itemR(ao, "threshold")
                sub.itemR(ao, "adapt_to_speed", slider=True)
            elif ao.sample_method == 'CONSTANT_JITTERED':
                sub.itemR(ao, "bias")

        if ao.gather_method == 'APPROXIMATE':
            if col2:
                col = split.column()

            col.itemL(text="Sampling:")
            col.itemR(ao, "passes")
            col.itemR(ao, "error_tolerance", text="Error")
            col.itemR(ao, "pixel_cache")
            col.itemR(ao, "correction")

        col = layout.column()
        col.itemL(text="Influence:")

        col.row().itemR(ao, "blend_mode", expand=True)

        split = layout.split()

        col = split.column()
        col.itemR(ao, "energy")

        if col2:
            col = split.column()
        col.itemR(ao, "color")

bpy.types.register(WORLD_PT_context_world)
bpy.types.register(WORLD_PT_preview)
bpy.types.register(WORLD_PT_world)
bpy.types.register(WORLD_PT_ambient_occlusion)
bpy.types.register(WORLD_PT_mist)
bpy.types.register(WORLD_PT_stars)
