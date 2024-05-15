# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import bpy

from .engine import StormHydraRenderEngine


class Panel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'render'
    COMPAT_ENGINES = {StormHydraRenderEngine.bl_idname}

    @classmethod
    def poll(cls, context):
        return context.engine in cls.COMPAT_ENGINES


#
# Quality render settings
#
class STORM_HYDRA_RENDER_PT_quality(Panel):
    bl_label = "Quality"

    def draw(self, layout):
        pass


class STORM_HYDRA_RENDER_PT_quality_viewport(Panel):
    bl_label = "Viewport"
    bl_parent_id = "STORM_HYDRA_RENDER_PT_quality"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.scene.hydra_storm.viewport
        layout.prop(settings, 'max_lights')
        layout.prop(settings, 'use_tiny_prim_culling')


class STORM_HYDRA_RENDER_PT_quality_render(Panel):
    bl_label = "Render"
    bl_parent_id = "STORM_HYDRA_RENDER_PT_quality"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.scene.hydra_storm.final
        layout.prop(settings, 'max_lights')
        layout.prop(settings, 'use_tiny_prim_culling')


#
# Volume render settings
#
class STORM_HYDRA_RENDER_PT_volumes(Panel):
    bl_label = "Volumes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, layout):
        pass


class STORM_HYDRA_RENDER_PT_volumes_viewport(Panel):
    bl_label = "Viewport"
    bl_parent_id = "STORM_HYDRA_RENDER_PT_volumes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.scene.hydra_storm.viewport

        col = layout.column(align=True)
        col.prop(settings, "volume_raymarching_step_size", text="Step Size")
        col.prop(settings, "volume_raymarching_step_size_lighting", text="Step Size Lightning")
        col.prop(settings, "volume_max_texture_memory_per_field")


class STORM_HYDRA_RENDER_PT_volumes_render(Panel):
    bl_label = "Render"
    bl_parent_id = "STORM_HYDRA_RENDER_PT_volumes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.scene.hydra_storm.final

        col = layout.column(align=True)
        col.prop(settings, "volume_raymarching_step_size", text="Step Size")
        col.prop(settings, "volume_raymarching_step_size_lighting", text="Step Size Lightning")
        col.prop(settings, "volume_max_texture_memory_per_field")


#
# Film settings
#
class STORM_HYDRA_RENDER_PT_film(Panel):
    bl_label = "Film"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(context.scene.render, "film_transparent", text="Transparent Background")


#
# View layer settings
#
class STORM_HYDRA_RENDER_PT_passes(Panel):
    bl_label = "Passes"
    bl_context = "view_layer"

    def draw(self, context):
        pass


class STORM_HYDRA_RENDER_PT_passes_data(Panel):
    bl_label = "Data"
    bl_context = "view_layer"
    bl_parent_id = "STORM_HYDRA_RENDER_PT_passes"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        col = layout.column(heading="Include", align=True)
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")


#
# Light settings
#
class STORM_HYDRA_LIGHT_PT_light(Panel):
    """Physical light sources"""
    bl_label = "Light"
    bl_context = 'data'

    @classmethod
    def poll(cls, context):
        return super().poll(context) and context.light

    def draw(self, context):
        layout = self.layout

        light = context.light

        layout.prop(light, "type", expand=True)

        layout.use_property_split = True
        layout.use_property_decorate = False

        main_col = layout.column()

        main_col.prop(light, "color")
        main_col.prop(light, "energy")
        main_col.separator()

        if light.type == 'POINT':
            row = main_col.row(align=True)
            row.prop(light, "shadow_soft_size", text="Radius")

        elif light.type == 'SPOT':
            col = main_col.column(align=True)
            col.prop(light, 'spot_size', slider=True)
            col.prop(light, 'spot_blend', slider=True)

            main_col.prop(light, 'show_cone')

        elif light.type == 'SUN':
            main_col.prop(light, "angle")

        elif light.type == 'AREA':
            main_col.prop(light, "shape", text="Shape")
            sub = main_col.column(align=True)

            if light.shape in {'SQUARE', 'DISK'}:
                sub.prop(light, "size")
            elif light.shape in {'RECTANGLE', 'ELLIPSE'}:
                sub.prop(light, "size", text="Size X")
                sub.prop(light, "size_y", text="Y")

            else:
                main_col.prop(light, 'size')


register_classes, unregister_classes = bpy.utils.register_classes_factory((
    STORM_HYDRA_RENDER_PT_quality,
    STORM_HYDRA_RENDER_PT_quality_viewport,
    STORM_HYDRA_RENDER_PT_quality_render,
    STORM_HYDRA_RENDER_PT_volumes,
    STORM_HYDRA_RENDER_PT_volumes_viewport,
    STORM_HYDRA_RENDER_PT_volumes_render,
    STORM_HYDRA_RENDER_PT_film,
    STORM_HYDRA_LIGHT_PT_light,
    STORM_HYDRA_RENDER_PT_passes,
    STORM_HYDRA_RENDER_PT_passes_data,
))


def get_panels():
    # Follow the Cycles model of excluding panels we don't want.
    exclude_panels = {
        'RENDER_PT_stamp',
        'DATA_PT_light',
        'DATA_PT_spot',
        'NODE_DATA_PT_light',
        'DATA_PT_falloff_curve',
        'RENDER_PT_post_processing',
        'RENDER_PT_simplify',
        'SCENE_PT_audio',
        'RENDER_PT_freestyle'
    }
    include_eevee_panels = {
        'MATERIAL_PT_preview',
        'EEVEE_MATERIAL_PT_context_material',
        'EEVEE_MATERIAL_PT_surface',
        'EEVEE_MATERIAL_PT_volume',
        'EEVEE_MATERIAL_PT_settings',
        'EEVEE_WORLD_PT_surface',
    }

    for panel_cls in bpy.types.Panel.__subclasses__():
        if hasattr(panel_cls, 'COMPAT_ENGINES') and (
            ('BLENDER_RENDER' in panel_cls.COMPAT_ENGINES and panel_cls.__name__ not in exclude_panels) or
            ('BLENDER_EEVEE' in panel_cls.COMPAT_ENGINES and panel_cls.__name__ in include_eevee_panels)
        ):
            yield panel_cls


def register():
    register_classes()

    for panel_cls in get_panels():
        panel_cls.COMPAT_ENGINES.add(StormHydraRenderEngine.bl_idname)


def unregister():
    unregister_classes()

    for panel_cls in get_panels():
        if StormHydraRenderEngine.bl_idname in panel_cls.COMPAT_ENGINES:
            panel_cls.COMPAT_ENGINES.remove(StormHydraRenderEngine.bl_idname)
