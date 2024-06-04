# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import bpy
from bpy.app.translations import contexts as i18n_contexts
from bpy_extras.node_utils import find_node_input
from bl_ui.utils import PresetPanel

from bpy.types import Panel, Menu

from bl_ui.properties_grease_pencil_common import GreasePencilSimplifyPanel
from bl_ui.properties_render import draw_curves_settings, CompositorPerformanceButtonsPanel
from bl_ui.properties_view_layer import ViewLayerCryptomattePanel, ViewLayerAOVPanel, ViewLayerLightgroupsPanel


class CyclesPresetPanel(PresetPanel, Panel):
    COMPAT_ENGINES = {'CYCLES'}
    preset_operator = "script.execute_preset"

    @staticmethod
    def post_cb(context, _filepath):
        # Modify an arbitrary built-in scene property to force a depsgraph
        # update, because add-on properties don't. (see #62325)
        render = context.scene.render
        render.filter_size = render.filter_size


class CYCLES_PT_sampling_presets(CyclesPresetPanel):
    bl_label = "Sampling Presets"
    preset_subdir = "cycles/sampling"
    preset_add_operator = "render.cycles_sampling_preset_add"


class CYCLES_PT_viewport_sampling_presets(CyclesPresetPanel):
    bl_label = "Viewport Sampling Presets"
    preset_subdir = "cycles/viewport_sampling"
    preset_add_operator = "render.cycles_viewport_sampling_preset_add"


class CYCLES_PT_integrator_presets(CyclesPresetPanel):
    bl_label = "Integrator Presets"
    preset_subdir = "cycles/integrator"
    preset_add_operator = "render.cycles_integrator_preset_add"


class CYCLES_PT_performance_presets(CyclesPresetPanel):
    bl_label = "Performance Presets"
    preset_subdir = "cycles/performance"
    preset_add_operator = "render.cycles_performance_preset_add"


class CyclesButtonsPanel:
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        return context.engine in cls.COMPAT_ENGINES


class CyclesDebugButtonsPanel(CyclesButtonsPanel):
    @classmethod
    def poll(cls, context):
        prefs = bpy.context.preferences
        return (CyclesButtonsPanel.poll(context)
                and prefs.experimental.use_cycles_debug
                and prefs.view.show_developer_ui)


# Adapt properties editor panel to display in node editor. We have to
# copy the class rather than inherit due to the way bpy registration works.
def node_panel(cls):
    node_cls = type('NODE_' + cls.__name__, cls.__bases__, dict(cls.__dict__))

    node_cls.bl_space_type = 'NODE_EDITOR'
    node_cls.bl_region_type = 'UI'
    node_cls.bl_category = "Options"
    if hasattr(node_cls, 'bl_parent_id'):
        node_cls.bl_parent_id = 'NODE_' + node_cls.bl_parent_id

    return node_cls


def get_device_type(context):
    return context.preferences.addons[__package__].preferences.compute_device_type


def backend_has_active_gpu(context):
    return context.preferences.addons[__package__].preferences.has_active_device()


def use_cpu(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'NONE' or cscene.device == 'CPU' or not backend_has_active_gpu(context))


def use_metal(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'METAL' and cscene.device == 'GPU' and backend_has_active_gpu(context))


def use_cuda(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'CUDA' and cscene.device == 'GPU' and backend_has_active_gpu(context))


def use_hip(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'HIP' and cscene.device == 'GPU' and backend_has_active_gpu(context))


def use_optix(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'OPTIX' and cscene.device == 'GPU' and backend_has_active_gpu(context))


def use_oneapi(context):
    cscene = context.scene.cycles

    return (get_device_type(context) == 'ONEAPI' and cscene.device == 'GPU' and backend_has_active_gpu(context))


def use_multi_device(context):
    cscene = context.scene.cycles
    if cscene.device != 'GPU':
        return False
    return context.preferences.addons[__package__].preferences.has_multi_device()


def show_device_active(context):
    cscene = context.scene.cycles
    if cscene.device != 'GPU':
        return True
    return backend_has_active_gpu(context)


def show_preview_denoise_active(context):
    cscene = context.scene.cycles
    if not cscene.use_preview_denoising:
        return False

    if cscene.preview_denoiser == 'OPTIX':
        return has_optixdenoiser_gpu_devices(context)

    # OIDN is always available, thanks to CPU support
    return True


def show_denoise_active(context):
    cscene = context.scene.cycles
    if not cscene.use_denoising:
        return False

    if cscene.denoiser == 'OPTIX':
        return has_optixdenoiser_gpu_devices(context)

    # OIDN is always available, thanks to CPU support
    return True


def get_effective_preview_denoiser(context, has_oidn_gpu):
    scene = context.scene
    cscene = scene.cycles

    if cscene.preview_denoiser != "AUTO":
        return cscene.preview_denoiser

    if has_oidn_gpu:
        return 'OPENIMAGEDENOISE'

    if context.preferences.addons[__package__].preferences.get_devices_for_type('OPTIX'):
        return 'OPTIX'

    return 'OPENIMAGEDENOISE'


def has_oidn_gpu_devices(context):
    return context.preferences.addons[__package__].preferences.has_oidn_gpu_devices()


def has_optixdenoiser_gpu_devices(context):
    return context.preferences.addons[__package__].preferences.has_optixdenoiser_gpu_devices()


def use_mnee(context):
    # The MNEE kernel doesn't compile on macOS < 13.
    if use_metal(context):
        import platform
        version, _, _ = platform.mac_ver()
        major_version = version.split(".")[0]
        if int(major_version) < 13:
            return False
    return True


class CYCLES_RENDER_PT_sampling(CyclesButtonsPanel, Panel):
    bl_label = "Sampling"

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_sampling_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"

    def draw_header_preset(self, context):
        CYCLES_PT_viewport_sampling_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        layout.use_property_split = True
        layout.use_property_decorate = False

        heading = layout.column(align=True, heading="Noise Threshold")
        row = heading.row(align=True)
        row.prop(cscene, "use_preview_adaptive_sampling", text="")
        sub = row.row()
        sub.active = cscene.use_preview_adaptive_sampling
        sub.prop(cscene, "preview_adaptive_threshold", text="")

        if cscene.use_preview_adaptive_sampling:
            col = layout.column(align=True)
            col.prop(cscene, "preview_samples", text="Max Samples")
            col.prop(cscene, "preview_adaptive_min_samples", text="Min Samples")
        else:
            layout.prop(cscene, "preview_samples", text="Samples")


