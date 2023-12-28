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

#include <stdbool.h>

#include "BLI_map.hh"
#include "BLI_math_bits.h"

#include "BKE_armature.hh"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"

struct bArmature;
struct Bone;
struct BoneCollection;
struct bPoseChannel;
struct EditBone;

/**
 * Construct a new #BoneCollection with the given name.
 *
 * The caller owns the returned pointer.
 *
 * You don't typically use this function directly, but rather create a bone collection on a
 * bArmature.
 *
 * \see #armature_bonecoll_new
 */
struct BoneCollection *ANIM_bonecoll_new(const char *name) ATTR_WARN_UNUSED_RESULT;

/**
 * Free the bone collection.
 *
 * You don't typically need this function, unless you created a bone collection outside the scope
 * of a bArmature. Normally bone collections are owned (and thus managed) by the armature.
 *
 * \see ANIM_armature_bonecoll_remove
 */
void ANIM_bonecoll_free(struct BoneCollection *bcoll);

/**
 * Recalculate the armature & bone runtime data.
 *
 * TODO: move to BKE?
 */
void ANIM_armature_runtime_refresh(struct bArmature *armature);

/**
 * Free armature & bone runtime data.
 * TODO: move to BKE?
 */
void ANIM_armature_runtime_free(struct bArmature *armature);

/**
 * Add a new bone collection to the given armature.
 *
 * The Armature owns the returned pointer.
 */
BoneCollection *ANIM_armature_bonecoll_new(bArmature *armature, const char *name);

/**
 * Add a bone collection to the Armature.
 *
 * If `anchor` is null or isn't found, this inserts the copy at the start
 * of the collection array.
 *
 * NOTE: this should not typically be used. It is only used by the library overrides system to
 * apply override operations.
 */
struct BoneCollection *ANIM_armature_bonecoll_insert_copy_after(
    struct bArmature *armature,
    struct BoneCollection *anchor,
    const struct BoneCollection *bcoll_to_copy);

/**
 * Remove the bone collection at `index` from the armature.
 */
void ANIM_armature_bonecoll_remove_from_index(bArmature *armature, const int index);

/**
 * Remove a bone collection from the armature.
 */
void ANIM_armature_bonecoll_remove(struct bArmature *armature, struct BoneCollection *bcoll);

/**
 * Set the given bone collection as the active one.
 *
 * Pass `nullptr` to clear the active bone collection.
 *
 * The bone collection MUST already be owned by this armature. If it is not,
 * this function will simply clear the active bone collection.
 */
void ANIM_armature_bonecoll_active_set(struct bArmature *armature, struct BoneCollection *bcoll);

/**
 * Set the bone collection with the given index as the active one.
 *
 * Pass -1 to clear the active bone collection.
 */
void ANIM_armature_bonecoll_active_index_set(struct bArmature *armature,
                                             int bone_collection_index);

/**
 * Set the bone collection with the given name as the active one.
 *
 * Pass an empty name to clear the active bone collection. A non-existent name will also cause the
 * active bone collection to be cleared.
 */
void ANIM_armature_bonecoll_active_name_set(struct bArmature *armature, const char *name);

/**
 * Refresh the Armature runtime info about the active bone collection.
 *
 * The ground truth for the active bone collection is the collection's name,
 * whereas the runtime info also contains the active collection's index and
 * pointer. This function updates the runtime info to point to the named
 * collection. If that named collection cannot be found, the name will be
 * cleared.
 */
void ANIM_armature_bonecoll_active_runtime_refresh(struct bArmature *armature);

/**
 * Determine whether the given bone collection is editable.
 *
 * Bone collections are editable when they are local, so either on a local Armature or added to a
 * linked Armature via a library override in the local file.
 */
bool ANIM_armature_bonecoll_is_editable(const struct bArmature *armature,
                                        const struct BoneCollection *bcoll);

/**
 * Move the bone collection at from_index to its sibling at to_index.
 *
 * The element at `to_index` is shifted to make space; it is not overwritten.
 * This shift happens towards `from_index`.
 *
 * This operation does not change the total number of elements in the array.
 *
 * \return true if the collection was successfully moved, false otherwise.
 * The latter happens if either index is out of bounds, or if the indices
 * are equal.
 * \note This function ensures that the element at index `from_index` (before
 * the call) will end up at `to_index` (after the call). The element at
 * `to_index` before the call will shift towards `from_index`; in other words,
 * depending on the direction of movement, the moved element will end up either
 * before or after that one.
 *
 * TODO: add ASCII-art illustration of left & right movement.
 */
