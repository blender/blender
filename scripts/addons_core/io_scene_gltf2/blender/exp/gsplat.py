# SPDX-FileCopyrightText: 2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import numpy as np

from ..com import conversion as gltf2_blender_conversion
from .primitive_extract import single_extract_attribute_data

SQRT3 = np.float32(np.sqrt(3))
SQRT6 = np.float32(np.sqrt(6))
SQRT10 = np.float32(np.sqrt(10))
SQRT15 = np.float32(np.sqrt(15))


# Coefficient conversion from Zup (Blender) to Yup (glTF)
# This is the inverse of the conversion in blender/imp/gsplat.py

def convert_sh_d1_batch(c0, c1, c2):
    o0, o1 = c0.copy(), c1.copy()
    new_c0 = -o1
    new_c1 = o0
    # No change for c2
    return new_c0, new_c1, c2


def convert_sh_d2_batch(c0, c1, c2, c3, c4):
    o0, o2, o3, o4 = c0.copy(), c2.copy(), c3.copy(), c4.copy()
    new_c0 = -o3
    new_c1 = -c1
    new_c2 = -0.5 * o2 - (SQRT3 / 2) * o4
    new_c3 = o0
    new_c4 = -(SQRT3 / 2) * o2 + 0.5 * o4
    return new_c0, new_c1, new_c2, new_c3, new_c4


def convert_sh_d3_batch(c0, c1, c2, c3, c4, c5, c6):
    o0, o2, o3, o4, o5, o6 = (
        c0.copy(), c2.copy(), c3.copy(),
        c4.copy(), c5.copy(), c6.copy(),
    )
    new_c0 = (SQRT10 / 4) * o3 - (SQRT6 / 4) * o5
    new_c1 = -c1
    new_c2 = (SQRT6 / 4) * o3 + (SQRT10 / 4) * o5
    new_c3 = -(SQRT6 / 4) * o2 - (SQRT10 / 4) * o0
    new_c4 = -0.25 * o4 - (SQRT15 / 4) * o6
    new_c5 = -(SQRT10 / 4) * o2 + (SQRT6 / 4) * o0
    new_c6 = -(SQRT15 / 4) * o4 + 0.25 * o6
    return new_c0, new_c1, new_c2, new_c3, new_c4, new_c5, new_c6


def detect_sh_order(attributes):
    # From list of attribute names, detect if the attributes correspond to SH coefficients of a certain order
    # Return None if no valid SH order is detected (so weird GS, or an PC that is not a GS)

    if "position" not in attributes:
        return None

    if "rotation" not in attributes:
        return None
    elif attributes["rotation"].data_type != 'QUATERNION':
        return None

    if "scale" not in attributes:
        return None
    elif attributes["scale"].data_type != 'FLOAT_VECTOR':
        return None

    if "radiance:base" not in attributes:
        return None
    elif attributes["radiance:base"].data_type != 'FLOAT4':
        return None

    # Retrieve the max attribute index for SH coefficients
    max_index = -1
    for attr in attributes:
        if attr.name.startswith("radiance:sh_"):
            try:
                index = int(attr.name.split("_")[-1])
                max_index = max(max_index, index)
            except ValueError:
                pass

    # Determine SH order based on the max index
    sh_order = None
    if max_index == -1:
        sh_order = 0
    elif max_index == 2:
        sh_order = 1
    elif max_index == 7:  # 2 + 5
        sh_order = 2
    elif max_index == 14:  # 2 + 5 + 7
        sh_order = 3

    if sh_order is not None and sh_order > 0:
        # Check we have all the expected SH coefficients for this order
        check = all(f"radiance:sh_{i}" in attributes for i in range((sh_order + 1) ** 2 - 1))
        if not check:
            return None

        # Check they all are VECTOR type
        if not all(attributes[f"radiance:sh_{i}"].data_type == 'FLOAT_VECTOR' for i in range((sh_order + 1) ** 2 - 1)):
            sh_order = None

    return sh_order


