# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from typing import List, Optional, Tuple
import numpy as np
from ...io.com import gltf2_io, constants as gltf2_io_constants, gltf2_io_extensions
from ...blender.com.data_path import get_sk_exported
from ...io.exp import binary_data as gltf2_io_binary_data
from .cache import cached, cached_by_key
from . import primitive_extract as gltf2_blender_gather_primitives_extract
from . import primitive_attributes as gltf2_blender_gather_primitive_attributes
from .accessors import gather_accessor, array_to_accessor
from .material.materials import get_final_material, gather_material, get_base_material, get_material_from_idx
from .material.extensions import variants as ext_variants


@cached
def gather_primitive_cache_key(
        blender_mesh,
        uuid_for_skined_data,
        vertex_groups,
        modifiers,
        materials,
        export_settings):

    # Use id of mesh
    # Do not use bpy.types that can be unhashable
    # Do not use mesh name, that can be not unique (when linked)

    # TODO check what is really needed for modifiers

    return (
        (id(blender_mesh),),
        (modifiers,),
        tuple(id(m) if m is not None else None for m in materials)
    )


@cached_by_key(key=gather_primitive_cache_key)
def gather_primitives(
        blender_mesh: bpy.types.Mesh,
        uuid_for_skined_data,
        vertex_groups: bpy.types.VertexGroups,
        modifiers: Optional[bpy.types.ObjectModifiers],
        materials: Tuple[bpy.types.Material],
        export_settings
) -> List[gltf2_io.MeshPrimitive]:
    """
    Extract the mesh primitives from a blender object

    :return: a list of glTF2 primitives
    """
    primitives = []

    blender_primitives, addional_materials_udim = __gather_cache_primitives(
        materials, blender_mesh, uuid_for_skined_data, vertex_groups, modifiers, export_settings)

    for internal_primitive, udim_material in zip(blender_primitives, addional_materials_udim):

        if udim_material is None:  # classic case, not an udim material
            # We already call this function, in order to retrieve uvmap info, if any
            # So here, only the cache will be used
            base_material, material_info = get_base_material(internal_primitive['material'], materials, export_settings)

            # Now, we can retrieve the real material, by checking attributes and active maps
            blender_mat = get_material_from_idx(internal_primitive['material'], materials, export_settings)
            material = get_final_material(
                blender_mesh,
                blender_mat,
                internal_primitive['uvmap_attributes_index'],
                base_material,
                material_info["uv_info"],
                export_settings)
        else:
            # UDIM case
            base_material, material_info, unique_material_id, tile = udim_material
            material = get_final_material(
                blender_mesh,
                unique_material_id,
                internal_primitive['uvmap_attributes_index'],
                base_material,
                material_info["uv_info"],
                export_settings)

            # Force change name of material to get the tile number in the name
            material.name = material.name + "." + tile

        primitive = gltf2_io.MeshPrimitive(
            attributes=internal_primitive['attributes'],
            extensions=__gather_extensions(
                blender_mesh,
                internal_primitive['material'],
                internal_primitive['uvmap_attributes_index'],
                export_settings),
            extras=None,
            indices=internal_primitive['indices'],
            material=material,
            mode=internal_primitive['mode'],
            targets=internal_primitive['targets'])
        primitives.append(primitive)

    return primitives


@cached
def get_primitive_cache_key(
        materials,
        blender_mesh,
        uuid_for_skined_data,
        vertex_groups,
        modifiers,
        export_settings):

    # Use id of mesh
    # Do not use bpy.types that can be unhashable
    # Do not use mesh name, that can be not unique (when linked)
    # Do not use materials here

    # TODO check what is really needed for modifiers

    return (
        (id(blender_mesh),),
        (modifiers,)
    )


