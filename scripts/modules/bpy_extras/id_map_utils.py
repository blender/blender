# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "get_id_reference_map",
    "get_all_referenced_ids",
)


def get_id_reference_map():
    """
    :return: Return a dictionary of direct data-block references for every data-block in the blend file.
    :rtype: dict[:class:`bpy.types.ID`, set[:class:`bpy.types.ID`]]
    """
    import bpy
    inv_map = {}
    for key, values in bpy.data.user_map().items():
        for value in values:
            if value == key:
                # So an object is not considered to be referencing itself.
                continue
            inv_map.setdefault(value, set()).add(key)
    return inv_map


# Recursively populate referenced_ids with IDs referenced by `id`.
def _recursive_get_referenced_ids(
        ref_map,  # `dict[ID, set[ID]]`
        id,  # `ID`
        referenced_ids,  # `set[ID]`
        visited,  # `set[ID]`
):  # `-> None`
    if id in visited:
        # Avoid infinite recursion from circular references.
        return
    visited.add(id)
    for ref in ref_map.get(id, []):
        referenced_ids.add(ref)
        _recursive_get_referenced_ids(
            ref_map=ref_map, id=ref, referenced_ids=referenced_ids, visited=visited
        )


def get_all_referenced_ids(id, ref_map):
    """
    :arg id: The ID to lookup.
    :type id: :class:`bpy.types.ID`
    :arg ref_map: The ID to lookup.
    :type ref_map:  dict[:class:`bpy.types.ID`, set[:class:`bpy.types.ID`]]
    :return: A set of IDs directly or indirectly referenced by ``id``.
    :rtype: set[:class:`bpy.types.ID`]
    """
    referenced_ids = set()
    _recursive_get_referenced_ids(
        ref_map=ref_map, id=id, referenced_ids=referenced_ids, visited=set()
    )
    return referenced_ids
