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
import _cycles

from bpy.types import (
    Panel,
    Menu,
    Operator,
)


class CYCLES_MT_sampling_presets(Menu):
    bl_label = "Sampling Presets"
    preset_subdir = "cycles/sampling"
    preset_operator = "script.execute_preset"
    COMPAT_ENGINES = {'CYCLES'}
    draw = Menu.draw_preset


class CYCLES_MT_integrator_presets(Menu):
    bl_label = "Integrator Presets"
    preset_subdir = "cycles/integrator"
    preset_operator = "script.execute_preset"
    COMPAT_ENGINES = {'CYCLES'}
    draw = Menu.draw_preset


class CyclesButtonsPanel:
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.engine in cls.COMPAT_ENGINES


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

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        row = layout.row(align=True)
        row.menu("CYCLES_MT_sampling_presets", text=bpy.types.CYCLES_MT_sampling_presets.bl_label)
        row.operator("render.cycles_sampling_preset_add", text="", icon="ZOOMIN")
        row.operator("render.cycles_sampling_preset_add", text="", icon="ZOOMOUT").remove_active = True

        row = layout.row()
        sub = row.row()
        sub.prop(cscene, "progressive", text="")
        row.prop(cscene, "use_square_samples")

        split = layout.split()

        col = split.column()
        sub = col.column(align=True)
        sub.label("Settings:")

        seed_sub = sub.row(align=True)
        seed_sub.prop(cscene, "seed")
        seed_sub.prop(cscene, "use_animated_seed", text="", icon="TIME")

        sub.prop(cscene, "sample_clamp_direct")
        sub.prop(cscene, "sample_clamp_indirect")
        sub.prop(cscene, "light_sampling_threshold")

        if cscene.progressive == 'PATH' or use_branched_path(context) is False:
            col = split.column()
            sub = col.column(align=True)
            sub.label(text="Samples:")
            sub.prop(cscene, "samples", text="Render")
            sub.prop(cscene, "preview_samples", text="Preview")
        else:
            sub.label(text="AA Samples:")
            sub.prop(cscene, "aa_samples", text="Render")
            sub.prop(cscene, "preview_aa_samples", text="Preview")

            col = split.column()
            sub = col.column(align=True)
            sub.label(text="Samples:")
            sub.prop(cscene, "diffuse_samples", text="Diffuse")
            sub.prop(cscene, "glossy_samples", text="Glossy")
            sub.prop(cscene, "transmission_samples", text="Transmission")
            sub.prop(cscene, "ao_samples", text="AO")

            subsub = sub.row(align=True)
            subsub.active = use_sample_all_lights(context)
            subsub.prop(cscene, "mesh_light_samples", text="Mesh Light")

            sub.prop(cscene, "subsurface_samples", text="Subsurface")
            sub.prop(cscene, "volume_samples", text="Volume")

            col = layout.column(align=True)
            col.prop(cscene, "sample_all_lights_direct")
            col.prop(cscene, "sample_all_lights_indirect")

        layout.row().prop(cscene, "sampling_pattern", text="Pattern")

        for rl in scene.render.layers:
            if rl.samples > 0:
                layout.separator()
                layout.row().prop(cscene, "use_layer_samples")
                break

        draw_samples_info(layout, context)


class CYCLES_RENDER_PT_geometry(CyclesButtonsPanel, Panel):
    bl_label = "Geometry"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles
        ccscene = scene.cycles_curves

        row = layout.row()
        row.label("Volume Sampling:")
        row = layout.row()
        row.prop(cscene, "volume_step_size")
        row.prop(cscene, "volume_max_steps")

        layout.separator()

        if cscene.feature_set == 'EXPERIMENTAL':
            layout.label("Subdivision Rate:")
            split = layout.split()

            col = split.column()
            sub = col.column(align=True)
            sub.prop(cscene, "dicing_rate", text="Render")
            sub.prop(cscene, "preview_dicing_rate", text="Preview")

            col = split.column()
            col.prop(cscene, "offscreen_dicing_scale", text="Offscreen Scale")
            col.prop(cscene, "max_subdivisions")

            layout.prop(cscene, "dicing_camera")

            layout.separator()

        layout.label("Hair:")
        layout.prop(ccscene, "use_curves", text="Use Hair")
        col = layout.column()
        col.active = ccscene.use_curves

        col.prop(ccscene, "primitive", text="Primitive")
        col.prop(ccscene, "shape", text="Shape")

        if not (ccscene.primitive in {'CURVE_SEGMENTS', 'LINE_SEGMENTS'} and ccscene.shape == 'RIBBONS'):
            col.prop(ccscene, "cull_backfacing", text="Cull back-faces")

        if ccscene.primitive == 'TRIANGLES' and ccscene.shape == 'THICK':
            col.prop(ccscene, "resolution", text="Resolution")
        elif ccscene.primitive == 'CURVE_SEGMENTS':
            col.prop(ccscene, "subdivisions", text="Curve subdivisions")

        row = col.row()
        row.prop(ccscene, "minimum_width", text="Min Pixels")
        row.prop(ccscene, "maximum_width", text="Max Extension")


