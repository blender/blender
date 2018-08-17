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
    Panel,
)
from bpy.app.translations import pgettext_iface as iface_
from bl_operators.presets import PresetMenu


class FLUID_PT_presets(PresetMenu):
    bl_label = "Fluid Presets"
    preset_subdir = "fluid"
    preset_operator = "script.execute_preset"
    preset_add_operator = "fluid.preset_add"


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll_fluid(context):
        ob = context.object
        if not ((ob and ob.type == 'MESH') and (context.fluid)):
            return False

        return (bpy.app.build_options.mod_fluid)

    def poll_fluid_settings(context):
        if not (PhysicButtonsPanel.poll_fluid(context)):
            return False

        md = context.fluid
        return md and md.settings and (md.settings.type != 'NONE')

    def poll_fluid_domain(context):
        if not PhysicButtonsPanel.poll_fluid(context):
            return False

        md = context.fluid
        return md and md.settings and (md.settings.type == 'DOMAIN')


class PHYSICS_PT_fluid(PhysicButtonsPanel, Panel):
    bl_label = "Fluid"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and context.engine in cls.COMPAT_ENGINES and (context.fluid)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        if not bpy.app.build_options.mod_fluid:
            col = layout.column()
            col.alignment = 'RIGHT'
            col.label("Built without fluids")
            return

        md = context.fluid
        fluid = md.settings

        col = layout.column()
        col.prop(fluid, "type")


