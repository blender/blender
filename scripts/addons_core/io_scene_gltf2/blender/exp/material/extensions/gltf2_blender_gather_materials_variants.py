# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from typing import Dict, Any
from .....io.com import gltf2_io_variants
from ...gltf2_blender_gather_cache import cached


@cached
def gather_variant(variant_idx, export_settings) -> Dict[str, Any]:

    variant = gltf2_io_variants.Variant(
        name=bpy.data.scenes[0].gltf2_KHR_materials_variants_variants[variant_idx].name,
        extensions=None,
        extras=None
    )
    return variant.to_dict()
