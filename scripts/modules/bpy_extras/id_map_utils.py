# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy


__all__ = (
    "get_id_reference_map",
    "get_all_referenced_ids",
)


def get_id_reference_map():
    """Return a dictionary of direct data-block references for every data-block in the blend file.

    :return: Each datablock of the .blend file mapped to the set of IDs they directly reference.
    :rtype: dict[bpy.types.ID, set[bpy.types.ID]]
    """
    inv_map = {}
    for key, values in bpy.data.user_map().items():
        for value in values:
            if value == key:
                # So an object is not considered to be referencing itself.
                continue
            inv_map.setdefault(value, set()).add(key)
    return inv_map


def get_all_referenced_ids(id, ref_map):
    """
    Return a set of IDs directly or indirectly referenced by id.

    :param id: Datablock whose references we're interested in.
    :type id: bpy.types.ID
    :param ref_map: The global ID reference map, retrieved from get_id_reference_map()
    :type ref_map: dict[bpy.types.ID, set[bpy.types.ID]]
    :return: Set of datablocks referenced by `id`.
    :rtype: set[bpy.types.ID]
    """

    def recursive_helper(ref_map, id, referenced_ids, visited):
        if id in visited:
            # Avoid infinite recursion from circular references.
            return
        visited.add(id)
        for ref in ref_map.get(id, []):
            referenced_ids.add(ref)
            recursive_helper(ref_map=ref_map, id=ref, referenced_ids=referenced_ids, visited=visited)

    referenced_ids = set()
    recursive_helper(ref_map=ref_map, id=id, referenced_ids=referenced_ids, visited=set())
    return referenced_ids
