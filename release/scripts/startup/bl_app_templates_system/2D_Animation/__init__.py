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

# Initialization script for 2D Animation template

import bpy
from bpy.app.handlers import persistent

def update_factory_startup_screens():
    # 2D Animation.
    screen = bpy.data.screens["2D Animation"]
    for area in screen.areas:
        if area.type == 'PROPERTIES':
            # Set Tool settings as default in properties panel.
            space = area.spaces.active
            space.context = 'TOOL'
        elif area.type == 'DOPESHEET_EDITOR':
            # Open sidebar in Dopesheet.
            space = area.spaces.active
            space.show_region_ui = True

    # 2D Full Canvas.
    screen = bpy.data.screens["2D Full Canvas"]
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.shading.type = 'MATERIAL'
            space.shading.use_scene_world = True


def update_factory_startup_scenes():
    for scene in bpy.data.scenes:
        scene.tool_settings.use_keyframe_insert_auto = True
        scene.tool_settings.gpencil_sculpt.use_scale_thickness = True


def update_factory_startup_grease_pencils():
    for gpd in bpy.data.grease_pencils:
        gpd.onion_keyframe_type = 'ALL'


@persistent
def load_handler(_):
    update_factory_startup_screens()
    update_factory_startup_scenes()
    update_factory_startup_grease_pencils()


def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)


def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