def get_sh_attributes(blender_pointcloud, sh_order, export_settings):
    sh_attributes = {}
    axis_change = bpy.app.debug_value != 100

    for attribute in blender_pointcloud.attributes:
        if attribute.name == "position":
            continue  # already handled

        if attribute.name.startswith("_"):
            continue  # already handled as custom attribute

        if attribute.name == "rotation":

            data = single_extract_attribute_data(attribute, export_settings)
            # Convert rotation from Zup (Blender) to Yup (glTF)
            gltf2_blender_conversion.convert_quats_batch(data, change_axis=axis_change)
            sh_attributes["KHR_gaussian_splatting:ROTATION"] = {
                'data': data,
                'data_type': gltf2_blender_conversion.get_data_type(attribute.data_type),
                'component_type': gltf2_blender_conversion.get_component_type(attribute.data_type)
            }

        elif attribute.name == "scale":
            data = single_extract_attribute_data(attribute, export_settings)
            # Convert scale from Zup (Blender) to Yup (glTF)
            gltf2_blender_conversion.convert_scales_batch(data, change_axis=axis_change)
            sh_attributes["KHR_gaussian_splatting:SCALE"] = {
                'data': data,
                'data_type': gltf2_blender_conversion.get_data_type(attribute.data_type),
                'component_type': gltf2_blender_conversion.get_component_type(attribute.data_type)
            }
        elif attribute.name == "radiance:base":
            # We need to split the radiance:base into 2 different attributes: opacity and SH degree 0
            data = single_extract_attribute_data(attribute, export_settings)
            sh_attributes['KHR_gaussian_splatting:OPACITY'] = {
                'data': data[:, 3:4],  # Opacity is stored in the 4th channel
                'data_type': gltf2_blender_conversion.get_data_type("FLOAT"),
                'component_type': gltf2_blender_conversion.get_component_type("FLOAT")
            }
            sh_attributes['KHR_gaussian_splatting:SH_DEGREE_0_COEF_0'] = {
                'data': data[:, :3],  # SH degree 0 is stored in the first 3 channels
                'data_type': gltf2_blender_conversion.get_data_type("FLOAT_VECTOR"),
                'component_type': gltf2_blender_conversion.get_component_type("FLOAT_VECTOR")
            }
        elif attribute.name.startswith("radiance:sh_"):
            # Let's retrieve all SH attributes
            sh_data = single_extract_attribute_data(attribute, export_settings)
            try:
                index = int(attribute.name.split("_")[-1])
            except ValueError:
                continue

            # Calculate Degree and Coefficient based on Blender index
            degree = int((index + 1) ** 0.5)
            coef = index - degree ** 2 + 1
            sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_{degree}_COEF_{coef}'] = {
                'data': sh_data.copy(),
                'data_type': gltf2_blender_conversion.get_data_type("FLOAT_VECTOR"),
                'component_type': gltf2_blender_conversion.get_component_type("FLOAT_VECTOR")
            }

        else:
            pass  # Nothing to do, not related to Gaussian Splat

    # Now we have all sh attribute, let's convert then to Yup if needed
    if sh_order >= 1:
        new_datas = convert_sh_d1_batch(
            *[sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_1_COEF_{i}']['data'] for i in range(3)])
        for i in range(3):
            sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_1_COEF_{i}']['data'] = new_datas[i]
    if sh_order >= 2:
        new_datas = convert_sh_d2_batch(
            *[sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_2_COEF_{i}']['data'] for i in range(5)])
        for i in range(5):
            sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_2_COEF_{i}']['data'] = new_datas[i]
    if sh_order >= 3:
        new_datas = convert_sh_d3_batch(
            *[sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_3_COEF_{i}']['data'] for i in range(7)])
        for i in range(7):
            sh_attributes[f'KHR_gaussian_splatting:SH_DEGREE_3_COEF_{i}']['data'] = new_datas[i]

    return sh_attributes
