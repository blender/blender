"""
It is also possible to run an operator in a particular part of the user
interface. For this we need to pass the window, area and sometimes a region.
"""

# Maximize 3d view in all windows.
import bpy
from bpy import context

for window in context.window_manager.windows:
    screen = window.screen
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            with context.temp_override(window=window, area=area):
                bpy.ops.screen.screen_full_area()
            break
