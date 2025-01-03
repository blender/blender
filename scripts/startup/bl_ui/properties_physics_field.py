# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import (
    Panel,
)
from bpy.app.translations import (
    contexts as i18n_contexts,
)
from bl_ui.properties_physics_common import (
    basic_force_field_settings_ui,
    basic_force_field_falloff_ui,
)


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @staticmethod
    def poll_force_field(context):
        ob = context.object
        return (ob and (ob.field) and (ob.field.type != 'NONE'))

    @staticmethod
    def poll_collision(context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.collision)


class PHYSICS_PT_field(PhysicButtonsPanel, Panel):
    bl_label = "Force Fields"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
    bl_parent_id = "PHYSICS_PT_field"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
            col.prop(field, "guide_free")
            col.prop(field, "falloff_power")
            col.prop(field, "use_guide_path_add")
            col.prop(field, "use_guide_path_weight")

            col.separator()

            col = flow.column()
            col.prop(field, "guide_clump_amount", text="Clumping Amount")
            col.prop(field, "guide_clump_shape")

            col.separator()

            col.prop(field, "guide_minimum", text="Min Distance")

            col = layout.column(align=False, heading="Max Distance")
            col.use_property_decorate = False
            row = col.row(align=True)
            sub = row.row(align=True)
            sub.prop(field, "use_max_distance", text="")
            sub = sub.row(align=True)
            sub.active = field.use_max_distance
            sub.prop(field, "distance_max", text="")
            row.prop_decorator(field, "distance_max")

        elif field.type == 'TEXTURE':
            col = flow.column()
            col.prop(field, "texture_mode")

            col.separator()

            col.prop(field, "strength")

            sub = col.column(heading="Affect")
            sub.prop(field, "apply_to_location", text="Location")

            col = flow.column()
            col.prop(field, "texture_nabla")
            col.prop(field, "use_object_coords")
            col.prop(field, "use_2d_force")

        elif field.type == 'FLUID_FLOW':
            col = flow.column()
            col.prop(field, "strength")
            col.prop(field, "flow")

            sub = col.column(heading="Affect")
            sub.prop(field, "apply_to_location", text="Location")
            sub.prop(field, "apply_to_rotation", text="Rotation")

            col = flow.column()
            col.prop(field, "source_object")
            col.prop(field, "use_smoke_density")
        else:
            del flow
            basic_force_field_settings_ui(self, field)


class PHYSICS_PT_field_settings_kink(PhysicButtonsPanel, Panel):
    bl_label = "Kink"
    bl_parent_id = "PHYSICS_PT_field_settings"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
    bl_parent_id = "PHYSICS_PT_field_settings"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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

        basic_force_field_falloff_ui(self, field)


class PHYSICS_PT_field_falloff_angular(PhysicButtonsPanel, Panel):
    bl_label = "Angular"
    bl_parent_id = "PHYSICS_PT_field_falloff"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
        col.prop(field, "radial_falloff", text="Power", text_ctxt=i18n_contexts.id_particlesettings)

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
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
        col.prop(field, "radial_falloff", text="Power", text_ctxt=i18n_contexts.id_particlesettings)

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
    row.label(text="No collision settings available")


class PHYSICS_PT_collision(PhysicButtonsPanel, Panel):
    bl_label = "Collision"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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
    bl_label = "Softbody & Cloth"
    bl_parent_id = "PHYSICS_PT_collision"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

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

        col = flow.column()
        col.prop(settings, "cloth_friction")

        col = flow.column()
        col.prop(settings, "use_culling")

        col = flow.column()
        col.prop(settings, "use_normal")


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
