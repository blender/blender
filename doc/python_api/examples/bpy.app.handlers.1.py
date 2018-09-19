"""
Persistent Handler Example
++++++++++++++++++++++++++

By default handlers are freed when loading new files, in some cases you may
want the handler stay running across multiple files (when the handler is
part of an add-on for example).

For this the :data:`bpy.app.handlers.persistent` decorator needs to be used.
"""

import bpy
from bpy.app.handlers import persistent


@persistent
def load_handler(dummy):
    print("Load Handler:", bpy.data.filepath)


bpy.app.handlers.load_post.append(load_handler)
