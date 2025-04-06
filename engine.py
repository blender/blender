

import bpy


class AquaRenderEngine(bpy.types.AquaRenderEngine):
    bl_idname = 'AQUA'
    bl_label = "Aqua"
    bl_info = "A brand new render engine for Blender"

    bl_use_preview = True
    bl_use_gpu_context = True
    bl_use_materialx = True

    bl_delegate_id = "AquaRendererPlugin"
    def get_render_settings(self, engine_type):
        settings = bpy.context.scene.aqua.viewport if engine_type == 'VIEWPORT' else \
            bpy.context.scene.aqua.final
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
    AquaRenderEngine,
))
