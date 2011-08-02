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
from blf import gettext as _

class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.fluid)


class PHYSICS_PT_fluid(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = _("Fluid")

    def draw(self, context):
        layout = self.layout

        md = context.fluid

        if md:
            fluid = md.settings

            row = layout.row()
            if fluid is None:
                row.label(_("built without fluids"))
                return

            row.prop(fluid, "type")
            if fluid.type not in {'NONE', 'DOMAIN', 'PARTICLE', 'FLUID'}:
                row.prop(fluid, "use", text="")

            layout = layout.column()
            if fluid.type not in {'NONE', 'DOMAIN', 'PARTICLE', 'FLUID'}:
                layout.active = fluid.use

            if fluid.type == 'DOMAIN':
                layout.operator("fluid.bake", text=_("Bake (Req. Memory:")+" %s)" % fluid.memory_estimate, icon='MOD_FLUIDSIM')
                split = layout.split()

                col = split.column()
                col.label(text=_("Resolution:"))
                col.prop(fluid, "resolution", text=_("Final"))
                col.label(text=_("Render Display:"))
                col.prop(fluid, "render_display_mode", text="")

                col = split.column()
                col.label()
                col.prop(fluid, "preview_resolution", text=_("Preview"))
                col.label(text=_("Viewport Display:"))
                col.prop(fluid, "viewport_display_mode", text="")

                split = layout.split()

                col = split.column()
                col.label(text=_("Time:"))
                sub = col.column(align=True)
                sub.prop(fluid, "start_time", text=_("Start"))
                sub.prop(fluid, "end_time", text=_("End"))

                col = split.column()
                col.label()
                col.prop(fluid, "use_speed_vectors")
                col.prop(fluid, "use_reverse_frames")

                layout.prop(fluid, "filepath", text="")

            elif fluid.type == 'FLUID':
                split = layout.split()

                col = split.column()
                col.label(text=_("Volume Initialization:"))
                col.prop(fluid, "volume_initialization", text="")
                col.prop(fluid, "use_animated_mesh")

                col = split.column()
                col.label(text=_("Initial Velocity:"))
                col.prop(fluid, "initial_velocity", text="")

            elif fluid.type == 'OBSTACLE':
                split = layout.split()

                col = split.column()
                col.label(text=_("Volume Initialization:"))
                col.prop(fluid, "volume_initialization", text="")
                col.prop(fluid, "use_animated_mesh")

                col = split.column()
                col.label(text=_("Slip Type:"))
                col.prop(fluid, "slip_type", text="")
                if fluid.slip_type == 'PARTIALSLIP':
                    col.prop(fluid, "partial_slip_factor", slider=True, text=_("Amount"))

                col.label(text=_("Impact:"))
                col.prop(fluid, "impact_factor", text=_("Factor"))

            elif fluid.type == 'INFLOW':
                split = layout.split()

                col = split.column()
                col.label(text=_("Volume Initialization:"))
                col.prop(fluid, "volume_initialization", text="")
                col.prop(fluid, "use_animated_mesh")
                row = col.row()
                row.active = not fluid.use_animated_mesh
                row.prop(fluid, "use_local_coords")

                col = split.column()
                col.label(text=_("Inflow Velocity:"))
                col.prop(fluid, "inflow_velocity", text="")

            elif fluid.type == 'OUTFLOW':
                split = layout.split()

                col = split.column()
                col.label(text=_("Volume Initialization:"))
                col.prop(fluid, "volume_initialization", text="")
                col.prop(fluid, "use_animated_mesh")

                split.column()

            elif fluid.type == 'PARTICLE':
                split = layout.split()

                col = split.column()
                col.label(text=_("Influence:"))
                col.prop(fluid, "particle_influence", text=_("Size"))
                col.prop(fluid, "alpha_influence", text=_("Alpha"))

                col = split.column()
                col.label(text=_("Type:"))
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
                col.label(text=_("Time:"))
                sub = col.column(align=True)
                sub.prop(fluid, "start_time", text=_("Start"))
                sub.prop(fluid, "end_time", text=_("End"))

                split = layout.split()

                col = split.column()
                col.label(text=_("Attraction Force:"))
                sub = col.column(align=True)
                sub.prop(fluid, "attraction_strength", text=_("Strength"))
                sub.prop(fluid, "attraction_radius", text=_("Radius"))

                col = split.column()
                col.label(text=_("Velocity Force:"))
                sub = col.column(align=True)
                sub.prop(fluid, "velocity_strength", text=_("Strength"))
                sub.prop(fluid, "velocity_radius", text=_("Radius"))


class PHYSICS_PT_domain_gravity(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = _("Domain World")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.fluid
        return md and md.settings and (md.settings.type == 'DOMAIN')

    def draw(self, context):
        layout = self.layout

        fluid = context.fluid.settings
        scene = context.scene

        split = layout.split()

        col = split.column()
        if scene.use_gravity:
            col.label(text=_("Using Scene Gravity"), icon="SCENE_DATA")
            sub = col.column()
            sub.enabled = False
            sub.prop(fluid, "gravity", text="")
        else:
            col.label(text=_("Gravity:"))
            col.prop(fluid, "gravity", text="")

        if scene.unit_settings.system != 'NONE':
            col.label(text=_("Using Scene Size Units"), icon="SCENE_DATA")
            sub = col.column()
            sub.enabled = False
            sub.prop(fluid, "simulation_scale", text=_("Metres"))
        else:
            col.label(text=_("Real World Size:"))
            col.prop(fluid, "simulation_scale", text=_("Metres"))

        col = split.column()
        col.label(text=_("Viscosity Presets:"))
        sub = col.column(align=True)
        sub.prop(fluid, "viscosity_preset", text="")

        if fluid.viscosity_preset == 'MANUAL':
            sub.prop(fluid, "viscosity_base", text=_("Base"))
            sub.prop(fluid, "viscosity_exponent", text=_("Exponent"), slider=True)

        col.label(text=_("Optimization:"))
        col.prop(fluid, "grid_levels", slider=True)
        col.prop(fluid, "compressibility", slider=True)


class PHYSICS_PT_domain_boundary(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = _("Domain Boundary")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.fluid
        return md and md.settings and (md.settings.type == 'DOMAIN')

    def draw(self, context):
        layout = self.layout

        fluid = context.fluid.settings

        split = layout.split()

        col = split.column()
        col.label(text=_("Slip Type:"))
        col.prop(fluid, "slip_type", text="")
        if fluid.slip_type == 'PARTIALSLIP':
            col.prop(fluid, "partial_slip_factor", slider=True, text=_("Amount"))
        col.prop(fluid, "surface_noobs")

        col = split.column()
        col.label(text=_("Surface:"))
        col.prop(fluid, "surface_smooth", text=_("Smoothing"))
        col.prop(fluid, "surface_subdivisions", text=_("Subdivisions"))


class PHYSICS_PT_domain_particles(PhysicButtonsPanel, bpy.types.Panel):
    bl_label = _("Domain Particles")
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        md = context.fluid
        return md and md.settings and (md.settings.type == 'DOMAIN')

    def draw(self, context):
        layout = self.layout

        fluid = context.fluid.settings

        col = layout.column(align=True)
        col.prop(fluid, "tracer_particles")
        col.prop(fluid, "generate_particles")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
