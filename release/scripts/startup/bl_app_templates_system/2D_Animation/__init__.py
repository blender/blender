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


@persistent
def load_handler(dummy):
    import bpy

    # 2D Animation
    screen = bpy.data.screens['2D Animation']
    if screen:
        for area in screen.areas:
            # Set Tool settings as default in properties panel.
            if area.type == 'PROPERTIES':
                for space in area.spaces:
                    if space.type != 'PROPERTIES':
                        continue
                    space.context = 'TOOL'

            # Open sidebar in Dopesheet.
            elif area.type == 'DOPESHEET_EDITOR':
                for space in area.spaces:
                    if space.type != 'DOPESHEET_EDITOR':
                        continue
                    space.show_region_ui = True

    # 2D Full Canvas
    screen = bpy.data.screens['2D Full Canvas']
    if screen:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                for space in area.spaces:
                    if space.type != 'VIEW_3D':
                        continue
                    space.shading.type = 'MATERIAL'
                    space.shading.use_scene_world = True

    # Grease pencil object
    scene = bpy.data.scenes[0]
    if scene:
        scene.tool_settings.use_keyframe_insert_auto = True
        for ob in scene.objects:
            if ob.type == 'GPENCIL':
                gpd = ob.data
                gpd.onion_keyframe_type = 'ALL'


def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)


def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
