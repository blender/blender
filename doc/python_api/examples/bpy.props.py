"""
Assigning to Existing Classes
+++++++++++++++++++++++++++++

Custom properties can be added to any subclass of an :class:`ID`,
:class:`Bone` and :class:`PoseBone`.

These properties can be animated, accessed by the user interface and python
like Blender's existing properties.

.. warning::

   Access to these properties might happen in threaded context, on a per-data-block level.
   This has to be carefully considered when using accessors or update callbacks.

   Typically, these callbacks should not affect any other data that the one owned by their data-block.
   When accessing external non-Blender data, thread safety mechanisms should be considered.

"""

import bpy

# Assign a custom property to an existing type.
bpy.types.Material.custom_float = bpy.props.FloatProperty(name="Test Property")

# Test the property is there.
bpy.data.materials[0].custom_float = 5.0
