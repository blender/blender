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

bl_info = {
    "name": "Cycles Render Engine",
    "author": "",
    "blender": (2, 80, 0),
    "description": "Cycles renderer integration",
    "warning": "",
    "wiki_url": "https://docs.blender.org/manual/en/dev/render/cycles/",
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
    bl_use_shading_nodes = True
    bl_use_preview = True
    bl_use_exclude_layers = True
    bl_use_save_buffers = True
    bl_use_spherical_stereo = True

    def __init__(self):
        self.session = None

    def __del__(self):
        engine.free(self)

    # final render
    def update(self, data, depsgraph):
        if not self.session:
            if self.is_preview:
                cscene = bpy.context.scene.cycles
                use_osl = cscene.shading_system and cscene.device == 'CPU'

                engine.create(self, data, preview_osl=use_osl)
            else:
                engine.create(self, data)

        engine.reset(self, data, depsgraph)

    def render(self, depsgraph):
        engine.render(self, depsgraph)

    def bake(self, depsgraph, obj, pass_type, pass_filter, object_id, pixel_array, num_pixels, depth, result):
        engine.bake(self, depsgraph, obj, pass_type, pass_filter, object_id, pixel_array, num_pixels, depth, result)

    # viewport render
    def view_update(self, context, depsgraph):
        if not self.session:
            engine.create(self, context.blend_data,
                          context.region, context.space_data, context.region_data)

        engine.reset(self, context.blend_data, depsgraph)
        engine.sync(self, depsgraph, context.blend_data)

    def view_draw(self, context, depsgraph):
        engine.draw(self, depsgraph, context.region, context.space_data, context.region_data)

    def update_script_node(self, node):
        if engine.with_osl():
            from . import osl
            osl.update_script_node(node, self.report)
        else:
            self.report({'ERROR'}, "OSL support disabled in this build.")

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
    import atexit

    bpy.app.handlers.version_update.remove(version_update.do_versions)

    ui.unregister()
    operators.unregister()
    properties.unregister()
    presets.unregister()

    for cls in classes:
        unregister_class(cls)