class CYCLES_RENDER_PT_sampling_viewport_denoise(CyclesButtonsPanel, Panel):
    bl_label = "Denoise"
    bl_parent_id = 'CYCLES_RENDER_PT_sampling_viewport'
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        scene = context.scene
        cscene = scene.cycles

        self.layout.prop(context.scene.cycles, "use_preview_denoising", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.active = cscene.use_preview_denoising

        sub = col.column()
        sub.active = show_preview_denoise_active(context)
        sub.prop(cscene, "preview_denoiser", text="Denoiser")

        col.prop(cscene, "preview_denoising_input_passes", text="Passes")

        has_oidn_gpu = has_oidn_gpu_devices(context)
        effective_preview_denoiser = get_effective_preview_denoiser(context, has_oidn_gpu)
        if effective_preview_denoiser == 'OPENIMAGEDENOISE':
            col.prop(cscene, "preview_denoising_prefilter", text="Prefilter")
            col.prop(cscene, "preview_denoising_quality", text="Quality")

        col.prop(cscene, "preview_denoising_start_sample", text="Start Sample")

        if effective_preview_denoiser == 'OPENIMAGEDENOISE':
            row = col.row()
            row.active = has_oidn_gpu_devices(context)
            row.prop(cscene, "preview_denoising_use_gpu", text="Use GPU")


class CYCLES_RENDER_PT_sampling_render(CyclesButtonsPanel, Panel):
    bl_label = "Render"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"

    def draw_header_preset(self, context):
        CYCLES_PT_sampling_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        layout.use_property_split = True
        layout.use_property_decorate = False

        heading = layout.column(align=True, heading="Noise Threshold")
        row = heading.row(align=True)
        row.prop(cscene, "use_adaptive_sampling", text="")
        sub = row.row()
        sub.active = cscene.use_adaptive_sampling
        sub.prop(cscene, "adaptive_threshold", text="")

        col = layout.column(align=True)
        if cscene.use_adaptive_sampling:
            col.prop(cscene, "samples", text="Max Samples")
            col.prop(cscene, "adaptive_min_samples", text="Min Samples")
        else:
            col.prop(cscene, "samples", text="Samples")
        col.prop(cscene, "time_limit")


class CYCLES_RENDER_PT_sampling_render_denoise(CyclesButtonsPanel, Panel):
    bl_label = "Denoise"
    bl_parent_id = 'CYCLES_RENDER_PT_sampling_render'
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        scene = context.scene
        cscene = scene.cycles

        self.layout.prop(context.scene.cycles, "use_denoising", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.active = cscene.use_denoising

        sub = col.column()
        sub.active = show_denoise_active(context)
        sub.prop(cscene, "denoiser", text="Denoiser")

        col.prop(cscene, "denoising_input_passes", text="Passes")
        if cscene.denoiser == 'OPENIMAGEDENOISE':
            col.prop(cscene, "denoising_prefilter", text="Prefilter")
            col.prop(cscene, "denoising_quality", text="Quality")

        if cscene.denoiser == 'OPENIMAGEDENOISE':
            row = col.row()
            row.active = has_oidn_gpu_devices(context)
            row.prop(cscene, "denoising_use_gpu", text="Use GPU")


class CYCLES_RENDER_PT_sampling_path_guiding(CyclesButtonsPanel, Panel):
    bl_label = "Path Guiding"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        from . import engine
        return use_cpu(context) and engine.with_path_guiding()

    def draw_header(self, context):
        scene = context.scene
        cscene = scene.cycles

        self.layout.prop(cscene, "use_guiding", text="")

    def draw(self, context):
        scene = context.scene
        cscene = scene.cycles

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.active = cscene.use_guiding

        layout.prop(cscene, "guiding_training_samples")

        col = layout.column(align=True)
        col.prop(cscene, "use_surface_guiding", text="Surface")
        col.prop(cscene, "use_volume_guiding", text="Volume", text_ctxt=i18n_contexts.id_id)


class CYCLES_RENDER_PT_sampling_path_guiding_debug(CyclesDebugButtonsPanel, Panel):
    bl_label = "Debug"
    bl_parent_id = "CYCLES_RENDER_PT_sampling_path_guiding"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        scene = context.scene
        cscene = scene.cycles

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.active = cscene.use_guiding

        layout.prop(cscene, "guiding_distribution_type", text="Distribution Type")
        layout.prop(cscene, "guiding_roughness_threshold")
        layout.prop(cscene, "guiding_directional_sampling_type", text="Directional Sampling Type")

        col = layout.column(align=True)
        col.prop(cscene, "surface_guiding_probability")
        col.prop(cscene, "volume_guiding_probability")

        col = layout.column(align=True)
        col.prop(cscene, "use_deterministic_guiding")
        col.prop(cscene, "use_guiding_direct_light")
        col.prop(cscene, "use_guiding_mis_weights")


class CYCLES_RENDER_PT_sampling_advanced(CyclesButtonsPanel, Panel):
    bl_label = "Advanced"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        row = layout.row(align=True)
        row.prop(cscene, "seed")
        row.prop(cscene, "use_animated_seed", text="", icon='TIME')

        col = layout.column(align=True)
        col.prop(cscene, "sample_offset")

        layout.separator()

        heading = layout.column(align=True, heading="Scrambling Distance")
        # Tabulated Sobol is used when the debug UI is turned off.
        heading.active = cscene.sampling_pattern == 'TABULATED_SOBOL' or not CyclesDebugButtonsPanel.poll(context)
        heading.prop(cscene, "auto_scrambling_distance", text="Automatic")
        heading.prop(cscene, "preview_scrambling_distance", text="Viewport")
        heading.prop(cscene, "scrambling_distance", text="Multiplier")

        layout.separator()

        col = layout.column(align=True)
        col.prop(cscene, "min_light_bounces")
        col.prop(cscene, "min_transparent_bounces")

        for view_layer in scene.view_layers:
            if view_layer.samples > 0:
                layout.separator()
                layout.row().prop(cscene, "use_layer_samples")
                break


class CYCLES_RENDER_PT_sampling_lights(CyclesButtonsPanel, Panel):
    bl_label = "Lights"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "use_light_tree")
        sub = col.row()
        sub.prop(cscene, "light_sampling_threshold", text="Light Threshold")
        sub.active = not cscene.use_light_tree


class CYCLES_RENDER_PT_sampling_debug(CyclesDebugButtonsPanel, Panel):
    bl_label = "Debug"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "sampling_pattern", text="Pattern")


class CYCLES_RENDER_PT_subdivision(CyclesButtonsPanel, Panel):
    bl_label = "Subdivision"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.scene.render.engine == 'CYCLES') and (context.scene.cycles.feature_set == 'EXPERIMENTAL')

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(cscene, "dicing_rate", text="Dicing Rate Render")
        sub.prop(cscene, "preview_dicing_rate", text="Viewport")

        col.separator()

        col.prop(cscene, "offscreen_dicing_scale", text="Offscreen Scale")
        col.prop(cscene, "max_subdivisions")

        col.prop(cscene, "dicing_camera")


class CYCLES_RENDER_PT_curves(CyclesButtonsPanel, Panel):
    bl_label = "Curves"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        ccscene = scene.cycles_curves

        col = layout.column()
        col.prop(ccscene, "shape", text="Shape")
        if ccscene.shape == 'RIBBONS':
            col.prop(ccscene, "subdivisions", text="Curve Subdivisions")


class CYCLES_RENDER_PT_curves_viewport_display(CyclesButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_parent_id = "CYCLES_RENDER_PT_curves"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        draw_curves_settings(self, context)


class CYCLES_RENDER_PT_volumes(CyclesButtonsPanel, Panel):
    bl_label = "Volumes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "volume_step_rate", text="Step Rate Render")
        col.prop(cscene, "volume_preview_step_rate", text="Viewport")

        layout.prop(cscene, "volume_max_steps", text="Max Steps")


class CYCLES_RENDER_PT_light_paths(CyclesButtonsPanel, Panel):
    bl_label = "Light Paths"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header_preset(self, context):
        CYCLES_PT_integrator_presets.draw_panel_header(self.layout)

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_light_paths_max_bounces(CyclesButtonsPanel, Panel):
    bl_label = "Max Bounces"
    bl_parent_id = "CYCLES_RENDER_PT_light_paths"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "max_bounces", text="Total")

        col = layout.column(align=True)
        col.prop(cscene, "diffuse_bounces", text="Diffuse")
        col.prop(cscene, "glossy_bounces", text="Glossy")
        col.prop(cscene, "transmission_bounces", text="Transmission")
        col.prop(cscene, "volume_bounces", text="Volume", text_ctxt=i18n_contexts.id_id)

        col = layout.column(align=True)
        col.prop(cscene, "transparent_max_bounces", text="Transparent")


class CYCLES_RENDER_PT_light_paths_clamping(CyclesButtonsPanel, Panel):
    bl_label = "Clamping"
    bl_parent_id = "CYCLES_RENDER_PT_light_paths"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "sample_clamp_direct", text="Direct Light")
        col.prop(cscene, "sample_clamp_indirect", text="Indirect Light")


