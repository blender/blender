"""
Get/Set Example
+++++++++++++++

Get/Set functions can be used for boolean, int, float, string and enum properties.
If these callbacks are defined the property will not be stored in the ID properties
automatically, instead the get/set functions will be called when the property is
read or written from the API.
"""

import bpy


# Simple property reading/writing from ID properties.
# This is what the RNA would do internally.
def get_float(self):
    return self["testprop"]


def set_float(self, value):
    self["testprop"] = value

bpy.types.Scene.test_float = bpy.props.FloatProperty(get=get_float, set=set_float)


# Read-only string property, returns the current date
def get_date(self):
    import datetime
    return str(datetime.datetime.now())

bpy.types.Scene.test_date = bpy.props.StringProperty(get=get_date)


# Boolean array. Set function stores a single boolean value, returned as the second component.
# Array getters must return a list or tuple
# Array size must match the property vector size exactly
def get_array(self):
    return (True, self["somebool"])


def set_array(self, values):
    self["somebool"] = values[0] and values[1]

bpy.types.Scene.test_array = bpy.props.BoolVectorProperty(size=2, get=get_array, set=set_array)


# Enum property.
# Note: the getter/setter callback must use integer identifiers!
test_items = [
    ("RED", "Red", "", 1),
    ("GREEN", "Red", "", 2),
    ("BLUE", "Red", "", 3),
    ("YELLOW", "Red", "", 4),
    ]


def get_enum(self):
    import random
    return random.randint(1, 4)


def set_enum(self, value):
    print("setting value", value)

bpy.types.Scene.test_enum = bpy.props.EnumProperty(items=test_items, get=get_enum, set=set_enum)


# Testing

scene = bpy.context.scene

scene.test_float = 12.34
print(scene.test_float)

scene.test_array = (True, False)
print([x for x in scene.test_array])

# scene.test_date = "blah"   # this would fail, property is read-only
print(scene.test_date)

scene.test_enum = 'BLUE'
print(scene.test_enum)


# >>> 12.34000015258789
# >>> [True, False]
# >>> 2013-01-05 16:33:52.135340
# >>> setting value 3
# >>> GREEN