class CYCLES_RENDER_PT_light_paths(CyclesButtonsPanel, Panel):
    bl_label = "Light Paths"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        row = layout.row(align=True)
        row.menu("CYCLES_MT_integrator_presets", text=bpy.types.CYCLES_MT_integrator_presets.bl_label)
        row.operator("render.cycles_integrator_preset_add", text="", icon="ZOOMIN")
        row.operator("render.cycles_integrator_preset_add", text="", icon="ZOOMOUT").remove_active = True

        split = layout.split()

        col = split.column()

        sub = col.column(align=True)
        sub.label("Transparency:")
        sub.prop(cscene, "transparent_max_bounces", text="Max")

        col.separator()

        col.prop(cscene, "caustics_reflective")
        col.prop(cscene, "caustics_refractive")
        col.prop(cscene, "blur_glossy")

        col = split.column()

        sub = col.column(align=True)
        sub.label(text="Bounces:")
        sub.prop(cscene, "max_bounces", text="Max")

        sub = col.column(align=True)
        sub.prop(cscene, "diffuse_bounces", text="Diffuse")
        sub.prop(cscene, "glossy_bounces", text="Glossy")
        sub.prop(cscene, "transmission_bounces", text="Transmission")
        sub.prop(cscene, "volume_bounces", text="Volume")


