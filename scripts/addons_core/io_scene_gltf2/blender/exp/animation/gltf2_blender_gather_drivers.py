# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ....blender.com.gltf2_blender_data_path import get_sk_exported, skip_sk
from ...com.gltf2_blender_data_path import get_target_object_path
from ..gltf2_blender_gather_cache import skdriverdiscovercache


@skdriverdiscovercache
def get_sk_drivers(blender_armature_uuid, export_settings):

    # If no SK are exported --> No driver animation to export
    if export_settings['gltf_morph_anim'] is False:
        return []

    drivers = []

    # Take into account skinned mesh, and mesh parented to a bone of the armature
    children_list = export_settings['vtree'].nodes[blender_armature_uuid].children.copy()
    for bone in export_settings['vtree'].get_all_bones(blender_armature_uuid):
        children_list.extend(export_settings['vtree'].nodes[bone].children)

    for child_uuid in children_list:
        uuid, channels = get_driver_on_shapekey(child_uuid, export_settings)
        if uuid is not None:
            drivers.append(child_uuid)

    return drivers


def get_driver_on_shapekey(blender_object_uuid, export_settings):
    if export_settings['vtree'].nodes[blender_object_uuid].blender_type == "BONE":
        return None, None

    child = export_settings['vtree'].nodes[blender_object_uuid].blender_object

    if not child.data:
        return None, None
    # child.data can be an armature - which has no shapekeys
    if not hasattr(child.data, 'shape_keys'):
        return None, None
    if not child.data.shape_keys:
        return None, None
    if not child.data.shape_keys.animation_data:
        return None, None
    if not child.data.shape_keys.animation_data.drivers:
        return None, None
    if len(child.data.shape_keys.animation_data.drivers) <= 0:
        return None, None

    shapekeys_idx = {}
    cpt_sk = 0
    for sk in get_sk_exported(child.data.shape_keys.key_blocks):
        shapekeys_idx[sk.name] = cpt_sk
        cpt_sk += 1

    # Note: channels will have some None items only for SK if some SK are not animated
    idx_channel_mapping = []
    all_sorted_channels = []
    for sk_c in child.data.shape_keys.animation_data.drivers:
        # Check if driver is valid. If not, ignore this driver channel
        try:
            # Check if driver is valid.
            # Try/Except is no more a suffisant check, starting with version Blender 3.0,
            # Blender crashes when trying to resolve path on invalid driver
            if not sk_c.is_valid:
                return None, None
            sk_name = child.data.shape_keys.path_resolve(get_target_object_path(sk_c.data_path)).name
        except:
            return None, None
        if skip_sk(child.data.shape_keys.key_blocks, child.data.shape_keys.key_blocks[sk_name]):
            return None, None
        idx_channel_mapping.append((shapekeys_idx[sk_name], sk_c))
    existing_idx = dict(idx_channel_mapping)
    for i in range(0, cpt_sk):
        if i not in existing_idx.keys():
            all_sorted_channels.append(None)
        else:
            all_sorted_channels.append(existing_idx[i])

    # Checks there are some driver on SK, and that there is not only invalid drivers
    if len(all_sorted_channels) > 0 and not all([i is None for i in all_sorted_channels]):
        return blender_object_uuid, all_sorted_channels
