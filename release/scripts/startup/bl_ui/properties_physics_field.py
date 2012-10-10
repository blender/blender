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

from bl_ui.properties_physics_common import (basic_force_field_settings_ui,
                                             basic_force_field_falloff_ui,
                                             )


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.object) and (not rd.use_game_engine)


class PHYSICS_PT_field(PhysicButtonsPanel, Panel):
    bl_label = "Force Fields"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (not rd.use_game_engine) and (ob.field) and (ob.field.type != 'NONE')

    def draw(self, context):
        layout = self.layout

        ob = context.object
        field = ob.field

        split = layout.split(percentage=0.2)
        split.label(text="Type:")

        split.prop(field, "type", text="")

        if field.type not in {'NONE', 'GUIDE', 'TEXTURE'}:
            split = layout.split(percentage=0.2)
            split.label(text="Shape:")
            split.prop(field, "shape", text="")
        elif field.type == 'TEXTURE':
            split = layout.split(percentage=0.2)
            split.label(text="Texture:")
            split.row().template_ID(field, "texture", new="texture.new")

        split = layout.split()

        if field.type == 'NONE':
            return  # nothing to draw
        elif field.type == 'GUIDE':
            col = split.column()
            col.prop(field, "guide_minimum")
            col.prop(field, "guide_free")
            col.prop(field, "falloff_power")
            col.prop(field, "use_guide_path_add")
            col.prop(field, "use_guide_path_weight")

            col = split.column()
            col.label(text="Clumping:")
            col.prop(field, "guide_clump_amount")
            col.prop(field, "guide_clump_shape")

            row = layout.row()
            row.prop(field, "use_max_distance")
            sub = row.row()
            sub.active = field.use_max_distance
            sub.prop(field, "distance_max")

            layout.separator()

            layout.prop(field, "guide_kink_type")
            if field.guide_kink_type != 'NONE':
                layout.prop(field, "guide_kink_axis")

                split = layout.split()

                col = split.column()
                col.prop(field, "guide_kink_frequency")
                col.prop(field, "guide_kink_shape")

                col = split.column()
                col.prop(field, "guide_kink_amplitude")

        elif field.type == 'TEXTURE':
            col = split.column()
            col.prop(field, "strength")
            col.prop(field, "texture_mode", text="")
            col.prop(field, "texture_nabla")

            col = split.column()
            col.prop(field, "use_object_coords")
            col.prop(field, "use_2d_force")
        elif field.type == 'SMOKE_FLOW':
            col = split.column()
            col.prop(field, "strength")
            col.prop(field, "flow")
            col = split.column()
            col.label(text="Domain Object:")
            col.prop(field, "source_object", "")
            col.prop(field, "use_smoke_density")
        else:
            basic_force_field_settings_ui(self, context, field)

        if field.type not in {'NONE', 'GUIDE'}:

            layout.label(text="Falloff:")
            layout.prop(field, "falloff_type", expand=True)

            basic_force_field_falloff_ui(self, context, field)

            if field.falloff_type == 'CONE':
                layout.separator()

                split = layout.split(percentage=0.35)

                col = split.column()
                col.label(text="Angular:")
                col.prop(field, "use_radial_min", text="Use Minimum")
                col.prop(field, "use_radial_max", text="Use Maximum")

                col = split.column()
                col.prop(field, "radial_falloff", text="Power")

                sub = col.column()
                sub.active = field.use_radial_min
                sub.prop(field, "radial_min", text="Angle")

                sub = col.column()
                sub.active = field.use_radial_max
                sub.prop(field, "radial_max", text="Angle")

            elif field.falloff_type == 'TUBE':
                layout.separator()

                split = layout.split(percentage=0.35)

                col = split.column()
                col.label(text="Radial:")
                col.prop(field, "use_radial_min", text="Use Minimum")
                col.prop(field, "use_radial_max", text="Use Maximum")

                col = split.column()
                col.prop(field, "radial_falloff", text="Power")

                sub = col.column()
                sub.active = field.use_radial_min
                sub.prop(field, "radial_min", text="Distance")

                sub = col.column()
                sub.active = field.use_radial_max
                sub.prop(field, "radial_max", text="Distance")


class PHYSICS_PT_collision(PhysicButtonsPanel, Panel):
    bl_label = "Collision"

    @classmethod
    def poll(cls, context):
        ob = context.object
        rd = context.scene.render
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine) and (context.collision)

    def draw(self, context):
        layout = self.layout

        md = context.collision

        split = layout.split()

        coll = md.settings

        if coll:
            settings = context.object.collision

            layout.active = settings.use

            split = layout.split()

            col = split.column()
            col.label(text="Particle:")
            col.prop(settings, "permeability", slider=True)
            col.prop(settings, "stickiness")
            col.prop(settings, "use_particle_kill")
            col.label(text="Particle Damping:")
            sub = col.column(align=True)
            sub.prop(settings, "damping_factor", text="Factor", slider=True)
            sub.prop(settings, "damping_random", text="Random", slider=True)

            col.label(text="Particle Friction:")
            sub = col.column(align=True)
            sub.prop(settings, "friction_factor", text="Factor", slider=True)
            sub.prop(settings, "friction_random", text="Random", slider=True)

            col = split.column()
            col.label(text="Soft Body and Cloth:")
            sub = col.column(align=True)
            sub.prop(settings, "thickness_outer", text="Outer", slider=True)
            sub.prop(settings, "thickness_inner", text="Inner", slider=True)

            col.label(text="Soft Body Damping:")
            col.prop(settings, "damping", text="Factor", slider=True)

            col.label(text="Force Fields:")
            col.prop(settings, "absorption", text="Absorption")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
