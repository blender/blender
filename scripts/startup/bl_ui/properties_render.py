# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Panel
from bpy.app.translations import contexts as i18n_contexts
from bl_ui.properties_grease_pencil_common import GreasePencilSimplifyPanel
from bl_ui.space_view3d import (
    VIEW3D_PT_shading_lighting,
    VIEW3D_PT_shading_color,
    VIEW3D_PT_shading_options,
    VIEW3D_PT_shading_cavity,
)
from bl_ui.utils import PresetPanel


class RenderButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)


class RENDER_PT_context(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    bl_options = {'HIDE_HEADER'}
    bl_label = ""

    @classmethod
    def poll(cls, context):
        return context.scene

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render

        if rd.has_multiple_engines:
            layout.prop(rd, "engine", text="Render Engine")


class RENDER_PT_color_management(RenderButtonsPanel, Panel):
    bl_label = "Color Management"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = 100
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        view = scene.view_settings

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)

        col = flow.column()
        col.prop(scene.display_settings, "display_device")

        col.separator()

        col.prop(view, "view_transform")
        col.prop(view, "look")

        if view.is_hdr and not context.window.support_hdr_color:
            row = col.split(factor=0.4)
            row.label()
            row.label(text="HDR display not supported", icon="INFO")

        col = flow.column()
        col.prop(view, "exposure")
        col.prop(view, "gamma")


class RENDER_PT_color_management_working_space(RenderButtonsPanel, Panel):
    bl_label = "Working Space"
    bl_parent_id = "RENDER_PT_color_management"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        blend_colorspace = context.blend_data.colorspace

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)

        col = flow.column()

        split = col.split(factor=0.4)
        row = split.row()
        row.label(text="File")
        row.alignment = 'RIGHT'
        split.operator_menu_enum(
            "wm.set_working_color_space",
            "working_space",
            text=blend_colorspace.working_space,
            text_ctxt=i18n_contexts.default,
        )

        col.prop(scene.sequencer_colorspace_settings, "name", text="Sequencer")


class RENDER_PT_color_management_advanced(RenderButtonsPanel, Panel):
    bl_label = "Advanced"
    bl_parent_id = "RENDER_PT_color_management"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene

        col = layout.column()
        col.active = scene.view_settings.support_emulation
        col.prop(scene.display_settings, "emulation")


