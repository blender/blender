# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


import bpy
from .gltf2_blender_json import is_json_convertible


# Custom properties, which are in most cases present and should not be imported/exported.
BLACK_LIST = ['cycles', 'cycles_visibility', 'cycles_curves', 'glTF2ExportSettings']


def generate_extras(blender_element):
    """Filter and create a custom property, which is stored in the glTF extra field."""
    if not blender_element:
        return None

    extras = {}

    for custom_property in blender_element.keys():
        if custom_property in BLACK_LIST:
            continue

        value = __to_json_compatible(blender_element[custom_property])

        if value is not None:
            extras[custom_property] = value

    if not extras:
        return None

    return extras


def __to_json_compatible(value):
    """Make a value (usually a custom property) compatible with json"""

    if isinstance(value, bpy.types.ID):
        return value

    elif isinstance(value, str):
        return value

    elif isinstance(value, (int, float)):
        return value

    # for list classes
    elif isinstance(value, list):
        value = list(value)
        # make sure contents are json-compatible too
        for index in range(len(value)):
            value[index] = __to_json_compatible(value[index])
        return value

    # for IDPropertyArray classes
    elif hasattr(value, "to_list"):
        value = value.to_list()
        return value

    elif hasattr(value, "to_dict"):
        value = value.to_dict()
        if is_json_convertible(value):
            return value

    return None


def set_extras(blender_element, extras, exclude=[]):
    """Copy extras onto a Blender object."""
    if not extras or not isinstance(extras, dict):
        return

    for custom_property, value in extras.items():
        if custom_property in BLACK_LIST:
            continue
        if custom_property in exclude:
            continue

        try:
            blender_element[custom_property] = value
        except Exception:
            # Try to convert to string
            try:
                blender_element[custom_property] = str(value)
            except Exception:
                print('Error setting property %s to value of type %s' % (custom_property, type(value)))
