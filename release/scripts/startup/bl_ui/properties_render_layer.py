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


class RenderLayerButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.view_render.engine in cls.COMPAT_ENGINES)


class RENDERLAYER_UL_renderlayers(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.SceneLayer)
        layer = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(layer, "name", text="", icon_value=icon, emboss=False)
            layout.prop(layer, "use", text="", index=index)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon)


class RENDERLAYER_PT_layers(RenderLayerButtonsPanel, Panel):
    bl_label = "Layer List"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        view_render = scene.view_render

        if view_render.engine == 'BLENDER_GAME':
            layout.label("Not available in the Game Engine")
            return

        row = layout.row()
        col = row.column()
        col.template_list("RENDERLAYER_UL_renderlayers", "", scene, "render_layers", scene.render_layers, "active_index", rows=2)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("scene.render_layer_add", icon='ZOOMIN', text="")
        sub.operator("scene.render_layer_remove", icon='ZOOMOUT', text="")
        col.prop(rd, "use_single_layer", icon_only=True)


class RENDERLAYER_UL_renderviews(UIList):
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


class RENDERLAYER_PT_views(RenderLayerButtonsPanel, Panel):
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


class RENDERLAYER_PT_clay_settings(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_CLAY']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "ssao_samples")


class RENDERLAYER_PT_eevee_ambient_occlusion(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "gtao_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "gtao_use_bent_normals")
        col.template_override_property(layer_props, scene_props, "gtao_denoise")
        col.template_override_property(layer_props, scene_props, "gtao_bounce")
        col.template_override_property(layer_props, scene_props, "gtao_samples")
        col.template_override_property(layer_props, scene_props, "gtao_distance")
        col.template_override_property(layer_props, scene_props, "gtao_factor")
        col.template_override_property(layer_props, scene_props, "gtao_quality")


class RENDERLAYER_PT_eevee_motion_blur(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "motion_blur_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "motion_blur_samples")
        col.template_override_property(layer_props, scene_props, "motion_blur_shutter")


class RENDERLAYER_PT_eevee_depth_of_field(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "dof_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "bokeh_max_size")
        col.template_override_property(layer_props, scene_props, "bokeh_threshold")


class RENDERLAYER_PT_eevee_bloom(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "bloom_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "bloom_threshold")
        col.template_override_property(layer_props, scene_props, "bloom_knee")
        col.template_override_property(layer_props, scene_props, "bloom_radius")
        col.template_override_property(layer_props, scene_props, "bloom_color")
        col.template_override_property(layer_props, scene_props, "bloom_intensity")
        col.template_override_property(layer_props, scene_props, "bloom_clamp")


class RENDERLAYER_PT_eevee_volumetric(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "volumetric_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "volumetric_start")
        col.template_override_property(layer_props, scene_props, "volumetric_end")
        col.template_override_property(layer_props, scene_props, "volumetric_samples")
        col.template_override_property(layer_props, scene_props, "volumetric_sample_distribution")
        col.template_override_property(layer_props, scene_props, "volumetric_lights")
        col.template_override_property(layer_props, scene_props, "volumetric_light_clamp")
        col.template_override_property(layer_props, scene_props, "volumetric_shadows")
        col.template_override_property(layer_props, scene_props, "volumetric_shadow_samples")
        col.template_override_property(layer_props, scene_props, "volumetric_colored_transmittance")


class RENDERLAYER_PT_eevee_screen_space_reflections(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        self.layout.template_override_property(layer_props, scene_props, "ssr_enable", text="")

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        scene_props = scene.layer_properties['BLENDER_EEVEE']
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "ssr_halfres")
        col.template_override_property(layer_props, scene_props, "ssr_refraction")
        col.template_override_property(layer_props, scene_props, "ssr_ray_count")
        col.template_override_property(layer_props, scene_props, "ssr_quality")
        col.template_override_property(layer_props, scene_props, "ssr_max_roughness")
        col.template_override_property(layer_props, scene_props, "ssr_thickness")
        col.template_override_property(layer_props, scene_props, "ssr_border_fade")
        col.template_override_property(layer_props, scene_props, "ssr_firefly_fac")


class RENDERLAYER_PT_eevee_shadows(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "shadow_method")
        col.template_override_property(layer_props, scene_props, "shadow_size")
        col.template_override_property(layer_props, scene_props, "shadow_high_bitdepth")


class RENDERLAYER_PT_eevee_sampling(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "taa_samples")


class RENDERLAYER_PT_eevee_indirect_lighting(RenderLayerButtonsPanel, Panel):
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
        layer = bpy.context.render_layer
        layer_props = layer.engine_overrides['BLENDER_EEVEE']

        col = layout.column()
        col.template_override_property(layer_props, scene_props, "gi_diffuse_bounces")


classes = (
    RENDERLAYER_UL_renderlayers,
    RENDERLAYER_PT_layers,
    RENDERLAYER_UL_renderviews,
    RENDERLAYER_PT_views,
    RENDERLAYER_PT_clay_settings,
    RENDERLAYER_PT_eevee_sampling,
    RENDERLAYER_PT_eevee_shadows,
    RENDERLAYER_PT_eevee_indirect_lighting,
    RENDERLAYER_PT_eevee_screen_space_reflections,
    RENDERLAYER_PT_eevee_ambient_occlusion,
    RENDERLAYER_PT_eevee_volumetric,
    RENDERLAYER_PT_eevee_motion_blur,
    RENDERLAYER_PT_eevee_depth_of_field,
    RENDERLAYER_PT_eevee_bloom,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
