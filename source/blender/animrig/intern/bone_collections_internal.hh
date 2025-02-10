/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Internal C++ functions to deal with bone collections. These are mostly here for internal
 * use in `bone_collections.cc` and have them testable by unit tests.
 */

#pragma once

struct bArmature;
struct BoneCollection;

namespace blender::animrig::internal {

/**
 * Move a block of BoneCollections in the Armature's `collections_array`, from
 * `start_index` to `start_index + direction`.
 *
 * The move operation is actually implemented as a rotation, so that no
 * `BoneCollection*` is lost. In other words, one of these operations is
 * performed, depending on `direction`. Here `B` indicates an element in the
 * moved block, and `X` indicates the rotated element.
 *
 * direction = +1: [. . . X B B B B . . .] -> [. . . B B B B X . . .]
 * direction = -1: [. . . B B B B X . . .] -> [. . . X B B B B . . .]
 *
 * This function does not alter the length of `collections_array`.
 * It only performs the rotation, and updates any `child_index` when they
 * reference elements of the moved block.
 *
 * It also does not touch any `child_count` properties of bone collections.
 * Updating those, as well as any references to the rotated element, is the
 * responsibility of the caller.
 *
 * \param direction: Must be either -1 or 1.
 */
void bonecolls_rotate_block(bArmature *armature, int start_index, int count, int direction);

/**
 * Move a bone collection to another index.
 *
 * This is implemented via a call to #bonecolls_rotate_block, so all the
 * documentation of that function (including its invariants and caveats) applies
 * here too.
 */
void bonecolls_move_to_index(bArmature *armature, int from_index, int to_index);

/**
 * Find the given bone collection in the armature's collections, and return its index.
 *
 * The bone collection is only searched for at the given index, index+1, and index-1.
 *
 * If the bone collection cannot be found, -1 is returned.
 */
int bonecolls_find_index_near(bArmature *armature, BoneCollection *bcoll, int index);

void bonecolls_debug_list(const bArmature *armature);

/**
 * Unassign all (edit)bones from this bone collection, and free it.
 *
 * Note that this does NOT take care of updating the collection hierarchy information. See
 * #ANIM_armature_bonecoll_remove_from_index and #ANIM_armature_bonecoll_remove for that.
 */
void bonecoll_unassign_and_free(bArmature *armature, BoneCollection *bcoll);

}  // namespace blender::animrig::internal
