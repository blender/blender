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

from bl_ui.properties_physics_common import (point_cache_ui,
                                             effector_weights_ui)


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.smoke)


class PHYSICS_PT_smoke(PhysicButtonsPanel, Panel):
    bl_label = "Smoke"

    def draw(self, context):
        layout = self.layout

        md = context.smoke
        ob = context.object

        layout.prop(md, "smoke_type", expand=True)

        if md.smoke_type == 'DOMAIN':
            domain = md.domain_settings

            split = layout.split()

            split.enabled = not domain.point_cache.is_baked

            col = split.column()
            col.label(text="Resolution:")
            col.prop(domain, "resolution_max", text="Divisions")
            col.label(text="Time:")
            col.prop(domain, "time_scale", text="Scale")
            col.label(text="Border Collisions:")
            col.prop(domain, "collision_extents", text="")

            col = split.column()
            col.label(text="Behavior:")
            col.prop(domain, "alpha")
            col.prop(domain, "beta", text="Temp. Diff.")
            col.prop(domain, "vorticity")
            col.prop(domain, "use_dissolve_smoke", text="Dissolve")
            sub = col.column()
            sub.active = domain.use_dissolve_smoke
            sub.prop(domain, "dissolve_speed", text="Time")
            sub.prop(domain, "use_dissolve_smoke_log", text="Slow")

        elif md.smoke_type == 'FLOW':

            flow = md.flow_settings

            layout.prop(flow, "smoke_flow_type", expand=False)

            if flow.smoke_flow_type != "OUTFLOW":
                split = layout.split()
                col = split.column()
                col.label(text="Flow Source:")
                col.prop(flow, "smoke_flow_source", expand=False, text="")
                if flow.smoke_flow_source == "PARTICLES":
                    col.label(text="Particle System:")
                    col.prop_search(flow, "particle_system", ob, "particle_systems", text="")
                    col.prop(flow, "use_particle_size", text="Set Size")
                    sub = col.column()
                    sub.active = flow.use_particle_size
                    sub.prop(flow, "particle_size")
                else:
                    col.prop(flow, "surface_distance")
                    col.prop(flow, "volume_density")

                sub = col.column(align=True)
                sub.prop(flow, "use_initial_velocity")

                sub = sub.column()
                sub.active = flow.use_initial_velocity
                sub.prop(flow, "velocity_factor")
                if flow.smoke_flow_source == "MESH":
                    sub.prop(flow, "velocity_normal")
                    #sub.prop(flow, "velocity_random")

                sub = split.column()
                sub.label(text="Initial Values:")
                sub.prop(flow, "use_absolute")
                if flow.smoke_flow_type in {'SMOKE', 'BOTH'}:
                    sub.prop(flow, "density")
                    sub.prop(flow, "temperature")
                    sub.prop(flow, "smoke_color")
                if flow.smoke_flow_type in {'FIRE', 'BOTH'}:
                    sub.prop(flow, "fuel_amount")
                sub.label(text="Sampling:")
                sub.prop(flow, "subframes")

        elif md.smoke_type == 'COLLISION':
            coll = md.coll_settings

            split = layout.split()

            col = split.column()
            col.prop(coll, "collision_type")


class PHYSICS_PT_smoke_flow_advanced(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Flow Advanced"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        return md and (md.smoke_type == 'FLOW') and (md.flow_settings.smoke_flow_source == "MESH")

    def draw(self, context):
        layout = self.layout
        ob = context.object
        flow = context.smoke.flow_settings

        split = layout.split()
        col = split.column()

        col.prop(flow, "use_texture")
        sub = col.column()
        sub.active = flow.use_texture
        sub.prop(flow, "noise_texture", text="")
        sub.label(text="Mapping:")
        sub.prop(flow, "texture_map_type", expand=False, text="")
        if flow.texture_map_type == "UV":
            sub.prop_search(flow, "uv_layer", ob.data, "uv_textures", text="")
        if flow.texture_map_type == "AUTO":
            sub.prop(flow, "texture_size")
        sub.prop(flow, "texture_offset")

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(flow, "density_vertex_group", ob, "vertex_groups", text="")


class PHYSICS_PT_smoke_fire(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Flames"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        return md and (md.smoke_type == 'DOMAIN')

    def draw(self, context):
        layout = self.layout
        domain = context.smoke.domain_settings

        split = layout.split()
        split.enabled = not domain.point_cache.is_baked

        col = split.column(align=True)
        col.label(text="Reaction:")
        col.prop(domain, "burning_rate")
        col.prop(domain, "flame_smoke")
        col.prop(domain, "flame_vorticity")

        col = split.column(align=True)
        col.label(text="Temperatures:")
        col.prop(domain, "flame_ignition")
        col.prop(domain, "flame_max_temp")
        col.prop(domain, "flame_smoke_color")


class PHYSICS_PT_smoke_adaptive_domain(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Adaptive Domain"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        return md and (md.smoke_type == 'DOMAIN')

    def draw_header(self, context):
        md = context.smoke.domain_settings

        self.layout.prop(md, "use_adaptive_domain", text="")

    def draw(self, context):
        layout = self.layout

        domain = context.smoke.domain_settings
        layout.active = domain.use_adaptive_domain

        split = layout.split()
        split.enabled = (not domain.point_cache.is_baked)

        col = split.column(align=True)
        col.label(text="Resolution:")
        col.prop(domain, "additional_res")
        col.prop(domain, "adapt_margin")

        col = split.column(align=True)
        col.label(text="Advanced:")
        col.prop(domain, "adapt_threshold")


class PHYSICS_PT_smoke_highres(PhysicButtonsPanel, Panel):
    bl_label = "Smoke High Resolution"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        rd = context.scene.render
        return md and (md.smoke_type == 'DOMAIN') and (not rd.use_game_engine)

    def draw_header(self, context):
        md = context.smoke.domain_settings

        self.layout.prop(md, "use_high_resolution", text="")

    def draw(self, context):
        layout = self.layout

        md = context.smoke.domain_settings

        layout.active = md.use_high_resolution

        split = layout.split()
        split.enabled = not md.point_cache.is_baked

        col = split.column()
        col.label(text="Resolution:")
        col.prop(md, "amplify", text="Divisions")
        col.label(text="Flow Sampling:")
        col.row().prop(md, "highres_sampling", text="")

        col = split.column()
        col.label(text="Noise Method:")
        col.row().prop(md, "noise_type", text="")
        col.prop(md, "strength")

        layout.prop(md, "show_high_resolution")


class PHYSICS_PT_smoke_groups(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Groups"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        rd = context.scene.render
        return md and (md.smoke_type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout
        domain = context.smoke.domain_settings

        split = layout.split()

        col = split.column()
        col.label(text="Flow Group:")
        col.prop(domain, "fluid_group", text="")

        #col.label(text="Effector Group:")
        #col.prop(domain, "effector_group", text="")

        col = split.column()
        col.label(text="Collision Group:")
        col.prop(domain, "collision_group", text="")


class PHYSICS_PT_smoke_cache(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Cache"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        rd = context.scene.render
        return md and (md.smoke_type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        md = context.smoke.domain_settings
        cache = md.point_cache

        layout.label(text="Compression:")
        layout.prop(md, "point_cache_compress_type", expand=True)

        point_cache_ui(self, context, cache, (cache.is_baked is False), 'SMOKE')


class PHYSICS_PT_smoke_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Smoke Field Weights"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.smoke
        rd = context.scene.render
        return md and (md.smoke_type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        domain = context.smoke.domain_settings
        effector_weights_ui(self, context, domain.effector_weights, 'SMOKE')

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
