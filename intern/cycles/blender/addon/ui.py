#
# Copyright 2011-2013 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# <pep8 compliant>

import bpy
from bpy_extras.node_utils import find_node_input
from bl_operators.presets import PresetMenu

from bpy.types import (
    Panel,
    Menu,
    Operator,
)


class CYCLES_MT_sampling_presets(PresetMenu):
    bl_label = "Sampling Presets"
    preset_subdir = "cycles/sampling"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.cycles_sampling_preset_add"
    COMPAT_ENGINES = {'CYCLES'}


class CYCLES_MT_integrator_presets(PresetMenu):
    bl_label = "Integrator Presets"
    preset_subdir = "cycles/integrator"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.cycles_integrator_preset_add"
    COMPAT_ENGINES = {'CYCLES'}


class CyclesButtonsPanel:
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        return context.engine in cls.COMPAT_ENGINES


def get_device_type(context):
    return context.user_preferences.addons[__package__].preferences.compute_device_type


def use_cpu(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'NONE' or cscene.device == 'CPU')


def use_opencl(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'OPENCL' and cscene.device == 'GPU')


def use_cuda(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'CUDA' and cscene.device == 'GPU')


def use_branched_path(context):
    cscene = context.scene.cycles

    return (cscene.progressive == 'BRANCHED_PATH')


def use_sample_all_lights(context):
    cscene = context.scene.cycles

    return cscene.sample_all_lights_direct or cscene.sample_all_lights_indirect


def show_device_active(context):
    cscene = context.scene.cycles
    if cscene.device != 'GPU':
        return True
    return context.user_preferences.addons[__package__].preferences.has_active_device()


def draw_samples_info(layout, context):
    cscene = context.scene.cycles
    integrator = cscene.progressive

    # Calculate sample values
    if integrator == 'PATH':
        aa = cscene.samples
        if cscene.use_square_samples:
            aa = aa * aa
    else:
        aa = cscene.aa_samples
        d = cscene.diffuse_samples
        g = cscene.glossy_samples
        t = cscene.transmission_samples
        ao = cscene.ao_samples
        ml = cscene.mesh_light_samples
        sss = cscene.subsurface_samples
        vol = cscene.volume_samples

        if cscene.use_square_samples:
            aa = aa * aa
            d = d * d
            g = g * g
            t = t * t
            ao = ao * ao
            ml = ml * ml
            sss = sss * sss
            vol = vol * vol

    # Draw interface
    # Do not draw for progressive, when Square Samples are disabled
    if use_branched_path(context) or (cscene.use_square_samples and integrator == 'PATH'):
        col = layout.column(align=True)
        col.scale_y = 0.6
        col.label("Total Samples:")
        col.separator()
        if integrator == 'PATH':
            col.label("%s AA" % aa)
        else:
            col.label("%s AA, %s Diffuse, %s Glossy, %s Transmission" %
                      (aa, d * aa, g * aa, t * aa))
            col.separator()
            col.label("%s AO, %s Mesh Light, %s Subsurface, %s Volume" %
                      (ao * aa, ml * aa, sss * aa, vol * aa))


class CYCLES_RENDER_PT_sampling(CyclesButtonsPanel, Panel):
    bl_label = "Sampling"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header_preset(self, context):
        CYCLES_MT_sampling_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False

        scene = context.scene
        cscene = scene.cycles

        layout.use_property_split = True

        layout.prop(cscene, "progressive")

        if cscene.progressive == 'PATH' or use_branched_path(context) is False:
            col = layout.column(align=True)
            col.prop(cscene, "samples", text="Render")
            col.prop(cscene, "preview_samples", text="Viewport")
            col.separator()
            col.prop(cscene, "use_square_samples")  # Duplicate below.
        else:

            col = layout.column(align=True)
            col.label(text="AA Samples")
            col.prop(cscene, "aa_samples", text="Render")
            col.prop(cscene, "preview_aa_samples", text="Preview")

            col = layout.column(align=True)
            col.label(text="Samples")
            col.prop(cscene, "diffuse_samples", text="Diffuse")
            col.prop(cscene, "glossy_samples", text="Glossy")
            col.prop(cscene, "transmission_samples", text="Transmission")
            col.prop(cscene, "ao_samples", text="AO")

            sub = col.row(align=True)
            sub.active = use_sample_all_lights(context)
            sub.prop(cscene, "mesh_light_samples", text="Mesh Light")
            col.prop(cscene, "subsurface_samples", text="Subsurface")
            col.prop(cscene, "volume_samples", text="Volume")
            col.separator()
            col.prop(cscene, "use_square_samples")  # Duplicate above.

            col = layout.column(align=True)
            col.prop(cscene, "sample_all_lights_direct")
            col.prop(cscene, "sample_all_lights_indirect")

        row = layout.row(align=True)
        row.prop(cscene, "seed")
        row.prop(cscene, "use_animated_seed", text="", icon='TIME')

        layout.prop(cscene, "sampling_pattern", text="Pattern")


class CYCLES_RENDER_PT_sampling_light(CyclesButtonsPanel, Panel):
    bl_label = "Light"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "light_sampling_threshold", text="Light Threshold")

        col = layout.column(align=True)
        col.prop(cscene, "sample_clamp_direct")
        col.prop(cscene, "sample_clamp_indirect")

        draw_samples_info(layout, context)


class CYCLES_RENDER_PT_geometry(CyclesButtonsPanel, Panel):
    bl_label = "Geometry"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_geometry_subdivision(CyclesButtonsPanel, Panel):
    bl_label = "Subdivision"
    bl_parent_id = "CYCLES_RENDER_PT_geometry"

    @classmethod
    def poll(self, context):
        return context.scene.cycles.feature_set == 'EXPERIMENTAL'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(cscene, "dicing_rate", text="Dicing Rate Render")
        sub.prop(cscene, "preview_dicing_rate", text="Preview")

        col.separator()

        col.prop(cscene, "offscreen_dicing_scale", text="Offscreen Scale")
        col.prop(cscene, "max_subdivisions")

        col.prop(cscene, "dicing_camera")