bool ANIM_armature_bonecoll_move_to_index(bArmature *armature, int from_index, int to_index);

/**
 * Move the bone collection by \a step places up/down.
 *
 * \return whether the move actually happened.
 */
bool ANIM_armature_bonecoll_move(struct bArmature *armature,
                                 struct BoneCollection *bcoll,
                                 int step);

struct BoneCollection *ANIM_armature_bonecoll_get_by_name(
    struct bArmature *armature, const char *name) ATTR_WARN_UNUSED_RESULT;

void ANIM_armature_bonecoll_name_set(struct bArmature *armature,
                                     struct BoneCollection *bcoll,
                                     const char *name);

void ANIM_bonecoll_show(struct BoneCollection *bcoll);
void ANIM_bonecoll_hide(struct BoneCollection *bcoll);

/**
 * Assign the bone to the bone collection.
 *
 * No-op if the bone is already a member of the collection.
 *
 * \return true if the bone was actually assigned, false if not (f.e. when it already was assigned
 * previously).
 */
bool ANIM_armature_bonecoll_assign(struct BoneCollection *bcoll, struct Bone *bone);
bool ANIM_armature_bonecoll_assign_editbone(struct BoneCollection *bcoll, struct EditBone *ebone);
bool ANIM_armature_bonecoll_assign_and_move(struct BoneCollection *bcoll, struct Bone *bone);
bool ANIM_armature_bonecoll_assign_and_move_editbone(struct BoneCollection *bcoll,
                                                     struct EditBone *ebone);
bool ANIM_armature_bonecoll_unassign(struct BoneCollection *bcoll, struct Bone *bone);
bool ANIM_armature_bonecoll_unassign_editbone(struct BoneCollection *bcoll,
                                              struct EditBone *ebone);
void ANIM_armature_bonecoll_unassign_all(struct Bone *bone);
void ANIM_armature_bonecoll_unassign_all_editbone(struct EditBone *ebone);

/* Assign the edit bone to the armature's active collection. */
void ANIM_armature_bonecoll_assign_active(const struct bArmature *armature,
                                          struct EditBone *ebone);

/**
 * Reconstruct the bone collection memberships, based on the bone runtime data.
 *
 * This is needed to transition out of armature edit mode. That removes all bones, and
 * recreates them from the edit-bones.
 */
void ANIM_armature_bonecoll_reconstruct(struct bArmature *armature);

/*
 * Armature/Bone Layer abstractions. These functions are intended as the sole
 * accessors for `bone->layer`, `armature->layer`, etc. to get a grip on which
 * queries & operations are performed.
 *
 * The functions are named "bonecoll" (short for "bone collection"), as that's
 * the soon-to-be-introduced replacement for armature layers. This API is the
 * first step towards replacement.
 */

/** Return true when any of the bone's collections is visible. */
bool ANIM_bonecoll_is_visible(const struct bArmature *armature, const struct Bone *bone);

inline bool ANIM_bone_is_visible(const struct bArmature *armature, const struct Bone *bone)
{
  const bool bone_itself_visible = (bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0;
  return bone_itself_visible && ANIM_bonecoll_is_visible(armature, bone);
}

bool ANIM_bonecoll_is_visible_editbone(const struct bArmature *armature,
                                       const struct EditBone *ebone);

inline bool ANIM_bone_is_visible_editbone(const struct bArmature *armature,
                                          const struct EditBone *ebone)
{
  const bool bone_itself_visible = (ebone->flag & BONE_HIDDEN_A) == 0;
  return bone_itself_visible && ANIM_bonecoll_is_visible_editbone(armature, ebone);
}

inline bool ANIM_bonecoll_is_visible_pchan(const struct bArmature *armature,
                                           const struct bPoseChannel *pchan)
{
  return ANIM_bonecoll_is_visible(armature, pchan->bone);
}

inline bool ANIM_bonecoll_is_visible_actbone(const struct bArmature *armature)
{
  return ANIM_bonecoll_is_visible(armature, armature->act_bone);
}

void ANIM_armature_bonecoll_show_all(struct bArmature *armature);
void ANIM_armature_bonecoll_hide_all(struct bArmature *armature);

void ANIM_armature_bonecoll_show_from_bone(struct bArmature *armature, const struct Bone *bone);
void ANIM_armature_bonecoll_show_from_ebone(struct bArmature *armature,
                                            const struct EditBone *ebone);
void ANIM_armature_bonecoll_show_from_pchan(struct bArmature *armature,
                                            const struct bPoseChannel *pchan);

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