class CYCLES_RENDER_PT_motion_blur(CyclesButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles
        rd = scene.render
        layout.active = rd.use_motion_blur

        col = layout.column()
        col.prop(cscene, "motion_blur_position", text="Position")
        col.prop(rd, "motion_blur_shutter")

        col = layout.column()
        col.label("Shutter curve:")
        col.template_curve_mapping(rd, "motion_blur_shutter_curve")

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("render.shutter_curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("render.shutter_curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("render.shutter_curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("render.shutter_curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("render.shutter_curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("render.shutter_curve_preset", icon='NOCURVE', text="").shape = 'MAX'

        col = layout.column()
        col.prop(cscene, "rolling_shutter_type")
        row = col.row()
        row.active = cscene.rolling_shutter_type != 'NONE'
        row.prop(cscene, "rolling_shutter_duration")


class CYCLES_RENDER_PT_film(CyclesButtonsPanel, Panel):
    bl_label = "Film"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        split = layout.split()

        col = split.column()
        col.prop(cscene, "film_exposure")
        col.separator()
        sub = col.column(align=True)
        sub.prop(cscene, "pixel_filter_type", text="")
        if cscene.pixel_filter_type != 'BOX':
            sub.prop(cscene, "filter_width", text="Width")

        col = split.column()
        col.prop(cscene, "film_transparent")
        sub = col.row()
        sub.prop(cscene, "film_transparent_glass", text="Transparent Glass")
        sub.active = cscene.film_transparent
        sub = col.row()
        sub.prop(cscene, "film_transparent_roughness", text="Roughness Threshold")
        sub.active = cscene.film_transparent and cscene.film_transparent_glass


class CYCLES_RENDER_PT_performance(CyclesButtonsPanel, Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        split = layout.split()

        col = split.column(align=True)

        col.label(text="Threads:")
        col.row(align=True).prop(rd, "threads_mode", expand=True)
        sub = col.column(align=True)
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")

        col.separator()

        sub = col.column(align=True)
        sub.label(text="Tiles:")
        sub.prop(cscene, "tile_order", text="")

        sub.prop(rd, "tile_x", text="X")
        sub.prop(rd, "tile_y", text="Y")

        subsub = sub.column()
        subsub.active = not rd.use_save_buffers
        for rl in rd.layers:
            if rl.cycles.use_denoising:
                subsub.active = False
        subsub.prop(cscene, "use_progressive_refine")

        col = split.column()

        col.label(text="Final Render:")
        col.prop(rd, "use_save_buffers")
        col.prop(rd, "use_persistent_data", text="Persistent Images")

        col.separator()

        col.label(text="Acceleration structure:")
        if _cycles.with_embree:
            row = col.row()
            row.active = use_cpu(context)
            row.prop(cscene, "use_bvh_embree")
        row = col.row()
        col.prop(cscene, "debug_use_spatial_splits")
        row = col.row()
        row.active = not cscene.use_bvh_embree or not _cycles.with_embree
        row.prop(cscene, "debug_use_hair_bvh")

        row = col.row()
        row.active = not cscene.debug_use_spatial_splits and not cscene.use_bvh_embree
        row.prop(cscene, "debug_bvh_time_steps")

        col = layout.column()
        col.label(text="Viewport Resolution:")
        split = col.split()
        split.prop(rd, "preview_pixel_size", text="")
        split.prop(cscene, "preview_start_resolution")


class CYCLES_RENDER_PT_layer_options(CyclesButtonsPanel, Panel):
    bl_label = "Layer"
    bl_context = "render_layer"

    def draw(self, context):
        layout = self.layout
        with_freestyle = bpy.app.build_options.freestyle

        scene = context.scene
        rd = scene.render
        rl = rd.layers.active

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text="Scene")
        col.prop(rl, "layers_exclude", text="Exclude")

        col = split.column()
        col.prop(rl, "layers", text="Layer")
        col.prop(rl, "layers_zmask", text="Mask Layer")

        split = layout.split()

        col = split.column()
        col.label(text="Material:")
        col.prop(rl, "material_override", text="")
        col.separator()
        col.prop(rl, "samples")

        col = split.column()
        col.prop(rl, "use_sky", "Use Environment")
        col.prop(rl, "use_ao", "Use AO")
        col.prop(rl, "use_solid", "Use Surfaces")
        col.prop(rl, "use_strand", "Use Hair")
        if with_freestyle:
            row = col.row()
            row.prop(rl, "use_freestyle", "Use Freestyle")
            row.active = rd.use_freestyle


class CYCLES_RENDER_PT_layer_passes(CyclesButtonsPanel, Panel):
    bl_label = "Passes"
    bl_context = "render_layer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rl = rd.layers.active
        crl = rl.cycles

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_pass_combined")
        col.prop(rl, "use_pass_z")
        col.prop(rl, "use_pass_mist")
        col.prop(rl, "use_pass_normal")
        row = col.row()
        row.prop(rl, "use_pass_vector")
        row.active = not rd.use_motion_blur
        col.prop(rl, "use_pass_uv")
        col.prop(rl, "use_pass_object_index")
        col.prop(rl, "use_pass_material_index")
        col.separator()
        col.prop(rl, "use_pass_shadow")
        col.prop(rl, "use_pass_ambient_occlusion")
        col.separator()
        col.prop(crl, "denoising_store_passes", text="Denoising Data")
        col.separator()
        col.prop(rl, "pass_alpha_threshold")

        col = split.column()
        col.label(text="Diffuse:")
        row = col.row(align=True)
        row.prop(rl, "use_pass_diffuse_direct", text="Direct", toggle=True)
        row.prop(rl, "use_pass_diffuse_indirect", text="Indirect", toggle=True)
        row.prop(rl, "use_pass_diffuse_color", text="Color", toggle=True)
        col.label(text="Glossy:")
        row = col.row(align=True)
        row.prop(rl, "use_pass_glossy_direct", text="Direct", toggle=True)
        row.prop(rl, "use_pass_glossy_indirect", text="Indirect", toggle=True)
        row.prop(rl, "use_pass_glossy_color", text="Color", toggle=True)
        col.label(text="Transmission:")
        row = col.row(align=True)
        row.prop(rl, "use_pass_transmission_direct", text="Direct", toggle=True)
        row.prop(rl, "use_pass_transmission_indirect", text="Indirect", toggle=True)
        row.prop(rl, "use_pass_transmission_color", text="Color", toggle=True)
        col.label(text="Subsurface:")
        row = col.row(align=True)
        row.prop(rl, "use_pass_subsurface_direct", text="Direct", toggle=True)
        row.prop(rl, "use_pass_subsurface_indirect", text="Indirect", toggle=True)
        row.prop(rl, "use_pass_subsurface_color", text="Color", toggle=True)
        col.label(text="Volume:")
        row = col.row(align=True)
        row.prop(crl, "use_pass_volume_direct", text="Direct", toggle=True)
        row.prop(crl, "use_pass_volume_indirect", text="Indirect", toggle=True)

        col.separator()
        col.prop(rl, "use_pass_emit", text="Emission")
        col.prop(rl, "use_pass_environment")

        col = layout.column()
        col.prop(crl, "pass_debug_render_time")
        if _cycles.with_cycles_debug:
            col.prop(crl, "pass_debug_bvh_traversed_nodes")
            col.prop(crl, "pass_debug_bvh_traversed_instances")
            col.prop(crl, "pass_debug_bvh_intersections")
            col.prop(crl, "pass_debug_ray_bounces")

        crl = rl.cycles
        layout.label("Cryptomatte:")
        row = layout.row(align=True)
        row.prop(crl, "use_pass_crypto_object", text="Object", toggle=True)
        row.prop(crl, "use_pass_crypto_material", text="Material", toggle=True)
        row.prop(crl, "use_pass_crypto_asset", text="Asset", toggle=True)
        row = layout.row(align=True)
        row.prop(crl, "pass_crypto_depth")
        row = layout.row(align=True)
        row.active = use_cpu(context)
        row.prop(crl, "pass_crypto_accurate", text="Accurate Mode")

class CYCLES_RENDER_PT_views(CyclesButtonsPanel, Panel):
    bl_label = "Views"
    bl_context = "render_layer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_multiview", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rv = rd.views.active

        layout.active = rd.use_multiview
        basic_stereo = (rd.views_format == 'STEREO_3D')

        row = layout.row()
        row.prop(rd, "views_format", expand=True)

        if basic_stereo:
            row = layout.row()
            row.template_list("RENDERLAYER_UL_renderviews", "name", rd, "stereo_views", rd.views, "active_index", rows=2)

            row = layout.row()
            row.label(text="File Suffix:")
            row.prop(rv, "file_suffix", text="")

        else:
            row = layout.row()
            row.template_list("RENDERLAYER_UL_renderviews", "name", rd, "views", rd.views, "active_index", rows=2)

            col = row.column(align=True)
            col.operator("scene.render_view_add", icon='ZOOMIN', text="")
            col.operator("scene.render_view_remove", icon='ZOOMOUT', text="")

            row = layout.row()
            row.label(text="Camera Suffix:")
            row.prop(rv, "camera_suffix", text="")


class CYCLES_RENDER_PT_denoising(CyclesButtonsPanel, Panel):
    bl_label = "Denoising"
    bl_context = "render_layer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        rd = context.scene.render
        rl = rd.layers.active
        crl = rl.cycles
        cscene = context.scene.cycles
        layout = self.layout

        layout.prop(crl, "use_denoising", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles
        rd = scene.render
        rl = rd.layers.active
        crl = rl.cycles

        split = layout.split()
        split.active = crl.use_denoising

        col = split.column()
        sub = col.column(align=True)
        sub.prop(crl, "denoising_radius", text="Radius")
        sub.prop(crl, "denoising_strength", slider=True, text="Strength")

        col = split.column()
        sub = col.column(align=True)
        sub.prop(crl, "denoising_feature_strength", slider=True, text="Feature Strength")
        sub.prop(crl, "denoising_relative_pca")

        layout.separator()

        row = layout.row()
        row.active = crl.use_denoising or crl.denoising_store_passes
        row.label(text="Diffuse:")
        sub = row.row(align=True)
        sub.prop(crl, "denoising_diffuse_direct", text="Direct", toggle=True)
        sub.prop(crl, "denoising_diffuse_indirect", text="Indirect", toggle=True)

        row = layout.row()
        row.active = crl.use_denoising or crl.denoising_store_passes
        row.label(text="Glossy:")
        sub = row.row(align=True)
        sub.prop(crl, "denoising_glossy_direct", text="Direct", toggle=True)
        sub.prop(crl, "denoising_glossy_indirect", text="Indirect", toggle=True)

        row = layout.row()
        row.active = crl.use_denoising or crl.denoising_store_passes
        row.label(text="Transmission:")
        sub = row.row(align=True)
        sub.prop(crl, "denoising_transmission_direct", text="Direct", toggle=True)
        sub.prop(crl, "denoising_transmission_indirect", text="Indirect", toggle=True)

        row = layout.row()
        row.active = crl.use_denoising or crl.denoising_store_passes
        row.label(text="Subsurface:")
        sub = row.row(align=True)
        sub.prop(crl, "denoising_subsurface_direct", text="Direct", toggle=True)
        sub.prop(crl, "denoising_subsurface_indirect", text="Indirect", toggle=True)


class CYCLES_PT_post_processing(CyclesButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        col = split.column()
        col.prop(rd, "dither_intensity", text="Dither", slider=True)


class CYCLES_CAMERA_PT_dof(CyclesButtonsPanel, Panel):
    bl_label = "Depth of Field"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.camera and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        cam = context.camera
        ccam = cam.cycles
        dof_options = cam.gpu_dof

        split = layout.split()

        col = split.column()
        col.label("Focus:")
        col.prop(cam, "dof_object", text="")

        sub = col.row()
        sub.active = cam.dof_object is None
        sub.prop(cam, "dof_distance", text="Distance")

        hq_support = dof_options.is_hq_supported
        sub = col.column(align=True)
        sub.label("Viewport:")
        subhq = sub.column()
        subhq.active = hq_support
        subhq.prop(dof_options, "use_high_quality")
        sub.prop(dof_options, "fstop")
        if dof_options.use_high_quality and hq_support:
            sub.prop(dof_options, "blades")

        col = split.column()

        col.label("Aperture:")
        sub = col.column(align=True)
        sub.prop(ccam, "aperture_type", text="")
        if ccam.aperture_type == 'RADIUS':
            sub.prop(ccam, "aperture_size", text="Size")
        elif ccam.aperture_type == 'FSTOP':
            sub.prop(ccam, "aperture_fstop", text="Number")

        sub = col.column(align=True)
        sub.prop(ccam, "aperture_blades", text="Blades")
        sub.prop(ccam, "aperture_rotation", text="Rotation")
        sub.prop(ccam, "aperture_ratio", text="Ratio")


class CYCLES_PT_context_material(CyclesButtonsPanel, Panel):
    bl_label = ""
    bl_context = "material"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
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
            if ob.dupli_type == 'GROUP' and ob.dupli_group:
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
                ob and ((ob.type in {'MESH', 'CURVE', 'SURFACE', 'FONT', 'META', 'LAMP'}) or
                        (ob.dupli_type == 'GROUP' and ob.dupli_group)))

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

        if ob.type != 'LAMP':
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
    """Enable nodes on a material, world or lamp"""
    bl_idname = "cycles.use_shading_nodes"
    bl_label = "Use Nodes"

    @classmethod
    def poll(cls, context):
        return (getattr(context, "material", False) or getattr(context, "world", False) or
                getattr(context, "lamp", False))

    def execute(self, context):
        if context.material:
            context.material.use_nodes = True
        elif context.world:
            context.world.use_nodes = True
        elif context.lamp:
            context.lamp.use_nodes = True

        return {'FINISHED'}


def find_node(material, nodetype):
    if material and material.node_tree:
        ntree = material.node_tree

        active_output_node = None
        for node in ntree.nodes:
            if getattr(node, "type", None) == nodetype:
                if getattr(node, "is_active_output", True):
                    return node
                if not active_output_node:
                    active_output_node = node
        return active_output_node

    return None


def find_node_input(node, name):
    for input in node.inputs:
        if input.name == name:
            return input

    return None


def panel_node_draw(layout, id_data, output_type, input_name):
    if not id_data.use_nodes:
        layout.operator("cycles.use_shading_nodes", icon='NODETREE')
        return False

    ntree = id_data.node_tree

    node = find_node(id_data, output_type)
    if not node:
        layout.label(text="No output node")
    else:
        input = find_node_input(node, input_name)
        layout.template_node_view(ntree, node, input)

    return True


class CYCLES_LAMP_PT_preview(CyclesButtonsPanel, Panel):
    bl_label = "Preview"
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.lamp and \
            not (context.lamp.type == 'AREA' and
                 context.lamp.cycles.is_portal) \
            and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.lamp)


class CYCLES_LAMP_PT_lamp(CyclesButtonsPanel, Panel):
    bl_label = "Lamp"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.lamp and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        clamp = lamp.cycles
        # cscene = context.scene.cycles

        layout.prop(lamp, "type", expand=True)

        split = layout.split()
        col = split.column(align=True)

        if lamp.type in {'POINT', 'SUN', 'SPOT'}:
            col.prop(lamp, "shadow_soft_size", text="Size")
        elif lamp.type == 'AREA':
            col.prop(lamp, "shape", text="")
            sub = col.column(align=True)

            if lamp.shape == 'SQUARE':
                sub.prop(lamp, "size")
            elif lamp.shape == 'RECTANGLE':
                sub.prop(lamp, "size", text="Size X")
                sub.prop(lamp, "size_y", text="Size Y")

        if not (lamp.type == 'AREA' and clamp.is_portal):
            sub = col.column(align=True)
            if use_branched_path(context):
                subsub = sub.row(align=True)
                subsub.active = use_sample_all_lights(context)
                subsub.prop(clamp, "samples")
            sub.prop(clamp, "max_bounces")

        col = split.column()

        sub = col.column(align=True)
        sub.active = not (lamp.type == 'AREA' and clamp.is_portal)
        sub.prop(clamp, "cast_shadow")
        sub.prop(clamp, "use_multiple_importance_sampling", text="Multiple Importance")

        if lamp.type == 'AREA':
            col.prop(clamp, "is_portal", text="Portal")

        if lamp.type == 'HEMI':
            layout.label(text="Not supported, interpreted as sun lamp")


class CYCLES_LAMP_PT_nodes(CyclesButtonsPanel, Panel):
    bl_label = "Nodes"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.lamp and not (context.lamp.type == 'AREA' and
                                     context.lamp.cycles.is_portal) and \
            CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp
        if not panel_node_draw(layout, lamp, 'OUTPUT_LAMP', 'Surface'):
            layout.prop(lamp, "color")


class CYCLES_LAMP_PT_spot(CyclesButtonsPanel, Panel):
    bl_label = "Spot Shape"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        return (lamp and lamp.type == 'SPOT') and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.prop(lamp, "spot_size", text="Size")
        sub.prop(lamp, "spot_blend", text="Blend", slider=True)

        col = split.column()
        col.prop(lamp, "show_cone")


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
            layout.prop(world, "horizon_color", text="Color")


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

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw_header(self, context):
        light = context.world.light_settings
        self.layout.prop(light, "use_ambient_occlusion", text="")

    def draw(self, context):
        layout = self.layout

        light = context.world.light_settings
        scene = context.scene

        row = layout.row()
        sub = row.row()
        sub.active = light.use_ambient_occlusion or scene.render.use_simplify
        sub.prop(light, "ao_factor", text="Factor")
        row.prop(light, "distance", text="Distance")


class CYCLES_WORLD_PT_mist(CyclesButtonsPanel, Panel):
    bl_label = "Mist Pass"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CyclesButtonsPanel.poll(context):
            if context.world:
                for rl in context.scene.render.layers:
                    if rl.use_pass_mist:
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

        world = context.world
        cworld = world.cycles
        # cscene = context.scene.cycles

        split = layout.split()

        col = split.column()

        col.label(text="Surface:")
        col.prop(cworld, "sampling_method", text="Sampling")

        sub = col.column(align=True)
        sub.active = cworld.sampling_method != 'NONE'
        subsub = sub.row(align=True)
        subsub.active = cworld.sampling_method == 'MANUAL'
        subsub.prop(cworld, "sample_map_resolution")
        if use_branched_path(context):
            subsub = sub.row(align=True)
            subsub.active = use_sample_all_lights(context)
            subsub.prop(cworld, "samples")
        sub.prop(cworld, "max_bounces")

        col = split.column()
        col.label(text="Volume:")
        sub = col.column()
        sub.active = use_cpu(context)
        sub.prop(cworld, "volume_sampling", text="")
        col.prop(cworld, "volume_interpolation", text="")
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

        mat = context.material
        cmat = mat.cycles

        split = layout.split()
        col = split.column()
        col.label(text="Surface:")
        col.prop(cmat, "sample_as_light", text="Multiple Importance")
        col.prop(cmat, "use_transparent_shadow")

        col.separator()
        col.label(text="Geometry:")
        col.prop(cmat, "displacement_method", text="")

        col = split.column()
        col.label(text="Volume:")
        sub = col.column()
        sub.active = use_cpu(context)
        sub.prop(cmat, "volume_sampling", text="")
        col.prop(cmat, "volume_interpolation", text="")
        col.prop(cmat, "homogeneous_volume", text="Homogeneous")

        col.separator()
        col.prop(mat, "pass_index")


class CYCLES_MATERIAL_PT_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.material and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        mat = context.material

        layout = self.layout
        split = layout.split()

        col = split.column(align=True)
        col.label("Color:")
        col.prop(mat, "diffuse_color", text="")
        col.prop(mat, "alpha")

        col.separator()
        col.label("Alpha:")
        col.prop(mat.game_settings, "alpha_blend", text="")

        col = split.column(align=True)
        col.label("Specular:")
        col.prop(mat, "specular_color", text="")
        col.prop(mat, "specular_hardness", text="Hardness")


class CYCLES_TEXTURE_PT_context(CyclesButtonsPanel, Panel):
    bl_label = ""
    bl_context = "texture"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        space = context.space_data
        pin_id = space.pin_id
        use_pin_id = space.use_pin_id
        user = context.texture_user

        space.use_limited_texture_context = False

        if not (use_pin_id and isinstance(pin_id, bpy.types.Texture)):
            pin_id = None

        if not pin_id:
            layout.template_texture_user()

        if user or pin_id:
            layout.separator()

            split = layout.split(percentage=0.65)
            col = split.column()

            if pin_id:
                col.template_ID(space, "pin_id")
            else:
                propname = context.texture_user_property.identifier
                col.template_ID(user, propname, new="texture.new")

            if tex:
                split = layout.split(percentage=0.2)
                split.label(text="Type:")
                split.prop(tex, "type", text="")


class CYCLES_TEXTURE_PT_node(CyclesButtonsPanel, Panel):
    bl_label = "Node"
    bl_context = "texture"

    @classmethod
    def poll(cls, context):
        node = context.texture_node
        return node and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        node = context.texture_node
        ntree = node.id_data
        layout.template_node_view(ntree, node, None)


class CYCLES_TEXTURE_PT_mapping(CyclesButtonsPanel, Panel):
    bl_label = "Mapping"
    bl_context = "texture"

    @classmethod
    def poll(cls, context):
        node = context.texture_node
        # TODO(sergey): perform a faster/nicer check?
        return node and hasattr(node, 'texture_mapping') and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        node = context.texture_node

        mapping = node.texture_mapping

        layout.prop(mapping, "vector_type", expand=True)

        row = layout.row()

        row.column().prop(mapping, "translation")
        row.column().prop(mapping, "rotation")
        row.column().prop(mapping, "scale")

        layout.label(text="Projection:")

        row = layout.row()
        row.prop(mapping, "mapping_x", text="")
        row.prop(mapping, "mapping_y", text="")
        row.prop(mapping, "mapping_z", text="")


class CYCLES_TEXTURE_PT_colors(CyclesButtonsPanel, Panel):
    bl_label = "Color"
    bl_context = "texture"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        # node = context.texture_node
        return False
        # return node and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        node = context.texture_node

        mapping = node.color_mapping

        split = layout.split()

        col = split.column()
        col.label(text="Blend:")
        col.prop(mapping, "blend_type", text="")
        col.prop(mapping, "blend_factor", text="Factor")
        col.prop(mapping, "blend_color", text="")

        col = split.column()
        col.label(text="Adjust:")
        col.prop(mapping, "brightness")
        col.prop(mapping, "contrast")
        col.prop(mapping, "saturation")

        layout.separator()

        layout.prop(mapping, "use_color_ramp", text="Ramp")
        if mapping.use_color_ramp:
            layout.template_color_ramp(mapping, "color_ramp", expand=True)


class CYCLES_PARTICLE_PT_textures(CyclesButtonsPanel, Panel):
    bl_label = "Textures"
    bl_context = "particle"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        psys = context.particle_system
        return psys and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings

        row = layout.row()
        row.template_list("TEXTURE_UL_texslots", "", part, "texture_slots", part, "active_texture_index", rows=2)

        col = row.column(align=True)
        col.operator("texture.slot_move", text="", icon='TRIA_UP').type = 'UP'
        col.operator("texture.slot_move", text="", icon='TRIA_DOWN').type = 'DOWN'
        col.menu("TEXTURE_MT_specials", icon='DOWNARROW_HLT', text="")

        if not part.active_texture:
            layout.template_ID(part, "active_texture", new="texture.new")
        else:
            slot = part.texture_slots[part.active_texture_index]
            layout.template_ID(slot, "texture", new="texture.new")


class CYCLES_RENDER_PT_bake(CyclesButtonsPanel, Panel):
    bl_label = "Bake"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles
        cbk = scene.render.bake

        layout.operator("object.bake", icon='RENDER_STILL').type = cscene.bake_type

        col = layout.column()
        col.prop(cscene, "bake_type")

        col = layout.column()

        if cscene.bake_type == 'NORMAL':
            col.prop(cbk, "normal_space", text="Space")

            row = col.row(align=True)
            row.label(text="Swizzle:")
            row.prop(cbk, "normal_r", text="")
            row.prop(cbk, "normal_g", text="")
            row.prop(cbk, "normal_b", text="")

        elif cscene.bake_type == 'COMBINED':
            row = col.row(align=True)
            row.prop(cbk, "use_pass_direct", toggle=True)
            row.prop(cbk, "use_pass_indirect", toggle=True)

            split = col.split()
            split.active = cbk.use_pass_direct or cbk.use_pass_indirect

            col = split.column()
            col.prop(cbk, "use_pass_diffuse")
            col.prop(cbk, "use_pass_glossy")
            col.prop(cbk, "use_pass_transmission")

            col = split.column()
            col.prop(cbk, "use_pass_subsurface")
            col.prop(cbk, "use_pass_ambient_occlusion")
            col.prop(cbk, "use_pass_emit")

        elif cscene.bake_type in {'DIFFUSE', 'GLOSSY', 'TRANSMISSION', 'SUBSURFACE'}:
            row = col.row(align=True)
            row.prop(cbk, "use_pass_direct", toggle=True)
            row.prop(cbk, "use_pass_indirect", toggle=True)
            row.prop(cbk, "use_pass_color", toggle=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(cbk, "margin")
        col.prop(cbk, "use_clear")

        col = split.column()
        col.prop(cbk, "use_selected_to_active")
        sub = col.column()
        sub.active = cbk.use_selected_to_active
        sub.prop(cbk, "use_cage", text="Cage")
        if cbk.use_cage:
            sub.prop(cbk, "cage_extrusion", text="Extrusion")
            sub.prop_search(cbk, "cage_object", scene, "objects", text="")
        else:
            sub.prop(cbk, "cage_extrusion", text="Ray Distance")


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


class CYCLES_PARTICLE_PT_curve_settings(CyclesButtonsPanel, Panel):
    bl_label = "Cycles Hair Settings"
    bl_context = "particle"

    @classmethod
    def poll(cls, context):
        scene = context.scene
        ccscene = scene.cycles_curves
        psys = context.particle_system
        use_curves = ccscene.use_curves and psys
        return CyclesButtonsPanel.poll(context) and use_curves and psys.settings.type == 'HAIR'

    def draw(self, context):
        layout = self.layout

        psys = context.particle_settings
        cpsys = psys.cycles

        row = layout.row()
        row.prop(cpsys, "shape", text="Shape")

        layout.label(text="Thickness:")
        row = layout.row()
        row.prop(cpsys, "root_width", text="Root")
        row.prop(cpsys, "tip_width", text="Tip")

        row = layout.row()
        row.prop(cpsys, "radius_scale", text="Scaling")
        row.prop(cpsys, "use_closetip", text="Close tip")


class CYCLES_SCENE_PT_simplify(CyclesButtonsPanel, Panel):
    bl_label = "Simplify"
    bl_context = "scene"
    COMPAT_ENGINES = {'CYCLES'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        col = layout.column(align=True)
        col.label(text="Subdivision")
        row = col.row(align=True)
        row.prop(rd, "simplify_subdivision", text="Viewport")
        row.prop(rd, "simplify_subdivision_render", text="Render")

        col = layout.column(align=True)
        col.label(text="Child Particles")
        row = col.row(align=True)
        row.prop(rd, "simplify_child_particles", text="Viewport")
        row.prop(rd, "simplify_child_particles_render", text="Render")

        col = layout.column(align=True)
        split = col.split()
        sub = split.column()
        sub.label(text="Texture Limit Viewport")
        sub.prop(cscene, "texture_limit", text="")
        sub = split.column()
        sub.label(text="Texture Limit Render")
        sub.prop(cscene, "texture_limit_render", text="")

        split = layout.split()
        col = split.column()
        col.prop(cscene, "use_camera_cull")
        row = col.row()
        row.active = cscene.use_camera_cull
        row.prop(cscene, "camera_cull_margin")

        col = split.column()
        col.prop(cscene, "use_distance_cull")
        row = col.row()
        row.active = cscene.use_distance_cull
        row.prop(cscene, "distance_cull_margin", text="Distance")

        split = layout.split()
        col = split.column()
        col.prop(cscene, "ao_bounces")

        col = split.column()
        col.prop(cscene, "ao_bounces_render")


def draw_device(self, context):
    scene = context.scene
    layout = self.layout

    if scene.render.engine == 'CYCLES':
        from . import engine
        cscene = scene.cycles

        layout.prop(cscene, "feature_set")

        split = layout.split(percentage=1 / 3)
        split.label("Device:")
        row = split.row()
        row.active = show_device_active(context)
        row.prop(cscene, "device", text="")

        if engine.with_osl() and use_cpu(context):
            layout.prop(cscene, "shading_system")


def draw_pause(self, context):
    layout = self.layout
    scene = context.scene

    if scene.render.engine == "CYCLES":
        view = context.space_data

        if view.viewport_shade == 'RENDERED':
            cscene = scene.cycles
            layername = scene.render.layers.active.name
            layout.prop(cscene, "preview_pause", icon="PAUSE", text="")
            layout.prop(cscene, "preview_active_layer", icon="RENDERLAYERS", text=layername)


def get_panels():
    exclude_panels = {
        'DATA_PT_area',
        'DATA_PT_camera_dof',
        'DATA_PT_falloff_curve',
        'DATA_PT_lamp',
        'DATA_PT_preview',
        'DATA_PT_shadow',
        'DATA_PT_spot',
        'DATA_PT_sunsky',
        'MATERIAL_PT_context_material',
        'MATERIAL_PT_diffuse',
        'MATERIAL_PT_flare',
        'MATERIAL_PT_halo',
        'MATERIAL_PT_mirror',
        'MATERIAL_PT_options',
        'MATERIAL_PT_pipeline',
        'MATERIAL_PT_preview',
        'MATERIAL_PT_shading',
        'MATERIAL_PT_shadow',
        'MATERIAL_PT_specular',
        'MATERIAL_PT_sss',
        'MATERIAL_PT_strand',
        'MATERIAL_PT_transp',
        'MATERIAL_PT_volume_density',
        'MATERIAL_PT_volume_integration',
        'MATERIAL_PT_volume_lighting',
        'MATERIAL_PT_volume_options',
        'MATERIAL_PT_volume_shading',
        'MATERIAL_PT_volume_transp',
        'RENDERLAYER_PT_layer_options',
        'RENDERLAYER_PT_layer_passes',
        'RENDERLAYER_PT_views',
        'RENDER_PT_antialiasing',
        'RENDER_PT_bake',
        'RENDER_PT_motion_blur',
        'RENDER_PT_performance',
        'RENDER_PT_post_processing',
        'RENDER_PT_shading',
        'SCENE_PT_simplify',
        'TEXTURE_PT_context_texture',
        'WORLD_PT_ambient_occlusion',
        'WORLD_PT_environment_lighting',
        'WORLD_PT_gather',
        'WORLD_PT_indirect_lighting',
        'WORLD_PT_mist',
        'WORLD_PT_preview',
        'WORLD_PT_world'
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
    CYCLES_RENDER_PT_geometry,
    CYCLES_RENDER_PT_light_paths,
    CYCLES_RENDER_PT_motion_blur,
    CYCLES_RENDER_PT_film,
    CYCLES_RENDER_PT_performance,
    CYCLES_RENDER_PT_layer_options,
    CYCLES_RENDER_PT_layer_passes,
    CYCLES_RENDER_PT_views,
    CYCLES_RENDER_PT_denoising,
    CYCLES_PT_post_processing,
    CYCLES_CAMERA_PT_dof,
    CYCLES_PT_context_material,
    CYCLES_OBJECT_PT_motion_blur,
    CYCLES_OBJECT_PT_cycles_settings,
    CYCLES_OT_use_shading_nodes,
    CYCLES_LAMP_PT_preview,
    CYCLES_LAMP_PT_lamp,
    CYCLES_LAMP_PT_nodes,
    CYCLES_LAMP_PT_spot,
    CYCLES_WORLD_PT_preview,
    CYCLES_WORLD_PT_surface,
    CYCLES_WORLD_PT_volume,
    CYCLES_WORLD_PT_ambient_occlusion,
    CYCLES_WORLD_PT_mist,
    CYCLES_WORLD_PT_ray_visibility,
    CYCLES_WORLD_PT_settings,
    CYCLES_MATERIAL_PT_preview,
    CYCLES_MATERIAL_PT_surface,
    CYCLES_MATERIAL_PT_volume,
    CYCLES_MATERIAL_PT_displacement,
    CYCLES_MATERIAL_PT_settings,
    CYCLES_MATERIAL_PT_viewport,
    CYCLES_TEXTURE_PT_context,
    CYCLES_TEXTURE_PT_node,
    CYCLES_TEXTURE_PT_mapping,
    CYCLES_TEXTURE_PT_colors,
    CYCLES_PARTICLE_PT_textures,
    CYCLES_RENDER_PT_bake,
    CYCLES_RENDER_PT_debug,
    CYCLES_PARTICLE_PT_curve_settings,
    CYCLES_SCENE_PT_simplify,
)


def register():
    from bpy.utils import register_class

    bpy.types.RENDER_PT_render.append(draw_device)
    bpy.types.VIEW3D_HT_header.append(draw_pause)

    for panel in get_panels():
        panel.COMPAT_ENGINES.add('CYCLES')

    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class

    bpy.types.RENDER_PT_render.remove(draw_device)
    bpy.types.VIEW3D_HT_header.remove(draw_pause)

    for panel in get_panels():
        if 'CYCLES' in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove('CYCLES')

    for cls in classes:
        unregister_class(cls)