class CYCLES_RENDER_PT_geometry_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_parent_id = "CYCLES_RENDER_PT_geometry"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles
        ccscene = scene.cycles_curves

        col = layout.column()
        col.prop(cscene, "volume_step_size", text="Step Size")
        col.prop(cscene, "volume_max_steps", text="Max Steps")


class CYCLES_RENDER_PT_geometry_hair(CyclesButtonsPanel, Panel):
    bl_label = "Hair"
    bl_parent_id = "CYCLES_RENDER_PT_geometry"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout
        scene = context.scene
        cscene = scene.cycles
        ccscene = scene.cycles_curves

        layout.prop(ccscene, "use_curves", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles
        ccscene = scene.cycles_curves

        layout.active = ccscene.use_curves

        col = layout.column()
        col.prop(ccscene, "minimum_width", text="Min Pixels")
        col.prop(ccscene, "maximum_width", text="Max Extension")
        col.prop(ccscene, "shape", text="Shape")
        if not (ccscene.primitive in {'CURVE_SEGMENTS', 'LINE_SEGMENTS'} and ccscene.shape == 'RIBBONS'):
            col.prop(ccscene, "cull_backfacing", text="Cull back-faces")
        col.prop(ccscene, "primitive", text="Primitive")

        if ccscene.primitive == 'TRIANGLES' and ccscene.shape == 'THICK':
            col.prop(ccscene, "resolution", text="Resolution")
        elif ccscene.primitive == 'CURVE_SEGMENTS':
            col.prop(ccscene, "subdivisions", text="Curve subdivisions")


class CYCLES_RENDER_PT_light_paths(CyclesButtonsPanel, Panel):
    bl_label = "Light Paths"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header_preset(self, context):
        CYCLES_MT_integrator_presets.draw_panel_header(self.layout)

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_light_paths_max_bounces(CyclesButtonsPanel, Panel):
    bl_label = "Max Bounces"
    bl_parent_id = "CYCLES_RENDER_PT_light_paths"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "max_bounces", text="Total")

        col = layout.column(align=True)
        col.prop(cscene, "diffuse_bounces", text="Diffuse")
        col.prop(cscene, "glossy_bounces", text="Glossy")
        col.prop(cscene, "transparent_max_bounces", text="Transparency")
        col.prop(cscene, "transmission_bounces", text="Transmission")
        col.prop(cscene, "volume_bounces", text="Volume")


class CYCLES_RENDER_PT_light_paths_caustics(CyclesButtonsPanel, Panel):
    bl_label = "Caustics"
    bl_parent_id = "CYCLES_RENDER_PT_light_paths"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "blur_glossy")
        col.prop(cscene, "caustics_reflective")
        col.prop(cscene, "caustics_refractive")


