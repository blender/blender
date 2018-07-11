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

from .properties_physics_common import (
    basic_force_field_settings_ui,
    basic_force_field_falloff_ui,
)


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        return (context.object) and (context.engine in cls.COMPAT_ENGINES)


class PHYSICS_PT_field(PhysicButtonsPanel, Panel):
    bl_label = "Force Fields"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        if not ob:
            return False
        return (context.engine in cls.COMPAT_ENGINES) and (ob.field) and (ob.field.type != 'NONE')

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        field = ob.field

        layout.prop(field, "type")

        if field.type not in {'NONE', 'GUIDE', 'TEXTURE'}:
            layout.prop(field, "shape", text="Shape")
        elif field.type == 'TEXTURE':
            layout.row().template_ID(field, "texture", new="texture.new")

        if field.type == 'NONE':
            return  # nothing to draw
        elif field.type == 'GUIDE':
            col = layout.column()
            col.prop(field, "guide_minimum")
            col.prop(field, "guide_free")
            col.prop(field, "falloff_power")
            col.prop(field, "use_guide_path_add")
            col.prop(field, "use_guide_path_weight")

            col.label(text="Clumping")
            col.prop(field, "guide_clump_amount")
            col.prop(field, "guide_clump_shape")

            col.prop(field, "use_max_distance")
            sub = col.column()
            sub.active = field.use_max_distance
            sub.prop(field, "distance_max")

            layout.separator()

            layout.prop(field, "guide_kink_type")
            if field.guide_kink_type != 'NONE':
                layout.prop(field, "guide_kink_axis")

                col = layout.column()
                col.prop(field, "guide_kink_frequency")
                col.prop(field, "guide_kink_shape")
                col.prop(field, "guide_kink_amplitude")

        elif field.type == 'TEXTURE':
            col = layout.column()
            col.prop(field, "strength")
            col.prop(field, "texture_mode")
            col.prop(field, "texture_nabla")

            col.prop(field, "use_object_coords")
            col.prop(field, "use_2d_force")
        elif field.type == 'SMOKE_FLOW':
            col = layout.column()
            col.prop(field, "strength")
            col.prop(field, "flow")
            col.prop(field, "source_object")
            col.prop(field, "use_smoke_density")
        else:
            basic_force_field_settings_ui(self, context, field)


class PHYSICS_PT_field_falloff(PhysicButtonsPanel, Panel):
    bl_label = "Falloff"
    bl_parent_id = "PHYSICS_PT_field"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (context.engine in cls.COMPAT_ENGINES) and (ob.field) and (ob.field.type not in {'NONE', 'GUIDE'})

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        field = ob.field

        layout.prop(field, "falloff_type", text="Shape")

        basic_force_field_falloff_ui(self, context, field)

        if field.falloff_type == 'CONE':
            layout.separator()

            col = layout.column()
            col.prop(field, "radial_falloff", text="Power")

            col.label(text="Angular")

            col.prop(field, "use_radial_min", text="Use Min Angle")
            sub = col.column()
            sub.active = field.use_radial_min
            sub.prop(field, "radial_min", text="Min Angle")

            col.prop(field, "use_radial_max", text="Use Max Angle")
            sub = col.column()
            sub.active = field.use_radial_max
            sub.prop(field, "radial_max", text="Max Angle")

        elif field.falloff_type == 'TUBE':
            layout.separator()

            col = layout.column()

            col.prop(field, "radial_falloff", text="Power")

            col.label(text="Radial")

            col.prop(field, "use_radial_min", text="Use Minimum")
            sub = col.column()
            sub.active = field.use_radial_min
            sub.prop(field, "radial_min", text="Distance")

            col.prop(field, "use_radial_max", text="Use Maximum")
            sub = col.column()
            sub.active = field.use_radial_max
            sub.prop(field, "radial_max", text="Distance")


class PHYSICS_PT_collision(PhysicButtonsPanel, Panel):
    bl_label = "Collision"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.engine in cls.COMPAT_ENGINES) and (context.collision)

    def draw(self, context):
        layout = self.layout

        md = context.collision

        split = layout.split()
        layout.use_property_split = True

        coll = md.settings

        if coll:
            settings = context.object.collision

            layout.active = settings.use

            col = layout.column()
            col.prop(settings, "absorption", text="Force Field Absorption")


class PHYSICS_PT_collision_particle(PhysicButtonsPanel, Panel):
    bl_label = "Particle"
    bl_parent_id = "PHYSICS_PT_collision"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.engine in cls.COMPAT_ENGINES) and (context.collision)

    def draw(self, context):
        layout = self.layout

        md = context.collision

        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        coll = md.settings

        if coll:
            settings = context.object.collision

            layout.active = settings.use

            col = flow.column()
            col.prop(settings, "permeability", slider=True)
            col.prop(settings, "stickiness")
            col = flow.column()
            col.prop(settings, "use_particle_kill")

            col = flow.column()
            sub = col.column(align=True)
            sub.prop(settings, "damping_factor", text="Damping", slider=True)
            sub.prop(settings, "damping_random", text="Randomize", slider=True)

            col = flow.column()
            sub = col.column(align=True)
            sub.prop(settings, "friction_factor", text="Friction", slider=True)
            sub.prop(settings, "friction_random", text="Randomize", slider=True)


class PHYSICS_PT_collision_softbody(PhysicButtonsPanel, Panel):
    bl_label = "Softbody"
    bl_parent_id = "PHYSICS_PT_collision"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.engine in cls.COMPAT_ENGINES) and (context.collision)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        md = context.collision
        coll = md.settings

        if coll:
            settings = context.object.collision

            layout.active = settings.use

            col = flow.column()
            col.prop(settings, "damping", text="Damping", slider=True)

            col = flow.column()
            col.prop(settings, "thickness_outer", text="Thickness Outer", slider=True)
            col.prop(settings, "thickness_inner", text="Inner", slider=True)


classes = (
    PHYSICS_PT_field,
    PHYSICS_PT_field_falloff,
    PHYSICS_PT_collision,
    PHYSICS_PT_collision_particle,
    PHYSICS_PT_collision_softbody,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
