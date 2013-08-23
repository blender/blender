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
from bpy.types import Panel, Menu
from bpy.app.translations import pgettext_iface as iface_


class FLUID_MT_presets(Menu):
    bl_label = "Fluid Presets"
    preset_subdir = "fluid"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.fluid)


class PHYSICS_PT_fluid(PhysicButtonsPanel, Panel):
    bl_label = "Fluid"

    def draw(self, context):
        layout = self.layout

        md = context.fluid
        fluid = md.settings

        col = layout.column()
        if not bpy.app.build_options.mod_fluid:
            col.label("Built without fluids")
            return

        col.prop(fluid, "type")
        if fluid.type not in {'NONE', 'DOMAIN', 'PARTICLE', 'FLUID', 'OBSTACLE'}:
            col.prop(fluid, "use")

        layout = layout.column()
        if fluid.type not in {'NONE', 'DOMAIN', 'PARTICLE', 'FLUID', 'OBSTACLE'}:
            layout.active = fluid.use

        if fluid.type == 'DOMAIN':
            # odd formatting here so translation script can extract string
            layout.operator("fluid.bake", text=iface_("Bake (Req. Memory: %s)") % fluid.memory_estimate,
                            translate=False, icon='MOD_FLUIDSIM')

            if bpy.app.build_options.openmp:
                layout.prop(fluid, "threads", text="Simulation Threads")

            split = layout.split()

            col = split.column()
            col.label(text="Resolution:")
            col.prop(fluid, "resolution", text="Final")
            col.label(text="Render Display:")
            col.prop(fluid, "render_display_mode", text="")

            col = split.column()
            col.label()
            col.prop(fluid, "preview_resolution", text="Preview")
            col.label(text="Viewport Display:")
            col.prop(fluid, "viewport_display_mode", text="")

            split = layout.split()

            col = split.column()
            col.label(text="Time:")
            sub = col.column(align=True)
            sub.prop(fluid, "start_time", text="Start")
            sub.prop(fluid, "end_time", text="End")
            col.prop(fluid, "simulation_rate", text="Speed")

            col = split.column()
            col.label()
            sub = col.column(align=True)
            sub.prop(fluid, "use_speed_vectors")
            sub.prop(fluid, "use_reverse_frames")
            col.prop(fluid, "frame_offset", text="Offset")

            layout.prop(fluid, "filepath", text="")

        elif fluid.type == 'FLUID':
            split = layout.split()

            col = split.column()
            col.label(text="Volume Initialization:")
            col.prop(fluid, "volume_initialization", text="")
            col.prop(fluid, "use_animated_mesh")

            col = split.column()
            col.label(text="Initial Velocity:")
            col.prop(fluid, "initial_velocity", text="")

        elif fluid.type == 'OBSTACLE':
            split = layout.split()

            col = split.column()
            col.label(text="Volume Initialization:")
            col.prop(fluid, "volume_initialization", text="")
            col.prop(fluid, "use_animated_mesh")

            col = split.column()
            subsplit = col.split()
            subcol = subsplit.column()
            if fluid.use_animated_mesh:
                subcol.enabled = False
            subcol.label(text="Slip Type:")
            subcol.prop(fluid, "slip_type", text="")
            if fluid.slip_type == 'PARTIALSLIP':
                subcol.prop(fluid, "partial_slip_factor", slider=True, text="Amount")

            col.label(text="Impact:")
            col.prop(fluid, "impact_factor", text="Factor")

        elif fluid.type == 'INFLOW':
            split = layout.split()

            col = split.column()
            col.label(text="Volume Initialization:")
            col.prop(fluid, "volume_initialization", text="")
            col.prop(fluid, "use_animated_mesh")
            row = col.row()
            row.active = not fluid.use_animated_mesh
            row.prop(fluid, "use_local_coords")

            col = split.column()
            col.label(text="Inflow Velocity:")
            col.prop(fluid, "inflow_velocity", text="")

        elif fluid.type == 'OUTFLOW':
            col = layout.column()
            col.label(text="Volume Initialization:")
            col.prop(fluid, "volume_initialization", text="")
            col.prop(fluid, "use_animated_mesh")

        elif fluid.type == 'PARTICLE':
            split = layout.split()

            col = split.column()
            col.label(text="Influence:")
            col.prop(fluid, "particle_influence", text="Size")
            col.prop(fluid, "alpha_influence", text="Alpha")

            col = split.column()
            col.label(text="Type:")
            col.prop(fluid, "use_drops")
            col.prop(fluid, "use_floats")
            col.prop(fluid, "show_tracer")

            layout.prop(fluid, "filepath", text="")

        elif fluid.type == 'CONTROL':
            split = layout.split()

            col = split.column()
            col.label(text="")
            col.prop(fluid, "quality", slider=True)
            col.prop(fluid, "use_reverse_frames")

            col = split.column()
            col.label(text="Time:")
            sub = col.column(align=True)
            sub.prop(fluid, "start_time", text="Start")
            sub.prop(fluid, "end_time", text="End")

            split = layout.split()

            col = split.column()
            col.label(text="Attraction Force:")
            sub = col.column(align=True)
            sub.prop(fluid, "attraction_strength", text="Strength")
            sub.prop(fluid, "attraction_radius", text="Radius")

            col = split.column()
            col.label(text="Velocity Force:")
            sub = col.column(align=True)
            sub.prop(fluid, "velocity_strength", text="Strength")
            sub.prop(fluid, "velocity_radius", text="Radius")