class RENDER_PT_color_management_curves(RenderButtonsPanel, Panel):
    bl_label = "Curves"
    bl_parent_id = "RENDER_PT_color_management"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):

        scene = context.scene
        view = scene.view_settings

        self.layout.prop(view, "use_curve_mapping", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        view = scene.view_settings

        layout.use_property_split = False
        layout.use_property_decorate = False  # No animation.

        layout.active = view.use_curve_mapping

        layout.template_curve_mapping(view, "curve_mapping", type='COLOR', levels=True)


class RENDER_PT_color_management_white_balance_presets(PresetPanel, Panel):
    bl_label = "White Balance Presets"
    preset_subdir = "color_management/white_balance"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.color_management_white_balance_preset_add"


class RENDER_PT_color_management_white_balance(RenderButtonsPanel, Panel):
    bl_label = "White Balance"
    bl_parent_id = "RENDER_PT_color_management"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        scene = context.scene
        view = scene.view_settings

        self.layout.prop(view, "use_white_balance", text="")

    def draw_header_preset(self, context):
        layout = self.layout

        RENDER_PT_color_management_white_balance_presets.draw_panel_header(layout)

        eye = layout.operator("ui.eyedropper_color", text="", icon='EYEDROPPER')
        eye.prop_data_path = "scene.view_settings.white_balance_whitepoint"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        view = scene.view_settings

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        layout.active = view.use_white_balance

        col = layout.column()
        col.prop(view, "white_balance_temperature")
        col.prop(view, "white_balance_tint")


class RENDER_PT_eevee_motion_blur(RenderButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.render
        self.layout.prop(props, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        props = scene.render
        eevee_props = scene.eevee

        layout.active = props.use_motion_blur
        col = layout.column()
        col.prop(props, "motion_blur_position", text="Position")
        col.prop(props, "motion_blur_shutter")
        col.separator()
        col.prop(eevee_props, "motion_blur_depth_scale")
        col.prop(eevee_props, "motion_blur_max")
        col.prop(eevee_props, "motion_blur_steps", text="Steps")


class RENDER_PT_eevee_motion_blur_curve(RenderButtonsPanel, Panel):
    bl_label = "Shutter Curve"
    bl_parent_id = "RENDER_PT_eevee_motion_blur"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

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


class RENDER_PT_eevee_depth_of_field(RenderButtonsPanel, Panel):
    bl_label = "Depth of Field"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.prop(props, "bokeh_max_size")
        col.prop(props, "bokeh_threshold")
        col.prop(props, "bokeh_neighbor_max")

        col = layout.column(align=False, heading="Jitter Camera")
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(props, "use_bokeh_jittered", text="")
        sub = sub.row(align=True)
        sub.active = props.use_bokeh_jittered
        sub.prop(props, "bokeh_overblur")


class RENDER_PT_eevee_volumes(RenderButtonsPanel, Panel):
    bl_label = "Volumes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        props = scene.eevee

        col = layout.column(align=True)
        col.prop(props, "volumetric_tile_size", text="Resolution")
        col.prop(props, "volumetric_samples", text="Steps")
        col.prop(props, "volumetric_sample_distribution", text="Distribution")

        col = layout.column()
        col.prop(props, "volumetric_ray_depth", text="Max Depth")


class RENDER_PT_eevee_volumes_range(RenderButtonsPanel, Panel):
    bl_label = "Custom Range"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_eevee_volumes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_volume_custom_range", text="")

    def draw(self, context):
        scene = context.scene
        props = scene.eevee

        layout = self.layout
        layout.active = props.use_volume_custom_range
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(align=True)
        col.prop(props, "volumetric_start")
        col.prop(props, "volumetric_end")


class RENDER_PT_eevee_raytracing_presets(PresetPanel, Panel):
    bl_label = "Raytracing Presets"
    preset_subdir = "eevee/raytracing"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.eevee_raytracing_preset_add"


class RENDER_PT_eevee_raytracing(RenderButtonsPanel, Panel):
    bl_label = "Raytracing"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        props = context.scene.eevee
        self.layout.prop(props, "use_raytracing", text="")

    def draw_header_preset(self, _context):
        RENDER_PT_eevee_raytracing_presets.draw_panel_header(self.layout)

    def draw(self, context):
        scene = context.scene
        props = scene.eevee

        layout = self.layout
        layout.active = props.use_raytracing
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(props, "ray_tracing_method", text="Method")

        options = context.scene.eevee.ray_tracing_options

        col.prop(options, "resolution_scale")


class RENDER_PT_eevee_screen_trace(RenderButtonsPanel, Panel):
    bl_label = "Screen Tracing"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_eevee_raytracing"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        use_screen_trace = (context.scene.eevee.ray_tracing_method == 'SCREEN')
        return (context.engine in cls.COMPAT_ENGINES) and use_screen_trace

    def draw(self, context):
        scene = context.scene
        props = scene.eevee

        layout = self.layout
        layout.active = props.use_raytracing
        layout.use_property_split = True
        layout.use_property_decorate = False

        props = context.scene.eevee.ray_tracing_options

        col = layout.column()
        col.prop(props, "screen_trace_quality", text="Precision")
        col.prop(props, "screen_trace_thickness", text="Thickness")


class RENDER_PT_eevee_gi_approximation(RenderButtonsPanel, Panel):
    bl_label = "Fast GI Approximation"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_eevee_raytracing"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        self.layout.active = context.scene.eevee.use_raytracing
        props = context.scene.eevee
        self.layout.prop(props, "use_fast_gi", text="")

    def draw(self, context):
        scene = context.scene
        props = scene.eevee
        options = scene.eevee.ray_tracing_options

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.active = props.use_raytracing and props.use_fast_gi
        col.prop(options, "trace_max_roughness", text="Threshold")

        is_valid = props.use_raytracing and props.use_fast_gi and props.ray_tracing_options.trace_max_roughness < 1

        col = layout.column()
        col.active = is_valid
        col.prop(props, "fast_gi_method")
        col.prop(props, "fast_gi_resolution", text="Resolution")

        sub = col.column(align=True)
        sub.prop(props, "fast_gi_ray_count", text="Rays")
        sub.prop(props, "fast_gi_step_count", text="Steps")
        sub.prop(props, "fast_gi_quality", text="Precision")

        sub = col.column(align=True)
        sub.prop(props, "fast_gi_distance")
        sub.prop(props, "fast_gi_thickness_near", text="Thickness Near")
        sub.prop(props, "fast_gi_thickness_far", text="Far")

        col.prop(props, "fast_gi_bias", text="Bias")


class RENDER_PT_eevee_denoise(RenderButtonsPanel, Panel):
    bl_label = "Denoising"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_eevee_raytracing"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        self.layout.active = context.scene.eevee.use_raytracing
        props = context.scene.eevee.ray_tracing_options
        self.layout.prop(props, "use_denoise", text="")

    def draw(self, context):
        scene = context.scene
        props = scene.eevee

        layout = self.layout
        layout.active = props.use_raytracing
        layout.use_property_split = True
        layout.use_property_decorate = False
        props = context.scene.eevee.ray_tracing_options

        col = layout.column()
        col.active = props.use_denoise
        col.prop(props, "denoise_spatial")

        col = layout.column()
        col.active = props.use_denoise and props.denoise_spatial
        col.prop(props, "denoise_temporal")

        col = layout.column()
        col.active = props.use_denoise and props.denoise_spatial and props.denoise_temporal
        col.prop(props, "denoise_bilateral")


class RENDER_PT_eevee_clamping(RenderButtonsPanel, Panel):
    bl_label = "Clamping"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        pass


class RENDER_PT_eevee_clamping_surface(RenderButtonsPanel, Panel):
    bl_label = "Surface"
    bl_parent_id = "RENDER_PT_eevee_clamping"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        scene = context.scene
        props = scene.eevee

        col = layout.column(align=True)
        col.prop(props, "clamp_surface_direct", text="Direct Light")
        col.prop(props, "clamp_surface_indirect", text="Indirect Light")


class RENDER_PT_eevee_clamping_volume(RenderButtonsPanel, Panel):
    bl_label = "Volume"
    bl_parent_id = "RENDER_PT_eevee_clamping"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        scene = context.scene
        props = scene.eevee

        col = layout.column(align=True)
        col.prop(props, "clamp_volume_direct", text="Direct Light")
        col.prop(props, "clamp_volume_indirect", text="Indirect Light")


class RENDER_PT_eevee_sampling_shadows(RenderButtonsPanel, Panel):
    bl_label = "Shadows"
    bl_parent_id = "RENDER_PT_eevee_sampling"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_shadows", text="")

    def draw(self, context):
        scene = context.scene
        props = scene.eevee

        layout = self.layout
        layout.active = props.use_shadows
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(heading="Tracing", align=True)
        col.prop(props, "shadow_ray_count", text="Rays")
        col.prop(props, "shadow_step_count", text="Steps")

        col = layout.column(align=False, heading="Volume Shadows")
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(props, "use_volumetric_shadows", text="")
        sub = sub.row(align=True)
        sub.active = props.use_volumetric_shadows
        sub.prop(props, "volumetric_shadow_samples", text="Steps")

        col = layout.column()
        col.prop(props, "shadow_resolution_scale", text="Resolution")


class RENDER_PT_eevee_sampling(RenderButtonsPanel, Panel):
    bl_label = "Sampling"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        pass


class RENDER_PT_eevee_sampling_viewport(RenderButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "RENDER_PT_eevee_sampling"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.prop(props, "taa_samples", text="Samples")
        col.prop(props, "use_taa_reprojection", text="Temporal Reprojection")
        col.prop(props, "use_shadow_jitter_viewport", text="Jittered Shadows")

        # Add SSS sample count here.


class RENDER_PT_eevee_sampling_render(RenderButtonsPanel, Panel):
    bl_label = "Render"
    bl_parent_id = "RENDER_PT_eevee_sampling"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        props = scene.eevee

        col = layout.column(align=True)
        col.prop(props, "taa_render_samples", text="Samples")

        # Add SSS sample count here.


class RENDER_PT_eevee_sampling_advanced(RenderButtonsPanel, Panel):
    bl_label = "Advanced"
    bl_parent_id = "RENDER_PT_eevee_sampling"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.prop(props, "light_threshold")


class RENDER_PT_eevee_film(RenderButtonsPanel, Panel):
    bl_label = "Film"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        props = scene.eevee

        col = layout.column()
        col.prop(rd, "filter_size")
        col.prop(rd, "film_transparent", text="Transparent")

        col = layout.column(align=False, heading="Overscan")
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(props, "use_overscan", text="")
        sub = sub.row(align=True)
        sub.active = props.use_overscan
        sub.prop(props, "overscan_size", text="")


def draw_curves_settings(self, context):
    layout = self.layout
    scene = context.scene
    rd = scene.render

    layout.use_property_split = True
    layout.use_property_decorate = False  # No animation.

    layout.prop(rd, "hair_type", text="Shape", expand=True)
    layout.prop(rd, "hair_subdiv")


class RENDER_PT_eevee_hair(RenderButtonsPanel, Panel):
    bl_label = "Curves"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        draw_curves_settings(self, context)


class RENDER_PT_eevee_performance(RenderButtonsPanel, Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        rd = scene.render

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        layout.prop(rd, "use_high_quality_normals")


class CompositorPerformanceButtonsPanel:
    bl_label = "Compositor"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        rd = scene.render

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        row = col.row()
        row.prop(rd, "compositor_device", text="Device", expand=True)
        if rd.compositor_device == 'GPU':
            col.prop(rd, "compositor_precision", text="Precision")


class CompositorDenoisePerformanceButtonsPanel:
    bl_label = "Denoise Nodes"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        rd = scene.render

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        row = col.row()
        row.prop(rd, "compositor_denoise_device", text="Denoising Device", expand=True)
        col.prop(rd, "compositor_denoise_preview_quality", text="Preview Quality")
        col.prop(rd, "compositor_denoise_final_quality", text="Final Quality")


class RENDER_PT_eevee_performance_compositor(RenderButtonsPanel, CompositorPerformanceButtonsPanel, Panel):
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_eevee_performance"
    COMPAT_ENGINES = {
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }


class RENDER_PT_eevee_performance_compositor_denoise_settings(
        RenderButtonsPanel, CompositorDenoisePerformanceButtonsPanel, Panel,
):
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_eevee_performance_compositor"
    COMPAT_ENGINES = {
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }


class RENDER_PT_eevee_performance_memory(RenderButtonsPanel, Panel):
    bl_label = "Memory"
    bl_parent_id = "RENDER_PT_eevee_performance"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        props = scene.eevee

        layout.prop(props, "shadow_pool_size", text="Shadow Pool")
        layout.prop(props, "gi_irradiance_pool_size", text="Light Probes Volume Pool")


class RENDER_PT_eevee_performance_viewport(RenderButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "RENDER_PT_eevee_performance"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render

        col = layout.column()
        col.prop(rd, "preview_pixel_size", text="Pixel Size")


# TODO(falk): To rename for 5.0
class RENDER_PT_gpencil(RenderButtonsPanel, Panel):
    bl_label = "Grease Pencil"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = 10
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        pass


class RENDER_PT_grease_pencil_viewport(RenderButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_gpencil"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.grease_pencil_settings

        col = layout.column()
        col.prop(props, "antialias_threshold", text="SMAA Threshold")


class RENDER_PT_grease_pencil_render(RenderButtonsPanel, Panel):
    bl_label = "Render"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "RENDER_PT_gpencil"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.grease_pencil_settings

        col = layout.column()
        col.prop(props, "antialias_threshold_render", text="SMAA Threshold")
        col.prop(props, "aa_samples", text="SSAA Samples")

        col = layout.column()
        col.active = scene.render.use_motion_blur
        col.prop(props, "motion_blur_steps")


class RENDER_PT_opengl_sampling(RenderButtonsPanel, Panel):
    bl_label = "Sampling"
    COMPAT_ENGINES = {'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.display

        col = layout.column()
        col.prop(props, "render_aa", text="Render")
        col.prop(props, "viewport_aa", text="Viewport")


class RENDER_PT_opengl_film(RenderButtonsPanel, Panel):
    bl_label = "Film"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render
        layout.prop(rd, "film_transparent", text="Transparent")


class RENDER_PT_opengl_lighting(RenderButtonsPanel, Panel):
    bl_label = "Lighting"
    COMPAT_ENGINES = {'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        VIEW3D_PT_shading_lighting.draw(self, context)


class RENDER_PT_opengl_color(RenderButtonsPanel, Panel):
    bl_label = "Object Color"
    COMPAT_ENGINES = {'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        VIEW3D_PT_shading_color._draw_color_type(self, context)


class RENDER_PT_opengl_options(RenderButtonsPanel, Panel):
    bl_label = "Options"
    COMPAT_ENGINES = {'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        VIEW3D_PT_shading_options.draw(self, context)

        # Cavity properties.
        VIEW3D_PT_shading_cavity.draw_header(self, context)
        VIEW3D_PT_shading_cavity.draw(self, context)


class RENDER_PT_simplify(RenderButtonsPanel, Panel):
    bl_label = "Simplify"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_simplify", text="")

    def draw(self, context):
        pass


class RENDER_PT_simplify_viewport(RenderButtonsPanel, Panel):
    bl_label = "Viewport"
    bl_parent_id = "RENDER_PT_simplify"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        rd = context.scene.render

        layout.active = rd.use_simplify

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)

        col = flow.column()
        col.prop(rd, "simplify_subdivision", text="Max Subdivision")

        col = flow.column()
        col.prop(rd, "simplify_child_particles", text="Max Child Particles")

        col = flow.column()
        col.prop(rd, "simplify_volumes", text="Volume Resolution")

        col = flow.column()
        col.prop(rd, "use_simplify_normals", text="Normals")


class RENDER_PT_simplify_render(RenderButtonsPanel, Panel):
    bl_label = "Render"
    bl_parent_id = "RENDER_PT_simplify"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        rd = context.scene.render

        layout.active = rd.use_simplify

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=True)

        col = flow.column()
        col.prop(rd, "simplify_subdivision_render", text="Max Subdivision")

        col = flow.column()
        col.prop(rd, "simplify_child_particles_render", text="Max Child Particles")


class RENDER_PT_simplify_greasepencil(RenderButtonsPanel, Panel, GreasePencilSimplifyPanel):
    bl_label = "Grease Pencil"
    bl_parent_id = "RENDER_PT_simplify"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }
    bl_options = {'DEFAULT_CLOSED'}


class RENDER_PT_hydra_debug(RenderButtonsPanel, Panel):
    bl_label = "Hydra Debug"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = 200
    COMPAT_ENGINES = {'HYDRA_STORM'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (context.engine in cls.COMPAT_ENGINES) and prefs.view.show_developer_ui

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        hydra = context.scene.hydra
        layout.prop(hydra, "export_method")


classes = (
    RENDER_PT_context,
    RENDER_PT_eevee_sampling,
    RENDER_PT_eevee_sampling_viewport,
    RENDER_PT_eevee_sampling_render,
    RENDER_PT_eevee_sampling_shadows,
    RENDER_PT_eevee_sampling_advanced,
    RENDER_PT_eevee_clamping,
    RENDER_PT_eevee_clamping_surface,
    RENDER_PT_eevee_clamping_volume,
    RENDER_PT_eevee_raytracing_presets,
    RENDER_PT_eevee_raytracing,
    RENDER_PT_eevee_screen_trace,
    RENDER_PT_eevee_denoise,
    RENDER_PT_eevee_gi_approximation,
    RENDER_PT_eevee_volumes,
    RENDER_PT_eevee_volumes_range,
    RENDER_PT_eevee_hair,
    RENDER_PT_simplify,
    RENDER_PT_simplify_viewport,
    RENDER_PT_simplify_render,
    RENDER_PT_simplify_greasepencil,
    RENDER_PT_eevee_depth_of_field,
    RENDER_PT_eevee_motion_blur,
    RENDER_PT_eevee_motion_blur_curve,
    RENDER_PT_eevee_film,
    RENDER_PT_eevee_performance,
    RENDER_PT_eevee_performance_memory,
    RENDER_PT_eevee_performance_viewport,
    RENDER_PT_eevee_performance_compositor,
    RENDER_PT_eevee_performance_compositor_denoise_settings,


    RENDER_PT_gpencil,
    RENDER_PT_grease_pencil_viewport,
    RENDER_PT_grease_pencil_render,
    RENDER_PT_opengl_sampling,
    RENDER_PT_opengl_lighting,
    RENDER_PT_opengl_color,
    RENDER_PT_opengl_options,
    RENDER_PT_opengl_film,
    RENDER_PT_hydra_debug,
    RENDER_PT_color_management,
    RENDER_PT_color_management_curves,
    RENDER_PT_color_management_white_balance_presets,
    RENDER_PT_color_management_white_balance,
    RENDER_PT_color_management_working_space,
    RENDER_PT_color_management_advanced,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