class PHYSICS_PT_fluid_settings(PhysicButtonsPanel, Panel):
    bl_label = "Settings"
    bl_parent_id = "PHYSICS_PT_fluid"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_settings(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.fluid
        fluid = md.settings
        if fluid.type not in {'NONE', 'DOMAIN', 'PARTICLE', 'FLUID', 'OBSTACLE'}:
            self.layout.prop(fluid, "use", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.fluid
        fluid = md.settings

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        if fluid.type not in {'NONE', 'DOMAIN', 'PARTICLE', 'FLUID', 'OBSTACLE'}:
            flow.active = fluid.use

        if fluid.type == 'DOMAIN':
            col = flow.column()

            if bpy.app.build_options.openmp:
                col.prop(fluid, "threads", text="Simulation Threads")
                col.separator()

            col.prop(fluid, "resolution", text="Final Resolution")
            col.prop(fluid, "preview_resolution", text="Preview")

            col.separator()

            col = flow.column()
            col.prop(fluid, "render_display_mode", text="Render Display")
            col.prop(fluid, "viewport_display_mode", text="Viewport")

            col.separator()

            col = flow.column()
            sub = col.column(align=True)
            sub.prop(fluid, "start_time", text="Time Start")
            sub.prop(fluid, "end_time", text="End")
            col.prop(fluid, "simulation_rate", text="Speed")

            col = flow.column()
            col.prop(fluid, "use_speed_vectors")
            col.prop(fluid, "use_reverse_frames")
            col.prop(fluid, "frame_offset", text="Offset")

        elif fluid.type == 'FLUID':
            col = flow.column()
            col.prop(fluid, "volume_initialization", text="Volume Initialization")
            col.prop(fluid, "use_animated_mesh")

            col = flow.column()
            col.prop(fluid, "initial_velocity", text="Initial Velocity")

        elif fluid.type == 'OBSTACLE':
            col = flow.column()
            col.prop(fluid, "volume_initialization", text="Volume Initialization")
            col.prop(fluid, "use_animated_mesh")

            col = flow.column()
            subcol = col.column()
            subcol.enabled = not fluid.use_animated_mesh
            subcol.prop(fluid, "slip_type", text="Slip Type")

            if fluid.slip_type == 'PARTIALSLIP':
                subcol.prop(fluid, "partial_slip_factor", text="Amount", slider=True)

            col.prop(fluid, "impact_factor", text="Impact Factor")

        elif fluid.type == 'INFLOW':
            col = flow.column()
            col.prop(fluid, "volume_initialization", text="Volume Initialization")
            col.prop(fluid, "use_animated_mesh")

            row = col.row()
            row.active = not fluid.use_animated_mesh
            row.prop(fluid, "use_local_coords")

            col = flow.column()
            col.prop(fluid, "inflow_velocity", text="Inflow Velocity")

        elif fluid.type == 'OUTFLOW':
            col = flow.column()
            col.prop(fluid, "volume_initialization", text="Volume Initialization")

            col = flow.column()
            col.prop(fluid, "use_animated_mesh")

        elif fluid.type == 'PARTICLE':
            col = flow.column()
            col.prop(fluid, "particle_influence", text="Influence Size")
            col.prop(fluid, "alpha_influence", text="Alpha")

            col = flow.column()
            col.prop(fluid, "use_drops")
            col.prop(fluid, "use_floats")
            col.prop(fluid, "show_tracer")

        elif fluid.type == 'CONTROL':
            col = flow.column()
            col.prop(fluid, "quality", slider=True)
            col.prop(fluid, "use_reverse_frames")

            col = flow.column()
            col.prop(fluid, "start_time", text="Time Start")
            col.prop(fluid, "end_time", text="End")

            col.separator()

            col = flow.column()
            col.prop(fluid, "attraction_strength", text="Attraction Strength")
            col.prop(fluid, "attraction_radius", text="Radius")

            col.separator()

            col = flow.column(align=True)
            col.prop(fluid, "velocity_strength", text="Velocity Strength")
            col.prop(fluid, "velocity_radius", text="Radius")


class PHYSICS_PT_fluid_particle_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cache"
    bl_parent_id = "PHYSICS_PT_fluid"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_settings(context):
            return False

        md = context.fluid
        return md and md.settings and (md.settings.type == 'PARTICLE') and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        md = context.fluid
        fluid = md.settings

        layout.prop(fluid, "filepath", text="")


class PHYSICS_PT_domain_bake(PhysicButtonsPanel, Panel):
    bl_label = "Bake"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        md = context.fluid
        fluid = md.settings

        row = layout.row(align=True)
        row.alignment = 'RIGHT'
        row.label("Cache Path")

        layout.prop(fluid, "filepath", text="")

        # odd formatting here so translation script can extract string
        layout.operator(
            "fluid.bake", text=iface_("Bake (Req. Memory: %s)") % fluid.memory_estimate,
            translate=False, icon='MOD_FLUIDSIM'
        )


class PHYSICS_PT_domain_gravity(PhysicButtonsPanel, Panel):
    bl_label = "World"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        fluid = context.fluid.settings
        scene = context.scene

        col = layout.column()

        use_gravity = scene.use_gravity
        use_units = scene.unit_settings.system != 'NONE'

        if use_gravity or use_units:
            s_gravity = " Gravity" if use_gravity else ""
            s_units = " Units" if use_units else ""
            s_and = " and " if use_gravity and use_units else ""
            warn = f"Using {s_gravity}{s_and}{s_units} from Scene"

            sub = col.column()
            sub.alignment = 'RIGHT'
            sub.label(text=warn)

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        sub = col.column()
        sub.enabled = not use_gravity
        sub.prop(fluid, "gravity", text="Gravity")

        sub = col.column()
        sub.enabled = not use_units
        sub.prop(fluid, "simulation_scale", text="Scene Size Meters" if use_units else "World Size Meters")

        col.separator()

        col = flow.column()
        col.prop(fluid, "grid_levels", text="Optimization", slider=True)
        col.prop(fluid, "compressibility", slider=True)


class PHYSICS_PT_domain_viscosity(PhysicButtonsPanel, Panel):
    bl_label = "Viscosity"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header_preset(self, context):
        FLUID_PT_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        fluid = context.fluid.settings

        col = flow.column()
        col.prop(fluid, "viscosity_base", text="Base")

        col = flow.column()
        col.prop(fluid, "viscosity_exponent", text="Exponent", slider=True)


class PHYSICS_PT_domain_boundary(PhysicButtonsPanel, Panel):
    bl_label = "Boundary"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        fluid = context.fluid.settings

        col = flow.column()
        col.prop(fluid, "slip_type", text="Type")

        col.separator()

        if fluid.slip_type == 'PARTIALSLIP':
            col.prop(fluid, "partial_slip_factor", slider=True, text="Amount")

        col = flow.column()
        col.prop(fluid, "surface_smooth", text="Surface Smoothing")
        col.prop(fluid, "surface_subdivisions", text="Subdivisions")
        col.prop(fluid, "use_surface_noobs")


class PHYSICS_PT_domain_particles(PhysicButtonsPanel, Panel):
    bl_label = "Particles"
    bl_parent_id = 'PHYSICS_PT_fluid'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_fluid_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        fluid = context.fluid.settings

        col = flow.column()
        col.prop(fluid, "tracer_particles", text="Tracer")

        col = flow.column()
        col.prop(fluid, "generate_particles", text="Generate")


classes = (
    FLUID_PT_presets,
    PHYSICS_PT_fluid,
    PHYSICS_PT_fluid_settings,
    PHYSICS_PT_fluid_particle_cache,
    PHYSICS_PT_domain_bake,
    PHYSICS_PT_domain_boundary,
    PHYSICS_PT_domain_particles,
    PHYSICS_PT_domain_gravity,
    PHYSICS_PT_domain_viscosity,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
