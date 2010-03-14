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
from rna_prop_ui import PropertyPanel

narrowui = 180


class LAMP_MT_sunsky_presets(bpy.types.Menu):
    bl_label = "Render Presets"
    preset_subdir = "sunsky"
    preset_operator = "script.python_file_run"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    draw = bpy.types.Menu.draw_preset


class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        engine = context.scene.render.engine
        return context.lamp and (engine in self.COMPAT_ENGINES)


class DATA_PT_preview(DataButtonsPanel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        self.layout.template_preview(context.lamp)


class DATA_PT_context_lamp(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        lamp = context.lamp
        space = context.space_data
        wide_ui = context.region.width > narrowui

        if wide_ui:
            split = layout.split(percentage=0.65)
            if ob:
                split.template_ID(ob, "data")
                split.separator()
            elif lamp:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            if ob:
                layout.template_ID(ob, "data")
            elif lamp:
                layout.template_ID(space, "pin_id")


class DATA_PT_custom_props_lamp(DataButtonsPanel, PropertyPanel):
    _context_path = "object.data"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}


class DATA_PT_lamp(DataButtonsPanel):
    bl_label = "Lamp"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(lamp, "type", expand=True)
        else:
            layout.prop(lamp, "type", text="")

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.prop(lamp, "color", text="")
        sub.prop(lamp, "energy")

        if lamp.type in ('POINT', 'SPOT'):
            sub.label(text="Falloff:")
            sub.prop(lamp, "falloff_type", text="")
            sub.prop(lamp, "distance")

            if lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED':
                col.label(text="Attenuation Factors:")
                sub = col.column(align=True)
                sub.prop(lamp, "linear_attenuation", slider=True, text="Linear")
                sub.prop(lamp, "quadratic_attenuation", slider=True, text="Quadratic")

            col.prop(lamp, "sphere")

        if lamp.type == 'AREA':
            col.prop(lamp, "distance")
            col.prop(lamp, "gamma")

        if wide_ui:
            col = split.column()
        col.prop(lamp, "negative")
        col.prop(lamp, "layer", text="This Layer Only")
        col.prop(lamp, "specular")
        col.prop(lamp, "diffuse")


class DATA_PT_sunsky(DataButtonsPanel):
    bl_label = "Sky & Atmosphere"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'SUN') and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp.sky
        wide_ui = context.region.width > narrowui

        row = layout.row(align=True)
        row.prop(lamp, "use_sky")
        row.menu("LAMP_MT_sunsky_presets", text="Presets")
        row.operator("lamp.sunsky_preset_add", text="Add")

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

        if wide_ui:
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

        if wide_ui:
            col = split.column()
        col.active = lamp.use_atmosphere
        col.label(text="Scattering:")
        sub = col.column(align=True)
        sub.prop(lamp, "atmosphere_inscattering", slider=True, text="Inscattering")
        sub.prop(lamp, "atmosphere_extinction", slider=True, text="Extinction")