class CYCLES_RENDER_PT_light_paths_caustics(CyclesButtonsPanel, Panel):
    bl_label = "Caustics"
    bl_parent_id = "CYCLES_RENDER_PT_light_paths"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "blur_glossy")
        col = layout.column(heading="Caustics", align=True)
        col.prop(cscene, "caustics_reflective", text="Reflective")
        col.prop(cscene, "caustics_refractive", text="Refractive")


class CYCLES_RENDER_PT_light_paths_fast_gi(CyclesButtonsPanel, Panel):
    bl_label = "Fast GI Approximation"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "CYCLES_RENDER_PT_light_paths"

    def draw_header(self, context):
        scene = context.scene
        cscene = scene.cycles

        self.layout.prop(cscene, "use_fast_gi", text="")

    def draw(self, context):
        scene = context.scene
        cscene = scene.cycles
        world = scene.world

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.active = cscene.use_fast_gi

        col = layout.column(align=True)
        col.prop(cscene, "fast_gi_method", text="Method")

        if world:
            light = world.light_settings
            col = layout.column(align=True)
            col.prop(light, "ao_factor", text="AO Factor")
            col.prop(light, "distance", text="AO Distance")

        if cscene.fast_gi_method == 'REPLACE':
            col = layout.column(align=True)
            col.prop(cscene, "ao_bounces", text="Viewport Bounces")
            col.prop(cscene, "ao_bounces_render", text="Render Bounces")


class CYCLES_RENDER_PT_motion_blur(CyclesButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles
        rd = scene.render
        layout.active = rd.use_motion_blur

        col = layout.column()
        col.prop(rd, "motion_blur_position", text="Position")
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
        layout.use_property_decorate = False

        scene = context.scene
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
        layout.use_property_decorate = False
        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "film_exposure")


