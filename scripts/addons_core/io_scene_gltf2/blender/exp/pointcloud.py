# SPDX-FileCopyrightText: 2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


import numpy as np

from ..com import conversion as gltf2_blender_conversion
from .attribute_utils import extract_attribute_data
from .material.materials import get_base_material
from .primitive_extract import LoopData, PrimitiveCreator


def gather_point_cloud(blender_pointcloud, materials, export_settings):

    primitives = []

    if not export_settings['gltf_pointclouds']:
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
    PrimitiveCreator.zup2yup(locs)

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

    # Detect Vertex Color usage
    loop_data = LoopData(
        vc_infos_index=0,
        materials_use_vc=None,
        warning_already_displayed=False,
        warning_already_displayed_vc_nodetree=False
    )
    base_material, material_info = get_base_material(0, materials, export_settings)

    vc_infos = PrimitiveCreator.manage_VC(
        base_material,
        0,
        material_info,
        blender_pointcloud,
        loop_data,
        export_settings,
        for_pointcloud=True
    )

    # Add COLOR_0 attribute if vertex colors are used
    if vc_infos:
        custom_attributes['COLOR_0'] = {
            'data': __get_color_attribute_data(blender_pointcloud.attributes[vc_infos[0]['color']], blender_pointcloud),
            'data_type': gltf2_blender_conversion.get_data_type('FLOAT_COLOR'),
            'component_type': gltf2_blender_conversion.get_component_type('FLOAT_COLOR')
        }

    # And now, create the primitive infos
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


def __get_color_attribute_data(attr, blender_data):

    colors = np.empty(len(blender_data.points) * 4, dtype=np.float32)
    attr.data.foreach_get('color', colors)

    return colors.reshape(-1, 4)
