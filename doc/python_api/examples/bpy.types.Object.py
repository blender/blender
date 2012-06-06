"""
Basic Object Operations Example
+++++++++++++++++++++++++++++++
This script demonstrates basic operations on object like creating new
object, placing it into scene, selecting it and making it active
"""

import bpy
from mathutils import Matrix

scene = bpy.context.scene

# Create new lamp datablock
lamp_data = bpy.data.lamps.new(name="New Lamp", type="POINT")

# Create new object with out lamp datablock
lamp_object = bpy.data.objects.new(name="New Lamp", object_data=lamp_data)

# Link lamp object to the scene so it'll appear in this scene
scene.objects.link(lamp_object)

# Place lamp to specified location
lamp_object.location = (5.0, 5.0, 5.0)

# And finally select it make active
lamp_object.select = True
scene.objects.active = lamp_object
