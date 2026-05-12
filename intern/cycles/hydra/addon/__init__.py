# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# Basic add-on for the Cycles Hydra render delegate. This is very incomplete
# and intended for developer testing only. The most obvious limitation is that
# materials and render settings are not supported.

import bpy

bl_info = {
    "name": "Hydra Cycles render engine",
    "author": "Blender Foundation",
    "version": (0, 1, 0),
    "blender": (5, 0, 0),
    "description": "Cycles path tracing renderer using the Hydra render delegate",
    "support": 'OFFICIAL',
    "category": "Render",
}


class CyclesHydraRenderEngine(bpy.types.HydraRenderEngine):
    bl_idname = 'HYDRA_CYCLES'
    bl_label = "Hydra Cycles"
    bl_info = "Cycles path tracing renderer using the Hydra render delegate"

    bl_use_preview = False
    bl_use_gpu_context = False
    bl_use_materialx = False

    bl_delegate_id = 'HdCyclesPlugin'

    @classmethod
    def register(cls):
        bpy.utils.expose_bundled_modules()

        import os
        plugin_dir = os.path.normpath(
            os.path.join(os.path.dirname(__file__), "..", "cycles", "hydra")
        )
        if not os.path.isfile(os.path.join(plugin_dir, "plugInfo.json")):
            print("Hydra Cycles: plugInfo.json not found at", plugin_dir)
            return

        import pxr.Plug
        pxr.Plug.Registry().RegisterPlugins([plugin_dir])

    def get_render_settings(self, engine_type):
        cscene = bpy.context.scene.cycles
        samples = cscene.preview_samples if engine_type == 'VIEWPORT' else cscene.samples
        result = {'cycles:samples': samples}
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


def _shared_panels():
    # Use all the same panels as regular Cycles, even if most options are
    # currently not supported. But for the ones that are supported it's not
    # worth making custom panels just for developer testing.
    for panel in bpy.types.Panel.__subclasses__():
        engines = getattr(panel, 'COMPAT_ENGINES', None)
        if engines and 'CYCLES' in engines:
            yield panel


def register():
    bpy.utils.register_class(CyclesHydraRenderEngine)

    for panel in _shared_panels():
        panel.COMPAT_ENGINES.add(CyclesHydraRenderEngine.bl_idname)


def unregister():
    for panel in _shared_panels():
        panel.COMPAT_ENGINES.discard(CyclesHydraRenderEngine.bl_idname)

    bpy.utils.unregister_class(CyclesHydraRenderEngine)
