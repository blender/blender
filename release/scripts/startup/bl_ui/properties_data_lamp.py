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
from rna_prop_ui import PropertyPanel


class LAMP_MT_sunsky_presets(Menu):
    bl_label = "Sun & Sky Presets"
    preset_subdir = "sunsky"
    preset_operator = "script.execute_preset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}
    draw = Menu.draw_preset


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.lamp and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_lamp(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        lamp = context.lamp
        space = context.space_data

        split = layout.split(percentage=0.65)

        if ob:
            split.template_ID(ob, "data")
        elif lamp:
            split.template_ID(space, "pin_id")


class DATA_PT_preview(DataButtonsPanel, Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        self.layout.template_preview(context.lamp)


class DATA_PT_lamp(DataButtonsPanel, Panel):
    bl_label = "Lamp"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY'}

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        layout.row().prop(lamp, "type", expand=True)

        layout.use_property_split = True

        col = col.column()
        col.prop(lamp, "color")
        col.prop(lamp, "energy")

        if lamp.type in {'POINT', 'SPOT'}:

            col = col.column()
            col.label(text="Falloff")
            col.prop(lamp, "falloff_type")
            col.prop(lamp, "distance")
            col.prop(lamp, "shadow_soft_size")

            if lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED':
                sub = col.column(align=True)
                sub.prop(lamp, "linear_attenuation", slider=True, text="Linear")
                sub.prop(lamp, "quadratic_attenuation", slider=True, text="Quadratic")

            elif lamp.falloff_type == 'INVERSE_COEFFICIENTS':
                col.label(text="Inverse Coefficients")
                sub = col.column(align=True)
                sub.prop(lamp, "constant_coefficient", text="Constant")
                sub.prop(lamp, "linear_coefficient", text="Linear")
                sub.prop(lamp, "quadratic_coefficient", text="Quadratic")

        if lamp.type == 'AREA':
            col.prop(lamp, "distance")

        col = split.column()
        col.label()


class DATA_PT_EEVEE_lamp(DataButtonsPanel, Panel):
    bl_label = "Lamp"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        lamp = context.lamp

        layout.row().prop(lamp, "type", expand=True)

        layout.use_property_split = True

        col = layout.column()
        col.prop(lamp, "color")
        col.prop(lamp, "energy")
        col.prop(lamp, "specular_factor", text="Specular")

        col.separator()

        if lamp.type in {'POINT', 'SPOT', 'SUN'}:
            col.prop(lamp, "shadow_soft_size", text="Radius")
        elif lamp.type == 'AREA':
            col.prop(lamp, "shape")

            sub = col.column(align=True)

            if lamp.shape in {'SQUARE', 'DISK'}:
                sub.prop(lamp, "size")
            elif lamp.shape in {'RECTANGLE', 'ELLIPSE'}:
                sub.prop(lamp, "size", text="Size X")
                sub.prop(lamp, "size_y", text="Y")


class DATA_PT_EEVEE_shadow(DataButtonsPanel, Panel):
    bl_label = "Shadow"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine
        return (lamp and lamp.type in {'POINT', 'SUN', 'SPOT', 'AREA'}) and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        lamp = context.lamp
        self.layout.prop(lamp, "use_shadow", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        lamp = context.lamp

        layout.active = lamp.use_shadow

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(lamp, "shadow_buffer_clip_start", text="Clip Start")
        sub.prop(lamp, "shadow_buffer_clip_end", text="End")

        col.prop(lamp, "shadow_buffer_soft", text="Softness")

        col.separator()

        col.prop(lamp, "shadow_buffer_bias", text="Bias")
        col.prop(lamp, "shadow_buffer_exp", text="Exponent")
        col.prop(lamp, "shadow_buffer_bleed_bias", text="Bleed Bias")



class DATA_PT_EEVEE_shadow_cascaded_shadow_map(DataButtonsPanel, Panel):
    bl_label = "Cascaded Shadow Map"
    bl_parent_id = "DATA_PT_EEVEE_shadow"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine

        return (lamp and lamp.type == 'SUN') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        lamp = context.lamp
        layout.use_property_split = True

        col = layout.column()

        col.prop(lamp, "shadow_cascade_count", text="Count")
        col.prop(lamp, "shadow_cascade_fade", text="Fade")

        col.prop(lamp, "shadow_cascade_max_distance", text="Max Distance")
        col.prop(lamp, "shadow_cascade_exponent", text="Distribution")


class DATA_PT_EEVEE_shadow_contact(DataButtonsPanel, Panel):
    bl_label = "Contact Shadows"
    bl_parent_id = "DATA_PT_EEVEE_shadow"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine
        return (lamp and lamp.type in {'POINT', 'SUN', 'SPOT', 'AREA'}) and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        lamp = context.lamp

        layout = self.layout
        layout.active = lamp.use_shadow
        layout.prop(lamp, "use_contact_shadow", text="")

    def draw(self, context):
        layout = self.layout
        lamp = context.lamp
        layout.use_property_split = True

        col = layout.column()
        col.active = lamp.use_shadow and lamp.use_contact_shadow

        col.prop(lamp, "contact_shadow_distance", text="Distance")
        col.prop(lamp, "contact_shadow_soft_size", text="Softness")
        col.prop(lamp, "contact_shadow_bias", text="Bias")
        col.prop(lamp, "contact_shadow_thickness", text="Thickness")


class DATA_PT_area(DataButtonsPanel, Panel):
    bl_label = "Area Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine
        return (lamp and lamp.type == 'AREA') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        col = layout.column()
        col.row().prop(lamp, "shape", expand=True)
        sub = col.row(align=True)

        if lamp.shape in {'SQUARE', 'DISK'}:
            sub.prop(lamp, "size")
        elif lamp.shape in {'RECTANGLE', 'ELLIPSE'}:
            sub.prop(lamp, "size", text="Size X")
            sub.prop(lamp, "size_y", text="Size Y")


class DATA_PT_spot(DataButtonsPanel, Panel):
    bl_label = "Spot Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine
        return (lamp and lamp.type == 'SPOT') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.prop(lamp, "spot_size", text="Size")
        sub.prop(lamp, "spot_blend", text="Blend", slider=True)
        col.prop(lamp, "use_square")
        col.prop(lamp, "show_cone")

        col = split.column()

        col.active = (lamp.shadow_method != 'BUFFER_SHADOW' or lamp.shadow_buffer_type != 'DEEP')
        col.prop(lamp, "use_halo")
        sub = col.column(align=True)
        sub.active = lamp.use_halo
        sub.prop(lamp, "halo_intensity", text="Intensity")
        if lamp.shadow_method == 'BUFFER_SHADOW':
            sub.prop(lamp, "halo_step", text="Step")


class DATA_PT_spot(DataButtonsPanel, Panel):
    bl_label = "Spot Shape"
    bl_parent_id = "DATA_PT_EEVEE_lamp"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine
        return (lamp and lamp.type == 'SPOT') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        lamp = context.lamp

        col = layout.column()

        col.prop(lamp, "spot_size", text="Size")
        col.prop(lamp, "spot_blend", text="Blend", slider=True)

        col.prop(lamp, "show_cone")


class DATA_PT_falloff_curve(DataButtonsPanel, Panel):
    bl_label = "Falloff Curve"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.engine

        return (lamp and lamp.type in {'POINT', 'SPOT'} and lamp.falloff_type == 'CUSTOM_CURVE') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        lamp = context.lamp

        self.layout.template_curve_mapping(lamp, "falloff_curve", use_negative_slope=True)


class DATA_PT_custom_props_lamp(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}
    _context_path = "object.data"
    _property_type = bpy.types.Lamp


classes = (
    LAMP_MT_sunsky_presets,
    DATA_PT_context_lamp,
    DATA_PT_preview,
    DATA_PT_lamp,
    DATA_PT_EEVEE_lamp,
    DATA_PT_EEVEE_shadow,
    DATA_PT_EEVEE_shadow_contact,
    DATA_PT_EEVEE_shadow_cascaded_shadow_map,
    DATA_PT_area,
    DATA_PT_spot,
    DATA_PT_falloff_curve,
    DATA_PT_custom_props_lamp,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