class CYCLES_RENDER_PT_film_transparency(CyclesButtonsPanel, Panel):
    bl_label = "Transparent"
    bl_parent_id = "CYCLES_RENDER_PT_film"

    def draw_header(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render

        layout.prop(rd, "film_transparent", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.film_transparent

        col = layout.column()
        col.prop(cscene, "film_transparent_glass", text="Transparent Glass")

        sub = col.column()
        sub.active = rd.film_transparent and cscene.film_transparent_glass
        sub.prop(cscene, "film_transparent_roughness", text="Roughness Threshold")


class CYCLES_RENDER_PT_film_pixel_filter(CyclesButtonsPanel, Panel):
    bl_label = "Pixel Filter"
    bl_parent_id = "CYCLES_RENDER_PT_film"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "pixel_filter_type", text="Type")
        if cscene.pixel_filter_type != 'BOX':
            col.prop(cscene, "filter_width", text="Width")


class CYCLES_RENDER_PT_performance(CyclesButtonsPanel, Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header_preset(self, context):
        CYCLES_PT_performance_presets.draw_panel_header(self.layout)

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_performance_compositor(CyclesButtonsPanel, CompositorPerformanceButtonsPanel, Panel):
    bl_parent_id = "CYCLES_RENDER_PT_performance"
    bl_options = {'DEFAULT_CLOSED'}


class CYCLES_RENDER_PT_performance_threads(CyclesButtonsPanel, Panel):
    bl_label = "Threads"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render

        col = layout.column()

        col.prop(rd, "threads_mode")
        sub = col.column(align=True)
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")


class CYCLES_RENDER_PT_performance_memory(CyclesButtonsPanel, Panel):
    bl_label = "Memory"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "use_auto_tile")
        sub = col.column()
        sub.active = cscene.use_auto_tile
        sub.prop(cscene, "tile_size")


class CYCLES_RENDER_PT_performance_acceleration_structure(CyclesButtonsPanel, Panel):
    bl_label = "Acceleration Structure"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    @classmethod
    def poll(cls, context):
        return not use_optix(context) or use_multi_device(context)

    def draw(self, context):
        import _cycles

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()

        use_embree = _cycles.with_embree

        if use_cpu(context):
            col.prop(cscene, "debug_use_spatial_splits")
            if use_embree:
                col.prop(cscene, "debug_use_compact_bvh")
            else:
                sub = col.column()
                sub.active = not cscene.debug_use_spatial_splits
                sub.prop(cscene, "debug_bvh_time_steps")

                col.prop(cscene, "debug_use_hair_bvh")

                sub = col.column(align=True)
                sub.label(text="Cycles built without Embree support")
                sub.label(text="CPU raytracing performance will be poor")
        else:
            col.prop(cscene, "debug_use_spatial_splits")
            sub = col.column()
            sub.active = not cscene.debug_use_spatial_splits
            sub.prop(cscene, "debug_bvh_time_steps")

            col.prop(cscene, "debug_use_hair_bvh")

            # CPU is used in addition to a GPU
            if use_multi_device(context) and use_embree:
                col.prop(cscene, "debug_use_compact_bvh")


class CYCLES_RENDER_PT_performance_final_render(CyclesButtonsPanel, Panel):
    bl_label = "Final Render"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render

        col = layout.column()

        col.prop(rd, "use_persistent_data", text="Persistent Data")


class CYCLES_RENDER_PT_performance_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        col = layout.column()
        col.prop(rd, "preview_pixel_size", text="Pixel Size")


class CYCLES_RENDER_PT_filter(CyclesButtonsPanel, Panel):
    bl_label = "Filter"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "view_layer"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render
        view_layer = context.view_layer

        col = layout.column(heading="Include")
        col.prop(view_layer, "use_sky", text="Environment")
        col.prop(view_layer, "use_solid", text="Surfaces")
        col.prop(view_layer, "use_strand", text="Curves")
        col.prop(view_layer, "use_volumes", text="Volumes")

        col = layout.column(heading="Use")
        sub = col.row()
        sub.prop(view_layer, "use_motion_blur", text="Motion Blur")
        sub.active = rd.use_motion_blur
        sub = col.row()
        sub.prop(view_layer.cycles, "use_denoising", text="Denoising")
        sub.active = scene.cycles.use_denoising


class CYCLES_RENDER_PT_override(CyclesButtonsPanel, Panel):
    bl_label = "Override"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "view_layer"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        layout.prop(view_layer, "material_override")
        layout.prop(view_layer, "world_override")
        layout.prop(view_layer, "samples")


class CYCLES_RENDER_PT_passes(CyclesButtonsPanel, Panel):
    bl_label = "Passes"
    bl_context = "view_layer"

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_passes_data(CyclesButtonsPanel, Panel):
    bl_label = "Data"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render
        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles

        col = layout.column(heading="Include", align=True)
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")
        col.prop(view_layer, "use_pass_mist")
        col.prop(view_layer, "use_pass_position")
        col.prop(view_layer, "use_pass_normal")
        sub = col.column()
        sub.active = not rd.use_motion_blur
        sub.prop(view_layer, "use_pass_vector")
        col.prop(view_layer, "use_pass_uv")

        col.prop(cycles_view_layer, "denoising_store_passes", text="Denoising Data")

        col = layout.column(heading="Indexes", align=True)
        col.prop(view_layer, "use_pass_object_index")
        col.prop(view_layer, "use_pass_material_index")

        col = layout.column(heading="Debug", align=True)
        col.prop(cycles_view_layer, "pass_debug_sample_count", text="Sample Count")

        layout.prop(view_layer, "pass_alpha_threshold")


class CYCLES_RENDER_PT_passes_light(CyclesButtonsPanel, Panel):
    bl_label = "Light"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles

        col = layout.column(heading="Diffuse", align=True)
        col.prop(view_layer, "use_pass_diffuse_direct", text="Direct")
        col.prop(view_layer, "use_pass_diffuse_indirect", text="Indirect")
        col.prop(view_layer, "use_pass_diffuse_color", text="Color")

        col = layout.column(heading="Glossy", align=True)
        col.prop(view_layer, "use_pass_glossy_direct", text="Direct")
        col.prop(view_layer, "use_pass_glossy_indirect", text="Indirect")
        col.prop(view_layer, "use_pass_glossy_color", text="Color")

        col = layout.column(heading="Transmission", align=True)
        col.prop(view_layer, "use_pass_transmission_direct", text="Direct")
        col.prop(view_layer, "use_pass_transmission_indirect", text="Indirect")
        col.prop(view_layer, "use_pass_transmission_color", text="Color")

        col = layout.column(heading="Volume", heading_ctxt=i18n_contexts.id_id, align=True)
        col.prop(cycles_view_layer, "use_pass_volume_direct", text="Direct")
        col.prop(cycles_view_layer, "use_pass_volume_indirect", text="Indirect")

        col = layout.column(heading="Other", align=True)
        col.prop(view_layer, "use_pass_emit", text="Emission")
        col.prop(view_layer, "use_pass_environment")
        col.prop(view_layer, "use_pass_ambient_occlusion", text="Ambient Occlusion")
        col.prop(cycles_view_layer, "use_pass_shadow_catcher")


class CYCLES_RENDER_PT_passes_crypto(CyclesButtonsPanel, ViewLayerCryptomattePanel, Panel):
    bl_label = "Cryptomatte"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"


class CYCLES_RENDER_PT_passes_aov(CyclesButtonsPanel, ViewLayerAOVPanel):
    bl_label = "Shader AOV"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"


class CYCLES_RENDER_PT_passes_lightgroups(CyclesButtonsPanel, ViewLayerLightgroupsPanel):
    bl_label = "Light Groups"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"


class CYCLES_PT_post_processing(CyclesButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "output"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        rd = context.scene.render

        col = layout.column(align=True, heading="Pipeline")
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        layout.prop(rd, "dither_intensity", text="Dither", slider=True)


class CYCLES_CAMERA_PT_dof(CyclesButtonsPanel, Panel):
    bl_label = "Depth of Field"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.camera and CyclesButtonsPanel.poll(context)

    def draw_header(self, context):
        cam = context.camera
        dof = cam.dof
        self.layout.prop(dof, "use_dof", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera
        dof = cam.dof
        layout.active = dof.use_dof

        split = layout.split()

        col = split.column()
        col.prop(dof, "focus_object", text="Focus Object")
        if dof.focus_object and dof.focus_object.type == 'ARMATURE':
            col.prop_search(dof, "focus_subtarget", dof.focus_object.data, "bones", text="Focus Bone")

        sub = col.row()
        sub.active = dof.focus_object is None
        sub.prop(dof, "focus_distance", text="Distance")


class CYCLES_CAMERA_PT_dof_aperture(CyclesButtonsPanel, Panel):
    bl_label = "Aperture"
    bl_parent_id = "CYCLES_CAMERA_PT_dof"

    @classmethod
    def poll(cls, context):
        return context.camera and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera
        dof = cam.dof
        layout.active = dof.use_dof
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(dof, "aperture_fstop")
        col.prop(dof, "aperture_blades")
        col.prop(dof, "aperture_rotation")
        col.prop(dof, "aperture_ratio")


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
            rows = 3
            if (is_sortable):
                rows = 4

            row = layout.row()

            row.template_list("MATERIAL_UL_matslots", "", ob, "material_slots", ob, "active_material_index", rows=rows)

            col = row.column(align=True)
            col.operator("object.material_slot_add", icon='ADD', text="")
            col.operator("object.material_slot_remove", icon='REMOVE', text="")
            col.separator()
            col.menu("MATERIAL_MT_context_menu", icon='DOWNARROW_HLT', text="")

            if is_sortable:
                col.separator()

                col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            if ob.mode == 'EDIT':
                row = layout.row(align=True)
                row.operator("object.material_slot_assign", text="Assign")
                row.operator("object.material_slot_select", text="Select")
                row.operator("object.material_slot_deselect", text="Deselect")

        row = layout.row()

        if ob:
            row.template_ID(ob, "active_material", new="material.new")

            if slot:
                icon_link = 'MESH_DATA' if slot.link == 'DATA' else 'OBJECT_DATA'
                row.prop(slot, "link", text="", icon=icon_link, icon_only=True)

        elif mat:
            layout.template_ID(space, "pin_id")
            layout.separator()


class CYCLES_OBJECT_PT_motion_blur(CyclesButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        if CyclesButtonsPanel.poll(context) and ob:
            if ob.type in {'MESH', 'CURVE', 'CURVE', 'SURFACE', 'FONT',
                           'META', 'CAMERA', 'CURVES', 'POINTCLOUD', 'VOLUME'}:
                return True
            if ob.instance_type == 'COLLECTION' and ob.instance_collection:
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
        layout.use_property_split = True

        rd = context.scene.render
        # scene = context.scene

        ob = context.object
        cob = ob.cycles

        layout.active = (rd.use_motion_blur and cob.use_motion_blur)

        col = layout.column()
        col.prop(cob, "motion_steps", text="Steps")
        if ob.type != 'CAMERA':
            col.prop(cob, "use_deform_motion", text="Deformation")


def has_geometry_visibility(ob):
    return ob and (
        (ob.type in {
            'MESH',
            'CURVE',
            'SURFACE',
            'FONT',
            'META',
            'LIGHT',
            'VOLUME',
            'POINTCLOUD',
            'CURVES',
        }) or (ob.instance_type == 'COLLECTION' and ob.instance_collection))


class CYCLES_OBJECT_PT_shading(CyclesButtonsPanel, Panel):
    bl_label = "Shading"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not CyclesButtonsPanel.poll(context):
            return False

        ob = context.object
        return ob and has_geometry_visibility(ob)

    def draw(self, context):
        pass


class CYCLES_OBJECT_PT_shading_shadow_terminator(CyclesButtonsPanel, Panel):
    bl_label = "Shadow Terminator"
    bl_parent_id = "CYCLES_OBJECT_PT_shading"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return context.object.type != 'LIGHT'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        ob = context.object
        cob = ob.cycles
        flow.prop(cob, "shadow_terminator_geometry_offset", text="Geometry Offset")
        flow.prop(cob, "shadow_terminator_offset", text="Shading Offset")


class CYCLES_OBJECT_PT_shading_gi_approximation(CyclesButtonsPanel, Panel):
    bl_label = "Fast GI Approximation"
    bl_parent_id = "CYCLES_OBJECT_PT_shading"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return context.object.type != 'LIGHT'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        ob = context.object

        cob = ob.cycles
        cscene = scene.cycles

        col = layout.column()
        col.active = cscene.use_fast_gi
        col.prop(cob, "ao_distance")


class CYCLES_OBJECT_PT_shading_caustics(CyclesButtonsPanel, Panel):
    bl_label = "Caustics"
    bl_parent_id = "CYCLES_OBJECT_PT_shading"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return CyclesButtonsPanel.poll(context) and use_mnee(context) and context.object.type != 'LIGHT'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()

        ob = context.object
        cob = ob.cycles
        col.prop(cob, "is_caustics_caster")
        col.prop(cob, "is_caustics_receiver")


class CYCLES_OBJECT_PT_lightgroup(CyclesButtonsPanel, Panel):
    bl_label = "Light Group"
    bl_parent_id = "CYCLES_OBJECT_PT_shading"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object

        view_layer = context.view_layer

        row = layout.row(align=True)
        row.use_property_decorate = False

        sub = row.column(align=True)
        sub.prop_search(ob, "lightgroup", view_layer, "lightgroups", text="Light Group", results_are_suggestions=True)

        sub = row.column(align=True)
        sub.enabled = bool(ob.lightgroup) and not any(lg.name == ob.lightgroup for lg in view_layer.lightgroups)
        sub.operator("scene.view_layer_add_lightgroup", icon='ADD', text="").name = ob.lightgroup


class CYCLES_OBJECT_MT_light_linking_context_menu(Menu):
    bl_label = "Light Linking Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.light_linking_receivers_select")


class CYCLES_OBJECT_MT_shadow_linking_context_menu(Menu):
    bl_label = "Shadow Linking Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.light_linking_blockers_select")


class CYCLES_OBJECT_PT_light_linking(CyclesButtonsPanel, Panel):
    bl_label = "Light Linking"
    bl_parent_id = "CYCLES_OBJECT_PT_shading"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        object = context.object
        light_linking = object.light_linking

        col = layout.column()

        col.template_ID(
            light_linking,
            "receiver_collection",
            new="object.light_linking_receiver_collection_new")

        if not light_linking.receiver_collection:
            return

        row = layout.row()
        col = row.column()
        col.template_light_linking_collection(row, light_linking, "receiver_collection")

        col = row.column()
        sub = col.column(align=True)
        prop = sub.operator("object.light_linking_receivers_link", icon='ADD', text="")
        prop.link_state = 'INCLUDE'
        sub.operator("object.light_linking_unlink_from_collection", icon='REMOVE', text="")
        sub = col.column()
        sub.menu("CYCLES_OBJECT_MT_light_linking_context_menu", icon='DOWNARROW_HLT', text="")


class CYCLES_OBJECT_PT_shadow_linking(CyclesButtonsPanel, Panel):
    bl_label = "Shadow Linking"
    bl_parent_id = "CYCLES_OBJECT_PT_shading"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        object = context.object
        light_linking = object.light_linking

        col = layout.column()

        col.template_ID(
            light_linking,
            "blocker_collection",
            new="object.light_linking_blocker_collection_new")

        if not light_linking.blocker_collection:
            return

        row = layout.row()
        col = row.column()
        col.template_light_linking_collection(row, light_linking, "blocker_collection")

        col = row.column()
        sub = col.column(align=True)
        prop = sub.operator("object.light_linking_blockers_link", icon='ADD', text="")
        prop.link_state = 'INCLUDE'
        sub.operator("object.light_linking_unlink_from_collection", icon='REMOVE', text="")
        sub = col.column()
        sub.menu("CYCLES_OBJECT_MT_shadow_linking_context_menu", icon='DOWNARROW_HLT', text="")


class CYCLES_OBJECT_PT_visibility(CyclesButtonsPanel, Panel):
    bl_label = "Visibility"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return CyclesButtonsPanel.poll(context) and (context.object)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object

        layout.prop(ob, "hide_select", text="Selectable", invert_checkbox=True, toggle=False)

        col = layout.column(heading="Show In")
        col.prop(ob, "hide_viewport", text="Viewports", invert_checkbox=True, toggle=False)
        col.prop(ob, "hide_render", text="Renders", invert_checkbox=True, toggle=False)

        if has_geometry_visibility(ob):
            col = layout.column(heading="Mask")
            col.prop(ob, "is_shadow_catcher")
            col.prop(ob, "is_holdout")


class CYCLES_OBJECT_PT_visibility_ray_visibility(CyclesButtonsPanel, Panel):
    bl_label = "Ray Visibility"
    bl_parent_id = "CYCLES_OBJECT_PT_visibility"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return CyclesButtonsPanel.poll(context) and has_geometry_visibility(ob)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        ob = context.object

        col = layout.column()
        col.prop(ob, "visible_camera", text="Camera")
        col.prop(ob, "visible_diffuse", text="Diffuse")
        col.prop(ob, "visible_glossy", text="Glossy")
        col.prop(ob, "visible_transmission", text="Transmission")
        col.prop(ob, "visible_volume_scatter", text="Volume Scatter")

        if ob.type != 'LIGHT':
            sub = col.column()
            sub.prop(ob, "visible_shadow", text="Shadow")


class CYCLES_OBJECT_PT_visibility_culling(CyclesButtonsPanel, Panel):
    bl_label = "Culling"
    bl_parent_id = "CYCLES_OBJECT_PT_visibility"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return CyclesButtonsPanel.poll(context) and has_geometry_visibility(ob)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles
        ob = context.object
        cob = ob.cycles

        row = layout.row()
        row.active = scene.render.use_simplify and cscene.use_camera_cull
        row.prop(cob, "use_camera_cull")

        row = layout.row()
        row.active = scene.render.use_simplify and cscene.use_distance_cull
        row.prop(cob, "use_distance_cull")


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
            col.prop(light, "shape", text="Shape")
            sub = col.column(align=True)

            if light.shape in {'SQUARE', 'DISK'}:
                sub.prop(light, "size")
            elif light.shape in {'RECTANGLE', 'ELLIPSE'}:
                sub.prop(light, "size", text="Size X")
                sub.prop(light, "size_y", text="Y")

        if not (light.type == 'AREA' and clamp.is_portal):
            col.separator()
            sub = col.column()
            sub.prop(clamp, "max_bounces")

        sub = col.column(align=True)
        sub.active = not (light.type == 'AREA' and clamp.is_portal)
        sub.prop(light, "use_shadow", text="Cast Shadow")
        sub.prop(clamp, "use_multiple_importance_sampling", text="Multiple Importance")
        if use_mnee(context):
            sub.prop(clamp, "is_caustics_light", text="Shadow Caustics")

        if light.type == 'AREA':
            col.prop(clamp, "is_portal", text="Portal")


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

        layout.use_property_split = True

        light = context.light
        panel_node_draw(layout, light, 'OUTPUT_LIGHT', 'Surface')


class CYCLES_LIGHT_PT_beam_shape(CyclesButtonsPanel, Panel):
    bl_label = "Beam Shape"
    bl_parent_id = "CYCLES_LIGHT_PT_light"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        if context.light.type in {'SPOT', 'AREA'}:
            return context.light and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        light = context.light
        layout.use_property_split = True

        col = layout.column()
        if light.type == 'SPOT':
            col.prop(light, "spot_size", text="Spot Size")
            col.prop(light, "spot_blend", text="Blend", slider=True)
            col.prop(light, "show_cone")
        elif light.type == 'AREA':
            col.prop(light, "spread", text="Spread")


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

        layout.use_property_split = True

        world = context.world

        if not panel_node_draw(layout, world, 'OUTPUT_WORLD', 'Surface'):
            layout.prop(world, "color")


class CYCLES_WORLD_PT_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_translation_context = i18n_contexts.id_id
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        world = context.world
        return world and world.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        world = context.world
        panel_node_draw(layout, world, 'OUTPUT_WORLD', 'Volume')


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
        layout.use_property_split = True

        world = context.world

        col = layout.column(align=True)
        col.prop(world.mist_settings, "start")
        col.prop(world.mist_settings, "depth")

        col = layout.column()
        col.prop(world.mist_settings, "falloff")


class CYCLES_WORLD_PT_ray_visibility(CyclesButtonsPanel, Panel):
    bl_label = "Ray Visibility"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return CyclesButtonsPanel.poll(context) and context.world

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        world = context.world
        visibility = world.cycles_visibility

        col = layout.column()
        col.prop(visibility, "camera")
        col.prop(visibility, "diffuse")
        col.prop(visibility, "glossy")
        col.prop(visibility, "transmission")
        col.prop(visibility, "scatter")


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
        layout.use_property_decorate = False

        layout.column()


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
        layout.use_property_decorate = False

        world = context.world
        cworld = world.cycles

        col = layout.column()
        col.prop(cworld, "sampling_method", text="Sampling")

        sub = col.column()
        sub.active = cworld.sampling_method != 'NONE'
        subsub = sub.row(align=True)
        subsub.active = cworld.sampling_method == 'MANUAL'
        subsub.prop(cworld, "sample_map_resolution")
        sub.prop(cworld, "max_bounces")
        sub.prop(cworld, "is_caustics_light", text="Shadow Caustics")


class CYCLES_WORLD_PT_settings_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_translation_context = i18n_contexts.id_id
    bl_parent_id = "CYCLES_WORLD_PT_settings"
    bl_context = "world"

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        world = context.world
        cworld = world.cycles

        col = layout.column()

        sub = col.column()
        col.prop(cworld, "volume_sampling", text="Sampling")
        col.prop(cworld, "volume_interpolation", text="Interpolation")
        col.prop(cworld, "homogeneous_volume", text="Homogeneous")
        sub = col.column()
        sub.active = not cworld.homogeneous_volume
        sub.prop(cworld, "volume_step_size")


class CYCLES_WORLD_PT_settings_light_group(CyclesButtonsPanel, Panel):
    bl_label = "Light Group"
    bl_parent_id = "CYCLES_WORLD_PT_settings"
    bl_context = "world"

    @classmethod
    def poll(cls, context):
        return context.world and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        world = context.world
        view_layer = context.view_layer

        row = layout.row(align=True)

        sub = row.column(align=True)
        sub.prop_search(
            world,
            "lightgroup",
            view_layer,
            "lightgroups",
            text="Light Group",
            results_are_suggestions=True,
        )

        sub = row.column(align=True)
        sub.enabled = bool(world.lightgroup) and not any(lg.name == world.lightgroup for lg in view_layer.lightgroups)
        sub.operator("scene.view_layer_add_lightgroup", icon='ADD', text="").name = world.lightgroup


class CYCLES_MATERIAL_PT_preview(CyclesButtonsPanel, Panel):
    bl_label = "Preview"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.material)


class CYCLES_MATERIAL_PT_surface(CyclesButtonsPanel, Panel):
    bl_label = "Surface"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        mat = context.material
        if not panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Surface'):
            layout.prop(mat, "diffuse_color")


class CYCLES_MATERIAL_PT_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_translation_context = i18n_contexts.id_id
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and mat.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        mat = context.material
        # cmat = mat.cycles

        panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Volume')


class CYCLES_MATERIAL_PT_displacement(CyclesButtonsPanel, Panel):
    bl_label = "Displacement"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and mat.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        mat = context.material
        panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Displacement')


class CYCLES_MATERIAL_PT_settings(CyclesButtonsPanel, Panel):
    bl_label = "Settings"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and CyclesButtonsPanel.poll(context)

    @staticmethod
    def draw_shared(self, mat):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(mat, "pass_index")

    def draw(self, context):
        self.draw_shared(self, context.material)


class CYCLES_MATERIAL_PT_settings_surface(CyclesButtonsPanel, Panel):
    bl_label = "Surface"
    bl_parent_id = "CYCLES_MATERIAL_PT_settings"
    bl_context = "material"

    @staticmethod
    def draw_shared(self, mat):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        cmat = mat.cycles

        col = layout.column()
        col.prop(mat, "displacement_method", text="Displacement")
        col.prop(cmat, "emission_sampling")
        col.prop(mat, "use_transparent_shadow")
        col.prop(cmat, "use_bump_map_correction")

    def draw(self, context):
        self.draw_shared(self, context.material)


class CYCLES_MATERIAL_PT_settings_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
    bl_translation_context = i18n_contexts.id_id
    bl_parent_id = "CYCLES_MATERIAL_PT_settings"
    bl_context = "material"

    @staticmethod
    def draw_shared(self, context, mat):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        cmat = mat.cycles

        col = layout.column()
        sub = col.column()
        col.prop(cmat, "volume_sampling", text="Sampling")
        col.prop(cmat, "volume_interpolation", text="Interpolation")
        col.prop(cmat, "homogeneous_volume", text="Homogeneous")
        sub = col.column()
        sub.active = not cmat.homogeneous_volume
        sub.prop(cmat, "volume_step_rate")

    def draw(self, context):
        self.draw_shared(self, context, context.material)


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

        if rd.use_bake_multires:
            layout.operator("object.bake_image", icon='RENDER_STILL')
            layout.prop(rd, "use_bake_multires")
            layout.prop(rd, "bake_type")

        else:
            layout.operator("object.bake", icon='RENDER_STILL').type = cscene.bake_type
            layout.prop(rd, "use_bake_multires")
            layout.prop(cscene, "bake_type")

        if not rd.use_bake_multires and cscene.bake_type not in {
                "AO", "POSITION", "NORMAL", "UV", "ROUGHNESS", "ENVIRONMENT"}:
            row = layout.row()
            row.prop(cbk, "view_from")
            row.active = scene.camera is not None


class CYCLES_RENDER_PT_bake_influence(CyclesButtonsPanel, Panel):
    bl_label = "Influence"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_bake"
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        cscene = scene.cycles
        rd = scene.render
        if rd.use_bake_multires == False and cscene.bake_type in {
                'NORMAL', 'COMBINED', 'DIFFUSE', 'GLOSSY', 'TRANSMISSION'}:
            return True

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        cscene = scene.cycles
        cbk = scene.render.bake
        rd = scene.render

        col = layout.column()

        if cscene.bake_type == 'NORMAL':
            col.prop(cbk, "normal_space", text="Space")

            sub = col.column(align=True)
            sub.prop(cbk, "normal_r", text="Swizzle R")
            sub.prop(cbk, "normal_g", text="G", text_ctxt=i18n_contexts.color)
            sub.prop(cbk, "normal_b", text="B", text_ctxt=i18n_contexts.color)

        elif cscene.bake_type == 'COMBINED':

            col = layout.column(heading="Lighting", align=True)
            col.prop(cbk, "use_pass_direct")
            col.prop(cbk, "use_pass_indirect")

            col = layout.column(heading="Contributions", align=True)
            col.active = cbk.use_pass_direct or cbk.use_pass_indirect
            col.prop(cbk, "use_pass_diffuse")
            col.prop(cbk, "use_pass_glossy")
            col.prop(cbk, "use_pass_transmission")
            col.prop(cbk, "use_pass_emit")

        elif cscene.bake_type in {'DIFFUSE', 'GLOSSY', 'TRANSMISSION'}:
            col = layout.column(heading="Contributions", align=True)
            col.prop(cbk, "use_pass_direct")
            col.prop(cbk, "use_pass_indirect")
            col.prop(cbk, "use_pass_color")


class CYCLES_RENDER_PT_bake_selected_to_active(CyclesButtonsPanel, Panel):
    bl_label = "Selected to Active"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_bake"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        rd = scene.render
        return rd.use_bake_multires == False

    def draw_header(self, context):
        scene = context.scene
        cbk = scene.render.bake
        self.layout.prop(cbk, "use_selected_to_active", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        cscene = scene.cycles
        cbk = scene.render.bake
        rd = scene.render

        layout.active = cbk.use_selected_to_active
        col = layout.column()

        col.prop(cbk, "use_cage", text="Cage")
        if cbk.use_cage:
            col.prop(cbk, "cage_object")
            col = layout.column()
            col.prop(cbk, "cage_extrusion")
            col.active = cbk.cage_object is None
        else:
            col.prop(cbk, "cage_extrusion", text="Extrusion")

        col = layout.column()
        col.prop(cbk, "max_ray_distance")


class CYCLES_RENDER_PT_bake_output(CyclesButtonsPanel, Panel):
    bl_label = "Output"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_bake"
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        cscene = scene.cycles
        cbk = scene.render.bake
        rd = scene.render

        if rd.use_bake_multires:
            layout.prop(rd, "use_bake_clear", text="Clear Image")
            if rd.bake_type == 'DISPLACEMENT':
                layout.prop(rd, "use_bake_lores_mesh")
        else:
            layout.prop(cbk, "target")
            if cbk.target == 'IMAGE_TEXTURES':
                layout.prop(cbk, "use_clear", text="Clear Image")


class CYCLES_RENDER_PT_bake_output_margin(CyclesButtonsPanel, Panel):
    bl_label = "Margin"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_bake_output"
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        cbk = scene.render.bake
        return cbk.target == 'IMAGE_TEXTURES'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        cscene = scene.cycles
        cbk = scene.render.bake
        rd = scene.render

        if (cscene.bake_type == 'NORMAL' and cbk.normal_space == 'TANGENT') or cscene.bake_type == 'UV':
            if rd.use_bake_multires:
                layout.prop(rd, "bake_margin", text="Size")
            else:
                if cbk.target == 'IMAGE_TEXTURES':
                    layout.prop(cbk, "margin", text="Size")
        else:
            if rd.use_bake_multires:
                layout.prop(rd, "bake_margin_type", text="Type")
                layout.prop(rd, "bake_margin", text="Size")
            else:
                if cbk.target == 'IMAGE_TEXTURES':
                    layout.prop(cbk, "margin_type", text="Type")
                    layout.prop(cbk, "margin", text="Size")


class CYCLES_RENDER_PT_debug(CyclesDebugButtonsPanel, Panel):
    bl_label = "Debug"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(heading="CPU")

        row = col.row(align=True)
        row.prop(cscene, "debug_use_cpu_sse42", toggle=True)
        row.prop(cscene, "debug_use_cpu_avx2", toggle=True)
        col.prop(cscene, "debug_bvh_layout", text="BVH")

        col.separator()

        col = layout.column(heading="CUDA")
        col.prop(cscene, "debug_use_cuda_adaptive_compile")
        col = layout.column(heading="OptiX")
        col.prop(cscene, "debug_use_optix_debug", text="Module Debug")

        col.separator()

        col.prop(cscene, "debug_bvh_type", text="Viewport BVH")

        col.separator()

        import _cycles
        if _cycles.with_debug:
            col.prop(cscene, "direct_light_sampling_type")


class CYCLES_RENDER_PT_simplify(CyclesButtonsPanel, Panel):
    bl_label = "Simplify"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        pass


class CYCLES_RENDER_PT_simplify_viewport(CyclesButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_simplify"
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        col = layout.column()
        col.prop(rd, "simplify_subdivision", text="Max Subdivision")
        col.prop(rd, "simplify_child_particles", text="Child Particles")
        col.prop(cscene, "texture_limit", text="Texture Limit")
        col.prop(rd, "simplify_volumes", text="Volume Resolution")
        col.prop(rd, "use_simplify_normals", text="Normals")


class CYCLES_RENDER_PT_simplify_render(CyclesButtonsPanel, Panel):
    bl_label = "Render"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_simplify"
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        col = layout.column()

        col.prop(rd, "simplify_subdivision_render", text="Max Subdivision")
        col.prop(rd, "simplify_child_particles_render", text="Child Particles")
        col.prop(cscene, "texture_limit_render", text="Texture Limit")


class CYCLES_RENDER_PT_simplify_culling(CyclesButtonsPanel, Panel):
    bl_label = "Culling"
    bl_context = "render"
    bl_parent_id = "CYCLES_RENDER_PT_simplify"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'CYCLES'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render
        cscene = scene.cycles

        layout.active = rd.use_simplify

        row = layout.row(heading="Camera Culling")
        row.prop(cscene, "use_camera_cull", text="")
        sub = row.column()
        sub.active = cscene.use_camera_cull
        sub.prop(cscene, "camera_cull_margin", text="")

        row = layout.row(heading="Distance Culling")
        row.prop(cscene, "use_distance_cull", text="")
        sub = row.column()
        sub.active = cscene.use_distance_cull
        sub.prop(cscene, "distance_cull_margin", text="")


class CyclesShadingButtonsPanel(CyclesButtonsPanel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_parent_id = 'VIEW3D_PT_shading'

    @classmethod
    def poll(cls, context):
        return (
            CyclesButtonsPanel.poll(context) and
            context.space_data.shading.type == 'RENDERED'
        )


class CYCLES_VIEW3D_PT_shading_render_pass(CyclesShadingButtonsPanel, Panel):
    bl_label = "Render Pass"

    def draw(self, context):
        shading = context.space_data.shading

        layout = self.layout
        layout.prop(shading.cycles, "render_pass", text="")


class CYCLES_VIEW3D_PT_shading_debug(CyclesDebugButtonsPanel,
                                     CyclesShadingButtonsPanel,
                                     Panel):
    bl_label = "Debug"

    @classmethod
    def poll(cls, context):
        return (
            CyclesDebugButtonsPanel.poll(context) and
            CyclesShadingButtonsPanel.poll(context)
        )

    def draw(self, context):
        shading = context.space_data.shading

        layout = self.layout
        layout.active = context.scene.cycles.use_preview_adaptive_sampling
        layout.prop(shading.cycles, "show_active_pixels")


class CYCLES_VIEW3D_PT_shading_lighting(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Lighting"
    bl_parent_id = 'VIEW3D_PT_shading'
    COMPAT_ENGINES = {'CYCLES'}

    @classmethod
    def poll(cls, context):
        return (
            context.engine in cls.COMPAT_ENGINES and
            context.space_data.shading.type == 'RENDERED'
        )

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        split = col.split(factor=0.9)

        shading = context.space_data.shading
        col.prop(shading, "use_scene_lights_render")
        col.prop(shading, "use_scene_world_render")

        if not shading.use_scene_world_render:
            col = layout.column()
            split = col.split(factor=0.9)

            col = split.column()
            sub = col.row()
            sub.scale_y = 0.6
            sub.template_icon_view(shading, "studio_light", scale_popup=3)

            col = split.column()
            col.operator("screen.userpref_show", emboss=False, text="", icon='PREFERENCES').section = 'LIGHTS'

            split = layout.split(factor=0.9)
            col = split.column()
            col.prop(shading, "studiolight_rotate_z", text="Rotation")
            col.prop(shading, "studiolight_intensity")
            col.prop(shading, "studiolight_background_alpha")


class CYCLES_VIEW3D_PT_simplify_greasepencil(CyclesButtonsPanel, Panel, GreasePencilSimplifyPanel):
    bl_label = "Grease Pencil"
    bl_parent_id = "CYCLES_RENDER_PT_simplify"
    COMPAT_ENGINES = {'CYCLES'}
    bl_options = {'DEFAULT_CLOSED'}


def draw_device(self, context):
    scene = context.scene
    layout = self.layout
    layout.use_property_split = True
    layout.use_property_decorate = False

    if context.engine == 'CYCLES':
        from . import engine
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "feature_set")

        col = layout.column()
        col.active = show_device_active(context)
        col.prop(cscene, "device")

        from . import engine
        if engine.with_osl() and (
            use_cpu(context) or (
                use_optix(context) and (
                engine.osl_version()[1] >= 13 or engine.osl_version()[0] > 1))):
            osl_col = layout.column()
            osl_col.prop(cscene, "shading_system")


def draw_pause(self, context):
    layout = self.layout
    scene = context.scene

    if context.engine == "CYCLES":
        view = context.space_data

        if view.shading.type == 'RENDERED':
            cscene = scene.cycles
            layout.prop(cscene, "preview_pause", icon='PLAY' if cscene.preview_pause else 'PAUSE', text="")


def draw_make_links(self, context):
    if context.engine == "CYCLES":
        layout = self.layout
        layout.separator()
        layout.operator_menu_enum("object.light_linking_receivers_link", "link_state")
        layout.operator_menu_enum("object.light_linking_blockers_link", "link_state")


def get_panels():
    exclude_panels = {
        'DATA_PT_camera_dof',
        'DATA_PT_falloff_curve',
        'DATA_PT_light',
        'DATA_PT_preview',
        'DATA_PT_spot',
        'MATERIAL_PT_context_material',
        'MATERIAL_PT_preview',
        'NODE_DATA_PT_light',
        'NODE_DATA_PT_spot',
        'OBJECT_PT_visibility',
        'VIEWLAYER_PT_filter',
        'VIEWLAYER_PT_layer_passes',
        'RENDER_PT_post_processing',
        'RENDER_PT_simplify',
    }

    panels = []
    for panel in bpy.types.Panel.__subclasses__():
        if hasattr(panel, 'COMPAT_ENGINES') and 'BLENDER_RENDER' in panel.COMPAT_ENGINES:
            if panel.__name__ not in exclude_panels:
                panels.append(panel)

    return panels


classes = (
    CYCLES_PT_sampling_presets,
    CYCLES_PT_viewport_sampling_presets,
    CYCLES_PT_integrator_presets,
    CYCLES_PT_performance_presets,
    CYCLES_RENDER_PT_sampling,
    CYCLES_RENDER_PT_sampling_viewport,
    CYCLES_RENDER_PT_sampling_viewport_denoise,
    CYCLES_RENDER_PT_sampling_render,
    CYCLES_RENDER_PT_sampling_render_denoise,
    CYCLES_RENDER_PT_sampling_path_guiding,
    CYCLES_RENDER_PT_sampling_path_guiding_debug,
    CYCLES_RENDER_PT_sampling_lights,
    CYCLES_RENDER_PT_sampling_advanced,
    CYCLES_RENDER_PT_sampling_debug,
    CYCLES_RENDER_PT_light_paths,
    CYCLES_RENDER_PT_light_paths_max_bounces,
    CYCLES_RENDER_PT_light_paths_clamping,
    CYCLES_RENDER_PT_light_paths_caustics,
    CYCLES_RENDER_PT_light_paths_fast_gi,
    CYCLES_RENDER_PT_volumes,
    CYCLES_RENDER_PT_subdivision,
    CYCLES_RENDER_PT_curves,
    CYCLES_RENDER_PT_curves_viewport_display,
    CYCLES_RENDER_PT_simplify,
    CYCLES_RENDER_PT_simplify_viewport,
    CYCLES_RENDER_PT_simplify_render,
    CYCLES_RENDER_PT_simplify_culling,
    CYCLES_VIEW3D_PT_simplify_greasepencil,
    CYCLES_VIEW3D_PT_shading_lighting,
    CYCLES_VIEW3D_PT_shading_render_pass,
    CYCLES_VIEW3D_PT_shading_debug,
    CYCLES_RENDER_PT_motion_blur,
    CYCLES_RENDER_PT_motion_blur_curve,
    CYCLES_RENDER_PT_film,
    CYCLES_RENDER_PT_film_pixel_filter,
    CYCLES_RENDER_PT_film_transparency,
    CYCLES_RENDER_PT_performance,
    CYCLES_RENDER_PT_performance_compositor,
    CYCLES_RENDER_PT_performance_threads,
    CYCLES_RENDER_PT_performance_memory,
    CYCLES_RENDER_PT_performance_acceleration_structure,
    CYCLES_RENDER_PT_performance_final_render,
    CYCLES_RENDER_PT_performance_viewport,
    CYCLES_RENDER_PT_passes,
    CYCLES_RENDER_PT_passes_data,
    CYCLES_RENDER_PT_passes_light,
    CYCLES_RENDER_PT_passes_crypto,
    CYCLES_RENDER_PT_passes_aov,
    CYCLES_RENDER_PT_passes_lightgroups,
    CYCLES_RENDER_PT_filter,
    CYCLES_RENDER_PT_override,
    CYCLES_PT_post_processing,
    CYCLES_CAMERA_PT_dof,
    CYCLES_CAMERA_PT_dof_aperture,
    CYCLES_PT_context_material,
    CYCLES_OBJECT_PT_motion_blur,
    CYCLES_OBJECT_PT_shading,
    CYCLES_OBJECT_PT_shading_shadow_terminator,
    CYCLES_OBJECT_PT_shading_gi_approximation,
    CYCLES_OBJECT_PT_shading_caustics,
    CYCLES_OBJECT_PT_lightgroup,
    CYCLES_OBJECT_MT_light_linking_context_menu,
    CYCLES_OBJECT_PT_light_linking,
    CYCLES_OBJECT_MT_shadow_linking_context_menu,
    CYCLES_OBJECT_PT_shadow_linking,
    CYCLES_OBJECT_PT_visibility,
    CYCLES_OBJECT_PT_visibility_ray_visibility,
    CYCLES_OBJECT_PT_visibility_culling,
    CYCLES_LIGHT_PT_preview,
    CYCLES_LIGHT_PT_light,
    CYCLES_LIGHT_PT_nodes,
    CYCLES_LIGHT_PT_beam_shape,
    CYCLES_WORLD_PT_preview,
    CYCLES_WORLD_PT_surface,
    CYCLES_WORLD_PT_volume,
    CYCLES_WORLD_PT_mist,
    CYCLES_WORLD_PT_ray_visibility,
    CYCLES_WORLD_PT_settings,
    CYCLES_WORLD_PT_settings_surface,
    CYCLES_WORLD_PT_settings_volume,
    CYCLES_WORLD_PT_settings_light_group,
    CYCLES_MATERIAL_PT_preview,
    CYCLES_MATERIAL_PT_surface,
    CYCLES_MATERIAL_PT_volume,
    CYCLES_MATERIAL_PT_displacement,
    CYCLES_MATERIAL_PT_settings,
    CYCLES_MATERIAL_PT_settings_surface,
    CYCLES_MATERIAL_PT_settings_volume,
    CYCLES_RENDER_PT_bake,
    CYCLES_RENDER_PT_bake_influence,
    CYCLES_RENDER_PT_bake_selected_to_active,
    CYCLES_RENDER_PT_bake_output,
    CYCLES_RENDER_PT_bake_output_margin,
    CYCLES_RENDER_PT_debug,
    node_panel(CYCLES_MATERIAL_PT_settings),
    node_panel(CYCLES_MATERIAL_PT_settings_surface),
    node_panel(CYCLES_MATERIAL_PT_settings_volume),
    node_panel(CYCLES_WORLD_PT_ray_visibility),
    node_panel(CYCLES_WORLD_PT_settings),
    node_panel(CYCLES_WORLD_PT_settings_surface),
    node_panel(CYCLES_WORLD_PT_settings_volume),
    node_panel(CYCLES_LIGHT_PT_light),
    node_panel(CYCLES_LIGHT_PT_beam_shape)
)


def register():
    from bpy.utils import register_class

    bpy.types.RENDER_PT_context.append(draw_device)
    bpy.types.VIEW3D_HT_header.append(draw_pause)
    bpy.types.VIEW3D_MT_make_links.append(draw_make_links)

    for panel in get_panels():
        panel.COMPAT_ENGINES.add('CYCLES')

    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class

    bpy.types.RENDER_PT_context.remove(draw_device)
    bpy.types.VIEW3D_HT_header.remove(draw_pause)
    bpy.types.VIEW3D_MT_make_links.remove(draw_make_links)

    for panel in get_panels():
        if 'CYCLES' in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove('CYCLES')

    for cls in classes:
        unregister_class(cls)
