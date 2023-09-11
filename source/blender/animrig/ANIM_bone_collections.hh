/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
 * \param do_id_user: when true, increments the user count of IDs that
 * the BoneCollections' custom properties point to, if any.
 *
 * \return a map from pointers-to-the-original-collections to
 * pointers-to-the-duplicate-collections. This can be used to remap
 * collection pointers in other data, such as EditBones.
 */
blender::Map<BoneCollection *, BoneCollection *> ANIM_bonecoll_listbase_copy_no_membership(
    ListBase *bone_colls_dst, ListBase *bone_colls_src, bool do_id_user);
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
 * \param do_id_user: when true, decrements the user count of IDs that
 * the BoneCollections' custom properties point to, if any.
 */
void ANIM_bonecoll_listbase_free(ListBase *bcolls, bool do_id_user);

}  // namespace blender::animrig
