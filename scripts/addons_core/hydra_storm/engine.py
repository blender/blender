# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import bpy


class StormHydraRenderEngine(bpy.types.HydraRenderEngine):
    bl_idname = 'HYDRA_STORM'
    bl_label = "Hydra Storm"
    bl_info = "USD's high performance rasterizing renderer"

    bl_use_preview = True
    bl_use_gpu_context = True
    bl_use_materialx = True

    bl_delegate_id = 'HdStormRendererPlugin'

    def get_render_settings(self, engine_type):
        settings = bpy.context.scene.hydra_storm.viewport if engine_type == 'VIEWPORT' else \
            bpy.context.scene.hydra_storm.final
        result = {
            'enableTinyPrimCulling': settings.use_tiny_prim_culling,
            'maxLights': settings.max_lights,
            'volumeRaymarchingStepSize': settings.volume_raymarching_step_size,
            'volumeRaymarchingStepSizeLighting': settings.volume_raymarching_step_size_lighting,
            'volumeMaxTextureMemoryPerField': settings.volume_max_texture_memory_per_field,
        }

        if engine_type != 'VIEWPORT':
            result |= {
                'aovToken:Combined': "color",
                'aovToken:Depth': "depth",
            }

        return result

    def update_render_passes(self, scene, render_layer):
        if render_layer.use_pass_combined:
            self.register_pass(scene, render_layer, 'Combined', 4, 'RGBA', 'COLOR')
        if render_layer.use_pass_z:
            self.register_pass(scene, render_layer, 'Depth', 1, 'Z', 'VALUE')


register, unregister = bpy.utils.register_classes_factory((
    StormHydraRenderEngine,
))
