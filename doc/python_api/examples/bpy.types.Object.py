"""
Basic Object Operations Example
+++++++++++++++++++++++++++++++

This script demonstrates basic operations on object like creating new
object, placing it into a view layer, selecting it and making it active.
"""

import bpy
from mathutils import Matrix

view_layer = bpy.context.view_layer

# Create new light datablock.
light_data = bpy.data.lights.new(name="New Light", type='POINT')

# Create new object with our light datablock.
light_object = bpy.data.objects.new(name="New Light", object_data=light_data)

# Link light object to the active collection of current view layer,
# so that it'll appear in the current scene.
view_layer.collections.active.collection.objects.link(light_object)

# Place light to a specified location.
light_object.location = (5.0, 5.0, 5.0)

# And finally select it and make it active.
light_object.select_set('SELECT')
view_layer.objects.active = light_object
