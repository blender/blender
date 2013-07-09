"""
This function is for advanced use only, misuse can crash blender since the user
count is used to prevent data being removed when it is used.
"""

# This example shows what _not_ to do, and will crash blender.
import bpy

# object which is in the scene.
obj = bpy.data.objects["Cube"]

# without this, removal would raise an error.
obj.user_clear()

# runs without an exception
# but will crash on redraw.
bpy.data.objects.remove(obj)
