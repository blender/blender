"""
Custom Properties
+++++++++++++++++

PropertyGroups are the base class for dynamically defined sets of properties.

They can be used to extend existing blender data with your own types which can
be animated, accessed from the user interface and from python.

.. note::

   The values assigned to blender data are saved to disk but the class
   definitions are not, this means whenever you load blender the class needs
   to be registered too.

   This is best done by creating an addon which loads on startup and registers
   your properties.

.. note::

   PropertyGroups must be registered before assigning them to blender data.

.. seealso::

   Property types used in class declarations are all in :mod:`bpy.props`
"""
import bpy


class MyPropertyGroup(bpy.types.PropertyGroup):
    custom_1 = bpy.props.FloatProperty(name="My Float")
    custom_2 = bpy.props.IntProperty(name="My Int")

bpy.utils.register_class(MyPropertyGroup)

bpy.types.Object.my_prop_grp = bpy.props.PointerProperty(type=MyPropertyGroup)


# test this worked
bpy.data.objects[0].my_prop_grp.custom_1 = 22.0
