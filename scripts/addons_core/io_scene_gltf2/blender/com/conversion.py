# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from math import sin, cos, tan, atan
from mathutils import Matrix, Vector
import numpy as np
from ...io.com import constants as gltf2_io_constants

PBR_WATTS_TO_LUMENS = 683
# Industry convention, biological peak at 555nm, scientific standard as part of SI candela definition.


# This means use the inverse of the TRS transform.
def inverted_trs_mapping_node(mapping_transform):
    offset = mapping_transform["offset"]
    rotation = mapping_transform["rotation"]
    scale = mapping_transform["scale"]

    # Inverse of a TRS is not always a TRS. This function will be right
    # at least when the following don't occur.
    if abs(rotation) > 1e-5 and abs(scale[0] - scale[1]) > 1e-5:
        return None
    if abs(scale[0]) < 1e-5 or abs(scale[1]) < 1e-5:
        return None

    new_offset = Matrix.Rotation(-rotation, 3, 'Z') @ Vector((-offset[0], -offset[1], 1))
    new_offset[0] /= scale[0]
    new_offset[1] /= scale[1]
    return {
        "offset": new_offset[0:2],
        "rotation": -rotation,
        "scale": [1 / scale[0], 1 / scale[1]],
    }


def texture_transform_blender_to_gltf(mapping_transform):
    """
    Converts the offset/rotation/scale from a Mapping node applied in Blender's
    UV space to the equivalent KHR_texture_transform.
    """
    offset = mapping_transform.get('offset', [0, 0])
    rotation = mapping_transform.get('rotation', 0)
    scale = mapping_transform.get('scale', [1, 1])
    return {
        'offset': [
            offset[0] - scale[1] * sin(rotation),
            1 - offset[1] - scale[1] * cos(rotation),
        ],
        'rotation': rotation,
        'scale': [scale[0], scale[1]],
    }


def texture_transform_gltf_to_blender(texture_transform):
    """
    Converts a KHR_texture_transform into the equivalent offset/rotation/scale
    for a Mapping node applied in Blender's UV space.
    """
    offset = texture_transform.get('offset', [0, 0])
    rotation = texture_transform.get('rotation', 0)
    scale = texture_transform.get('scale', [1, 1])
    return {
        'offset': [
            offset[0] + scale[1] * sin(rotation),
            1 - offset[1] - scale[1] * cos(rotation),
        ],
        'rotation': rotation,
        'scale': [scale[0], scale[1]],
    }


def get_target(property):
    return {
        "delta_location": "translation",
        "delta_rotation_euler": "rotation",
        "delta_rotation_quaternion": "rotation",
        "delta_scale": "scale",
        "location": "translation",
        "rotation_axis_angle": "rotation",
        "rotation_euler": "rotation",
        "rotation_quaternion": "rotation",
        "scale": "scale",
        "value": "weights"
    }.get(property, None)


def get_component_type(attribute_component_type):
    return {
        "INT8": gltf2_io_constants.ComponentType.Float,
        "BYTE_COLOR": gltf2_io_constants.ComponentType.UnsignedShort,
        "FLOAT2": gltf2_io_constants.ComponentType.Float,
        "FLOAT_COLOR": gltf2_io_constants.ComponentType.Float,
        "FLOAT_VECTOR": gltf2_io_constants.ComponentType.Float,
        "FLOAT_VECTOR_4": gltf2_io_constants.ComponentType.Float,
        "QUATERNION": gltf2_io_constants.ComponentType.Float,
        "FLOAT4X4": gltf2_io_constants.ComponentType.Float,
        "INT": gltf2_io_constants.ComponentType.Float,  # No signed Int in glTF accessor
        "FLOAT": gltf2_io_constants.ComponentType.Float,
        "BOOLEAN": gltf2_io_constants.ComponentType.Float,
        "UNSIGNED_BYTE": gltf2_io_constants.ComponentType.UnsignedByte
    }.get(attribute_component_type)


def get_channel_from_target(target):
    return {
        "rotation": "rotation_quaternion",
        "translation": "location",
        "scale": "scale"
    }.get(target)


