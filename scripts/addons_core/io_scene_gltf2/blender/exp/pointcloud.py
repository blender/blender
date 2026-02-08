# SPDX-FileCopyrightText: 2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


from ..com import conversion as gltf2_blender_conversion
from .attribute_utils import extract_attribute_data
import numpy as np


def gather_point_cloud(blender_pointcloud, export_settings):

    primitives = []

    # Do not export if we don't export loose points
    if not export_settings['gltf_loose_points']:
        return []

    # Position
    locs = np.empty(
        len(blender_pointcloud.attributes['position'].data) * 3, dtype=np.float32)
    position_attribute = gltf2_blender_conversion.get_attribute(
        blender_pointcloud.attributes, 'position', 'FLOAT_VECTOR', 'POINT')
    source = position_attribute.data if position_attribute else None
    foreach_attribute = 'vector'
    if source:
        source.foreach_get(foreach_attribute, locs)
    locs = locs.reshape(len(blender_pointcloud.attributes['position'].data), 3)

    # Radius
    radius = np.empty(
        len(blender_pointcloud.attributes['radius'].data), dtype=np.float32)
    radius_attribute = gltf2_blender_conversion.get_attribute(
        blender_pointcloud.attributes, 'radius', 'FLOAT', 'POINT')
    source = radius_attribute.data if radius_attribute else None
    foreach_attribute = 'value'
    if source:
        source.foreach_get(foreach_attribute, radius)
    radius = radius.reshape(len(blender_pointcloud.attributes['radius'].data))

    # Get any other attributes that may be present, starting with an underscore
    custom_attributes = __get_custom_attributes(blender_pointcloud, export_settings)

    custom_attributes['POSITION'] = {
        'data': locs,
        'data_type': gltf2_blender_conversion.get_data_type('FLOAT_VECTOR'),
        'component_type': gltf2_blender_conversion.get_component_type('FLOAT_VECTOR')
    }
    custom_attributes['_RADIUS'] = {
        'data': radius,
        'data_type': gltf2_blender_conversion.get_data_type('FLOAT'),
        'component_type': gltf2_blender_conversion.get_component_type('FLOAT')
    }

    primitives.append({
        'attributes': custom_attributes,
        'mode': 0,  # POINTS
        'material': 0,  # TODOPC
        'uvmap_attributes_index': {}
    })

    export_settings['log'].info(
        'Point Cloud Primitives created: %d' % len(primitives))

    return primitives


def __get_custom_attributes(blender_pointcloud, export_settings):
    custom_attributes = {}
    for attribute in blender_pointcloud.attributes:
        if attribute.domain != 'POINT':
            continue
        if attribute.name in ['position', 'radius']:
            continue
        if not attribute.name.startswith("_"):
            continue

        len_attr = gltf2_blender_conversion.get_data_length(
            attribute.data_type)

        data = extract_attribute_data(
            attribute,
            len(attribute.data),
            np.float32,
            attribute.data_type,
            attribute.domain,
            len_attr,
            export_settings=export_settings
        )

        if len_attr > 1:
            data = data.reshape(-1, len_attr)
        custom_attributes[attribute.name] = {
            'data': data,
            'data_type': gltf2_blender_conversion.get_data_type(attribute.data_type),
            'component_type': gltf2_blender_conversion.get_component_type(attribute.data_type)
        }
    return custom_attributes
