# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

bl_info = {
    "name": "Cycles Render Engine",
    "author": "",
    "blender": (2, 80, 0),
    "description": "Cycles renderer integration",
    "warning": "",
    "doc_url": "https://docs.blender.org/manual/en/latest/render/cycles/",
    "tracker_url": "",
    "support": 'OFFICIAL',
    "category": "Render"}

# Support 'reload' case.
if "bpy" in locals():
    import importlib
    if "engine" in locals():
        importlib.reload(engine)
    if "version_update" in locals():
        importlib.reload(version_update)
    if "ui" in locals():
        importlib.reload(ui)
    if "operators" in locals():
        importlib.reload(operators)
    if "properties" in locals():
        importlib.reload(properties)
    if "presets" in locals():
        importlib.reload(presets)

import bpy

from . import (
    engine,
    version_update,
)


class CyclesRender(bpy.types.RenderEngine):
    bl_idname = 'CYCLES'
    bl_label = "Cycles"
    bl_use_eevee_viewport = True
    bl_use_preview = True
    bl_use_exclude_layers = True
    bl_use_spherical_stereo = True
    bl_use_custom_freestyle = True

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.session = None

    def __del__(self):
        engine.free(self)

    # final render
    def update(self, data, depsgraph):
        if not self.session:
            if self.is_preview:
                cscene = bpy.context.scene.cycles
                use_osl = cscene.shading_system

                engine.create(self, data, preview_osl=use_osl)
            else:
                engine.create(self, data)

        engine.reset(self, data, depsgraph)

    def render(self, depsgraph):
        engine.render(self, depsgraph)

    def render_frame_finish(self):
        engine.render_frame_finish(self)

    def draw(self, context, depsgraph):
        engine.draw(self, depsgraph, context.space_data)

    def bake(self, depsgraph, obj, pass_type, pass_filter, width, height):
        engine.bake(self, depsgraph, obj, pass_type, pass_filter, width, height)

    # viewport render
    def view_update(self, context, depsgraph):
        if not self.session:
            # When starting a new render session in viewport (by switching
            # viewport to Rendered shading) unpause the render. The way to think
            # of it is: artist requests render, so we start to render.
            # Do it for both original and evaluated scene so that Cycles
            # immediately reacts to un-paused render.
            cscene = context.scene.cycles
            cscene_eval = depsgraph.scene_eval.cycles
            if cscene.preview_pause or cscene_eval.preview_pause:
                cscene.preview_pause = False
                cscene_eval.preview_pause = False

            engine.create(self, context.blend_data,
                          context.region, context.space_data, context.region_data)

        engine.reset(self, context.blend_data, depsgraph)
        engine.sync(self, depsgraph, context.blend_data)

    def view_draw(self, context, depsgraph):
        engine.view_draw(self, depsgraph, context.region, context.space_data, context.region_data)

    def update_script_node(self, node):
        if engine.with_osl():
            from . import osl
            osl.update_script_node(node, self.report)
        else:
            self.report({'ERROR'}, "OSL support disabled in this build")

    def update_custom_camera(self, cam):
        if engine.with_osl():
            from . import osl
            osl.update_custom_camera_shader(cam, self.report)
        else:
            self.report({'ERROR'}, "OSL support disabled in this build")

    def update_render_passes(self, scene, srl):
        engine.register_passes(self, scene, srl)


def engine_exit():
    engine.exit()


classes = (
    CyclesRender,
)


def register():
    from bpy.utils import register_class
    from . import ui
    from . import operators
    from . import properties
    from . import presets
    import atexit

    # Make sure we only registered the callback once.
    atexit.unregister(engine_exit)
    atexit.register(engine_exit)

    engine.init()

    properties.register()
    ui.register()
    operators.register()
    presets.register()

    for cls in classes:
        register_class(cls)

    bpy.app.handlers.version_update.append(version_update.do_versions)


def unregister():
    from bpy.utils import unregister_class
    from . import ui
    from . import operators
    from . import properties
    from . import presets

    bpy.app.handlers.version_update.remove(version_update.do_versions)

    ui.unregister()
    operators.unregister()
    properties.unregister()
    presets.unregister()

    for cls in classes:
        unregister_class(cls)
