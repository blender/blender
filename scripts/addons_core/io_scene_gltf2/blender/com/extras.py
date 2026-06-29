# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


import bpy
from .json_util import is_json_convertible


# Custom properties, which are in most cases present and should not be imported/exported.
BLACK_LIST = [
    'cycles',
    'cycles_visibility',
    'cycles_curves',
    'glTF2ExportSettings',
    'gltf2_mesh_applied',
    'gltf2_KHR_materials_variants_variants',
    'gltf2_active_variant',
    'gltf2_variant_default_materials',
    'gltf2_variant_mesh_data']


def generate_extras(blender_element, blender_data_type, export_settings):
    """Filter and create a custom property, which is stored in the glTF extra field."""
    if not blender_element:
        return None

    # Warning for devs: there is another dict in anim_extra_utils.py
    # Please keep them in sync for new entries
    gltf_data_type = {
        'objects': 'nodes',
        'bones': 'nodes',
        'meshes': 'meshes',
        'materials': 'materials',
        'lights': 'extensions/KHR_lights_punctual/lights'}.get(
        blender_data_type,
        blender_data_type)

    extras = {}

    if export_settings['gltf_export_anim_pointer'] is True:
        export_settings['KHR_animation_pointer']['extras'][blender_data_type][id(blender_element)] = {}
        export_settings['KHR_animation_pointer']['extras'][blender_data_type][id(blender_element)]['paths'] = {}

    # Custom properties
    for custom_property in blender_element.keys():
        if custom_property in BLACK_LIST:
            continue

        value = __to_json_compatible(blender_element[custom_property])

        if value is not None:
            extras[custom_property] = value

            if export_settings['gltf_export_anim_pointer'] is True:
                # We are supporting only 1 item custom properties for now
                if not isinstance(value, (int, float, bool)):
                    continue

                # Store the path of the custom property for KHR_animation_pointer
                path_ = {}
                path_['length'] = 1
                path_['path'] = "/" + gltf_data_type + "/XXX/extras/" + custom_property

                export_settings['KHR_animation_pointer']['extras'][blender_data_type][id(
                    blender_element)]['paths']["[\"" + custom_property + "\"]"] = path_

    # System Custom Properties (ID properties)
    properties = blender_element.bl_system_properties_get() or {}
    for custom_property in properties.keys():
        if custom_property in BLACK_LIST:
            continue

        value = __to_json_compatible(properties[custom_property])

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
        if custom_property in BLACK_LIST + ["gltf_tmp_data_animations"]:
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
