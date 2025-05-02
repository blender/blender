# SPDX-FileCopyrightText: 2018-2024 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...com.extras import generate_extras

def gather_extras(blender_material, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_material)
    return None

def gather_name(blender_material, export_settings):
    return blender_material.name
