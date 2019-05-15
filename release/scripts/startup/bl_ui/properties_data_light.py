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
from rna_prop_ui import PropertyPanel


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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

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
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

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
        col.prop(light, "specular_factor", text="Specular")

        col.separator()

        if light.type in {'POINT', 'SPOT'}:
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
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine

        return (light and light.type != 'SUN') and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        light = context.light

        layout = self.layout
        layout.active = light.use_shadow
        layout.prop(light, "use_custom_distance", text="")

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.use_property_split = True

        col = layout.column()

        col.prop(light, "cutoff_distance", text="Distance")


class DATA_PT_EEVEE_shadow(DataButtonsPanel, Panel):
    bl_label = "Shadow"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine
        return (
            (light and light.type in {'POINT', 'SUN', 'SPOT', 'AREA'}) and
            (engine in cls.COMPAT_ENGINES)
        )

    def draw_header(self, context):
        light = context.light
        self.layout.prop(light, "use_shadow", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        light = context.light

        layout.active = light.use_shadow

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(light, "shadow_buffer_clip_start", text="Clip Start")
        if light.type == 'SUN':
            sub.prop(light, "shadow_buffer_clip_end", text="End")

        col.prop(light, "shadow_buffer_soft", text="Softness")

        col.separator()

        col.prop(light, "shadow_buffer_bias", text="Bias")
        col.prop(light, "shadow_buffer_exp", text="Exponent")
        col.prop(light, "shadow_buffer_bleed_bias", text="Bleed Bias")


class DATA_PT_EEVEE_shadow_cascaded_shadow_map(DataButtonsPanel, Panel):
    bl_label = "Cascaded Shadow Map"
    bl_parent_id = "DATA_PT_EEVEE_shadow"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine

        return (light and light.type == 'SUN') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.use_property_split = True

        col = layout.column()

        col.prop(light, "shadow_cascade_count", text="Count")
        col.prop(light, "shadow_cascade_fade", text="Fade")

        col.prop(light, "shadow_cascade_max_distance", text="Max Distance")
        col.prop(light, "shadow_cascade_exponent", text="Distribution")


class DATA_PT_EEVEE_shadow_contact(DataButtonsPanel, Panel):
    bl_label = "Contact Shadows"
    bl_parent_id = "DATA_PT_EEVEE_shadow"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine
        return (
            (light and light.type in {'POINT', 'SUN', 'SPOT', 'AREA'}) and
            (engine in cls.COMPAT_ENGINES)
        )

    def draw_header(self, context):
        light = context.light

        layout = self.layout
        layout.active = light.use_shadow
        layout.prop(light, "use_contact_shadow", text="")

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.use_property_split = True

        col = layout.column()
        col.active = light.use_shadow and light.use_contact_shadow

        col.prop(light, "contact_shadow_distance", text="Distance")
        col.prop(light, "contact_shadow_soft_size", text="Softness")
        col.prop(light, "contact_shadow_bias", text="Bias")
        col.prop(light, "contact_shadow_thickness", text="Thickness")


class DATA_PT_area(DataButtonsPanel, Panel):
    bl_label = "Area Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine
        return (light and light.type == 'AREA') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        light = context.light

        col = layout.column()
        col.row().prop(light, "shape", expand=True)
        sub = col.row(align=True)

        if light.shape in {'SQUARE', 'DISK'}:
            sub.prop(light, "size")
        elif light.shape in {'RECTANGLE', 'ELLIPSE'}:
            sub.prop(light, "size", text="Size X")
            sub.prop(light, "size_y", text="Size Y")


class DATA_PT_spot(DataButtonsPanel, Panel):
    bl_label = "Spot Shape"
    bl_parent_id = "DATA_PT_EEVEE_light"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

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


class DATA_PT_falloff_curve(DataButtonsPanel, Panel):
    bl_label = "Falloff Curve"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine

        return (
            (light and light.type in {'POINT', 'SPOT'} and light.falloff_type == 'CUSTOM_CURVE') and
            (engine in cls.COMPAT_ENGINES)
        )

    def draw(self, context):
        light = context.light

        self.layout.template_curve_mapping(light, "falloff_curve", use_negative_slope=True)


class DATA_PT_custom_props_light(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "object.data"
    _property_type = bpy.types.Light


classes = (
    DATA_PT_context_light,
    DATA_PT_preview,
    DATA_PT_light,
    DATA_PT_EEVEE_light,
    DATA_PT_EEVEE_light_distance,
    DATA_PT_EEVEE_shadow,
    DATA_PT_EEVEE_shadow_contact,
    DATA_PT_EEVEE_shadow_cascaded_shadow_map,
    DATA_PT_area,
    DATA_PT_spot,
    DATA_PT_falloff_curve,
    DATA_PT_custom_props_light,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