@cached_by_key(key=get_primitive_cache_key)
def __gather_cache_primitives(
        materials,
        blender_mesh: bpy.types.Mesh,
        uuid_for_skined_data,
        vertex_groups: bpy.types.VertexGroups,
        modifiers: Optional[bpy.types.ObjectModifiers],
        export_settings
) -> List[dict]:
    """
    Gather parts that are identical for instances, i.e. excluding materials
    """
    primitives = []

    blender_primitives, additional_materials_udim, shared_attributes = gltf2_blender_gather_primitives_extract.extract_primitives(
        materials, blender_mesh, uuid_for_skined_data, vertex_groups, modifiers, export_settings)

    if shared_attributes is not None:

        if len(blender_primitives) > 0:
            shared = {}
            shared["attributes"] = shared_attributes

            attributes = __gather_attributes(shared, blender_mesh, modifiers, export_settings)
            targets = __gather_targets(shared, blender_mesh, modifiers, export_settings)

        for internal_primitive in blender_primitives:
            if internal_primitive.get('mode') is None:

                primitive = {
                    "attributes": attributes,
                    "indices": __gather_indices(internal_primitive, blender_mesh, modifiers, export_settings),
                    "mode": internal_primitive.get('mode'),
                    "material": internal_primitive.get('material'),
                    "targets": targets,
                    "uvmap_attributes_index": internal_primitive.get('uvmap_attributes_index')
                }

            else:
                # Edges & points, no shared attributes
                primitive = {
                    "attributes": __gather_attributes(internal_primitive, blender_mesh, modifiers, export_settings),
                    "indices": __gather_indices(internal_primitive, blender_mesh, modifiers, export_settings),
                    "mode": internal_primitive.get('mode'),
                    "material": internal_primitive.get('material'),
                    "targets": __gather_targets(internal_primitive, blender_mesh, modifiers, export_settings),
                    "uvmap_attributes_index": internal_primitive.get('uvmap_attributes_index')
                }
            primitives.append(primitive)

    else:

        for internal_primitive in blender_primitives:
            primitive = {
                "attributes": __gather_attributes(internal_primitive, blender_mesh, modifiers, export_settings),
                "indices": __gather_indices(internal_primitive, blender_mesh, modifiers, export_settings),
                "mode": internal_primitive.get('mode'),
                "material": internal_primitive.get('material'),
                "targets": __gather_targets(internal_primitive, blender_mesh, modifiers, export_settings),
                "uvmap_attributes_index": internal_primitive.get('uvmap_attributes_index')
            }
            primitives.append(primitive)

    return primitives, additional_materials_udim


def __gather_indices(blender_primitive, blender_mesh, modifiers, export_settings):
    indices = blender_primitive.get('indices')
    if indices is None:
        return None

    # NOTE: Values used by some graphics APIs as "primitive restart" values are disallowed.
    # Specifically, the values 65535 (in UINT16) and 4294967295 (in UINT32) cannot be used as indices.
    # https://github.com/KhronosGroup/glTF/issues/1142
    # https://github.com/KhronosGroup/glTF/pull/1476/files
    # Also, UINT8 mode is not supported:
    # https://github.com/KhronosGroup/glTF/issues/1471
    max_index = indices.max()
    if max_index < 65535:
        component_type = gltf2_io_constants.ComponentType.UnsignedShort
        indices = indices.astype(np.uint16, copy=False)
    elif max_index < 4294967295:
        component_type = gltf2_io_constants.ComponentType.UnsignedInt
        indices = indices.astype(np.uint32, copy=False)
    else:
        export_settings['log'].error(
            'A mesh contains too many vertices (' +
            str(max_index) +
            ') and needs to be split before export.')
        return None

    element_type = gltf2_io_constants.DataType.Scalar
    binary_data = gltf2_io_binary_data.BinaryData(
        indices.tobytes(), bufferViewTarget=gltf2_io_constants.BufferViewTarget.ELEMENT_ARRAY_BUFFER)
    return gather_accessor(
        binary_data,
        component_type,
        len(indices),
        None,
        None,
        element_type,
        export_settings
    )


def __gather_attributes(blender_primitive, blender_mesh, modifiers, export_settings):
    return gltf2_blender_gather_primitive_attributes.gather_primitive_attributes(blender_primitive, export_settings)


