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
from .properties_physics_common import (
    basic_force_field_settings_ui,
    basic_force_field_falloff_ui,
)


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll_force_field(context):
        ob = context.object
        return (ob and (ob.field) and (ob.field.type != 'NONE'))

    def poll_collision(context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.collision)


class PHYSICS_PT_field(PhysicButtonsPanel, Panel):
    bl_label = "Force Fields"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        field = ob.field

        layout.prop(field, "type")


class PHYSICS_PT_field_settings(PhysicButtonsPanel, Panel):
    bl_label = "Settings"
    bl_parent_id = 'PHYSICS_PT_field'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        field = ob.field

        if field.type not in {'NONE', 'GUIDE', 'TEXTURE'}:
            layout.prop(field, "shape", text="Shape")

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        if field.type == 'NONE':
            return  # nothing to draw.

        elif field.type == 'GUIDE':
            col = flow.column()
            col.prop(field, "guide_minimum")
            col.prop(field, "guide_free")
            col.prop(field, "falloff_power")
            col.prop(field, "use_guide_path_add")
            col.prop(field, "use_guide_path_weight")

            col.separator()

            col = flow.column()
            col.prop(field, "guide_clump_amount", text="Clumping amount")
            col.prop(field, "guide_clump_shape")
            col.prop(field, "use_max_distance")

            sub = col.column()
            sub.active = field.use_max_distance
            sub.prop(field, "distance_max")

        elif field.type == 'TEXTURE':
            col = flow.column()
            col.prop(field, "texture_mode")

            col.separator()

            col.prop(field, "strength")

            col = flow.column()
            col.prop(field, "texture_nabla")
            col.prop(field, "use_object_coords")
            col.prop(field, "use_2d_force")

        elif field.type == 'SMOKE_FLOW':
            col = flow.column()
            col.prop(field, "strength")
            col.prop(field, "flow")

            col = flow.column()
            col.prop(field, "source_object")
            col.prop(field, "use_smoke_density")
        else:
            del flow
            basic_force_field_settings_ui(self, context, field)


class PHYSICS_PT_field_settings_kink(PhysicButtonsPanel, Panel):
    bl_label = "Kink"
    bl_parent_id = 'PHYSICS_PT_field_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        ob = context.object
        return ((ob.field.type == 'GUIDE') and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        field = ob.field

        layout.prop(field, "guide_kink_type", text="Type")

        if field.guide_kink_type != 'NONE':
            flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

            col = flow.column()
            col.prop(field, "guide_kink_axis")
            col.prop(field, "guide_kink_frequency")

            col = flow.column()
            col.prop(field, "guide_kink_shape")
            col.prop(field, "guide_kink_amplitude")


class PHYSICS_PT_field_settings_texture_select(PhysicButtonsPanel, Panel):
    bl_label = "Texture"
    bl_parent_id = 'PHYSICS_PT_field_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        ob = context.object
        return ((ob.field.type == 'TEXTURE') and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        field = ob.field

        layout.row().template_ID(field, "texture", new="texture.new")


class PHYSICS_PT_field_falloff(PhysicButtonsPanel, Panel):
    bl_label = "Falloff"
    bl_parent_id = "PHYSICS_PT_field"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        ob = context.object
        return ((ob.field.type not in {'NONE', 'GUIDE'}) and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        field = ob.field

        layout.prop(field, "falloff_type", text="Shape")

        basic_force_field_falloff_ui(self, context, field)


class PHYSICS_PT_field_falloff_angular(PhysicButtonsPanel, Panel):
    bl_label = "Angular"
    bl_parent_id = "PHYSICS_PT_field_falloff"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        ob = context.object
        return ((ob.field.falloff_type == 'CONE') and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        field = ob.field

        col = flow.column()
        col.prop(field, "radial_falloff", text="Power")

        col = flow.column()
        col.prop(field, "use_radial_min", text="Use Min Angle")

        sub = col.column()
        sub.active = field.use_radial_min
        sub.prop(field, "radial_min", text="Min Angle")

        col = flow.column()
        col.prop(field, "use_radial_max", text="Use Max Angle")

        sub = col.column()
        sub.active = field.use_radial_max
        sub.prop(field, "radial_max", text="Max Angle")


class PHYSICS_PT_field_falloff_radial(PhysicButtonsPanel, Panel):
    bl_label = "Radial"
    bl_parent_id = "PHYSICS_PT_field_falloff"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_force_field(context):
            return False

        ob = context.object
        return ((ob.field.falloff_type == 'TUBE') and (context.engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        field = ob.field

        col = flow.column()
        col.prop(field, "radial_falloff", text="Power")

        col = flow.column()
        col.prop(field, "use_radial_min", text="Use Minimum")

        sub = col.column()
        sub.active = field.use_radial_min
        sub.prop(field, "radial_min", text="Min Distance")

        col = flow.column()
        col.prop(field, "use_radial_max", text="Use Maximum")

        sub = col.column()
        sub.active = field.use_radial_max
        sub.prop(field, "radial_max", text="Max Distance")


def collision_warning(layout):
    row = layout.row(align=True)
    row.alignment = 'RIGHT'
    row.label("No collision settings available")


class PHYSICS_PT_collision(PhysicButtonsPanel, Panel):
    bl_label = "Collision"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_collision(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.collision
        coll = md.settings

        if not coll:
            collision_warning(layout)
            return

        settings = context.object.collision

        layout.active = settings.use

        col = layout.column()
        col.prop(settings, "absorption", text="Field Absorption")


class PHYSICS_PT_collision_particle(PhysicButtonsPanel, Panel):
    bl_label = "Particle"
    bl_parent_id = "PHYSICS_PT_collision"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_collision(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        md = context.collision

        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        coll = md.settings

        if not coll:
            collision_warning(layout)
            return

        settings = context.object.collision

        layout.active = settings.use

        col = flow.column()
        col.prop(settings, "permeability", slider=True)
        col.prop(settings, "stickiness")
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
        if not PhysicButtonsPanel.poll_collision(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        md = context.collision
        coll = md.settings

        if not coll:
            collision_warning(layout)
            return

        settings = context.object.collision

        layout.active = settings.use

        col = flow.column()
        col.prop(settings, "damping", text="Damping", slider=True)

        col = flow.column()
        col.prop(settings, "thickness_outer", text="Thickness Outer", slider=True)

        col = flow.column()
        col.prop(settings, "thickness_inner", text="Inner", slider=True)


classes = (
    PHYSICS_PT_field,
    PHYSICS_PT_field_settings,
    PHYSICS_PT_field_settings_kink,
    PHYSICS_PT_field_settings_texture_select,
    PHYSICS_PT_field_falloff,
    PHYSICS_PT_field_falloff_angular,
    PHYSICS_PT_field_falloff_radial,
    PHYSICS_PT_collision,
    PHYSICS_PT_collision_particle,
    PHYSICS_PT_collision_softbody,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
