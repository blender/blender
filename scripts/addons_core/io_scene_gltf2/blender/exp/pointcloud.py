# SPDX-FileCopyrightText: 2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


import numpy as np

from ...io.com.gltf2_io_extensions import Extension
from ..com import conversion as gltf2_blender_conversion
from .material.materials import get_base_material
from .primitive_extract import LoopData, PrimitiveCreator, single_extract_attribute_data
from .gsplat import detect_sh_order, get_sh_attributes


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
    if 'radius' in blender_pointcloud.attributes:
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
    if 'radius' in blender_pointcloud.attributes:
        custom_attributes['_RADIUS'] = {
            'data': radius,
            'data_type': gltf2_blender_conversion.get_data_type('FLOAT'),
            'component_type': gltf2_blender_conversion.get_component_type('FLOAT')
        }

    # This point cloud can be a Gaussian Splat, or a regular point cloud, depending on the attributes present
    sh_order = detect_sh_order(blender_pointcloud.attributes)

    extensions = None
    if sh_order is None:
        # Regular point cloud, trying to retrieve color attributes
        custom_attributes.update(__get_color_attribute(blender_pointcloud, materials, export_settings))
    else:
        # Gaussian Splat, retrieving all SH attributes
        custom_attributes.update(get_sh_attributes(blender_pointcloud, sh_order, export_settings))

        # Set extensions for Gaussian Splat
        extension_data = {
            "kernel": "ellipse",
            "color_space": "srgb_rec709_display",
            "projection": "perspective",
            "sortingMethod": "cameraDistance"
        }
        extensions = {}
        extensions["KHR_gaussian_splatting"] = Extension(
            name="KHR_gaussian_splatting", extension=extension_data, required=False)

    # And now, create the primitive infos
    primitives.append({
        'attributes': custom_attributes,
        'mode': 0,  # POINTS
        'material': 0,  # TODOPC
        'uvmap_attributes_index': {},
        'extensions': extensions
    })

    export_settings['log'].info(
        'Point Cloud Primitives created: %d' % len(primitives))

    return primitives


def __get_color_attribute(blender_pointcloud, materials, export_settings):

    additional_custom_attributes = {}

    # Detect Vertex Color usage
    loop_data = LoopData(
        vc_infos_index=0,
        materials_use_vc=None,
        warning_already_displayed=False,
        warning_already_displayed_vc_nodetree=False
    )
    _, base_material, material_info = get_base_material(0, materials, export_settings)

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
        additional_custom_attributes['COLOR_0'] = {
            'data': __get_color_attribute_data(blender_pointcloud.attributes[vc_infos[0]['color']], blender_pointcloud),
            'data_type': gltf2_blender_conversion.get_data_type('FLOAT_COLOR'),
            'component_type': gltf2_blender_conversion.get_component_type('FLOAT_COLOR')
        }
    return additional_custom_attributes


def __get_custom_attributes(blender_pointcloud, export_settings):
    custom_attributes = {}

    # Be sure that user choose to export custom attributes
    if not export_settings['gltf_attributes']:
        return custom_attributes

    for attribute in blender_pointcloud.attributes:
        if attribute.domain != 'POINT':
            continue
        if attribute.name in ['position', 'radius']:
            continue
        if not attribute.name.startswith("_"):
            continue

        data = single_extract_attribute_data(attribute, export_settings)

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