class DATA_PT_shadow(DataButtonsPanel):
    bl_label = "Shadow"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type in ('POINT', 'SUN', 'SPOT', 'AREA')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(lamp, "shadow_method", expand=True)
        else:
            layout.prop(lamp, "shadow_method", text="")

        if lamp.shadow_method != 'NOSHADOW':
            split = layout.split()

            col = split.column()
            col.prop(lamp, "shadow_color", text="")

            if wide_ui:
                col = split.column()
            col.prop(lamp, "shadow_layer", text="This Layer Only")
            col.prop(lamp, "only_shadow")

        if lamp.shadow_method == 'RAY_SHADOW':
            col = layout.column()
            col.label(text="Sampling:")
            if wide_ui:
                col.row().prop(lamp, "shadow_ray_sampling_method", expand=True)
            else:
                col.prop(lamp, "shadow_ray_sampling_method", text="")

            if lamp.type in ('POINT', 'SUN', 'SPOT'):
                split = layout.split()

                col = split.column()
                col.prop(lamp, "shadow_soft_size", text="Soft Size")

                col.prop(lamp, "shadow_ray_samples", text="Samples")
                if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
                    col.prop(lamp, "shadow_adaptive_threshold", text="Threshold")
                if wide_ui:
                    col = split.column()

            elif lamp.type == 'AREA':
                split = layout.split()

                col = split.column()

                if lamp.shape == 'SQUARE':
                    col.prop(lamp, "shadow_ray_samples_x", text="Samples")
                elif lamp.shape == 'RECTANGLE':
                    col.prop(lamp, "shadow_ray_samples_x", text="Samples X")
                    col.prop(lamp, "shadow_ray_samples_y", text="Samples Y")

                if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
                    col.prop(lamp, "shadow_adaptive_threshold", text="Threshold")
                    if wide_ui:
                        col = split.column()

                elif lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
                    if wide_ui:
                        col = split.column()
                    col.prop(lamp, "umbra")
                    col.prop(lamp, "dither")
                    col.prop(lamp, "jitter")
                else:
                    if wide_ui:
                        col = split.column()


        elif lamp.shadow_method == 'BUFFER_SHADOW':
            col = layout.column()
            col.label(text="Buffer Type:")
            if wide_ui:
                col.row().prop(lamp, "shadow_buffer_type", expand=True)
            else:
                col.row().prop(lamp, "shadow_buffer_type", text="")

            if lamp.shadow_buffer_type in ('REGULAR', 'HALFWAY', 'DEEP'):
                split = layout.split()

                col = split.column()
                col.label(text="Filter Type:")
                col.prop(lamp, "shadow_filter_type", text="")
                sub = col.column(align=True)
                sub.prop(lamp, "shadow_buffer_soft", text="Soft")
                sub.prop(lamp, "shadow_buffer_bias", text="Bias")

                if wide_ui:
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
            col.prop(lamp, "auto_clip_start", text="Autoclip Start")
            sub = col.column()
            sub.active = not lamp.auto_clip_start
            sub.prop(lamp, "shadow_buffer_clip_start", text="Clip Start")

            if wide_ui:
                col = split.column()
            col.prop(lamp, "auto_clip_end", text="Autoclip End")
            sub = col.column()
            sub.active = not lamp.auto_clip_end
            sub.prop(lamp, "shadow_buffer_clip_end", text=" Clip End")


class DATA_PT_area(DataButtonsPanel):
    bl_label = "Area Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'AREA') and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        split = layout.split()

        col = split.column()
        col.row().prop(lamp, "shape", expand=True)

        sub = col.column(align=True)
        if (lamp.shape == 'SQUARE'):
            sub.prop(lamp, "size")
        elif (lamp.shape == 'RECTANGLE'):
            sub.prop(lamp, "size", text="Size X")
            sub.prop(lamp, "size_y", text="Size Y")


class DATA_PT_spot(DataButtonsPanel):
    bl_label = "Spot Shape"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'SPOT') and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.prop(lamp, "spot_size", text="Size")
        sub.prop(lamp, "spot_blend", text="Blend", slider=True)
        col.prop(lamp, "square")
        col.prop(lamp, "show_cone")

        if wide_ui:
            col = split.column()
        else:
            col.separator()
        col.prop(lamp, "halo")
        sub = col.column(align=True)
        sub.active = lamp.halo
        sub.prop(lamp, "halo_intensity", text="Intensity")
        if lamp.shadow_method == 'BUFFER_SHADOW':
            sub.prop(lamp, "halo_step", text="Step")


class DATA_PT_falloff_curve(DataButtonsPanel):
    bl_label = "Falloff Curve"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        lamp = context.lamp
        engine = context.scene.render.engine

        return (lamp and lamp.type in ('POINT', 'SPOT') and lamp.falloff_type == 'CUSTOM_CURVE') and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        lamp = context.lamp

        self.layout.template_curve_mapping(lamp, "falloff_curve")


classes = [
    LAMP_MT_sunsky_presets,

    DATA_PT_context_lamp,
    DATA_PT_preview,
    DATA_PT_lamp,
    DATA_PT_falloff_curve,
    DATA_PT_area,
    DATA_PT_spot,
    DATA_PT_shadow,
    DATA_PT_sunsky,

    DATA_PT_custom_props_lamp]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
