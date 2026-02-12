"""
This is the most simple example of inserting a keyframe from Python.
"""

import bpy

obj = bpy.context.object

# Set the keyframe at frame 1.
obj.location = (3.0, 4.0, 10.0)
obj.keyframe_insert(data_path="location", frame=1)
