"""
Overriding the context can be useful to set the context after loading files
(which would otherwise by None). For example:
"""

import bpy
from bpy import context

# Reload the current file and select all.
bpy.ops.wm.open_mainfile(filepath=bpy.data.filepath)
window = context.window_manager.windows[0]
with context.temp_override(window=window):
    bpy.ops.mesh.primitive_uv_sphere_add()
    # The context override is needed so it's possible to set edit-mode.
    bpy.ops.object.mode_set(mode='EDIT')
