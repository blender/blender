# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


__all__ = (
    "get_id_reference_map",
    "get_all_referenced_ids",
)


def get_id_reference_map():  # `-> dict[bpy.types.ID, set[bpy.types.ID]]`
    """Return a dictionary of direct datablock references for every datablock in the blend file."""
    import bpy

    inv_map = {}
    for key, values in bpy.data.user_map().items():
        for value in values:
            if value == key:
                # So an object is not considered to be referencing itself.
                continue
            inv_map.setdefault(value, set()).add(key)
    return inv_map


def recursive_get_referenced_ids(
        ref_map,  # `dict[bpy.types.ID, set[bpy.types.ID]]`
        id,  # `bpy.types.ID`
        referenced_ids,  # `set`
        visited,  # `set`
):  # `-> None`
    """Recursively populate referenced_ids with IDs referenced by id."""
    if id in visited:
        # Avoid infinite recursion from circular references.
        return
    visited.add(id)
    for ref in ref_map.get(id, []):
        referenced_ids.add(ref)
        recursive_get_referenced_ids(
            ref_map=ref_map, id=ref, referenced_ids=referenced_ids, visited=visited
        )


def get_all_referenced_ids(
        id,  # `bpy.types.ID`
        ref_map,  # `dict[bpy.types.ID, set[bpy.types.ID]]`
):  # `-> set[bpy.types.ID]`
    """Return a set of IDs directly or indirectly referenced by id."""
    referenced_ids = set()
    recursive_get_referenced_ids(
        ref_map=ref_map, id=id, referenced_ids=referenced_ids, visited=set()
    )
    return referenced_ids
