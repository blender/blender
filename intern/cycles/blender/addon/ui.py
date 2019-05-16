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
from bl_ui.utils import PresetPanel

from bpy.types import Panel


class CYCLES_PT_sampling_presets(PresetPanel, Panel):
    bl_label = "Sampling Presets"
    preset_subdir = "cycles/sampling"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.cycles_sampling_preset_add"
    COMPAT_ENGINES = {'CYCLES'}


class CYCLES_PT_integrator_presets(PresetPanel, Panel):
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


# Adapt properties editor panel to display in node editor. We have to
# copy the class rather than inherit due to the way bpy registration works.
def node_panel(cls):
    node_cls = type('NODE_' + cls.__name__, cls.__bases__, dict(cls.__dict__))

    node_cls.bl_space_type = 'NODE_EDITOR'
    node_cls.bl_region_type = 'UI'
    node_cls.bl_category = "Node"
    if hasattr(node_cls, 'bl_parent_id'):
        node_cls.bl_parent_id = 'NODE_' + node_cls.bl_parent_id

    return node_cls


def get_device_type(context):
    return context.preferences.addons[__package__].preferences.compute_device_type


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
    return context.preferences.addons[__package__].preferences.has_active_device()


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
        col.label(text="Total Samples:")
        col.separator()
        if integrator == 'PATH':
            col.label(text="%s AA" % aa)
        else:
            col.label(text="%s AA, %s Diffuse, %s Glossy, %s Transmission" %
                      (aa, d * aa, g * aa, t * aa))
            col.separator()
            col.label(text="%s AO, %s Mesh Light, %s Subsurface, %s Volume" %
                      (ao * aa, ml * aa, sss * aa, vol * aa))


class CYCLES_RENDER_PT_sampling(CyclesButtonsPanel, Panel):
    bl_label = "Sampling"

    def draw_header_preset(self, context):
        CYCLES_PT_sampling_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        cscene = scene.cycles

        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(cscene, "progressive")

        if cscene.progressive == 'PATH' or use_branched_path(context) is False:
            col = layout.column(align=True)
            col.prop(cscene, "samples", text="Render")
            col.prop(cscene, "preview_samples", text="Viewport")

            draw_samples_info(layout, context)
        else:
            col = layout.column(align=True)
            col.prop(cscene, "aa_samples", text="Render")
            col.prop(cscene, "preview_aa_samples", text="Viewport")