def __gather_targets(blender_primitive, blender_mesh, modifiers, export_settings):
    if export_settings['gltf_morph']:
        targets = []
        if blender_mesh.shape_keys is not None:
            morph_index = 0
            for blender_shape_key in get_sk_exported(blender_mesh.shape_keys.key_blocks):

                target_position_id = 'MORPH_POSITION_' + str(morph_index)
                target_normal_id = 'MORPH_NORMAL_' + str(morph_index)
                target_tangent_id = 'MORPH_TANGENT_' + str(morph_index)

                if blender_primitive["attributes"].get(target_position_id) is not None:
                    target = {}
                    internal_target_position = blender_primitive["attributes"][target_position_id]["data"]
                    target["POSITION"] = array_to_accessor(
                        internal_target_position,
                        export_settings,
                        component_type=gltf2_io_constants.ComponentType.Float,
                        data_type=gltf2_io_constants.DataType.Vec3,
                        include_max_and_min=True,
                        sparse_type='SK'
                    )

                    if export_settings['gltf_normals'] \
                            and export_settings['gltf_morph_normal'] \
                            and blender_primitive["attributes"].get(target_normal_id) is not None:

                        internal_target_normal = blender_primitive["attributes"][target_normal_id]["data"]
                        target['NORMAL'] = array_to_accessor(
                            internal_target_normal,
                            export_settings,
                            component_type=gltf2_io_constants.ComponentType.Float,
                            data_type=gltf2_io_constants.DataType.Vec3,
                            sparse_type='SK'
                        )

                    if export_settings['gltf_tangents'] \
                            and export_settings['gltf_morph_tangent'] \
                            and blender_primitive["attributes"].get(target_tangent_id) is not None:
                        internal_target_tangent = blender_primitive["attributes"][target_tangent_id]["data"]
                        target['TANGENT'] = array_to_accessor(
                            internal_target_tangent,
                            export_settings,
                            component_type=gltf2_io_constants.ComponentType.Float,
                            data_type=gltf2_io_constants.DataType.Vec3,
                            sparse_type='SK'
                        )
                    targets.append(target)
                    morph_index += 1
        return targets
    return None


def __gather_extensions(blender_mesh,
                        material_idx: int,
                        attr_indices: dict,
                        export_settings):
    extensions = {}

    if bpy.context.preferences.addons['io_scene_gltf2'].preferences.KHR_materials_variants_ui is False:
        return None

    if not bpy.data.scenes[0].gltf2_KHR_materials_variants_variants:
        return None

    # Material idx is the slot idx. Retrieve associated variant, if any
    mapping = []
    variants_idx_in_use = []
    for i in [v for v in blender_mesh.gltf2_variant_mesh_data if v.material_slot_index == material_idx]:
        variants = []
        for idx, v in enumerate(i.variants):
            if v.variant.variant_idx in [o.variant.variant_idx for o in i.variants[:idx]]:
                # Avoid duplicates
                continue

            # Avoid duplicates from a previous variant (in mapping list)
            # This happen only before fix of #2542
            if v.variant.variant_idx in variants_idx_in_use:
                # Avoid duplicates
                export_settings['log'].warning(
                    'Variant ' + str(v.variant.variant_idx) +
                    ' has 2 different materials for a single slot. Skipping it.')
                continue

            vari = ext_variants.gather_variant(v.variant.variant_idx, export_settings)
            if vari is not None:
                variants_idx_in_use.append(v.variant.variant_idx)
                variant_extension = gltf2_io_extensions.ChildOfRootExtension(
                    name="KHR_materials_variants",
                    path=["variants"],
                    extension=vari,
                    required=False
                )
            variants.append(variant_extension)
        if len(variants) > 0:
            if i.material:
                export_settings['current_paths'] = {}  # Used for KHR_animation_pointer.
                base_material, material_info = gather_material(
                    i.material,
                    export_settings
                )
            else:
                # empty slot
                base_material = None

            if base_material is not None:
                # Now, we can retrieve the real material, by checking attributes and active maps
                mat = get_final_material(
                    blender_mesh,
                    i.material,
                    attr_indices,
                    base_material,
                    material_info["uv_info"],
                    export_settings)
            else:
                mat = None

            mapping.append({'material': mat, 'variants': variants})

    if len(mapping) > 0:
        extensions["KHR_materials_variants"] = gltf2_io_extensions.Extension(
            name="KHR_materials_variants",
            extension={
                "mappings": mapping
            },
            required=False
        )

    return extensions if extensions else None
