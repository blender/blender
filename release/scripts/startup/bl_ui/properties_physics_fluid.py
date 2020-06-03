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
from bpy.types import Menu, Panel
from bl_ui.utils import PresetPanel
from .properties_physics_common import (
    effector_weights_ui,
)


class FLUID_PT_presets(PresetPanel, Panel):
    bl_label = "Fluid Presets"
    preset_subdir = "fluid"
    preset_operator = "script.execute_preset"
    preset_add_operator = "fluid.preset_add"


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @staticmethod
    def check_domain_has_unbaked_guide(domain):
        return (
            domain.use_guide and not domain.has_cache_baked_guide and
            ((domain.guide_source == 'EFFECTOR') or
             (domain.guide_source == 'DOMAIN' and not domain.guide_parent))
        )

    @staticmethod
    def poll_fluid(context):
        ob = context.object
        if not ((ob and ob.type == 'MESH') and (context.fluid)):
            return False

        md = context.fluid
        return md and (context.fluid.fluid_type != 'NONE')

    @staticmethod
    def poll_fluid_domain(context):
        if not PhysicButtonsPanel.poll_fluid(context):
            return False

        md = context.fluid
        return md and (md.fluid_type == 'DOMAIN')

    @staticmethod
    def poll_gas_domain(context):
        if not PhysicButtonsPanel.poll_fluid(context):
            return False

        md = context.fluid
        if md and (md.fluid_type == 'DOMAIN'):
            domain = md.domain_settings
            return domain.domain_type in {'GAS'}
        return False

    @staticmethod
    def poll_liquid_domain(context):
        if not PhysicButtonsPanel.poll_fluid(context):
            return False

        md = context.fluid
        if md and (md.fluid_type == 'DOMAIN'):
            domain = md.domain_settings
            return domain.domain_type in {'LIQUID'}
        return False

    @staticmethod
    def poll_fluid_flow(context):
        if not PhysicButtonsPanel.poll_fluid(context):
            return False

        md = context.fluid
        return md and (md.fluid_type == 'FLOW')

    @staticmethod
    def poll_fluid_flow_outflow(context):
        if not PhysicButtonsPanel.poll_fluid_flow(context):
            return False

        md = context.fluid
        flow = md.flow_settings
        if (flow.flow_behavior == 'OUTFLOW'):
            return True

    @staticmethod
    def poll_fluid_flow_liquid(context):
        if not PhysicButtonsPanel.poll_fluid_flow(context):
            return False

        md = context.fluid
        flow = md.flow_settings
        if (flow.flow_type == 'LIQUID'):
            return True


class PHYSICS_PT_fluid(PhysicButtonsPanel, Panel):
    bl_label = "Fluid"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.engine in cls.COMPAT_ENGINES) and (context.fluid)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        if not bpy.app.build_options.fluid:
            col = layout.column(align=True)
            col.alignment = 'RIGHT'
            col.label(text="Built without Fluid modifier")
            return
        md = context.fluid

        layout.prop(md, "fluid_type")


