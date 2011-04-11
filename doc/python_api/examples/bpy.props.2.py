"""
PropertyGroup Example
+++++++++++++++++++++

PropertyGroups can be used for collecting custom settings into one value
to avoid many indervidual settings mixed in together.
"""

import bpy


class MaterialSettings(bpy.types.PropertyGroup):
    my_int = bpy.props.IntProperty()
    my_float = bpy.props.FloatProperty()
    my_string = bpy.props.StringProperty()

bpy.utils.register_class(MaterialSettings)

bpy.types.Material.my_settings = \
    bpy.props.PointerProperty(type=MaterialSettings)

# test the new settings work
material = bpy.data.materials[0]

material.my_settings.my_int = 5
material.my_settings.my_float = 3.0
material.my_settings.my_string = "Foo"
