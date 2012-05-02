"""
It is also possible to run an operator in a particular part of the user
interface. For this we need to pass the window, screen, area and sometimes
a region.
"""

# maximize 3d view in all windows
import bpy

for window in bpy.context.window_manager.windows:
    screen = window.screen
    
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            override = {'window': window, 'screen': screen, 'area': area}
            bpy.ops.screen.screen_full_area(override)
            break