class PHYSICS_PT_settings(PhysicButtonsPanel, Panel):
    bl_label = "Settings"
    bl_parent_id = 'PHYSICS_PT_fluid'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        ob = context.object
        scene = context.scene

        if md.fluid_type == 'DOMAIN':
            domain = md.domain_settings

            is_baking_any = domain.is_cache_baking_any
            has_baked_data = domain.has_cache_baked_data

            row = layout.row()
            row.enabled = not is_baking_any and not has_baked_data
            row.prop(domain, "domain_type", expand=False)

            flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
            flow.enabled = not is_baking_any and not has_baked_data

            col = flow.column()
            col.enabled = not domain.has_cache_baked_guide
            col.prop(domain, "resolution_max", text="Resolution Divisions")
            col.prop(domain, "time_scale", text="Time Scale")
            col.prop(domain, "cfl_condition", text="CFL Number")

            col = flow.column()
            col.prop(domain, "use_adaptive_timesteps")
            sub = col.column(align=True)
            sub.active = domain.use_adaptive_timesteps
            sub.prop(domain, "timesteps_max", text="Timesteps Maximum")
            sub.prop(domain, "timesteps_min", text="Minimum")

            col.separator()

            col = flow.column()
            if scene.use_gravity:
                sub = col.column()
                sub.enabled = False
                sub.prop(domain, "gravity", text="Using Scene Gravity", icon='SCENE_DATA')
            else:
                col.prop(domain, "gravity", text="Gravity")

            col = flow.column()
            if PhysicButtonsPanel.poll_gas_domain(context):
                col.prop(domain, "clipping", text="Empty Space")
            col.prop(domain, "delete_in_obstacle", text="Delete In Obstacle")

            if domain.cache_type == 'MODULAR':
                col.separator()

                # Deactivate bake operator if guides are enabled but not baked yet.
                note_flag = True
                if self.check_domain_has_unbaked_guide(domain) and domain.cache_type == 'MODULAR':
                    note = layout.split()
                    note_flag = False
                    note.enabled = note_flag
                    note.label(icon='INFO', text="Unbaked Guides: Bake Guides or disable them")

                split = layout.split()
                split.enabled = note_flag

                bake_incomplete = (domain.cache_frame_pause_data < domain.cache_frame_end)
                if domain.has_cache_baked_data and not domain.is_cache_baking_data and bake_incomplete:
                    col = split.column()
                    col.operator("fluid.bake_data", text="Resume")
                    col = split.column()
                    col.operator("fluid.free_data", text="Free")
                elif domain.is_cache_baking_data and not domain.has_cache_baked_data:
                    split.enabled = False
                    split.operator("fluid.pause_bake", text="Baking Data - ESC to pause")
                elif not domain.has_cache_baked_data and not domain.is_cache_baking_data:
                    split.operator("fluid.bake_data", text="Bake Data")
                else:
                    split.operator("fluid.free_data", text="Free Data")

        elif md.fluid_type == 'FLOW':
            flow = md.flow_settings

            row = layout.row()
            row.prop(flow, "flow_type", expand=False)

            grid = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

            col = grid.column()
            col.prop(flow, "flow_behavior", expand=False)
            if flow.flow_behavior in {'INFLOW', 'OUTFLOW'}:
                col.prop(flow, "use_inflow")

            col.prop(flow, "subframes", text="Sampling Substeps")

            if not flow.flow_behavior == 'OUTFLOW' and flow.flow_type in {'SMOKE', 'BOTH', 'FIRE'}:

                if flow.flow_type in {'SMOKE', 'BOTH'}:
                    col.prop(flow, "smoke_color", text="Smoke Color")

                col = grid.column(align=True)
                col.prop(flow, "use_absolute", text="Absolute Density")

                if flow.flow_type in {'SMOKE', 'BOTH'}:
                    col.prop(flow, "temperature", text="Initial Temperature")
                    col.prop(flow, "density", text="Density")

                if flow.flow_type in {'FIRE', 'BOTH'}:
                    col.prop(flow, "fuel_amount", text="Fuel")

                col.separator()
                col.prop_search(flow, "density_vertex_group", ob, "vertex_groups", text="Vertex Group")

        elif md.fluid_type == 'EFFECTOR':
            effector_settings = md.effector_settings

            row = layout.row()
            row.prop(effector_settings, "effector_type")

            grid = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

            col = grid.column()
            col.prop(effector_settings, "subframes", text="Sampling Substeps")
            col.prop(effector_settings, "surface_distance", text="Surface Thickness")

            col = grid.column()

            col.prop(effector_settings, "use_effector", text="Use Effector")
            col.prop(effector_settings, "use_plane_init", text="Is Planar")

            if effector_settings.effector_type == 'GUIDE':
                col.prop(effector_settings, "velocity_factor", text="Velocity Factor")
                col.prop(effector_settings, "guide_mode", text="Guide Mode")


class PHYSICS_PT_borders(PhysicButtonsPanel, Panel):
    bl_label = "Border Collisions"
    bl_parent_id = 'PHYSICS_PT_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        domain = md.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_data

        col = flow.column()
        col.prop(domain, "use_collision_border_front", text="Front")
        col = flow.column()
        col.prop(domain, "use_collision_border_back", text="Back")
        col = flow.column()
        col.prop(domain, "use_collision_border_right", text="Right")
        col = flow.column()
        col.prop(domain, "use_collision_border_left", text="Left")
        col = flow.column()
        col.prop(domain, "use_collision_border_top", text="Top")
        col = flow.column()
        col.prop(domain, "use_collision_border_bottom", text="Bottom")