class CYCLES_RENDER_PT_motion_blur(CyclesButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles
        rd = scene.render
        layout.active = rd.use_motion_blur

        col = layout.column()
        col.prop(cscene, "motion_blur_position", text="Position")
        col.prop(rd, "motion_blur_shutter")
        col.separator()
        col.prop(cscene, "rolling_shutter_type", text="Rolling Shutter")
        sub = col.column()
        sub.active = cscene.rolling_shutter_type != 'NONE'
        sub.prop(cscene, "rolling_shutter_duration")


class CYCLES_RENDER_PT_motion_blur_curve(CyclesButtonsPanel, Panel):
    bl_label = "Shutter Curve"
    bl_parent_id = "CYCLES_RENDER_PT_motion_blur"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles
        rd = scene.render
        layout.active = rd.use_motion_blur

        col = layout.column()

        col.template_curve_mapping(rd, "motion_blur_shutter_curve")

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("render.shutter_curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("render.shutter_curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("render.shutter_curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("render.shutter_curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("render.shutter_curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("render.shutter_curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class CYCLES_RENDER_PT_film(CyclesButtonsPanel, Panel):
    bl_label = "Film"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "film_exposure")


class CYCLES_RENDER_PT_film_transparency(CyclesButtonsPanel, Panel):
    bl_label = "Transparency"
    bl_parent_id = "CYCLES_RENDER_PT_film"

    def draw_header(self, context):
        layout = self.layout
        rd = context.scene.render

        scene = context.scene
        cscene = scene.cycles

        layout.prop(cscene, "film_transparent", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        cscene = scene.cycles

        layout.active = cscene.film_transparent

        col = layout.column()
        col.prop(cscene, "film_transparent_glass", text="Transparent Glass")

        sub = col.column()
        sub.active = cscene.film_transparent and cscene.film_transparent_glass
        sub.prop(cscene, "film_transparent_roughness", text="Roughness Threshold")


class CYCLES_RENDER_PT_film_pixel_filter(CyclesButtonsPanel, Panel):
    bl_label = "Pixel Filter"
    bl_parent_id = "CYCLES_RENDER_PT_film"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "pixel_filter_type", text="Type")
        if cscene.pixel_filter_type != 'BOX':
            col.prop(cscene, "filter_width", text="Width")


class CYCLES_RENDER_PT_performance(CyclesButtonsPanel, Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()
        col.active = show_device_active(context)
        col.prop(cscene, "device")

        from . import engine
        if engine.with_osl() and use_cpu(context):
            col.prop(cscene, "shading_system")


class CYCLES_RENDER_PT_performance_threads(CyclesButtonsPanel, Panel):
    bl_label = "Threads"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()

        col.prop(rd, "threads_mode")
        sub = col.column(align=True)
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")


class CYCLES_RENDER_PT_performance_tiles(CyclesButtonsPanel, Panel):
    bl_label = "Tiles"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()

        sub = col.column(align=True)
        sub.prop(rd, "tile_x", text="Tiles X")
        sub.prop(rd, "tile_y", text="Y")
        col.prop(cscene, "tile_order", text="Order")

        sub = col.column()
        sub.active = not rd.use_save_buffers
        for view_layer in scene.view_layers:
            if view_layer.cycles.use_denoising:
                sub.active = False
        sub.prop(cscene, "use_progressive_refine")


class CYCLES_RENDER_PT_performance_acceleration_structure(CyclesButtonsPanel, Panel):
    bl_label = "Acceleration Structure"
    bl_parent_id = "CYCLES_RENDER_PT_performance"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()

        col.prop(cscene, "debug_use_spatial_splits")
        col.prop(cscene, "debug_use_hair_bvh")
        sub = col.column()
        sub.active = not cscene.debug_use_spatial_splits
        sub.prop(cscene, "debug_bvh_time_steps")


class CYCLES_RENDER_PT_performance_final_render(CyclesButtonsPanel, Panel):
    bl_label = "Final Render"
    bl_parent_id = "CYCLES_RENDER_PT_performance"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()

        col.prop(rd, "use_save_buffers")
        col.prop(rd, "use_persistent_data", text="Persistent Images")


class CYCLES_RENDER_PT_performance_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "CYCLES_RENDER_PT_performance"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()
        col.prop(rd, "preview_pixel_size", text="Pixel Size")
        col.prop(cscene, "preview_start_resolution", text="Start Pixels")


class CYCLES_RENDER_PT_filter(CyclesButtonsPanel, Panel):
    bl_label = "Filter"
    bl_context = "view_layer"

    def draw(self, context):
        layout = self.layout
        with_freestyle = bpy.app.build_options.freestyle

        scene = context.scene
        rd = scene.render
        view_layer = context.view_layer

        col = layout.column()
        col.prop(view_layer, "use_sky", "Use Environment")
        col.prop(view_layer, "use_ao", "Use Ambient Occlusion")
        col.prop(view_layer, "use_solid", "Use Surfaces")
        col.prop(view_layer, "use_strand", "Use Hair")
        if with_freestyle:
            row = col.row()
            row.prop(view_layer, "use_freestyle", "Use Freestyle")
            row.active = rd.use_freestyle


class CYCLES_RENDER_PT_layer_passes(CyclesButtonsPanel, Panel):
    bl_label = "Passes"
    bl_context = "view_layer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        import _cycles

        layout = self.layout

        scene = context.scene
        rd = scene.render
        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles

        split = layout.split()

        col = split.column()
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")
        col.prop(view_layer, "use_pass_mist")
        col.prop(view_layer, "use_pass_normal")
        row = col.row()
        row.prop(view_layer, "use_pass_vector")
        row.active = not rd.use_motion_blur
        col.prop(view_layer, "use_pass_uv")
        col.prop(view_layer, "use_pass_object_index")
        col.prop(view_layer, "use_pass_material_index")
        col.separator()
        col.prop(view_layer, "use_pass_shadow")
        col.prop(view_layer, "use_pass_ambient_occlusion", text="Ambient Occlusion")
        col.separator()
        col.prop(view_layer, "pass_alpha_threshold")

        col = split.column()
        col.label(text="Diffuse:")
        row = col.row(align=True)
        row.prop(view_layer, "use_pass_diffuse_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_diffuse_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_diffuse_color", text="Color", toggle=True)
        col.label(text="Glossy:")
        row = col.row(align=True)
        row.prop(view_layer, "use_pass_glossy_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_glossy_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_glossy_color", text="Color", toggle=True)
        col.label(text="Transmission:")
        row = col.row(align=True)
        row.prop(view_layer, "use_pass_transmission_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_transmission_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_transmission_color", text="Color", toggle=True)
        col.label(text="Subsurface:")
        row = col.row(align=True)
        row.prop(view_layer, "use_pass_subsurface_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_subsurface_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_subsurface_color", text="Color", toggle=True)
        col.label(text="Volume:")
        row = col.row(align=True)
        row.prop(cycles_view_layer, "use_pass_volume_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "use_pass_volume_indirect", text="Indirect", toggle=True)

        col.separator()
        col.prop(view_layer, "use_pass_emit", text="Emission")
        col.prop(view_layer, "use_pass_environment")

        if context.scene.cycles.feature_set == 'EXPERIMENTAL':
            col.separator()
            sub = col.column()
            sub.active = cycles_view_layer.use_denoising
            sub.prop(cycles_view_layer, "denoising_store_passes", text="Denoising")

        col = layout.column()
        col.prop(cycles_view_layer, "pass_debug_render_time")
        if _cycles.with_cycles_debug:
            col.prop(cycles_view_layer, "pass_debug_bvh_traversed_nodes")
            col.prop(cycles_view_layer, "pass_debug_bvh_traversed_instances")
            col.prop(cycles_view_layer, "pass_debug_bvh_intersections")
            col.prop(cycles_view_layer, "pass_debug_ray_bounces")


class CYCLES_RENDER_PT_denoising(CyclesButtonsPanel, Panel):
    bl_label = "Denoising"
    bl_context = "view_layer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        scene = context.scene
        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles
        cscene = scene.cycles
        layout = self.layout

        layout.prop(cycles_view_layer, "use_denoising", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        cscene = scene.cycles
        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles

        layout.active = cycles_view_layer.use_denoising

        col = layout.column()
        sub = col.column()
        sub.prop(cycles_view_layer, "denoising_radius", text="Radius")
        sub.prop(cycles_view_layer, "denoising_strength", slider=True, text="Strength")

        sub = col.column(align=True)
        sub.prop(cycles_view_layer, "denoising_feature_strength", slider=True, text="Feature Strength")
        sub.prop(cycles_view_layer, "denoising_relative_pca")

#        layout.use_property_split = False

        """
        layout.separator()

        col = layout.column(align=True)
        col.prop(cycles_view_layer, "denoising_diffuse_direct", text="Diffuse Direct")
        col.prop(cycles_view_layer, "denoising_diffuse_indirect", text="Indirect")

        col = layout.column(align=True)
        col.prop(cycles_view_layer, "denoising_glossy_direct", text="Glossy Direct")
        col.prop(cycles_view_layer, "denoising_glossy_indirect", text="Indirect")

        col = layout.column(align=True)
        col.prop(cycles_view_layer, "denoising_transmission_direct", text="Transmission Direct")
        col.prop(cycles_view_layer, "denoising_transmission_indirect", text="Indirect")

        col = layout.column(align=True)
        col.prop(cycles_view_layer, "denoising_subsurface_direct", text="Subsurface Direct")
        col.prop(cycles_view_layer, "denoising_subsurface_indirect", text="Indirect")
        """

        layout.use_property_split = False

        split = layout.split(percentage=0.5)
        split.label(text="Diffuse")
        col = split.column()
        row = col.row(align=True)
        row.prop(cycles_view_layer, "denoising_diffuse_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_diffuse_indirect", text="Indirect", toggle=True)

        split = layout.split(percentage=0.5)
        split.label(text="Glossy")
        col = split.column()
        row = col.row(align=True)
        row.prop(cycles_view_layer, "denoising_glossy_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_glossy_indirect", text="Indirect", toggle=True)

        split = layout.split(percentage=0.5)
        split.label(text="Transmission")
        col = split.column()
        row = col.row(align=True)
        row.prop(cycles_view_layer, "denoising_transmission_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_transmission_indirect", text="Indirect", toggle=True)

        split = layout.split(percentage=0.5)
        split.label(text="Subsurface")
        col = split.column()
        row = col.row(align=True)
        row.prop(cycles_view_layer, "denoising_subsurface_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_subsurface_indirect", text="Indirect", toggle=True)


class CYCLES_PT_post_processing(CyclesButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        rd = context.scene.render

        col = layout.column(align=True)
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        layout.prop(rd, "dither_intensity", text="Dither", slider=True)


class CYCLES_CAMERA_PT_dof(CyclesButtonsPanel, Panel):
    bl_label = "Depth of Field"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.camera and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera
        ccam = cam.cycles
        dof_options = cam.gpu_dof

        split = layout.split()

        col = split.column()
        col.prop(cam, "dof_object", text="Focus Object")

        sub = col.row()
        sub.active = cam.dof_object is None
        sub.prop(cam, "dof_distance", text="Distance")


class CYCLES_CAMERA_PT_dof_aperture(CyclesButtonsPanel, Panel):
    bl_label = "Aperture"
    bl_parent_id = "CYCLES_CAMERA_PT_dof"

    @classmethod
    def poll(cls, context):
        return context.camera and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        cam = context.camera
        ccam = cam.cycles
        dof_options = cam.gpu_dof

        col = flow.column()
        col.prop(ccam, "aperture_type")
        if ccam.aperture_type == 'RADIUS':
            col.prop(ccam, "aperture_size", text="Size")
        elif ccam.aperture_type == 'FSTOP':
            col.prop(ccam, "aperture_fstop", text="Number")
        col.separator()

        col = flow.column()
        col.prop(ccam, "aperture_blades", text="Blades")
        col.prop(ccam, "aperture_rotation", text="Rotation")
        col.prop(ccam, "aperture_ratio", text="Ratio")


class CYCLES_CAMERA_PT_dof_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "CYCLES_CAMERA_PT_dof"

    @classmethod
    def poll(cls, context):
        return context.camera and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        cam = context.camera
        dof_options = cam.gpu_dof

        sub = flow.column(align=True)
        sub.prop(dof_options, "fstop")
        sub.prop(dof_options, "blades")


class CYCLES_PT_context_material(CyclesButtonsPanel, Panel):
    bl_label = ""
    bl_context = "material"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        if context.active_object and context.active_object.type == 'GPENCIL':
            return False
        else:
            return (context.material or context.object) and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        mat = context.material
        ob = context.object
        slot = context.material_slot
        space = context.space_data

        if ob:
            is_sortable = len(ob.material_slots) > 1
            rows = 1
            if (is_sortable):
                rows = 4

            row = layout.row()

            row.template_list("MATERIAL_UL_matslots", "", ob, "material_slots", ob, "active_material_index", rows=rows)

            col = row.column(align=True)
            col.operator("object.material_slot_add", icon='ZOOMIN', text="")
            col.operator("object.material_slot_remove", icon='ZOOMOUT', text="")

            col.menu("MATERIAL_MT_specials", icon='DOWNARROW_HLT', text="")

            if is_sortable:
                col.separator()

                col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            if ob.mode == 'EDIT':
                row = layout.row(align=True)
                row.operator("object.material_slot_assign", text="Assign")
                row.operator("object.material_slot_select", text="Select")
                row.operator("object.material_slot_deselect", text="Deselect")

        split = layout.split(percentage=0.65)

        if ob:
            split.template_ID(ob, "active_material", new="material.new")
            row = split.row()

            if slot:
                row.prop(slot, "link", text="")
            else:
                row.label()
        elif mat:
            split.template_ID(space, "pin_id")
            split.separator()


class CYCLES_OBJECT_PT_motion_blur(CyclesButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        if CyclesButtonsPanel.poll(context) and ob:
            if ob.type in {'MESH', 'CURVE', 'CURVE', 'SURFACE', 'FONT', 'META', 'CAMERA'}:
                return True
            if ob.dupli_type == 'COLLECTION' and ob.dupli_group:
                return True
            # TODO(sergey): More duplicator types here?
        return False

    def draw_header(self, context):
        layout = self.layout

        rd = context.scene.render
        # scene = context.scene

        layout.active = rd.use_motion_blur

        ob = context.object
        cob = ob.cycles

        layout.prop(cob, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        # scene = context.scene

        ob = context.object
        cob = ob.cycles

        layout.active = (rd.use_motion_blur and cob.use_motion_blur)

        row = layout.row()
        if ob.type != 'CAMERA':
            row.prop(cob, "use_deform_motion", text="Deformation")
        row.prop(cob, "motion_steps", text="Steps")


class CYCLES_OBJECT_PT_cycles_settings(CyclesButtonsPanel, Panel):
    bl_label = "Cycles Settings"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (CyclesButtonsPanel.poll(context) and
                ob and ((ob.type in {'MESH', 'CURVE', 'SURFACE', 'FONT', 'META', 'LIGHT'}) or
                        (ob.dupli_type == 'COLLECTION' and ob.dupli_group)))

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles
        ob = context.object
        cob = ob.cycles
        visibility = ob.cycles_visibility

        layout.label(text="Ray Visibility:")
        flow = layout.column_flow()

        flow.prop(visibility, "camera")
        flow.prop(visibility, "diffuse")
        flow.prop(visibility, "glossy")
        flow.prop(visibility, "transmission")
        flow.prop(visibility, "scatter")

        if ob.type != 'LIGHT':
            flow.prop(visibility, "shadow")

        row = layout.row()
        row.prop(cob, "is_shadow_catcher")
        row.prop(cob, "is_holdout")

        col = layout.column()
        col.label(text="Performance:")
        row = col.row()
        sub = row.row()
        sub.active = scene.render.use_simplify and cscene.use_camera_cull
        sub.prop(cob, "use_camera_cull")

        sub = row.row()
        sub.active = scene.render.use_simplify and cscene.use_distance_cull
        sub.prop(cob, "use_distance_cull")


class CYCLES_OT_use_shading_nodes(Operator):
    """Enable nodes on a material, world or light"""
    bl_idname = "cycles.use_shading_nodes"
    bl_label = "Use Nodes"

    @classmethod
    def poll(cls, context):
        return (getattr(context, "material", False) or getattr(context, "world", False) or
                getattr(context, "light", False))

    def execute(self, context):
        if context.material:
            context.material.use_nodes = True
        elif context.world:
            context.world.use_nodes = True
        elif context.light:
            context.light.use_nodes = True

        return {'FINISHED'}


def panel_node_draw(layout, id_data, output_type, input_name):
    if not id_data.use_nodes:
        layout.operator("cycles.use_shading_nodes", icon='NODETREE')
        return False

    ntree = id_data.node_tree

    node = ntree.get_output_node('CYCLES')
    if node:
        input = find_node_input(node, input_name)
        if input:
            layout.template_node_view(ntree, node, input)
        else:
            layout.label(text="Incompatible output node")
    else:
        layout.label(text="No output node")

    return True


class CYCLES_LIGHT_PT_preview(CyclesButtonsPanel, Panel):
    bl_label = "Preview"
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (
            context.light and
            not (
                context.light.type == 'AREA' and
                context.light.cycles.is_portal
            ) and
            CyclesButtonsPanel.poll(context)
        )

    def draw(self, context):
        self.layout.template_preview(context.light)


class CYCLES_LIGHT_PT_light(CyclesButtonsPanel, Panel):
    bl_label = "Light"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.light and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        light = context.light
        clamp = light.cycles
        # cscene = context.scene.cycles

        layout.prop(light, "type", expand=True)

        layout.use_property_split = True

        col = layout.column()

        if light.type in {'POINT', 'SUN', 'SPOT'}:
            col.prop(light, "shadow_soft_size", text="Size")
        elif light.type == 'AREA':
            col.prop(light, "shape", text="Shape")
            sub = col.column(align=True)

            if light.shape in {'SQUARE', 'DISK'}:
                sub.prop(light, "size")
            elif light.shape in {'RECTANGLE', 'ELLIPSE'}:
                sub.prop(light, "size", text="Size X")
                sub.prop(light, "size_y", text="Y")

        if not (light.type == 'AREA' and clamp.is_portal):
            sub = col.column()
            if use_branched_path(context):
                subsub = sub.row(align=True)
                subsub.active = use_sample_all_lights(context)
                subsub.prop(clamp, "samples")
            sub.prop(clamp, "max_bounces")

        sub = col.column(align=True)
        sub.active = not (light.type == 'AREA' and clamp.is_portal)
        sub.prop(clamp, "cast_shadow")
        sub.prop(clamp, "use_multiple_importance_sampling", text="Multiple Importance")

        if light.type == 'AREA':
            col.prop(clamp, "is_portal", text="Portal")

        if light.type == 'HEMI':
            layout.label(text="Not supported, interpreted as sun light")


class CYCLES_LIGHT_PT_nodes(CyclesButtonsPanel, Panel):
    bl_label = "Nodes"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.light and not (context.light.type == 'AREA' and
                                      context.light.cycles.is_portal) and \
            CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        light = context.light
        if not panel_node_draw(layout, light, 'OUTPUT_LIGHT', 'Surface'):
            layout.prop(light, "color")


class CYCLES_LIGHT_PT_spot(CyclesButtonsPanel, Panel):
    bl_label = "Spot Shape"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        light = context.light
        return (light and light.type == 'SPOT') and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.use_property_split = True

        col = layout.column()
        col.prop(light, "spot_size", text="Size")
        col.prop(light, "spot_blend", text="Blend", slider=True)
        col.prop(light, "show_cone")


class CYCLES_WORLD_PT_preview(CyclesButtonsPanel, Panel):
    bl_label = "Preview"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.world)


class CYCLES_WORLD_PT_surface(CyclesButtonsPanel, Panel):
    bl_label = "Surface"
    bl_context = "world"

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        world = context.world

        if not panel_node_draw(layout, world, 'OUTPUT_WORLD', 'Surface'):
            layout.prop(world, "color")


class CYCLES_WORLD_PT_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        world = context.world
        return world and world.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        world = context.world
        panel_node_draw(layout, world, 'OUTPUT_WORLD', 'Volume')


class CYCLES_WORLD_PT_ambient_occlusion(CyclesButtonsPanel, Panel):
    bl_label = "Ambient Occlusion"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw_header(self, context):
        light = context.world.light_settings
        self.layout.prop(light, "use_ambient_occlusion", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        light = context.world.light_settings
        scene = context.scene

        col = layout.column()
        sub = col.column()
        sub.active = light.use_ambient_occlusion or scene.render.use_simplify
        sub.prop(light, "ao_factor", text="Factor")
        col.prop(light, "distance", text="Distance")


class CYCLES_WORLD_PT_mist(CyclesButtonsPanel, Panel):
    bl_label = "Mist Pass"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CyclesButtonsPanel.poll(context):
            if context.world:
                for view_layer in context.scene.view_layers:
                    if view_layer.use_pass_mist:
                        return True

        return False

    def draw(self, context):
        layout = self.layout

        world = context.world

        split = layout.split(align=True)
        split.prop(world.mist_settings, "start")
        split.prop(world.mist_settings, "depth")

        layout.prop(world.mist_settings, "falloff")


class CYCLES_WORLD_PT_ray_visibility(CyclesButtonsPanel, Panel):
    bl_label = "Ray Visibility"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return CyclesButtonsPanel.poll(context) and context.world

    def draw(self, context):
        layout = self.layout

        world = context.world
        visibility = world.cycles_visibility

        flow = layout.column_flow()

        flow.prop(visibility, "camera")
        flow.prop(visibility, "diffuse")
        flow.prop(visibility, "glossy")
        flow.prop(visibility, "transmission")
        flow.prop(visibility, "scatter")


class CYCLES_WORLD_PT_settings(CyclesButtonsPanel, Panel):
    bl_label = "Settings"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        world = context.world
        cworld = world.cycles
        # cscene = context.scene.cycles

        col = layout.column()


class CYCLES_WORLD_PT_settings_surface(CyclesButtonsPanel, Panel):
    bl_label = "Surface"
    bl_parent_id = "CYCLES_WORLD_PT_settings"
    bl_context = "world"

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        world = context.world
        cworld = world.cycles

        col = layout.column()
        col.prop(cworld, "sampling_method", text="Sampling")

        sub = col.column()
        sub.active = cworld.sampling_method != 'NONE'
        subsub = sub.row(align=True)
        subsub.active = cworld.sampling_method == 'MANUAL'
        subsub.prop(cworld, "sample_map_resolution")
        if use_branched_path(context):
            subsub = sub.column(align=True)
            subsub.active = use_sample_all_lights(context)
            subsub.prop(cworld, "samples")
        sub.prop(cworld, "max_bounces")


class CYCLES_WORLD_PT_settings_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_parent_id = "CYCLES_WORLD_PT_settings"
    bl_context = "world"

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        world = context.world
        cworld = world.cycles

        col = layout.column()

        sub = col.column()
        sub.active = use_cpu(context)
        sub.prop(cworld, "volume_sampling", text="Sampling")
        col.prop(cworld, "volume_interpolation", text="Interpolation")
        col.prop(cworld, "homogeneous_volume", text="Homogeneous")


class CYCLES_MATERIAL_PT_preview(CyclesButtonsPanel, Panel):
    bl_label = "Preview"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.material and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.material)


class CYCLES_MATERIAL_PT_surface(CyclesButtonsPanel, Panel):
    bl_label = "Surface"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        return context.material and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        mat = context.material
        if not panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Surface'):
            layout.prop(mat, "diffuse_color")


class CYCLES_MATERIAL_PT_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and mat.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        mat = context.material
        # cmat = mat.cycles

        panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Volume')


class CYCLES_MATERIAL_PT_displacement(CyclesButtonsPanel, Panel):
    bl_label = "Displacement"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and mat.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        mat = context.material
        panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Displacement')


class CYCLES_MATERIAL_PT_settings(CyclesButtonsPanel, Panel):
    bl_label = "Settings"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        return context.material and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mat = context.material
        cmat = mat.cycles

        layout.prop(mat, "pass_index")


class CYCLES_MATERIAL_PT_settings_surface(CyclesButtonsPanel, Panel):
    bl_label = "Surface"
    bl_parent_id = "CYCLES_MATERIAL_PT_settings"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        return context.material and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mat = context.material
        cmat = mat.cycles

        col = layout.column()
        col.prop(cmat, "sample_as_light", text="Multiple Importance")
        col.prop(cmat, "use_transparent_shadow")
        col.prop(cmat, "displacement_method", text="Displacement Method")


class CYCLES_MATERIAL_PT_settings_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_parent_id = "CYCLES_MATERIAL_PT_settings"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        return context.material and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mat = context.material
        cmat = mat.cycles

        col = layout.column()
        sub = col.column()
        sub.active = use_cpu(context)
        sub.prop(cmat, "volume_sampling", text="Sampling")
        col.prop(cmat, "volume_interpolation", text="Interpolation")
        col.prop(cmat, "homogeneous_volume", text="Homogeneous")


class CYCLES_RENDER_PT_bake(CyclesButtonsPanel, Panel):
    bl_label = "Bake"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        cscene = scene.cycles
        cbk = scene.render.bake
        rd = scene.render

        col = layout.column()
        col.prop(rd, "use_bake_multires")
        if rd.use_bake_multires:
            col.prop(rd, "bake_type")

            col = layout.column()
            col.prop(rd, "bake_margin")
            col.prop(rd, "use_bake_clear")

            if rd.bake_type == 'DISPLACEMENT':
                col.prop(rd, "use_bake_lores_mesh")

            col.operator("object.bake_image", icon='RENDER_STILL')

        else:
            col.prop(cscene, "bake_type")

            col = layout.column()

            if cscene.bake_type == 'NORMAL':
                col.prop(cbk, "normal_space", text="Space")

                sub = col.column(align=True)
                sub.prop(cbk, "normal_r", text="Swizzle R")
                sub.prop(cbk, "normal_g", text="G")
                sub.prop(cbk, "normal_b", text="B")

            elif cscene.bake_type == 'COMBINED':
                row = col.row(align=True)
                row.use_property_split = False
                row.prop(cbk, "use_pass_direct", toggle=True)
                row.prop(cbk, "use_pass_indirect", toggle=True)

                col = col.column()
                col.active = cbk.use_pass_direct or cbk.use_pass_indirect
                col.prop(cbk, "use_pass_diffuse")
                col.prop(cbk, "use_pass_glossy")
                col.prop(cbk, "use_pass_transmission")
                col.prop(cbk, "use_pass_subsurface")
                col.prop(cbk, "use_pass_ambient_occlusion")
                col.prop(cbk, "use_pass_emit")

            elif cscene.bake_type in {'DIFFUSE', 'GLOSSY', 'TRANSMISSION', 'SUBSURFACE'}:
                row = col.row(align=True)
                row.use_property_split = False
                row.prop(cbk, "use_pass_direct", toggle=True)
                row.prop(cbk, "use_pass_indirect", toggle=True)
                row.prop(cbk, "use_pass_color", toggle=True)

            layout.separator()

            col = layout.column()
            col.prop(cbk, "margin")
            col.prop(cbk, "use_clear", text="Clear Image")

            col.separator()

            col.prop(cbk, "use_selected_to_active")
            sub = col.column()
            sub.active = cbk.use_selected_to_active
            sub.prop(cbk, "use_cage", text="Cage")
            if cbk.use_cage:
                sub.prop(cbk, "cage_extrusion", text="Extrusion")
                sub.prop_search(cbk, "cage_object", scene, "objects", text="Cage Object")
            else:
                sub.prop(cbk, "cage_extrusion", text="Ray Distance")

            layout.separator()

            layout.operator("object.bake", icon='RENDER_STILL').type = cscene.bake_type


class CYCLES_RENDER_PT_debug(CyclesButtonsPanel, Panel):
    bl_label = "Debug"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        return CyclesButtonsPanel.poll(context) and bpy.app.debug_value == 256

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()

        col.label('CPU Flags:')
        row = col.row(align=True)
        row.prop(cscene, "debug_use_cpu_sse2", toggle=True)
        row.prop(cscene, "debug_use_cpu_sse3", toggle=True)
        row.prop(cscene, "debug_use_cpu_sse41", toggle=True)
        row.prop(cscene, "debug_use_cpu_avx", toggle=True)
        row.prop(cscene, "debug_use_cpu_avx2", toggle=True)
        col.prop(cscene, "debug_bvh_layout")
        col.prop(cscene, "debug_use_cpu_split_kernel")

        col.separator()

        col = layout.column()
        col.label('CUDA Flags:')
        col.prop(cscene, "debug_use_cuda_adaptive_compile")
        col.prop(cscene, "debug_use_cuda_split_kernel")

        col.separator()

        col = layout.column()
        col.label('OpenCL Flags:')
        col.prop(cscene, "debug_opencl_kernel_type", text="Kernel")
        col.prop(cscene, "debug_opencl_device_type", text="Device")
        col.prop(cscene, "debug_opencl_kernel_single_program", text="Single Program")
        col.prop(cscene, "debug_use_opencl_debug", text="Debug")
        col.prop(cscene, "debug_opencl_mem_limit")

        col.separator()

        col = layout.column()
        col.prop(cscene, "debug_bvh_type")


class CYCLES_SCENE_PT_simplify(CyclesButtonsPanel, Panel):
    bl_label = "Simplify"
    bl_context = "scene"
    COMPAT_ENGINES = {'CYCLES'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        pass


class CYCLES_SCENE_PT_simplify_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_context = "scene"
    bl_parent_id = "CYCLES_SCENE_PT_simplify"
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        col = layout.column()
        col.prop(rd, "simplify_subdivision", text="Max Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")
        col.prop(cscene, "texture_limit", text="Texture Limit")
        col.prop(cscene, "ao_bounces", text="AO Bounces")


class CYCLES_SCENE_PT_simplify_render(CyclesButtonsPanel, Panel):
    bl_label = "Render"
    bl_context = "scene"
    bl_parent_id = "CYCLES_SCENE_PT_simplify"
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        col = layout.column()

        col.prop(rd, "simplify_subdivision_render", text="Max Subdivision")
        col.prop(rd, "simplify_child_particles_render", text="Child Particles")
        col.prop(cscene, "texture_limit_render", text="Texture Limit")
        col.prop(cscene, "ao_bounces_render", text="AO Bounces")


class CYCLES_SCENE_PT_simplify_culling(CyclesButtonsPanel, Panel):
    bl_label = "Culling"
    bl_context = "scene"
    bl_parent_id = "CYCLES_SCENE_PT_simplify"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        col = layout.column()
        col.prop(cscene, "use_camera_cull")
        sub = col.column()
        sub.active = cscene.use_camera_cull
        sub.prop(cscene, "camera_cull_margin")

        col = layout.column()
        col.prop(cscene, "use_distance_cull")
        sub = col.column()
        sub.active = cscene.use_distance_cull
        sub.prop(cscene, "distance_cull_margin", text="Distance")


def draw_device(self, context):
    scene = context.scene
    layout = self.layout
    layout.use_property_split = True

    if context.engine == 'CYCLES':
        from . import engine
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "feature_set")


def draw_pause(self, context):
    layout = self.layout
    scene = context.scene

    if context.engine == "CYCLES":
        view = context.space_data

        if view.shading.type == 'RENDERED':
            cscene = scene.cycles
            layout.prop(cscene, "preview_pause", icon='PAUSE', text="")


def get_panels():
    exclude_panels = {
        'DATA_PT_area',
        'DATA_PT_camera_dof',
        'DATA_PT_falloff_curve',
        'DATA_PT_light',
        'DATA_PT_preview',
        'DATA_PT_spot',
        'MATERIAL_PT_context_material',
        'MATERIAL_PT_preview',
        'VIEWLAYER_PT_filter',
        'VIEWLAYER_PT_layer_passes',
        'RENDER_PT_post_processing',
        'SCENE_PT_simplify',
    }

    panels = []
    for panel in bpy.types.Panel.__subclasses__():
        if hasattr(panel, 'COMPAT_ENGINES') and 'BLENDER_RENDER' in panel.COMPAT_ENGINES:
            if panel.__name__ not in exclude_panels:
                panels.append(panel)

    return panels


classes = (
    CYCLES_MT_sampling_presets,
    CYCLES_MT_integrator_presets,
    CYCLES_RENDER_PT_sampling,
    CYCLES_RENDER_PT_sampling_light,
    CYCLES_RENDER_PT_geometry,
    CYCLES_RENDER_PT_geometry_subdivision,
    CYCLES_RENDER_PT_geometry_volume,
    CYCLES_RENDER_PT_geometry_hair,
    CYCLES_RENDER_PT_light_paths,
    CYCLES_RENDER_PT_light_paths_max_bounces,
    CYCLES_RENDER_PT_light_paths_caustics,
    CYCLES_RENDER_PT_motion_blur,
    CYCLES_RENDER_PT_motion_blur_curve,
    CYCLES_RENDER_PT_film,
    CYCLES_RENDER_PT_film_transparency,
    CYCLES_RENDER_PT_film_pixel_filter,
    CYCLES_RENDER_PT_performance,
    CYCLES_RENDER_PT_performance_threads,
    CYCLES_RENDER_PT_performance_tiles,
    CYCLES_RENDER_PT_performance_acceleration_structure,
    CYCLES_RENDER_PT_performance_final_render,
    CYCLES_RENDER_PT_performance_viewport,
    CYCLES_RENDER_PT_filter,
    CYCLES_RENDER_PT_layer_passes,
    CYCLES_RENDER_PT_denoising,
    CYCLES_PT_post_processing,
    CYCLES_CAMERA_PT_dof,
    CYCLES_CAMERA_PT_dof_aperture,
    CYCLES_CAMERA_PT_dof_viewport,
    CYCLES_PT_context_material,
    CYCLES_OBJECT_PT_motion_blur,
    CYCLES_OBJECT_PT_cycles_settings,
    CYCLES_OT_use_shading_nodes,
    CYCLES_LIGHT_PT_preview,
    CYCLES_LIGHT_PT_light,
    CYCLES_LIGHT_PT_nodes,
    CYCLES_LIGHT_PT_spot,
    CYCLES_WORLD_PT_preview,
    CYCLES_WORLD_PT_surface,
    CYCLES_WORLD_PT_volume,
    CYCLES_WORLD_PT_ambient_occlusion,
    CYCLES_WORLD_PT_mist,
    CYCLES_WORLD_PT_ray_visibility,
    CYCLES_WORLD_PT_settings,
    CYCLES_WORLD_PT_settings_surface,
    CYCLES_WORLD_PT_settings_volume,
    CYCLES_MATERIAL_PT_preview,
    CYCLES_MATERIAL_PT_surface,
    CYCLES_MATERIAL_PT_volume,
    CYCLES_MATERIAL_PT_displacement,
    CYCLES_MATERIAL_PT_settings,
    CYCLES_MATERIAL_PT_settings_surface,
    CYCLES_MATERIAL_PT_settings_volume,
    CYCLES_RENDER_PT_bake,
    CYCLES_RENDER_PT_debug,
    CYCLES_SCENE_PT_simplify,
    CYCLES_SCENE_PT_simplify_viewport,
    CYCLES_SCENE_PT_simplify_render,
    CYCLES_SCENE_PT_simplify_culling,
)


def register():
    from bpy.utils import register_class

    bpy.types.RENDER_PT_context.append(draw_device)
    bpy.types.VIEW3D_HT_header.append(draw_pause)

    for panel in get_panels():
        panel.COMPAT_ENGINES.add('CYCLES')

    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class

    bpy.types.RENDER_PT_context.remove(draw_device)
    bpy.types.VIEW3D_HT_header.remove(draw_pause)

    for panel in get_panels():
        if 'CYCLES' in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove('CYCLES')

    for cls in classes:
        unregister_class(cls)
