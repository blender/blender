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
from bpy.types import Panel, UIList


class ViewLayerButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "view_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)


class VIEWLAYER_UL_viewlayers(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.SceneLayer)
        layer = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(layer, "name", text="", icon_value=icon, emboss=False)
            layout.prop(layer, "use", text="", index=index)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon)


class VIEWLAYER_PT_layers(ViewLayerButtonsPanel, Panel):
    bl_label = "Layer List"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        view_render = scene.view_render

        row = layout.row()
        col = row.column()
        col.template_list("VIEWLAYER_UL_viewlayers", "", scene, "view_layers", scene.view_layers, "active_index", rows=2)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("scene.view_layer_add", icon='ZOOMIN', text="")
        sub.operator("scene.view_layer_remove", icon='ZOOMOUT', text="")
        col.prop(rd, "use_single_layer", icon_only=True)


class VIEWLAYER_UL_renderviews(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.SceneRenderView)
        view = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if view.name in {'left', 'right'}:
                layout.label(view.name, icon_value=icon + (not view.use))
            else:
                layout.prop(view, "name", text="", index=index, icon_value=icon, emboss=False)
            layout.prop(view, "use", text="", index=index)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon + (not view.use))


class VIEWLAYER_PT_views(ViewLayerButtonsPanel, Panel):
    bl_label = "Views"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}
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
        basic_stereo = rd.views_format == 'STEREO_3D'

        row = layout.row()
        row.prop(rd, "views_format", expand=True)

        if basic_stereo:
            row = layout.row()
            row.template_list("VIEWLAYER_UL_renderviews", "name", rd, "stereo_views", rd.views, "active_index", rows=2)

            row = layout.row()
            row.label(text="File Suffix:")
            row.prop(rv, "file_suffix", text="")

        else:
            row = layout.row()
            row.template_list("VIEWLAYER_UL_renderviews", "name", rd, "views", rd.views, "active_index", rows=2)

            col = row.column(align=True)
            col.operator("scene.render_view_add", icon='ZOOMIN', text="")
            col.operator("scene.render_view_remove", icon='ZOOMOUT', text="")

            row = layout.row()
            row.label(text="Camera Suffix:")
            row.prop(rv, "camera_suffix", text="")


class VIEWLAYER_PT_clay_settings(ViewLayerButtonsPanel, Panel):
    bl_label = "Render Settings"
    COMPAT_ENGINES = {'BLENDER_CLAY'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_CLAY']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_CLAY']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "ssao_samples")


class VIEWLAYER_PT_eevee_ambient_occlusion(ViewLayerButtonsPanel, Panel):
    bl_label = "Ambient Occlusion"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "gtao_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "gtao_use_bent_normals")
        col.template_override_property(layer_props, scene_props, "gtao_bounce")
        col.template_override_property(layer_props, scene_props, "gtao_distance")
        col.template_override_property(layer_props, scene_props, "gtao_factor")
        col.template_override_property(layer_props, scene_props, "gtao_quality")