class PHYSICS_PT_smoke(PhysicButtonsPanel, Panel):
    bl_label = "Gas"
    bl_parent_id = 'PHYSICS_PT_fluid'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_gas_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        domain = md.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_data

        col = flow.column(align=True)
        col.prop(domain, "alpha", text="Buoyancy Density")
        col.prop(domain, "beta", text="Heat")
        col = flow.column()
        col.prop(domain, "vorticity")


class PHYSICS_PT_smoke_dissolve(PhysicButtonsPanel, Panel):
    bl_label = "Dissolve"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_gas_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any

        self.layout.enabled = not is_baking_any
        self.layout.prop(md, "use_dissolve_smoke", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        domain = md.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_data

        layout.active = domain.use_dissolve_smoke

        col = flow.column()
        col.prop(domain, "dissolve_speed", text="Time")

        col = flow.column()
        col.prop(domain, "use_dissolve_smoke_log", text="Slow")


class PHYSICS_PT_fire(PhysicButtonsPanel, Panel):
    bl_label = "Fire"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_gas_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        domain = md.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_data

        col = flow.column()
        col.prop(domain, "burning_rate", text="Reaction Speed")
        row = col.row()
        sub = row.column(align=True)
        sub.prop(domain, "flame_smoke", text="Flame Smoke")
        sub.prop(domain, "flame_vorticity", text="Vorticity")

        col = flow.column(align=True)
        col.prop(domain, "flame_max_temp", text="Temperature Maximum")
        col.prop(domain, "flame_ignition", text="Minimum")
        row = col.row()
        row.prop(domain, "flame_smoke_color", text="Flame Color")


class PHYSICS_PT_liquid(PhysicButtonsPanel, Panel):
    bl_label = "Liquid"
    bl_parent_id = 'PHYSICS_PT_fluid'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_liquid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any

        self.layout.enabled = not is_baking_any
        self.layout.prop(md, "use_flip_particles", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        domain = md.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data

        layout.enabled = not is_baking_any and not has_baked_data
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(domain, "simulation_method", expand=False)
        col.prop(domain, "flip_ratio", text="FLIP Ratio")
        col = col.column(align=True)
        col.prop(domain, "particle_radius", text="Particle Radius")
        col.prop(domain, "particle_number", text="Sampling")
        col.prop(domain, "particle_randomness", text="Randomness")

        col = flow.column()
        col = col.column(align=True)
        col.prop(domain, "particle_max", text="Particles Maximum")
        col.prop(domain, "particle_min", text="Minimum")

        col.separator()

        col = col.column()
        col.prop(domain, "particle_band_width", text="Narrow Band Width")

        col = col.column()
        col.prop(domain, "use_fractions", text="Fractional Obstacles")
        sub = col.column()
        sub.active = domain.use_fractions
        sub.prop(domain, "fractions_threshold", text="Obstacle-Fluid Threshold")


class PHYSICS_PT_flow_source(PhysicButtonsPanel, Panel):
    bl_label = "Flow Source"
    bl_parent_id = 'PHYSICS_PT_settings'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_flow(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        flow = context.fluid.flow_settings

        col = layout.column()
        col.prop(flow, "flow_source", expand=False, text="Flow Source")
        if flow.flow_source == 'PARTICLES':
            col.prop_search(flow, "particle_system", ob, "particle_systems", text="Particle System")

        grid = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = grid.column()
        if flow.flow_source == 'MESH':
            col.prop(flow, "use_plane_init", text="Is Planar")
            col.prop(flow, "surface_distance", text="Surface Emission")
            if flow.flow_type in {'SMOKE', 'BOTH', 'FIRE'}:
                col = grid.column()
                col.prop(flow, "volume_density", text="Volume Emission")

        if flow.flow_source == 'PARTICLES':
            col.prop(flow, "use_particle_size", text="Set Size")
            sub = col.column()
            sub.active = flow.use_particle_size
            sub.prop(flow, "particle_size")


class PHYSICS_PT_flow_initial_velocity(PhysicButtonsPanel, Panel):
    bl_label = "Initial Velocity"
    bl_parent_id = 'PHYSICS_PT_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_flow(context):
            return False

        if PhysicButtonsPanel.poll_fluid_flow_outflow(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid
        flow_smoke = md.flow_settings

        self.layout.prop(flow_smoke, "use_initial_velocity", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        md = context.fluid
        flow_smoke = md.flow_settings

        flow.active = flow_smoke.use_initial_velocity

        col = flow.column()
        col.prop(flow_smoke, "velocity_factor")

        if flow_smoke.flow_source == 'MESH':
            col.prop(flow_smoke, "velocity_normal")
            # col.prop(flow_smoke, "velocity_random")
            col = flow.column()
            col.prop(flow_smoke, "velocity_coord")


class PHYSICS_PT_flow_texture(PhysicButtonsPanel, Panel):
    bl_label = "Texture"
    bl_parent_id = 'PHYSICS_PT_settings'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_flow(context):
            return False

        if PhysicButtonsPanel.poll_fluid_flow_outflow(context):
            return False

        if PhysicButtonsPanel.poll_fluid_flow_liquid(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid
        flow_smoke = md.flow_settings

        self.layout.prop(flow_smoke, "use_texture", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        ob = context.object
        flow_smoke = context.fluid.flow_settings

        sub = flow.column()
        sub.active = flow_smoke.use_texture
        sub.prop(flow_smoke, "noise_texture")
        sub.prop(flow_smoke, "texture_map_type", text="Mapping")

        col = flow.column()
        sub = col.column()
        sub.active = flow_smoke.use_texture

        if flow_smoke.texture_map_type == 'UV':
            sub.prop_search(flow_smoke, "uv_layer", ob.data, "uv_layers")

        if flow_smoke.texture_map_type == 'AUTO':
            sub.prop(flow_smoke, "texture_size")

        sub.prop(flow_smoke, "texture_offset")


class PHYSICS_PT_adaptive_domain(PhysicButtonsPanel, Panel):
    bl_label = "Adaptive Domain"
    bl_parent_id = 'PHYSICS_PT_settings'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_gas_domain(context):
            return False

        md = context.fluid
        domain = md.domain_settings
        # Effector guides require a fixed domain size
        if domain.use_guide and domain.guide_source == 'EFFECTOR':
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_any = domain.has_cache_baked_any

        self.layout.enabled = not is_baking_any and not has_baked_any
        self.layout.prop(md, "use_adaptive_domain", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings
        layout.active = domain.use_adaptive_domain

        is_baking_any = domain.is_cache_baking_any
        has_baked_any = domain.has_cache_baked_any

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)
        flow.enabled = not is_baking_any and not has_baked_any

        col = flow.column()
        col.prop(domain, "additional_res", text="Add Resolution")
        col.prop(domain, "adapt_margin")

        col.separator()

        col = flow.column()
        col.prop(domain, "adapt_threshold", text="Threshold")


class PHYSICS_PT_noise(PhysicButtonsPanel, Panel):
    bl_label = "Noise"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_gas_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings
        is_baking_any = domain.is_cache_baking_any
        self.layout.enabled = not is_baking_any
        self.layout.prop(md, "use_noise", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings
        layout.active = domain.use_noise

        is_baking_any = domain.is_cache_baking_any
        has_baked_noise = domain.has_cache_baked_noise

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_noise

        col = flow.column()
        col.prop(domain, "noise_scale", text="Upres Factor")
        # TODO (sebbas): Mantaflow only supports wavelet noise. Maybe get rid of noise type field.
        col.prop(domain, "noise_type", text="Noise Method")

        col = flow.column()
        col.prop(domain, "noise_strength", text="Strength")
        col.prop(domain, "noise_pos_scale", text="Scale")
        col.prop(domain, "noise_time_anim", text="Time")

        if domain.cache_type == 'MODULAR':
            col.separator()

            # Deactivate bake operator if data has not been baked yet.
            note_flag = True
            if domain.use_noise and not domain.has_cache_baked_data and domain.cache_type == 'MODULAR':
                note = layout.split()
                note_flag = False
                note.enabled = note_flag
                note.label(icon='INFO', text="Unbaked Data: Bake Data first")

            split = layout.split()
            split.enabled = domain.has_cache_baked_data and note_flag

            bake_incomplete = (domain.cache_frame_pause_noise < domain.cache_frame_end)
            if domain.has_cache_baked_noise and not domain.is_cache_baking_noise and bake_incomplete:
                col = split.column()
                col.operator("fluid.bake_noise", text="Resume")
                col = split.column()
                col.operator("fluid.free_noise", text="Free")
            elif not domain.has_cache_baked_noise and domain.is_cache_baking_noise:
                split.enabled = False
                split.operator("fluid.pause_bake", text="Baking Noise - ESC to pause")
            elif not domain.has_cache_baked_noise and not domain.is_cache_baking_noise:
                split.operator("fluid.bake_noise", text="Bake Noise")
            else:
                split.operator("fluid.free_noise", text="Free Noise")


class PHYSICS_PT_mesh(PhysicButtonsPanel, Panel):
    bl_label = "Mesh"
    bl_parent_id = 'PHYSICS_PT_liquid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_liquid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings
        is_baking_any = domain.is_cache_baking_any
        self.layout.enabled = not is_baking_any
        self.layout.prop(md, "use_mesh", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings
        layout.active = domain.use_mesh

        is_baking_any = domain.is_cache_baking_any
        has_baked_mesh = domain.has_cache_baked_mesh

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_mesh

        col = flow.column()

        col.prop(domain, "mesh_scale", text="Upres Factor")
        col.prop(domain, "mesh_particle_radius", text="Particle Radius")

        col = flow.column()
        col.prop(domain, "use_speed_vectors", text="Use Speed Vectors")

        col.separator()
        col.prop(domain, "mesh_generator", text="Mesh Generator")

        if domain.mesh_generator in {'IMPROVED'}:
            col = flow.column(align=True)
            col.prop(domain, "mesh_smoothen_pos", text="Smoothing Positive")
            col.prop(domain, "mesh_smoothen_neg", text="Negative")

            col = flow.column(align=True)
            col.prop(domain, "mesh_concave_upper", text="Concavity Upper")
            col.prop(domain, "mesh_concave_lower", text="Lower")

        # TODO (sebbas): for now just interpolate any upres grids, ie not sampling highres grids
        #col.prop(domain, "highres_sampling", text="Flow Sampling:")

        if domain.cache_type == 'MODULAR':
            col.separator()

            # Deactivate bake operator if data has not been baked yet.
            note_flag = True
            if domain.use_mesh and not domain.has_cache_baked_data and domain.cache_type == 'MODULAR':
                note = layout.split()
                note_flag = False
                note.enabled = note_flag
                note.label(icon='INFO', text="Unbaked Data: Bake Data first")

            split = layout.split()
            split.enabled = domain.has_cache_baked_data and note_flag

            bake_incomplete = (domain.cache_frame_pause_mesh < domain.cache_frame_end)
            if domain.has_cache_baked_mesh and not domain.is_cache_baking_mesh and bake_incomplete:
                col = split.column()
                col.operator("fluid.bake_mesh", text="Resume")
                col = split.column()
                col.operator("fluid.free_mesh", text="Free")
            elif not domain.has_cache_baked_mesh and domain.is_cache_baking_mesh:
                split.enabled = False
                split.operator("fluid.pause_bake", text="Baking Mesh - ESC to pause")
            elif not domain.has_cache_baked_mesh and not domain.is_cache_baking_mesh:
                split.operator("fluid.bake_mesh", text="Bake Mesh")
            else:
                split.operator("fluid.free_mesh", text="Free Mesh")


class PHYSICS_PT_particles(PhysicButtonsPanel, Panel):
    bl_label = "Particles"
    bl_parent_id = 'PHYSICS_PT_liquid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_liquid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_particles = domain.has_cache_baked_particles
        using_particles = domain.use_spray_particles or domain.use_foam_particles or domain.use_bubble_particles

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any

        sndparticle_combined_export = domain.sndparticle_combined_export
        col = flow.column()
        row = col.row()
        row.enabled = sndparticle_combined_export in {'OFF', 'FOAM + BUBBLES'}
        row.prop(domain, "use_spray_particles", text="Spray")
        row.prop(domain, "use_foam_particles", text="Foam")
        row.prop(domain, "use_bubble_particles", text="Bubbles")

        col.separator()

        col.prop(domain, "sndparticle_combined_export")

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_particles
        flow.active = using_particles

        col = flow.column()
        col.prop(domain, "particle_scale", text="Upres Factor")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_potential_max_wavecrest", text="Wave Crest Potential Maximum")
        col.prop(domain, "sndparticle_potential_min_wavecrest", text="Minimum")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_potential_max_trappedair", text="Trapped Air Potential Maximum")
        col.prop(domain, "sndparticle_potential_min_trappedair", text="Minimum")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_potential_max_energy", text="Kinetic Energy Potential Maximum")
        col.prop(domain, "sndparticle_potential_min_energy", text="Minimum")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_potential_radius", text="Potential Radius")
        col.prop(domain, "sndparticle_update_radius", text="Particle Update Radius")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_sampling_wavecrest", text="Wave Crest Particle Sampling")
        col.prop(domain, "sndparticle_sampling_trappedair", text="Trapped Air Particle Sampling")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_life_max", text="Particle Life Maximum")
        col.prop(domain, "sndparticle_life_min", text="Minimum")
        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "sndparticle_bubble_buoyancy", text="Bubble Buoyancy")
        col.prop(domain, "sndparticle_bubble_drag", text="Bubble Drag")
        col.separator()

        col = flow.column()
        col.prop(domain, "sndparticle_boundary", text="Particles in Boundary:")

        if domain.cache_type == 'MODULAR':
            col.separator()

            # Deactivate bake operator if data has not been baked yet.
            note_flag = True
            if using_particles and not domain.has_cache_baked_data and domain.cache_type == 'MODULAR':
                note = layout.split()
                note_flag = False
                note.enabled = note_flag
                note.label(icon='INFO', text="Unbaked Data: Bake Data first")

            split = layout.split()
            split.enabled = (
                note_flag and
                domain.has_cache_baked_data and
                (domain.use_spray_particles or
                 domain.use_bubble_particles or
                 domain.use_foam_particles or
                 domain.use_tracer_particles)
            )

            bake_incomplete = (domain.cache_frame_pause_particles < domain.cache_frame_end)
            if domain.has_cache_baked_particles and not domain.is_cache_baking_particles and bake_incomplete:
                col = split.column()
                col.operator("fluid.bake_particles", text="Resume")
                col = split.column()
                col.operator("fluid.free_particles", text="Free")
            elif not domain.has_cache_baked_particles and domain.is_cache_baking_particles:
                split.enabled = False
                split.operator("fluid.pause_bake", text="Baking Particles - ESC to pause")
            elif not domain.has_cache_baked_particles and not domain.is_cache_baking_particles:
                split.operator("fluid.bake_particles", text="Bake Particles")
            else:
                split.operator("fluid.free_particles", text="Free Particles")


class PHYSICS_PT_diffusion(PhysicButtonsPanel, Panel):
    bl_label = "Diffusion"
    bl_parent_id = 'PHYSICS_PT_liquid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        # Fluid diffusion only enabled for liquids (surface tension and viscosity not relevant for smoke)
        if not PhysicButtonsPanel.poll_liquid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings
        is_baking_any = domain.is_cache_baking_any
        has_baked_any = domain.has_cache_baked_any
        self.layout.enabled = not is_baking_any and not has_baked_any
        self.layout.prop(md, "use_diffusion", text="")

    def draw_header_preset(self, _context):
        FLUID_PT_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings
        layout.active = domain.use_diffusion

        is_baking_any = domain.is_cache_baking_any
        has_baked_any = domain.has_cache_baked_any
        has_baked_data = domain.has_cache_baked_data

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_any and not has_baked_data

        col = flow.column(align=True)
        col.prop(domain, "viscosity_base", text="Base")
        col.prop(domain, "viscosity_exponent", text="Exponent", slider=True)

        col = flow.column()
        col.prop(domain, "surface_tension", text="Surface Tension")


class PHYSICS_PT_guide(PhysicButtonsPanel, Panel):
    bl_label = "Guides"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid.domain_settings
        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any

        self.layout.enabled = not is_baking_any
        self.layout.prop(md, "use_guide", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings

        layout.active = domain.use_guide

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_data

        col = flow.column()
        col.prop(domain, "guide_alpha", text="Weight")
        col.prop(domain, "guide_beta", text="Size")
        col.prop(domain, "guide_vel_factor", text="Velocity Factor")

        col = flow.column()
        col.prop(domain, "guide_source", text="Velocity Source")
        if domain.guide_source == 'DOMAIN':
            col.prop(domain, "guide_parent", text="Guide Parent")

        if domain.cache_type == 'MODULAR':
            col.separator()

            if domain.guide_source == 'EFFECTOR':
                split = layout.split()
                bake_incomplete = (domain.cache_frame_pause_guide < domain.cache_frame_end)
                if domain.has_cache_baked_guide and not domain.is_cache_baking_guide and bake_incomplete:
                    col = split.column()
                    col.operator("fluid.bake_guides", text="Resume")
                    col = split.column()
                    col.operator("fluid.free_guides", text="Free")
                elif not domain.has_cache_baked_guide and domain.is_cache_baking_guide:
                    split.enabled = False
                    split.operator("fluid.pause_bake", text="Baking Guides - ESC to pause")
                elif not domain.has_cache_baked_guide and not domain.is_cache_baking_guide:
                    split.operator("fluid.bake_guides", text="Bake Guides")
                else:
                    split.operator("fluid.free_guides", text="Free Guides")


class PHYSICS_PT_collections(PhysicButtonsPanel, Panel):
    bl_label = "Collections"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(domain, "fluid_group", text="Flow")

        # col.prop(domain, "effector_group", text="Forces")
        col.prop(domain, "effector_group", text="Effector")


class PHYSICS_PT_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cache"
    bl_parent_id = 'PHYSICS_PT_fluid'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        md = context.fluid
        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_data = domain.has_cache_baked_data
        has_baked_noise = domain.has_cache_baked_noise
        has_baked_mesh = domain.has_cache_baked_mesh
        has_baked_particles = domain.has_cache_baked_particles

        col = layout.column()
        col.prop(domain, "cache_directory", text="")
        col.enabled = not is_baking_any

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(domain, "cache_type", expand=False)
        col.enabled = not is_baking_any

        col.separator()

        row = col.row()
        col = row.column(align=True)
        col.prop(domain, "cache_frame_start", text="Frame Start")
        col.prop(domain, "cache_frame_end", text="End")
        row.enabled = not is_baking_any

        col.separator()

        col = flow.column()
        row = col.row()
        row.enabled = not is_baking_any and not has_baked_data
        row.prop(domain, "cache_data_format", text="Data File Format")

        if md.domain_settings.domain_type in {'GAS'}:
            if domain.use_noise:
                row = col.row()
                row.enabled = not is_baking_any and not has_baked_noise
                row.prop(domain, "cache_noise_format", text="Noise File Format")

        if md.domain_settings.domain_type in {'LIQUID'}:
            # File format for all particle systemes (FLIP and secondary)
            row = col.row()
            row.enabled = not is_baking_any and not has_baked_particles and not has_baked_data
            row.prop(domain, "cache_particle_format", text="Particle File Format")

            if domain.use_mesh:
                row = col.row()
                row.enabled = not is_baking_any and not has_baked_mesh
                row.prop(domain, "cache_mesh_format", text="Mesh File Format")

        if domain.cache_type == 'FINAL':

            col.separator()
            split = layout.split()

            if domain.is_cache_baking_data and not domain.has_cache_baked_data:
                split.enabled = False
                split.operator("fluid.pause_bake", text="Baking All - ESC to cancel")
            elif not domain.has_cache_baked_data and not domain.is_cache_baking_data:
                split.operator("fluid.bake_all", text="Bake All")
            else:
                split.operator("fluid.free_all", text="Free All")


class PHYSICS_PT_export(PhysicButtonsPanel, Panel):
    bl_label = "Advanced"
    bl_parent_id = 'PHYSICS_PT_cache'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        # Only show the advanced panel to advanced users who know Mantaflow's birthday :)
        if not PhysicButtonsPanel.poll_fluid_domain(context) or bpy.app.debug_value != 3001:
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings

        is_baking_any = domain.is_cache_baking_any
        has_baked_any = domain.has_cache_baked_any

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = not is_baking_any and not has_baked_any

        col = flow.column()
        col.prop(domain, "export_manta_script", text="Export Mantaflow Script")


class PHYSICS_PT_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Field Weights"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        domain = context.fluid.domain_settings
        effector_weights_ui(self, domain.effector_weights, 'SMOKE')


class PHYSICS_PT_viewport_display(PhysicButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (PhysicButtonsPanel.poll_gas_domain(context))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        domain = context.fluid.domain_settings
        slice_method = domain.slice_method
        axis_slice_method = domain.axis_slice_method

        do_axis_slicing = (slice_method == 'AXIS_ALIGNED')
        do_full_slicing = (axis_slice_method == 'FULL')

        col = flow.column(align=False)
        col.prop(domain, "display_thickness")
        col.prop(domain, "display_interpolation")
        col.separator()

        col = flow.column()
        col.prop(domain, "slice_method", text="Slicing")

        col = col.column()
        col.active = do_axis_slicing
        col.prop(domain, "axis_slice_method")

        if not do_full_slicing and do_axis_slicing:
            col.prop(domain, "slice_axis")
            col.prop(domain, "slice_depth")

        col = col.column()
        col.active = do_full_slicing or not do_axis_slicing
        col.prop(domain, "slice_per_voxel")


class PHYSICS_PT_viewport_display_color(PhysicButtonsPanel, Panel):
    bl_label = "Color Mapping"
    bl_parent_id = 'PHYSICS_PT_viewport_display'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (PhysicButtonsPanel.poll_gas_domain(context))

    def draw_header(self, context):
        md = context.fluid.domain_settings

        self.layout.prop(md, "use_color_ramp", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.fluid.domain_settings
        col = layout.column()
        col.active = domain.use_color_ramp
        col.prop(domain, "coba_field")

        col.use_property_split = False

        col = col.column()
        col.template_color_ramp(domain, "color_ramp", expand=True)


class PHYSICS_PT_viewport_display_debug(PhysicButtonsPanel, Panel):
    bl_label = "Debug Velocity"
    bl_parent_id = 'PHYSICS_PT_viewport_display'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (PhysicButtonsPanel.poll_gas_domain(context))

    def draw_header(self, context):
        md = context.fluid.domain_settings

        self.layout.prop(md, "show_velocity", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        domain = context.fluid.domain_settings

        col = flow.column()
        col.active = domain.show_velocity
        col.prop(domain, "vector_display_type", text="Display As")
        col.prop(domain, "vector_scale")


classes = (
    FLUID_PT_presets,
    PHYSICS_PT_fluid,
    PHYSICS_PT_settings,
    PHYSICS_PT_borders,
    PHYSICS_PT_adaptive_domain,
    PHYSICS_PT_smoke,
    PHYSICS_PT_smoke_dissolve,
    PHYSICS_PT_noise,
    PHYSICS_PT_fire,
    PHYSICS_PT_liquid,
    PHYSICS_PT_diffusion,
    PHYSICS_PT_particles,
    PHYSICS_PT_mesh,
    PHYSICS_PT_guide,
    PHYSICS_PT_collections,
    PHYSICS_PT_cache,
    PHYSICS_PT_export,
    PHYSICS_PT_field_weights,
    PHYSICS_PT_flow_source,
    PHYSICS_PT_flow_initial_velocity,
    PHYSICS_PT_flow_texture,
    PHYSICS_PT_viewport_display,
    PHYSICS_PT_viewport_display_color,
    PHYSICS_PT_viewport_display_debug,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
