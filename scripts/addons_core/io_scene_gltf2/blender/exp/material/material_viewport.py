# SPDX-FileCopyrightText: 2018-2024 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ....io.com import gltf2_io
from .material_utils import gather_extras, gather_name

def export_viewport_material(blender_material, export_settings):

    pbr_metallic_roughness = gltf2_io.MaterialPBRMetallicRoughness(
        base_color_factor=list(blender_material.diffuse_color),
        base_color_texture=None,
        metallic_factor=blender_material.metallic,
        roughness_factor=blender_material.roughness,
        metallic_roughness_texture=None,
        extensions=None,
        extras=None
    )

    return gltf2_io.Material(
        alpha_cutoff=None,
        alpha_mode=None,
        double_sided=None,
        emissive_factor=None,
        emissive_texture=None,
        extensions=None,
        extras=gather_extras(blender_material, export_settings),
        name=gather_name(blender_material, export_settings),
        normal_texture=None,
        occlusion_texture=None,
        pbr_metallic_roughness=pbr_metallic_roughness
    )
