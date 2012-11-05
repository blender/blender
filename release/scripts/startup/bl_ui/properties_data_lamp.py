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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    draw = Menu.draw_preset


class DataButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return context.lamp and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_lamp(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        lamp = context.lamp
        space = context.space_data

        split = layout.split(percentage=0.65)

        texture_count = len(lamp.texture_slots.keys())

        if ob:
            split.template_ID(ob, "data")
        elif lamp:
            split.template_ID(space, "pin_id")

        if texture_count != 0:
            split.label(text=str(texture_count), icon='TEXTURE')


class DATA_PT_preview(DataButtonsPanel, Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        self.layout.template_preview(context.lamp)


class DATA_PT_lamp(DataButtonsPanel, Panel):
    bl_label = "Lamp"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        layout.prop(lamp, "type", expand=True)

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.prop(lamp, "color", text="")
        sub.prop(lamp, "energy")

        if lamp.type in {'POINT', 'SPOT'}:
            sub.label(text="Falloff:")
            sub.prop(lamp, "falloff_type", text="")
            sub.prop(lamp, "distance")

            if lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED':
                col.label(text="Attenuation Factors:")
                sub = col.column(align=True)
                sub.prop(lamp, "linear_attenuation", slider=True, text="Linear")
                sub.prop(lamp, "quadratic_attenuation", slider=True, text="Quadratic")

            col.prop(lamp, "use_sphere")

        if lamp.type == 'AREA':
            col.prop(lamp, "distance")
            col.prop(lamp, "gamma")

        col = split.column()
        col.prop(lamp, "use_negative")
        col.prop(lamp, "use_own_layer", text="This Layer Only")
        col.prop(lamp, "use_specular")
        col.prop(lamp, "use_diffuse")


class DATA_PT_sunsky(DataButtonsPanel, Panel):
    bl_label = "Sky & Atmosphere"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'SUN') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp.sky

        row = layout.row(align=True)
        row.prop(lamp, "use_sky")
        row.menu("LAMP_MT_sunsky_presets", text=bpy.types.LAMP_MT_sunsky_presets.bl_label)
        row.operator("lamp.sunsky_preset_add", text="", icon='ZOOMIN')
        row.operator("lamp.sunsky_preset_add", text="", icon='ZOOMOUT').remove_active = True

        row = layout.row()
        row.active = lamp.use_sky or lamp.use_atmosphere
        row.prop(lamp, "atmosphere_turbidity", text="Turbidity")

        split = layout.split()

        col = split.column()
        col.active = lamp.use_sky
        col.label(text="Blending:")
        sub = col.column()
        sub.prop(lamp, "sky_blend_type", text="")
        sub.prop(lamp, "sky_blend", text="Factor")

        col.label(text="Color Space:")
        sub = col.column()
        sub.row().prop(lamp, "sky_color_space", expand=True)
        sub.prop(lamp, "sky_exposure", text="Exposure")

        col = split.column()
        col.active = lamp.use_sky
        col.label(text="Horizon:")
        sub = col.column()
        sub.prop(lamp, "horizon_brightness", text="Brightness")
        sub.prop(lamp, "spread", text="Spread")

        col.label(text="Sun:")
        sub = col.column()
        sub.prop(lamp, "sun_brightness", text="Brightness")
        sub.prop(lamp, "sun_size", text="Size")
        sub.prop(lamp, "backscattered_light", slider=True, text="Back Light")

        layout.separator()

        layout.prop(lamp, "use_atmosphere")

        split = layout.split()

        col = split.column()
        col.active = lamp.use_atmosphere
        col.label(text="Intensity:")
        col.prop(lamp, "sun_intensity", text="Sun")
        col.prop(lamp, "atmosphere_distance_factor", text="Distance")

        col = split.column()
        col.active = lamp.use_atmosphere
        col.label(text="Scattering:")
        sub = col.column(align=True)
        sub.prop(lamp, "atmosphere_inscattering", slider=True, text="Inscattering")
        sub.prop(lamp, "atmosphere_extinction", slider=True, text="Extinction")


class DATA_PT_shadow(DataButtonsPanel, Panel):
    bl_label = "Shadow"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type in {'POINT', 'SUN', 'SPOT', 'AREA'}) and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        layout.prop(lamp, "shadow_method", expand=True)

        if lamp.shadow_method == 'NOSHADOW' and lamp.type == 'AREA':
            split = layout.split()

            col = split.column()
            col.label(text="Form factor sampling:")

            sub = col.row(align=True)

            if lamp.shape == 'SQUARE':
                sub.prop(lamp, "shadow_ray_samples_x", text="Samples")
            elif lamp.shape == 'RECTANGLE':
                sub.prop(lamp, "shadow_ray_samples_x", text="Samples X")
                sub.prop(lamp, "shadow_ray_samples_y", text="Samples Y")

        if lamp.shadow_method != 'NOSHADOW':
            split = layout.split()

            col = split.column()
            col.prop(lamp, "shadow_color", text="")

            col = split.column()
            col.prop(lamp, "use_shadow_layer", text="This Layer Only")
            col.prop(lamp, "use_only_shadow")

        if lamp.shadow_method == 'RAY_SHADOW':
            split = layout.split()

            col = split.column()
            col.label(text="Sampling:")

            if lamp.type in {'POINT', 'SUN', 'SPOT'}:
                sub = col.row()

                sub.prop(lamp, "shadow_ray_samples", text="Samples")
                sub.prop(lamp, "shadow_soft_size", text="Soft Size")

            elif lamp.type == 'AREA':
                sub = col.row(align=True)

                if lamp.shape == 'SQUARE':
                    sub.prop(lamp, "shadow_ray_samples_x", text="Samples")
                elif lamp.shape == 'RECTANGLE':
                    sub.prop(lamp, "shadow_ray_samples_x", text="Samples X")
                    sub.prop(lamp, "shadow_ray_samples_y", text="Samples Y")

            col.row().prop(lamp, "shadow_ray_sample_method", expand=True)

            if lamp.shadow_ray_sample_method == 'ADAPTIVE_QMC':
                layout.prop(lamp, "shadow_adaptive_threshold", text="Threshold")

            if lamp.type == 'AREA' and lamp.shadow_ray_sample_method == 'CONSTANT_JITTERED':
                row = layout.row()
                row.prop(lamp, "use_umbra")
                row.prop(lamp, "use_dither")
                row.prop(lamp, "use_jitter")

        elif lamp.shadow_method == 'BUFFER_SHADOW':
            col = layout.column()
            col.label(text="Buffer Type:")
            col.row().prop(lamp, "shadow_buffer_type", expand=True)

            if lamp.shadow_buffer_type in {'REGULAR', 'HALFWAY', 'DEEP'}:
                split = layout.split()

                col = split.column()
                col.label(text="Filter Type:")
                col.prop(lamp, "shadow_filter_type", text="")
                sub = col.column(align=True)
                sub.prop(lamp, "shadow_buffer_soft", text="Soft")
                sub.prop(lamp, "shadow_buffer_bias", text="Bias")

                col = split.column()
                col.label(text="Sample Buffers:")
                col.prop(lamp, "shadow_sample_buffers", text="")
                sub = col.column(align=True)
                sub.prop(lamp, "shadow_buffer_size", text="Size")
                sub.prop(lamp, "shadow_buffer_samples", text="Samples")
                if lamp.shadow_buffer_type == 'DEEP':
                    col.prop(lamp, "compression_threshold")

            elif lamp.shadow_buffer_type == 'IRREGULAR':
                layout.prop(lamp, "shadow_buffer_bias", text="Bias")

            split = layout.split()

            col = split.column()
            col.prop(lamp, "use_auto_clip_start", text="Autoclip Start")
            sub = col.column()
            sub.active = not lamp.use_auto_clip_start
            sub.prop(lamp, "shadow_buffer_clip_start", text="Clip Start")

            col = split.column()
            col.prop(lamp, "use_auto_clip_end", text="Autoclip End")
            sub = col.column()
            sub.active = not lamp.use_auto_clip_end
            sub.prop(lamp, "shadow_buffer_clip_end", text=" Clip End")


