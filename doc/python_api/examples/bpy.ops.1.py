"""
Overriding Context
------------------

It is possible to override context members that the operator sees, so that they
act on specified rather than the selected or active data, or to execute an
operator in the different part of the user interface.

The context overrides are passed as a dictionary, with keys matching the context
member names in bpy.context.
For example to override ``bpy.context.active_object``,
you would pass ``{'active_object': object}``.

.. note::

   You will nearly always want to use a copy of the actual current context as basis
   (otherwise, you'll have to find and gather all needed data yourself).
"""

# remove all objects in scene rather than the selected ones
import bpy
override = bpy.context.copy()
override['selected_objects'] = list(bpy.context.scene.objects)
bpy.ops.object.delete(override)