class VIEWLAYER_PT_eevee_motion_blur(ViewLayerButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "motion_blur_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "motion_blur_samples")
        col.template_override_property(layer_props, scene_props, "motion_blur_shutter")


class VIEWLAYER_PT_eevee_depth_of_field(ViewLayerButtonsPanel, Panel):
    bl_label = "Depth Of Field"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "dof_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "bokeh_max_size")
        col.template_override_property(layer_props, scene_props, "bokeh_threshold")


class VIEWLAYER_PT_eevee_bloom(ViewLayerButtonsPanel, Panel):
    bl_label = "Bloom"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "bloom_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "bloom_threshold")
        col.template_override_property(layer_props, scene_props, "bloom_knee")
        col.template_override_property(layer_props, scene_props, "bloom_radius")
        col.template_override_property(layer_props, scene_props, "bloom_color")
        col.template_override_property(layer_props, scene_props, "bloom_intensity")
        col.template_override_property(layer_props, scene_props, "bloom_clamp")


class VIEWLAYER_PT_eevee_volumetric(ViewLayerButtonsPanel, Panel):
    bl_label = "Volumetric"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "volumetric_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "volumetric_start")
        col.template_override_property(layer_props, scene_props, "volumetric_end")
        col.template_override_property(layer_props, scene_props, "volumetric_tile_size")
        col.template_override_property(layer_props, scene_props, "volumetric_samples")
        col.template_override_property(layer_props, scene_props, "volumetric_sample_distribution")
        col.template_override_property(layer_props, scene_props, "volumetric_lights")
        col.template_override_property(layer_props, scene_props, "volumetric_light_clamp")
        col.template_override_property(layer_props, scene_props, "volumetric_shadows")
        col.template_override_property(layer_props, scene_props, "volumetric_shadow_samples")
        col.template_override_property(layer_props, scene_props, "volumetric_colored_transmittance")


class VIEWLAYER_PT_eevee_subsurface_scattering(ViewLayerButtonsPanel, Panel):
    bl_label = "Subsurface Scattering"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "sss_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "sss_samples")
        col.template_override_property(layer_props, scene_props, "sss_jitter_threshold")
        col.template_override_property(layer_props, scene_props, "sss_separate_albedo")


class VIEWLAYER_PT_eevee_screen_space_reflections(ViewLayerButtonsPanel, Panel):
    bl_label = "Screen Space Reflections"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "ssr_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "ssr_halfres")
        col.template_override_property(layer_props, scene_props, "ssr_refraction")
        col.template_override_property(layer_props, scene_props, "ssr_quality")
        col.template_override_property(layer_props, scene_props, "ssr_max_roughness")
        col.template_override_property(layer_props, scene_props, "ssr_thickness")
        col.template_override_property(layer_props, scene_props, "ssr_border_fade")
        col.template_override_property(layer_props, scene_props, "ssr_firefly_fac")


class VIEWLAYER_PT_eevee_shadows(ViewLayerButtonsPanel, Panel):
    bl_label = "Shadows"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "shadow_method")
        col.template_override_property(layer_props, scene_props, "shadow_size")
        col.template_override_property(layer_props, scene_props, "shadow_high_bitdepth")


class VIEWLAYER_PT_eevee_sampling(ViewLayerButtonsPanel, Panel):
    bl_label = "Sampling"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "taa_samples")
        col.template_override_property(layer_props, scene_props, "taa_render_samples")


class VIEWLAYER_PT_eevee_indirect_lighting(ViewLayerButtonsPanel, Panel):
    bl_label = "Indirect Lighting"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.view_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "gi_diffuse_bounces")
        col.template_override_property(layer_props, scene_props, "gi_cubemap_resolution")
        col.template_override_property(layer_props, scene_props, "gi_visibility_resolution")


class VIEWLAYER_PT_eevee_layer_passes(ViewLayerButtonsPanel, Panel):
    bl_label = "Passes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        view_layer = scene.view_layers.active

        split = layout.split()

        col = split.column()
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")
        col.prop(view_layer, "use_pass_mist")
        col.prop(view_layer, "use_pass_normal")
        col.separator()
        col.prop(view_layer, "use_pass_ambient_occlusion")

        col = split.column()
        col.label(text="Subsurface:")
        row = col.row(align=True)
        row.prop(view_layer, "use_pass_subsurface_direct", text="Direct", toggle=True)
        row.prop(view_layer, "use_pass_subsurface_color", text="Color", toggle=True)


classes = (
    VIEWLAYER_UL_viewlayers,
    VIEWLAYER_PT_layers,
    VIEWLAYER_UL_renderviews,
    VIEWLAYER_PT_views,
    VIEWLAYER_PT_clay_settings,
    VIEWLAYER_PT_eevee_sampling,
    VIEWLAYER_PT_eevee_shadows,
    VIEWLAYER_PT_eevee_indirect_lighting,
    VIEWLAYER_PT_eevee_subsurface_scattering,
    VIEWLAYER_PT_eevee_screen_space_reflections,
    VIEWLAYER_PT_eevee_ambient_occlusion,
    VIEWLAYER_PT_eevee_volumetric,
    VIEWLAYER_PT_eevee_motion_blur,
    VIEWLAYER_PT_eevee_depth_of_field,
    VIEWLAYER_PT_eevee_bloom,
    VIEWLAYER_PT_eevee_layer_passes,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
