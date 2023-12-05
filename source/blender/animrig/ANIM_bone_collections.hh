/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief C++ functions to deal with Armature collections (i.e. the successor of bone layers).
 */

#pragma once

#ifndef __cplusplus
#  error This is a C++ header.
#endif

#include "BLI_map.hh"

#include "ANIM_bone_collections.h"

namespace blender::animrig {

/* --------------------------------------------------------------------
 * The following functions are only used by edit-mode Armature undo:
 */

/**
 * Duplicates a list of BoneCollections for edit-mode undo purposes, and
 * returns original-to-duplicate remapping data.
 *
 * IMPORTANT: this discards membership data in the duplicate collections.
 * This is because this function is only intended to be used with
 * edit-mode Armatures, where the membership information in collections
 * is not definitive, instead being stored in the EditBones. The
 * assumption is that the membership information in the collections will
 * be rebuilt from the EditBones when leaving edit mode.
 *
 * The source and destination each have two parts: a heap-allocated array of
 * `BoneCollection *`, and an integer that keeps track of that array's length.
 * The destination parameters are pointers to those components, so they can
 * be modified.  The destination array should be empty and unallocated.
 *
 * \param bcoll_array_dst,bcoll_array_dst_num: the destination BoneCollection
 * array and array size.
 * \param bcoll_array_src,bcoll_array_src_num: the source BoneCollection array
 * and array size.
 * \param do_id_user: when true, increments the user count of IDs that
 * the BoneCollections' custom properties point to, if any.
 *
 * \return a map from pointers-to-the-original-collections to
 * pointers-to-the-duplicate-collections. This can be used to remap
 * collection pointers in other data, such as EditBones.
 */
blender::Map<BoneCollection *, BoneCollection *> ANIM_bonecoll_array_copy_no_membership(
    BoneCollection ***bcoll_array_dst,
    int *bcoll_array_dst_num,
    BoneCollection **bcoll_array_src,
    int bcoll_array_src_num,
    bool do_id_user);
/**
 * Frees a list of BoneCollections.
 *
 * IMPORTANT: although there is nothing about this function that
 * fundamentally prevents it from being used generally, other data
 * structures like Armature runtime data and EditBones often store
 * direct pointers to BoneCollections, which this function does NOT
 * handle. Prefer using higher-level functions to remove BoneCollections
 * from Armatures.
 *
 * \param bcoll_array: pointer to the heap-allocated array of `BoneCollection *`
 * to be freed.
 * \param bcoll_array_num: pointer to the integer that tracks the length of
 * bcoll_array.
 * \param do_id_user: when true, decrements the user count of IDs that
 * the BoneCollections' custom properties point to, if any.
 */
void ANIM_bonecoll_array_free(BoneCollection ***bcoll_array,
                              int *bcoll_array_num,
                              bool do_id_user);

}  // namespace blender::animrig
