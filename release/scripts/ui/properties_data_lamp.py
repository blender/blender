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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180

class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return context.lamp


class DATA_PT_preview(DataButtonsPanel):
    bl_label = "Preview"

    def draw(self, context):
        self.layout.template_preview(context.lamp)


class DATA_PT_context_lamp(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        lamp = context.lamp
        space = context.space_data
        col2 = context.region.width > narrowui


        if col2:
            split = layout.split(percentage=0.65)
            if ob:
                split.template_ID(ob, "data")
                split.itemS()
            elif lamp:
                split.template_ID(space, "pin_id")
                split.itemS()
        else:
            layout.template_ID(ob, "data")


class DATA_PT_lamp(DataButtonsPanel):
    bl_label = "Lamp"

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        col2 = context.region.width > narrowui

        if col2:
            layout.itemR(lamp, "type", expand=True)
        else:
            layout.itemR(lamp, "type", text="")

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.itemR(lamp, "color", text="")
        sub.itemR(lamp, "energy")

        if lamp.type in ('POINT', 'SPOT'):
            sub.itemL(text="Falloff:")
            sub.itemR(lamp, "falloff_type", text="")
            sub.itemR(lamp, "distance")

            if lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED':
                col.itemL(text="Attenuation Factors:")
                sub = col.column(align=True)
                sub.itemR(lamp, "linear_attenuation", slider=True, text="Linear")
                sub.itemR(lamp, "quadratic_attenuation", slider=True, text="Quadratic")

            col.itemR(lamp, "sphere")

        if lamp.type == 'AREA':
            col.itemR(lamp, "distance")
            col.itemR(lamp, "gamma")

        if col2:
            col = split.column()
        col.itemR(lamp, "negative")
        col.itemR(lamp, "layer", text="This Layer Only")
        col.itemR(lamp, "specular")
        col.itemR(lamp, "diffuse")


class DATA_PT_sunsky(DataButtonsPanel):
    bl_label = "Sky & Atmosphere"

    def poll(self, context):
        lamp = context.lamp
        return (lamp and lamp.type == 'SUN')

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp.sky
        col2 = context.region.width > narrowui

        layout.itemR(lamp, "sky")

        row = layout.row()
        row.active = lamp.sky or lamp.atmosphere
        row.itemR(lamp, "atmosphere_turbidity", text="Turbidity")

        split = layout.split()

        col = split.column()
        col.active = lamp.sky
        col.itemL(text="Blending:")
        sub = col.column()
        sub.itemR(lamp, "sky_blend_type", text="")
        sub.itemR(lamp, "sky_blend", text="Factor")

        col.itemL(text="Color Space:")
        sub = col.column()
        sub.row().itemR(lamp, "sky_color_space", expand=True)
        sub.itemR(lamp, "sky_exposure", text="Exposure")

        if col2:
            col = split.column()
        col.active = lamp.sky
        col.itemL(text="Horizon:")
        sub = col.column()
        sub.itemR(lamp, "horizon_brightness", text="Brightness")
        sub.itemR(lamp, "spread", text="Spread")

        col.itemL(text="Sun:")
        sub = col.column()
        sub.itemR(lamp, "sun_brightness", text="Brightness")
        sub.itemR(lamp, "sun_size", text="Size")
        sub.itemR(lamp, "backscattered_light", slider=True, text="Back Light")

        layout.itemS()

        layout.itemR(lamp, "atmosphere")

        split = layout.split()

        col = split.column()
        col.active = lamp.atmosphere
        col.itemL(text="Intensity:")
        col.itemR(lamp, "sun_intensity", text="Sun")
        col.itemR(lamp, "atmosphere_distance_factor", text="Distance")

        if col2:
            col = split.column()
        col.active = lamp.atmosphere
        col.itemL(text="Scattering:")
        sub = col.column(align=True)
        sub.itemR(lamp, "atmosphere_inscattering", slider=True, text="Inscattering")
        sub.itemR(lamp, "atmosphere_extinction", slider=True, text="Extinction")


class DATA_PT_shadow(DataButtonsPanel):
    bl_label = "Shadow"

    def poll(self, context):
        lamp = context.lamp
        return (lamp and lamp.type in ('POINT', 'SUN', 'SPOT', 'AREA'))

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        col2 = context.region.width > narrowui

        if col2:
            layout.itemR(lamp, "shadow_method", expand=True)
        else:
            layout.itemR(lamp, "shadow_method", text="")

        if lamp.shadow_method != 'NOSHADOW':
            split = layout.split()

            col = split.column()
            col.itemR(lamp, "shadow_color", text="")

            if col2:
                col = split.column()
            col.itemR(lamp, "shadow_layer", text="This Layer Only")
            col.itemR(lamp, "only_shadow")

        if lamp.shadow_method == 'RAY_SHADOW':
            col = layout.column()
            col.itemL(text="Sampling:")
            if col2:
                col.row().itemR(lamp, "shadow_ray_sampling_method", expand=True)
            else:
                col.itemR(lamp, "shadow_ray_sampling_method", text="")

            if lamp.type in ('POINT', 'SUN', 'SPOT'):
                split = layout.split()

                col = split.column()
                col.itemR(lamp, "shadow_soft_size", text="Soft Size")

                col.itemR(lamp, "shadow_ray_samples", text="Samples")
                if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
                    col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
                if col2:
                    col = split.column()

            elif lamp.type == 'AREA':
                split = layout.split()

                col = split.column()
                
                if lamp.shape == 'SQUARE':
                    col.itemR(lamp, "shadow_ray_samples_x", text="Samples")
                elif lamp.shape == 'RECTANGLE':
                    col.itemR(lamp, "shadow_ray_samples_x", text="Samples X")
                    col.itemR(lamp, "shadow_ray_samples_y", text="Samples Y")

                if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
                    col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
                    if col2:
                        col = split.column()

                elif lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
                    if col2:
                        col = split.column()
                    col.itemR(lamp, "umbra")
                    col.itemR(lamp, "dither")
                    col.itemR(lamp, "jitter")
                else:
                    if col2:
                        col = split.column()
                

        elif lamp.shadow_method == 'BUFFER_SHADOW':
            col = layout.column()
            col.itemL(text="Buffer Type:")
            if col2:
                col.row().itemR(lamp, "shadow_buffer_type", expand=True)
            else:
                col.row().itemR(lamp, "shadow_buffer_type", text="")

            if lamp.shadow_buffer_type in ('REGULAR', 'HALFWAY', 'DEEP'):
                split = layout.split()

                col = split.column()
                col.itemL(text="Filter Type:")
                col.itemR(lamp, "shadow_filter_type", text="")
                sub = col.column(align=True)
                sub.itemR(lamp, "shadow_buffer_soft", text="Soft")
                sub.itemR(lamp, "shadow_buffer_bias", text="Bias")

                if col2:
                    col = split.column()
                col.itemL(text="Sample Buffers:")
                col.itemR(lamp, "shadow_sample_buffers", text="")
                sub = col.column(align=True)
                sub.itemR(lamp, "shadow_buffer_size", text="Size")
                sub.itemR(lamp, "shadow_buffer_samples", text="Samples")
                if lamp.shadow_buffer_type == 'DEEP':
                    col.itemR(lamp, "compression_threshold")

            elif lamp.shadow_buffer_type == 'IRREGULAR':
                layout.itemR(lamp, "shadow_buffer_bias", text="Bias")

            split = layout.split()

            col = split.column()
            col.itemR(lamp, "auto_clip_start", text="Autoclip Start")
            sub = col.column()
            sub.active = not lamp.auto_clip_start
            sub.itemR(lamp, "shadow_buffer_clip_start", text="Clip Start")

            if col2:
                col = split.column()
            col.itemR(lamp, "auto_clip_end", text="Autoclip End")
            sub = col.column()
            sub.active = not lamp.auto_clip_end
            sub.itemR(lamp, "shadow_buffer_clip_end", text=" Clip End")


class DATA_PT_area(DataButtonsPanel):
    bl_label = "Area Shape"

    def poll(self, context):
        lamp = context.lamp
        return (lamp and lamp.type == 'AREA')

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        split = layout.split()

        col = split.column()
        col.row().itemR(lamp, "shape", expand=True)

        sub = col.column(align=True)
        if (lamp.shape == 'SQUARE'):
            sub.itemR(lamp, "size")
        elif (lamp.shape == 'RECTANGLE'):
            sub.itemR(lamp, "size", text="Size X")
            sub.itemR(lamp, "size_y", text="Size Y")


class DATA_PT_spot(DataButtonsPanel):
    bl_label = "Spot Shape"

    def poll(self, context):
        lamp = context.lamp
        return (lamp and lamp.type == 'SPOT')

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        col2 = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.itemR(lamp, "spot_size", text="Size")
        sub.itemR(lamp, "spot_blend", text="Blend", slider=True)
        col.itemR(lamp, "square")

        if col2:
            col = split.column()
        else:
            col.itemS()
        col.itemR(lamp, "halo")
        sub = col.column(align=True)
        sub.active = lamp.halo
        sub.itemR(lamp, "halo_intensity", text="Intensity")
        if lamp.shadow_method == 'BUFFER_SHADOW':
            sub.itemR(lamp, "halo_step", text="Step")


class DATA_PT_falloff_curve(DataButtonsPanel):
    bl_label = "Falloff Curve"
    bl_default_closed = True

    def poll(self, context):
        lamp = context.lamp

        return (lamp and lamp.type in ('POINT', 'SPOT') and lamp.falloff_type == 'CUSTOM_CURVE')

    def draw(self, context):
        lamp = context.lamp

        self.layout.template_curve_mapping(lamp, "falloff_curve")

bpy.types.register(DATA_PT_context_lamp)
bpy.types.register(DATA_PT_preview)
bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_falloff_curve)
bpy.types.register(DATA_PT_area)
bpy.types.register(DATA_PT_spot)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_sunsky)