class DATA_PT_area(DataButtonsPanel, Panel):
    bl_label = "Area Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'AREA') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        col = layout.column()
        col.row().prop(lamp, "shape", expand=True)
        sub = col.row(align=True)

        if lamp.shape == 'SQUARE':
            sub.prop(lamp, "size")
        elif lamp.shape == 'RECTANGLE':
            sub.prop(lamp, "size", text="Size X")
            sub.prop(lamp, "size_y", text="Size Y")


class DATA_PT_spot(DataButtonsPanel, Panel):
    bl_label = "Spot Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
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

        col.prop(lamp, "use_halo")
        sub = col.column(align=True)
        sub.active = lamp.use_halo
        sub.prop(lamp, "halo_intensity", text="Intensity")
        if lamp.shadow_method == 'BUFFER_SHADOW':
            sub.prop(lamp, "halo_step", text="Step")


class DATA_PT_falloff_curve(DataButtonsPanel, Panel):
    bl_label = "Falloff Curve"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine

        return (lamp and lamp.type in {'POINT', 'SPOT'} and lamp.falloff_type == 'CUSTOM_CURVE') and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        lamp = context.lamp

        self.layout.template_curve_mapping(lamp, "falloff_curve")


class DATA_PT_custom_props_lamp(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Lamp

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
