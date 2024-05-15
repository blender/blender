# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from typing import Optional, Dict, List, Any, Tuple
from ...io.com import gltf2_io
from ...blender.com.gltf2_blender_data_path import get_sk_exported
from ...io.exp.gltf2_io_user_extensions import export_user_extensions
from ..com.gltf2_blender_extras import generate_extras
from . import gltf2_blender_gather_primitives
from .gltf2_blender_gather_cache import cached_by_key


def get_mesh_cache_key(blender_mesh,
                       blender_object,
                       vertex_groups,
                       modifiers,
                       materials,
                       original_mesh,
                       export_settings):
    # Use id of original mesh
    # Do not use bpy.types that can be unhashable
    # Do not use mesh name, that can be not unique (when linked)

    # If materials are not exported, no need to cache by material
    if export_settings['gltf_materials'] is None:
        mats = None
    else:
        mats = tuple(id(m) if m is not None else None for m in materials)

    # TODO check what is really needed for modifiers

    mesh_to_id_cache = blender_mesh if original_mesh is None else original_mesh
    return (
        (id(mesh_to_id_cache),),
        (modifiers,),
        mats
    )


@cached_by_key(key=get_mesh_cache_key)
def gather_mesh(blender_mesh: bpy.types.Mesh,
                uuid_for_skined_data,
                vertex_groups: bpy.types.VertexGroups,
                modifiers: Optional[bpy.types.ObjectModifiers],
                materials: Tuple[bpy.types.Material],
                original_mesh: bpy.types.Mesh,
                export_settings
                ) -> Optional[gltf2_io.Mesh]:
    if not __filter_mesh(blender_mesh, vertex_groups, modifiers, export_settings):
        return None

    mesh = gltf2_io.Mesh(
        extensions=__gather_extensions(
            blender_mesh, vertex_groups, modifiers, export_settings), extras=__gather_extras(
            blender_mesh, vertex_groups, modifiers, export_settings), name=__gather_name(
                blender_mesh, vertex_groups, modifiers, export_settings), weights=__gather_weights(
                    blender_mesh, vertex_groups, modifiers, export_settings), primitives=__gather_primitives(
                        blender_mesh, uuid_for_skined_data, vertex_groups, modifiers, materials, export_settings), )

    if len(mesh.primitives) == 0:
        export_settings['log'].warning("Mesh '{}' has no primitives and will be omitted.".format(mesh.name))
        return None

    blender_object = None
    if uuid_for_skined_data:
        blender_object = export_settings['vtree'].nodes[uuid_for_skined_data].blender_object

    export_user_extensions('gather_mesh_hook',
                           export_settings,
                           mesh,
                           blender_mesh,
                           blender_object,
                           vertex_groups,
                           modifiers,
                           materials)

    return mesh


def __filter_mesh(blender_mesh: bpy.types.Mesh,
                  vertex_groups: bpy.types.VertexGroups,
                  modifiers: Optional[bpy.types.ObjectModifiers],
                  export_settings
                  ) -> bool:
    return True


def __gather_extensions(blender_mesh: bpy.types.Mesh,
                        vertex_groups: bpy.types.VertexGroups,
                        modifiers: Optional[bpy.types.ObjectModifiers],
                        export_settings
                        ) -> Any:
    return None


def __gather_extras(blender_mesh: bpy.types.Mesh,
                    vertex_groups: bpy.types.VertexGroups,
                    modifiers: Optional[bpy.types.ObjectModifiers],
                    export_settings
                    ) -> Optional[Dict[Any, Any]]:

    extras = {}

    if export_settings['gltf_extras']:
        extras = generate_extras(blender_mesh) or {}

    # Not for GN Instances
    if export_settings['gltf_morph'] and blender_mesh.shape_keys and blender_mesh.users != 0:
        morph_max = len(blender_mesh.shape_keys.key_blocks) - 1
        if morph_max > 0:
            extras['targetNames'] = [k.name for k in get_sk_exported(blender_mesh.shape_keys.key_blocks)]

    if extras:
        return extras

    return None


def __gather_name(blender_mesh: bpy.types.Mesh,
                  vertex_groups: bpy.types.VertexGroups,
                  modifiers: Optional[bpy.types.ObjectModifiers],
                  export_settings
                  ) -> str:
    return blender_mesh.name


def __gather_primitives(blender_mesh: bpy.types.Mesh,
                        uuid_for_skined_data,
                        vertex_groups: bpy.types.VertexGroups,
                        modifiers: Optional[bpy.types.ObjectModifiers],
                        materials: Tuple[bpy.types.Material],
                        export_settings
                        ) -> List[gltf2_io.MeshPrimitive]:
    return gltf2_blender_gather_primitives.gather_primitives(blender_mesh,
                                                             uuid_for_skined_data,
                                                             vertex_groups,
                                                             modifiers,
                                                             materials,
                                                             export_settings)


def __gather_weights(blender_mesh: bpy.types.Mesh,
                     vertex_groups: bpy.types.VertexGroups,
                     modifiers: Optional[bpy.types.ObjectModifiers],
                     export_settings
                     ) -> Optional[List[float]]:
    if not export_settings['gltf_morph'] or not blender_mesh.shape_keys:
        return None

    # Not for GN Instances
    if blender_mesh.users == 0:
        return None

    morph_max = len(blender_mesh.shape_keys.key_blocks) - 1
    if morph_max <= 0:
        return None

    return [k.value for k in get_sk_exported(blender_mesh.shape_keys.key_blocks)]
