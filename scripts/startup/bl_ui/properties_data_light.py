# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.app.translations import contexts as i18n_contexts
from bpy.types import Panel
from rna_prop_ui import PropertyPanel
from .space_properties import PropertiesAnimationMixin


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.light and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_light(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout

        ob = context.object
        light = context.light
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif light:
            layout.template_ID(space, "pin_id")


class DATA_PT_preview(DataButtonsPanel, Panel):
    bl_label = "Preview"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
    }

    def draw(self, context):
        self.layout.template_preview(context.light)


class DATA_PT_light(DataButtonsPanel, Panel):
    bl_label = "Light"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        light = context.light

        # Compact layout for node editor.
        if self.bl_space_type == 'PROPERTIES':
            layout.row().prop(light, "type", expand=True)
            layout.use_property_split = True
        else:
            layout.use_property_split = True
            layout.row().prop(light, "type")


class DATA_PT_EEVEE_light(DataButtonsPanel, Panel):
    bl_label = "Light"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        light = context.light

        # Compact layout for node editor.
        if self.bl_space_type == 'PROPERTIES':
            layout.row().prop(light, "type", expand=True)
            layout.use_property_split = True
        else:
            layout.use_property_split = True
            layout.row().prop(light, "type")

        col = layout.column()
        col.prop(light, "color")
        col.prop(light, "energy")

        col.separator()

        if light.type in {'POINT', 'SPOT'}:
            col.prop(light, "use_soft_falloff")
            col.prop(light, "shadow_soft_size", text="Radius")
        elif light.type == 'SUN':
            col.prop(light, "angle")
        elif light.type == 'AREA':
            col.prop(light, "shape")

            sub = col.column(align=True)

            if light.shape in {'SQUARE', 'DISK'}:
                sub.prop(light, "size")
            elif light.shape in {'RECTANGLE', 'ELLIPSE'}:
                sub.prop(light, "size", text="Size X")
                sub.prop(light, "size_y", text="Y")


class DATA_PT_EEVEE_light_distance(DataButtonsPanel, Panel):
    bl_label = "Custom Distance"
    bl_parent_id = "DATA_PT_EEVEE_light"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine

        return (light and light.type != 'SUN') and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        light = context.light

        layout = self.layout
        layout.prop(light, "use_custom_distance", text="")

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.active = light.use_custom_distance
        layout.use_property_split = True

        layout.prop(light, "cutoff_distance", text="Distance")


class DATA_PT_EEVEE_light_shadow(DataButtonsPanel, Panel):
    bl_label = "Shadow"
    bl_parent_id = "DATA_PT_EEVEE_light"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw_header(self, context):
        light = context.light
        self.layout.prop(light, "use_shadow", text="")

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.use_property_split = True
        layout.active = context.scene.eevee.use_shadows and light.use_shadow

        col = layout.column(align=False, heading="Jitter")
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(light, "use_shadow_jitter", text="")
        sub = sub.row(align=True)
        sub.active = light.use_shadow_jitter
        sub.prop(light, "shadow_jitter_overblur", text="Overblur")

        col = layout.column()
        col.prop(light, "shadow_filter_radius", text="Filter")

        sub = col.column(align=True)
        row = sub.row(align=True)
        row.prop(light, "shadow_maximum_resolution", text="Resolution Limit")
        if light.type != 'SUN':
            row.prop(light, "use_absolute_resolution", text="", icon='DRIVER_DISTANCE')


class DATA_PT_EEVEE_light_influence(DataButtonsPanel, Panel):
    bl_label = "Influence"
    bl_parent_id = "DATA_PT_EEVEE_light"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        ob = context.object
        light = context.light
        layout.use_property_split = True

        col = layout.column(align=True)

        sub = col.column(align=True)
        sub.active = ob is None or ob.visible_diffuse
        sub.prop(light, "diffuse_factor", text="Diffuse")

        sub = col.column(align=True)
        sub.active = ob is None or ob.visible_glossy
        sub.prop(light, "specular_factor", text="Glossy")

        sub = col.column(align=True)
        sub.active = ob is None or ob.visible_transmission
        sub.prop(light, "transmission_factor", text="Transmission")

        sub = col.column(align=True)
        sub.active = ob is None or ob.visible_volume_scatter
        sub.prop(light, "volume_factor", text="Volume Scatter", text_ctxt=i18n_contexts.id_id)


class DATA_PT_spot(DataButtonsPanel, Panel):
    bl_label = "Beam Shape"
    bl_parent_id = "DATA_PT_EEVEE_light"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine
        return (light and light.type == 'SPOT') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        light = context.light

        col = layout.column()

        col.prop(light, "spot_size", text="Size")
        col.prop(light, "spot_blend", text="Blend", slider=True)

        col.prop(light, "show_cone")


class DATA_PT_light_animation(DataButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        # DataButtonsPanel.poll ensures this is not None.
        light = context.light

        col = layout.column(align=True)
        col.label(text="Light")
        self.draw_action_and_slot_selector(context, col, light)

        if node_tree := light.node_tree:
            col = layout.column(align=True)
            col.label(text="Shader Node Tree")
            self.draw_action_and_slot_selector(context, col, node_tree)


class DATA_PT_custom_props_light(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }
    _context_path = "object.data"
    _property_type = bpy.types.Light


classes = (
    DATA_PT_context_light,
    DATA_PT_preview,
    DATA_PT_light,
    DATA_PT_EEVEE_light,
    DATA_PT_spot,
    DATA_PT_EEVEE_light_shadow,
    DATA_PT_EEVEE_light_influence,
    DATA_PT_EEVEE_light_distance,
    DATA_PT_light_animation,
    DATA_PT_custom_props_light,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