class PHYSICS_PT_domain_gravity(PhysicButtonsPanel, Panel):
    bl_label = "Fluid World"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.fluid
        rd = context.scene.render
        return md and md.settings and (md.settings.type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        fluid = context.fluid.settings
        scene = context.scene

        split = layout.split()

        col = split.column()
        if scene.use_gravity:
            col.label(text="Use Scene Gravity", icon='SCENE_DATA')
            sub = col.column()
            sub.enabled = False
            sub.prop(fluid, "gravity", text="")
        else:
            col.label(text="Gravity:")
            col.prop(fluid, "gravity", text="")

        if scene.unit_settings.system != 'NONE':
            col.label(text="Use Scene Size Units", icon='SCENE_DATA')
            sub = col.column()
            sub.enabled = False
            sub.prop(fluid, "simulation_scale", text="Meters")
        else:
            col.label(text="Real World Size:")
            col.prop(fluid, "simulation_scale", text="Meters")

        col = split.column()
        col.label(text="Viscosity Presets:")
        sub = col.row(align=True)
        sub.menu("FLUID_MT_presets", text=bpy.types.FLUID_MT_presets.bl_label)
        sub.operator("fluid.preset_add", text="", icon='ZOOMIN')
        sub.operator("fluid.preset_add", text="", icon='ZOOMOUT').remove_active = True

        sub = col.column(align=True)
        sub.prop(fluid, "viscosity_base", text="Base")
        sub.prop(fluid, "viscosity_exponent", text="Exponent", slider=True)

        col.label(text="Optimization:")
        col.prop(fluid, "grid_levels", slider=True)
        col.prop(fluid, "compressibility", slider=True)


class PHYSICS_PT_domain_boundary(PhysicButtonsPanel, Panel):
    bl_label = "Fluid Boundary"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.fluid
        rd = context.scene.render
        return md and md.settings and (md.settings.type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        fluid = context.fluid.settings

        split = layout.split()

        col = split.column()
        col.label(text="Slip Type:")
        col.prop(fluid, "slip_type", text="")
        if fluid.slip_type == 'PARTIALSLIP':
            col.prop(fluid, "partial_slip_factor", slider=True, text="Amount")
        col.prop(fluid, "use_surface_noobs")

        col = split.column()
        col.label(text="Surface:")
        col.prop(fluid, "surface_smooth", text="Smoothing")
        col.prop(fluid, "surface_subdivisions", text="Subdivisions")


class PHYSICS_PT_domain_particles(PhysicButtonsPanel, Panel):
    bl_label = "Fluid Particles"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.fluid
        rd = context.scene.render
        return md and md.settings and (md.settings.type == 'DOMAIN') and (not rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        fluid = context.fluid.settings

        row = layout.row()
        row.prop(fluid, "tracer_particles", text="Tracer")
        row.prop(fluid, "generate_particles", text="Generate")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