class CYCLES_RENDER_PT_sampling_sub_samples(CyclesButtonsPanel, Panel):
    bl_label = "Sub Samples"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"

    @classmethod
    def poll(cls, context):
        scene = context.scene
        cscene = scene.cycles
        return cscene.progressive != 'PATH' and use_branched_path(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column(align=True)
        col.prop(cscene, "diffuse_samples", text="Diffuse")
        col.prop(cscene, "glossy_samples", text="Glossy")
        col.prop(cscene, "transmission_samples", text="Transmission")
        col.prop(cscene, "ao_samples", text="AO")

        sub = col.row(align=True)
        sub.active = use_sample_all_lights(context)
        sub.prop(cscene, "mesh_light_samples", text="Mesh Light")
        col.prop(cscene, "subsurface_samples", text="Subsurface")
        col.prop(cscene, "volume_samples", text="Volume")

        draw_samples_info(layout, context)


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

        layout.prop(cscene, "sampling_pattern", text="Pattern")

        layout.prop(cscene, "use_square_samples")

        layout.separator()

        col = layout.column(align=True)
        col.prop(cscene, "light_sampling_threshold", text="Light Threshold")

        if cscene.progressive != 'PATH' and use_branched_path(context):
            col = layout.column(align=True)
            col.prop(cscene, "sample_all_lights_direct")
            col.prop(cscene, "sample_all_lights_indirect")

        for view_layer in scene.view_layers:
            if view_layer.samples > 0:
                layout.separator()
                layout.row().prop(cscene, "use_layer_samples")
                break


class CYCLES_RENDER_PT_sampling_total(CyclesButtonsPanel, Panel):
    bl_label = "Total Samples"
    bl_parent_id = "CYCLES_RENDER_PT_sampling"

    @classmethod
    def poll(cls, context):
        scene = context.scene
        cscene = scene.cycles

        if cscene.use_square_samples:
            return True

        return cscene.progressive != 'PATH' and use_branched_path(context)

    def draw(self, context):
        layout = self.layout
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

        col = layout.column(align=True)
        col.scale_y = 0.6
        if integrator == 'PATH':
            col.label(text="%s AA" % aa)
        else:
            col.label(text="%s AA, %s Diffuse, %s Glossy, %s Transmission" %
                      (aa, d * aa, g * aa, t * aa))
            col.separator()
            col.label(text="%s AO, %s Mesh Light, %s Subsurface, %s Volume" %
                      (ao * aa, ml * aa, sss * aa, vol * aa))


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
        sub.prop(cscene, "preview_dicing_rate", text="Preview")

        col.separator()

        col.prop(cscene, "offscreen_dicing_scale", text="Offscreen Scale")
        col.prop(cscene, "max_subdivisions")

        col.prop(cscene, "dicing_camera")


class CYCLES_RENDER_PT_hair(CyclesButtonsPanel, Panel):
    bl_label = "Hair"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout
        scene = context.scene
        ccscene = scene.cycles_curves

        layout.prop(ccscene, "use_curves", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        ccscene = scene.cycles_curves

        layout.active = ccscene.use_curves

        col = layout.column()
        col.prop(ccscene, "shape", text="Shape")
        if not (ccscene.primitive in {'CURVE_SEGMENTS', 'LINE_SEGMENTS'} and ccscene.shape == 'RIBBONS'):
            col.prop(ccscene, "cull_backfacing", text="Cull back-faces")
        col.prop(ccscene, "primitive", text="Primitive")

        if ccscene.primitive == 'TRIANGLES' and ccscene.shape == 'THICK':
            col.prop(ccscene, "resolution", text="Resolution")
        elif ccscene.primitive == 'CURVE_SEGMENTS':
            col.prop(ccscene, "subdivisions", text="Curve subdivisions")


class CYCLES_RENDER_PT_volumes(CyclesButtonsPanel, Panel):
    bl_label = "Volumes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "volume_step_size", text="Step Size")
        col.prop(cscene, "volume_max_steps", text="Max Steps")


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
        col.prop(cscene, "transparent_max_bounces", text="Transparency")
        col.prop(cscene, "transmission_bounces", text="Transmission")
        col.prop(cscene, "volume_bounces", text="Volume")


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
        layout.use_property_decorate = False

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

    def draw(self, context):
        pass


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


class CYCLES_RENDER_PT_performance_tiles(CyclesButtonsPanel, Panel):
    bl_label = "Tiles"
    bl_parent_id = "CYCLES_RENDER_PT_performance"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

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

    def draw(self, context):
        import _cycles

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        col = layout.column()

        if _cycles.with_embree:
            row = col.row()
            row.active = use_cpu(context)
            row.prop(cscene, "use_bvh_embree")
        col.prop(cscene, "debug_use_spatial_splits")
        sub = col.column()
        sub.active = not cscene.use_bvh_embree or not _cycles.with_embree
        sub.prop(cscene, "debug_use_hair_bvh")
        sub = col.column()
        sub.active = not cscene.debug_use_spatial_splits and not cscene.use_bvh_embree
        sub.prop(cscene, "debug_bvh_time_steps")


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

        col.prop(rd, "use_save_buffers")
        col.prop(rd, "use_persistent_data", text="Persistent Images")


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
        col.prop(cscene, "preview_start_resolution", text="Start Pixels")


class CYCLES_RENDER_PT_filter(CyclesButtonsPanel, Panel):
    bl_label = "Filter"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "view_layer"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        with_freestyle = bpy.app.build_options.freestyle

        scene = context.scene
        rd = scene.render
        view_layer = context.view_layer

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(view_layer, "use_sky", text="Environment")
        col = flow.column()
        col.prop(view_layer, "use_ao", text="Ambient Occlusion")
        col = flow.column()
        col.prop(view_layer, "use_solid", text="Surfaces")
        col = flow.column()
        col.prop(view_layer, "use_strand", text="Hair")
        if with_freestyle:
            col = flow.column()
            col.prop(view_layer, "use_freestyle", text="Freestyle")
            col.active = rd.use_freestyle


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

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        col = flow.column()
        col.prop(view_layer, "use_pass_combined")
        col = flow.column()
        col.prop(view_layer, "use_pass_z")
        col = flow.column()
        col.prop(view_layer, "use_pass_mist")
        col = flow.column()
        col.prop(view_layer, "use_pass_normal")
        col = flow.column()
        col.prop(view_layer, "use_pass_vector")
        col.active = not rd.use_motion_blur
        col = flow.column()
        col.prop(view_layer, "use_pass_uv")
        col = flow.column()
        col.prop(view_layer, "use_pass_object_index")
        col = flow.column()
        col.prop(view_layer, "use_pass_material_index")

        layout.separator()

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        col = flow.column()
        col.prop(cycles_view_layer, "denoising_store_passes", text="Denoising Data")
        col = flow.column()
        col.prop(cycles_view_layer, "pass_debug_render_time", text="Render Time")

        layout.separator()

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

        split = layout.split(factor=0.35)
        split.use_property_split = False
        split.label(text="Diffuse")
        row = split.row(align=True)
        row.prop(view_layer, "use_pass_diffuse_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_diffuse_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_diffuse_color", text="Color", toggle=True)

        split = layout.split(factor=0.35)
        split.use_property_split = False
        split.label(text="Glossy")
        row = split.row(align=True)
        row.prop(view_layer, "use_pass_glossy_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_glossy_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_glossy_color", text="Color", toggle=True)

        split = layout.split(factor=0.35)
        split.use_property_split = False
        split.label(text="Transmission")
        row = split.row(align=True)
        row.prop(view_layer, "use_pass_transmission_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_transmission_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_transmission_color", text="Color", toggle=True)

        split = layout.split(factor=0.35)
        split.use_property_split = False
        split.label(text="Subsurface")
        row = split.row(align=True)
        row.prop(view_layer, "use_pass_subsurface_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_subsurface_indirect", text="Indirect", toggle=True)
        row.prop(view_layer, "use_pass_subsurface_color", text="Color", toggle=True)

        split = layout.split(factor=0.35)
        split.use_property_split = False
        split.label(text="Volume")
        row = split.row(align=True)
        row.prop(cycles_view_layer, "use_pass_volume_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "use_pass_volume_indirect", text="Indirect", toggle=True)

        col = layout.column(align=True)
        col.prop(view_layer, "use_pass_emit", text="Emission")
        col.prop(view_layer, "use_pass_environment")
        col.prop(view_layer, "use_pass_shadow")
        col.prop(view_layer, "use_pass_ambient_occlusion", text="Ambient Occlusion")


class CYCLES_RENDER_PT_passes_crypto(CyclesButtonsPanel, Panel):
    bl_label = "Cryptomatte"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"

    def draw(self, context):
        import _cycles

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        cycles_view_layer = context.view_layer.cycles

        row = layout.row(align=True)
        row.use_property_split = False
        row.prop(cycles_view_layer, "use_pass_crypto_object", text="Object", toggle=True)
        row.prop(cycles_view_layer, "use_pass_crypto_material", text="Material", toggle=True)
        row.prop(cycles_view_layer, "use_pass_crypto_asset", text="Asset", toggle=True)

        layout.prop(cycles_view_layer, "pass_crypto_depth", text="Levels")

        row = layout.row(align=True)
        row.active = use_cpu(context)
        row.prop(cycles_view_layer, "pass_crypto_accurate", text="Accurate Mode")


class CYCLES_RENDER_PT_passes_debug(CyclesButtonsPanel, Panel):
    bl_label = "Debug"
    bl_context = "view_layer"
    bl_parent_id = "CYCLES_RENDER_PT_passes"

    @classmethod
    def poll(cls, context):
        import _cycles
        return _cycles.with_cycles_debug

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        cycles_view_layer = context.view_layer.cycles

        layout.prop(cycles_view_layer, "pass_debug_bvh_traversed_nodes")
        layout.prop(cycles_view_layer, "pass_debug_bvh_traversed_instances")
        layout.prop(cycles_view_layer, "pass_debug_bvh_intersections")
        layout.prop(cycles_view_layer, "pass_debug_ray_bounces")


class CYCLES_RENDER_PT_denoising(CyclesButtonsPanel, Panel):
    bl_label = "Denoising"
    bl_context = "view_layer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        scene = context.scene
        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles
        layout = self.layout

        layout.prop(cycles_view_layer, "use_denoising", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        view_layer = context.view_layer
        cycles_view_layer = view_layer.cycles

        split = layout.split()
        split.active = cycles_view_layer.use_denoising

        layout = layout.column(align=True)
        layout.prop(cycles_view_layer, "denoising_radius", text="Radius")
        layout.prop(cycles_view_layer, "denoising_strength", slider=True, text="Strength")
        layout.prop(cycles_view_layer, "denoising_feature_strength", slider=True, text="Feature Strength")
        layout.prop(cycles_view_layer, "denoising_relative_pca")

        layout.separator()

        split = layout.split(factor=0.5)
        split.active = cycles_view_layer.use_denoising or cycles_view_layer.denoising_store_passes

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Diffuse")

        row = split.row(align=True)
        row.use_property_split = False
        row.prop(cycles_view_layer, "denoising_diffuse_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_diffuse_indirect", text="Indirect", toggle=True)

        split = layout.split(factor=0.5)
        split.active = cycles_view_layer.use_denoising or cycles_view_layer.denoising_store_passes

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Glossy")

        row = split.row(align=True)
        row.use_property_split = False
        row.prop(cycles_view_layer, "denoising_glossy_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_glossy_indirect", text="Indirect", toggle=True)

        split = layout.split(factor=0.5)
        split.active = cycles_view_layer.use_denoising or cycles_view_layer.denoising_store_passes

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Transmission")

        row = split.row(align=True)
        row.use_property_split = False
        row.prop(cycles_view_layer, "denoising_transmission_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_transmission_indirect", text="Indirect", toggle=True)

        split = layout.split(factor=0.5)
        split.active = cycles_view_layer.use_denoising or cycles_view_layer.denoising_store_passes

        col = split.column()
        col.alignment = 'RIGHT'
        col.label(text="Subsurface")

        row = split.row(align=True)
        row.use_property_split = False
        row.prop(cycles_view_layer, "denoising_subsurface_direct", text="Direct", toggle=True)
        row.prop(cycles_view_layer, "denoising_subsurface_indirect", text="Indirect", toggle=True)


class CYCLES_PT_post_processing(CyclesButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "output"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

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
            col.operator("object.material_slot_add", icon='ADD', text="")
            col.operator("object.material_slot_remove", icon='REMOVE', text="")

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

        split = layout.split(factor=0.65)

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
                        (ob.instance_type == 'COLLECTION' and ob.instance_collection)))

    def draw(self, context):
        pass


class CYCLES_OBJECT_PT_cycles_settings_ray_visibility(CyclesButtonsPanel, Panel):
    bl_label = "Ray Visibility"
    bl_parent_id = "CYCLES_OBJECT_PT_cycles_settings"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        ob = context.object
        cob = ob.cycles
        visibility = ob.cycles_visibility

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(visibility, "camera")
        col = flow.column()
        col.prop(visibility, "diffuse")
        col = flow.column()
        col.prop(visibility, "glossy")
        col = flow.column()
        col.prop(visibility, "transmission")
        col = flow.column()
        col.prop(visibility, "scatter")

        if ob.type != 'LIGHT':
            col = flow.column()
            col.prop(visibility, "shadow")

        layout.separator()

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(cob, "is_shadow_catcher")
        col = flow.column()
        col.prop(cob, "is_holdout")


class CYCLES_OBJECT_PT_cycles_settings_performance(CyclesButtonsPanel, Panel):
    bl_label = "Performance"
    bl_parent_id = "CYCLES_OBJECT_PT_cycles_settings"
    bl_context = "object"


    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles
        ob = context.object
        cob = ob.cycles

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.active = scene.render.use_simplify and cscene.use_camera_cull
        col.prop(cob, "use_camera_cull")

        col = flow.column()
        col.active = scene.render.use_simplify and cscene.use_distance_cull
        col.prop(cob, "use_distance_cull")


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

        layout.use_property_decorate = False

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
            col.prop(light, "shadow_soft_size", text="Size")
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
        panel_node_draw(layout, light, 'OUTPUT_LIGHT', 'Surface')


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
        layout.use_property_decorate = False

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
        layout.use_property_decorate = False

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
        layout.use_property_decorate = False

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
        return mat and (not mat.grease_pencil) and mat.node_tree and CyclesButtonsPanel.poll(context)

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
        return mat and (not mat.grease_pencil) and mat.node_tree and CyclesButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

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
        col.prop(cmat, "sample_as_light", text="Multiple Importance")
        col.prop(cmat, "use_transparent_shadow")
        col.prop(cmat, "displacement_method", text="Displacement Method")

    def draw(self, context):
        self.draw_shared(self, context.material)


class CYCLES_MATERIAL_PT_settings_volume(CyclesButtonsPanel, Panel):
    bl_label = "Volume"
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
        sub.active = use_cpu(context)
        sub.prop(cmat, "volume_sampling", text="Sampling")
        col.prop(cmat, "volume_interpolation", text="Interpolation")
        col.prop(cmat, "homogeneous_volume", text="Homogeneous")

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
                'NORMAL', 'COMBINED', 'DIFFUSE', 'GLOSSY', 'TRANSMISSION', 'SUBSURFACE'}:
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
            sub.prop(cbk, "normal_g", text="G")
            sub.prop(cbk, "normal_b", text="B")

        elif cscene.bake_type == 'COMBINED':
            row = col.row(align=True)
            row.use_property_split = False
            row.prop(cbk, "use_pass_direct", toggle=True)
            row.prop(cbk, "use_pass_indirect", toggle=True)

            flow = col.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=True)

            flow.active = cbk.use_pass_direct or cbk.use_pass_indirect
            flow.prop(cbk, "use_pass_diffuse")
            flow.prop(cbk, "use_pass_glossy")
            flow.prop(cbk, "use_pass_transmission")
            flow.prop(cbk, "use_pass_subsurface")
            flow.prop(cbk, "use_pass_ambient_occlusion")
            flow.prop(cbk, "use_pass_emit")

        elif cscene.bake_type in {'DIFFUSE', 'GLOSSY', 'TRANSMISSION', 'SUBSURFACE'}:
            row = col.row(align=True)
            row.use_property_split = False
            row.prop(cbk, "use_pass_direct", toggle=True)
            row.prop(cbk, "use_pass_indirect", toggle=True)
            row.prop(cbk, "use_pass_color", toggle=True)


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
            col.prop(cbk, "cage_extrusion", text="Extrusion")
            col.prop(cbk, "cage_object", text="Cage Object")
        else:
            col.prop(cbk, "cage_extrusion", text="Ray Distance")


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
            layout.prop(rd, "bake_margin")
            layout.prop(rd, "use_bake_clear", text="Clear Image")

            if rd.bake_type == 'DISPLACEMENT':
                col.prop(rd, "use_bake_lores_mesh")
        else:

            layout.prop(cbk, "margin")
            layout.prop(cbk, "use_clear", text="Clear Image")


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

        col.label(text="CPU Flags:")
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
        col.label(text="CUDA Flags:")
        col.prop(cscene, "debug_use_cuda_adaptive_compile")
        col.prop(cscene, "debug_use_cuda_split_kernel")

        col.separator()

        col = layout.column()
        col.label(text='OpenCL Flags:')
        col.prop(cscene, "debug_opencl_device_type", text="Device")
        col.prop(cscene, "debug_use_opencl_debug", text="Debug")
        col.prop(cscene, "debug_opencl_mem_limit")

        col.separator()

        col = layout.column()
        col.prop(cscene, "debug_bvh_type")


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
        col.prop(cscene, "ao_bounces", text="AO Bounces")
        col.prop(rd, "use_simplify_smoke_highres")

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
        col.prop(cscene, "ao_bounces_render", text="AO Bounces")


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
    layout.use_property_decorate = False

    if context.engine == 'CYCLES':
        from . import engine
        cscene = scene.cycles

        col = layout.column()
        col.prop(cscene, "feature_set")

        scene = context.scene

        col = layout.column()
        col.active = show_device_active(context)
        col.prop(cscene, "device")

        from . import engine
        if engine.with_osl() and use_cpu(context):
            col.prop(cscene, "shading_system")


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
        'NODE_DATA_PT_light',
        'NODE_DATA_PT_spot',
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
    CYCLES_PT_integrator_presets,
    CYCLES_RENDER_PT_sampling,
    CYCLES_RENDER_PT_sampling_sub_samples,
    CYCLES_RENDER_PT_sampling_advanced,
    CYCLES_RENDER_PT_light_paths,
    CYCLES_RENDER_PT_light_paths_max_bounces,
    CYCLES_RENDER_PT_light_paths_clamping,
    CYCLES_RENDER_PT_light_paths_caustics,
    CYCLES_RENDER_PT_volumes,
    CYCLES_RENDER_PT_subdivision,
    CYCLES_RENDER_PT_hair,
    CYCLES_RENDER_PT_simplify,
    CYCLES_RENDER_PT_simplify_viewport,
    CYCLES_RENDER_PT_simplify_render,
    CYCLES_RENDER_PT_simplify_culling,
    CYCLES_RENDER_PT_motion_blur,
    CYCLES_RENDER_PT_motion_blur_curve,
    CYCLES_RENDER_PT_film,
    CYCLES_RENDER_PT_film_pixel_filter,
    CYCLES_RENDER_PT_film_transparency,
    CYCLES_RENDER_PT_performance,
    CYCLES_RENDER_PT_performance_threads,
    CYCLES_RENDER_PT_performance_tiles,
    CYCLES_RENDER_PT_performance_acceleration_structure,
    CYCLES_RENDER_PT_performance_final_render,
    CYCLES_RENDER_PT_performance_viewport,
    CYCLES_RENDER_PT_passes,
    CYCLES_RENDER_PT_passes_data,
    CYCLES_RENDER_PT_passes_light,
    CYCLES_RENDER_PT_passes_crypto,
    CYCLES_RENDER_PT_passes_debug,
    CYCLES_RENDER_PT_filter,
    CYCLES_RENDER_PT_override,
    CYCLES_RENDER_PT_denoising,
    CYCLES_PT_post_processing,
    CYCLES_CAMERA_PT_dof,
    CYCLES_CAMERA_PT_dof_aperture,
    CYCLES_CAMERA_PT_dof_viewport,
    CYCLES_PT_context_material,
    CYCLES_OBJECT_PT_motion_blur,
    CYCLES_OBJECT_PT_cycles_settings,
    CYCLES_OBJECT_PT_cycles_settings_ray_visibility,
    CYCLES_OBJECT_PT_cycles_settings_performance,
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
    CYCLES_RENDER_PT_bake_influence,
    CYCLES_RENDER_PT_bake_selected_to_active,
    CYCLES_RENDER_PT_bake_output,
    CYCLES_RENDER_PT_debug,
    node_panel(CYCLES_MATERIAL_PT_settings),
    node_panel(CYCLES_MATERIAL_PT_settings_surface),
    node_panel(CYCLES_MATERIAL_PT_settings_volume),
    node_panel(CYCLES_WORLD_PT_ray_visibility),
    node_panel(CYCLES_WORLD_PT_settings),
    node_panel(CYCLES_WORLD_PT_settings_surface),
    node_panel(CYCLES_WORLD_PT_settings_volume),
    node_panel(CYCLES_LIGHT_PT_light),
    node_panel(CYCLES_LIGHT_PT_spot),
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
