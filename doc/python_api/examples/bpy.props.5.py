"""
Getter/Setter Example
+++++++++++++++++++++

Accessor functions can be used for boolean, int, float, string and enum properties.

If ``get`` or ``set`` callbacks are defined, the property will not be stored in the ID properties
automatically. Instead, the ``get`` and ``set`` functions will be called when the property
is respectively read or written from the API, and are responsible to handle the data storage.

Note that:

- It is illegal to define a ``set`` callback without a matching ``get`` one.
- When a ``get`` callback is defined but no ``set`` one, the property is read-only.

``get_transform`` and ``set_transform`` can be used when the returned value needs to be modified,
but the default internal storage is still used. They can only transform the value before it is
set or returned, but do not control how/where that data is stored.

.. note::

   It is possible to define both ``get``/``set`` and ``get_transform``/``set_transform`` callbacks
   for a same property. In practice however, this should rarely be needed, as most 'transform'
   operation can also happen within a ``get``/``set`` callback.

.. warning::

   Remember that these callbacks may be executed in threaded context.

.. warning::

   Take care when accessing other properties in these callbacks, as it can easily trigger
   complex issues, such as infinite loops (if e.g. two properties try to also set the other
   property's value in their own ``set`` callback), or unexpected side effects due to changes
   in data, caused e.g. by an ``update`` callback.

"""
import bpy


scene = bpy.context.scene


# Simple property reading/writing from 'custom' IDProperties.
# This is similar to what the RNA would do internally, albeit using it own separate,
# internal 'system' IDProperty storage, since Blender 5.0.
def get_float(self):
    return self.get("testprop", 0.0)


def set_float(self, value):
    self["testprop"] = value


bpy.types.Scene.test_float = bpy.props.FloatProperty(get=get_float, set=set_float)

# Testing the property:
print("test_float:", scene.test_float)
scene.test_float = 7.5
print("test_float:", scene.test_float)

# The above outputs:
# test_float: 0.0
# test_float: 7.5


# Read-only string property, returns the current date.
def get_date(self):
    import datetime
    return str(datetime.datetime.now())


bpy.types.Scene.test_date = bpy.props.StringProperty(get=get_date)

# Testing the property:
# scene.test_date = "blah"   # This would fail, property is read-only.
print("test_date:", scene.test_date)

# The above outputs something like:
# test_date: 2018-03-14 11:36:53.158653


# Boolean array.
# - Set function stores a single boolean value, returned as the second component.
# - Array getters must return a list or tuple.
# - Array size must match the property vector size exactly.
def get_array(self):
    return (True, self.get("somebool", True))


def set_array(self, values):
    self["somebool"] = values[0] and values[1]


bpy.types.Scene.test_array = bpy.props.BoolVectorProperty(size=2, get=get_array, set=set_array)

# Testing the property:
print("test_array:", tuple(scene.test_array))
scene.test_array = (True, False)
print("test_array:", tuple(scene.test_array))

# The above outputs:
# test_array: (True, True)
# test_array: (True, False)


# Boolean array, using 'transform' accessors.
# Note how the same result is achieved as with previous get/set example, but using default RNA storage.
# Transform accessors also have access to more information.
# Also note how the stored data _is_ a two-items array.
# - Set function stores a single boolean value, returned as the second component.
# - Array getters must return a list or tuple.
# - Array size must match the property vector size exactly.
def get_array_transform(self, curr_value, is_set):
    print("Stored data:", curr_value, "(is set:", is_set, ")")
    return (True, curr_value[1])


def set_array_transform(self, new_value, curr_value, is_set):
    print("New data:", new_value, "; Stored data:", curr_value, "(is set:", is_set, ")")
    return True, new_value[0] and new_value[1]


bpy.types.Scene.test_array_transform = bpy.props.BoolVectorProperty(
    size=2, get_transform=get_array_transform, set_transform=set_array_transform)

# Testing the property:
print("test_array_transform:", tuple(scene.test_array_transform))
scene.test_array_transform = (True, False)
print("test_array_transform:", tuple(scene.test_array_transform))

# The above outputs:
# Stored data: (False, False) (is set: False )
# test_array_transform: (True, False)
# New data: (True, False) ; Stored data: (False, False) (is set: False )
# Stored data: (True, False) (is set: True )
# test_array_transform: (True, False)


# Enum property.
# Note: the getter/setter callback must use integer identifiers!
test_items = [
    ("RED", "Red", "", 1),
    ("GREEN", "Green", "", 2),
    ("BLUE", "Blue", "", 3),
    ("YELLOW", "Yellow", "", 4),
]


def get_enum(self):
    import random
    return random.randint(1, 4)


def set_enum(self, value):
    print("setting value", value)


bpy.types.Scene.test_enum = bpy.props.EnumProperty(items=test_items, get=get_enum, set=set_enum)

# Testing the property:
print("test_enum:", scene.test_enum)
scene.test_enum = 'BLUE'
print("test_enum:", scene.test_enum)

# The above outputs something like:
# test_enum: YELLOW
# setting value 3
# test_enum: GREEN


# String, using 'transform' accessors to validate data before setting/returning it.
def get_string_transform(self, curr_value, is_set):
    import os
    is_valid_path = os.path.exists(curr_value)
    print("Stored data:", curr_value, "(is set:", is_set, ", is valid path:", is_valid_path, ")")
    return curr_value if is_valid_path else ""


def set_string_transform(self, new_value, curr_value, is_set):
    import os
    is_valid_path = os.path.exists(new_value)
    print("New data:", new_value, "(is_valid_path:", is_valid_path, ");",
          "Stored data:", curr_value, "(is set:", is_set, ")")
    return new_value if is_valid_path else curr_value


bpy.types.Scene.test_string_transform = bpy.props.StringProperty(
    subtype='DIR_PATH',
    default="an/invalid/path",
    get_transform=get_string_transform,
    set_transform=set_string_transform,
)

# Testing the property:
print("test_string_transform:", scene.test_string_transform)
scene.test_string_transform = "try\\to\\find\\me"
print("test_string_transform:", scene.test_string_transform)

# The above outputs something like:
# Stored data: an/invalid/path (is set: False , is valid path: False )
# test_string_transform:
# New data: try\to\find\me (is_valid_path: False ) ; Stored data: an/invalid/path (is set: False )
# Stored data: an/invalid/path (is set: True , is valid path: False )
# test_string_transform:
