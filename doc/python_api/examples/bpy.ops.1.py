"""
Overriding Context
------------------

It is possible to override context members that the operator sees, so that they
act on specified rather than the selected or active data, or to execute an
operator in the different part of the user interface.

The context overrides are passed as a dictionary, with keys matching the context
member names in bpy.context. For example to override bpy.context.active_object,
you would pass {'active_object': object}.
"""

# remove all objects in scene rather than the selected ones
import bpy
override = {'selected_bases': list(bpy.context.scene.object_bases)}
bpy.ops.object.delete(override)
