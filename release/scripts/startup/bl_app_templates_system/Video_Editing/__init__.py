
import bpy
from bpy.app.handlers import persistent


@persistent
def load_handler(dummy):
    import os
    from bpy import context
    screen = context.screen
    for area in screen.areas:
        if area.type == 'FILE_BROWSER':
            space = area.spaces.active
            params = space.params
            params.directory = os.path.expanduser("~")
            params.use_filter_folder = True

def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)

def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
