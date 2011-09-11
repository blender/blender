#
# Copyright 2011, Blender Foundation.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

bl_info = {
    "name": "Cycles Render Engine",
    "author": "",
    "version": (0,0),
    "blender": (2, 5, 6),
    "api": 34462,
    "location": "Info header, render engine menu",
    "description": "Cycles Render Engine integration.",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Render"}

import bpy

from cycles import ui
from cycles import properties
from cycles import xml
from cycles import engine
from cycles import presets

class CyclesRender(bpy.types.RenderEngine):
    bl_idname = 'CYCLES'
    bl_label = "Cycles"
    bl_use_rendered = True

    def __init__(self):
        engine.init()
        self.session = None
    
    def __del__(self):
        engine.free(self)

    # final render
    def update(self, data, scene):
        engine.create(self, data, scene)
        engine.update(self, data, scene)

    def render(self):
        engine.render(self)

    # preview render
    # def preview_update(self, context, id):
    #    pass
    #
    # def preview_render(self):
    #    pass
    
    # viewport render
    def view_update(self, context):
        if not self.session:
            engine.create(self, context.blend_data, context.scene,
                context.region, context.space_data, context.region_data)
        engine.update(self, context.blend_data, context.scene)

    def view_draw(self, context):
        engine.draw(self, context.region, context.space_data, context.region_data)

def register():
    properties.register()
    ui.register()
    xml.register()
    presets.register()
    bpy.utils.register_module(__name__)

def unregister():
    xml.unregister()
    ui.unregister()
    properties.unregister()
    presets.unregister()
    bpy.utils.unregister_module(__name__)

