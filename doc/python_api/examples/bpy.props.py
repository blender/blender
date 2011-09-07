"""
Assigning to Existing Classes
+++++++++++++++++++++++++++++

Custom properties can be added to any subclass of an :class:`ID`,
:class:`Bone` and :class:`PoseBone`.

These properties can be animated, accessed by the user interface and python
like blenders existing properties.
"""

import bpy

# Assign a custom property to an existing type.
bpy.types.Material.custom_float = bpy.props.FloatProperty(name="Test Prob")

# Test the property is there.
bpy.data.materials[0].custom_float = 5.0