def get_data_type(attribute_component_type):
    return {
        "INT8": gltf2_io_constants.DataType.Scalar,
        "BYTE_COLOR": gltf2_io_constants.DataType.Vec4,
        "FLOAT2": gltf2_io_constants.DataType.Vec2,
        "FLOAT_COLOR": gltf2_io_constants.DataType.Vec4,
        "FLOAT_VECTOR": gltf2_io_constants.DataType.Vec3,
        "FLOAT_VECTOR_4": gltf2_io_constants.DataType.Vec4,
        "QUATERNION": gltf2_io_constants.DataType.Vec4,
        "FLOAT4X4": gltf2_io_constants.DataType.Mat4,
        "INT": gltf2_io_constants.DataType.Scalar,
        "FLOAT": gltf2_io_constants.DataType.Scalar,
        "BOOLEAN": gltf2_io_constants.DataType.Scalar,
    }.get(attribute_component_type)


def get_data_length(attribute_component_type):
    return {
        "INT8": 1,
        "BYTE_COLOR": 4,
        "FLOAT2": 2,
        "FLOAT_COLOR": 4,
        "FLOAT_VECTOR": 3,
        "FLOAT_VECTOR_4": 4,
        "QUATERNION": 4,
        "FLOAT4X4": 16,
        "INT": 1,
        "FLOAT": 1,
        "BOOLEAN": 1
    }.get(attribute_component_type)


def get_numpy_type(attribute_component_type):
    return {
        "INT8": np.float32,
        "BYTE_COLOR": np.float32,
        "FLOAT2": np.float32,
        "FLOAT_COLOR": np.float32,
        "FLOAT_VECTOR": np.float32,
        "FLOAT_VECTOR_4": np.float32,
        "QUATERNION": np.float32,
        "FLOAT4X4": np.float32,
        "INT": np.float32,  # signed integer are not supported by glTF
        "FLOAT": np.float32,
        "BOOLEAN": np.float32,
        "UNSIGNED_BYTE": np.uint8,
    }.get(attribute_component_type)


def get_attribute_type(component_type, data_type):
    if gltf2_io_constants.DataType.num_elements(data_type) == 1:
        return {
            gltf2_io_constants.ComponentType.Float: "FLOAT",
            gltf2_io_constants.ComponentType.UnsignedByte: "INT"  # What is the best for compatibility?
        }.get(component_type, None)
    elif gltf2_io_constants.DataType.num_elements(data_type) == 2:
        return {
            gltf2_io_constants.ComponentType.Float: "FLOAT2"
        }.get(component_type, None)
    elif gltf2_io_constants.DataType.num_elements(data_type) == 3:
        return {
            gltf2_io_constants.ComponentType.Float: "FLOAT_VECTOR"
        }.get(component_type, None)
    elif gltf2_io_constants.DataType.num_elements(data_type) == 4:
        return {
            gltf2_io_constants.ComponentType.Float: "FLOAT_COLOR",
            gltf2_io_constants.ComponentType.UnsignedShort: "BYTE_COLOR",
            gltf2_io_constants.ComponentType.UnsignedByte: "BYTE_COLOR"  # What is the best for compatibility?
        }.get(component_type, None)
    elif gltf2_io_constants.DataType.num_elements(data_type) == 16:
        return {
            gltf2_io_constants.ComponentType.Float: "FLOAT4X4"
        }.get(component_type, None)
    else:
        pass


def get_attribute(attributes, name, data_type, domain):
    attribute = attributes.get(name)
    if attribute is not None and attribute.data_type == data_type and attribute.domain == domain:
        return attribute
    else:
        return None


def get_gltf_interpolation(interpolation, export_settings):
    return {
        "BEZIER": "CUBICSPLINE",
        "LINEAR": "LINEAR",
        "CONSTANT": "STEP"
    }.get(interpolation, export_settings['gltf_sampling_interpolation_fallback']) # If unknown, default to the mode chosen by the user


def get_anisotropy_rotation_gltf_to_blender(rotation):
    # glTF rotation is in randian, Blender in 0 to 1
    return rotation / (2 * np.pi)


def get_anisotropy_rotation_blender_to_gltf(rotation):
    # glTF rotation is in randian, Blender in 0 to 1
    return rotation * (2 * np.pi)


def yvof_blender_to_gltf(angle, width, height, sensor_fit):

    aspect_ratio = width / height

    if width >= height:
        if sensor_fit != 'VERTICAL':
            return 2.0 * atan(tan(angle * 0.5) / aspect_ratio)
        else:
            return angle
    else:
        if sensor_fit != 'HORIZONTAL':
            return angle
        else:
            return 2.0 * atan(tan(angle * 0.5) / aspect_ratio)
