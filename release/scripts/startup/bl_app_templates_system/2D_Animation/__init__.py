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


def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)


def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
