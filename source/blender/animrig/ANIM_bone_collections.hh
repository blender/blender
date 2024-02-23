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
BoneCollection *ANIM_bonecoll_new(const char *name) ATTR_WARN_UNUSED_RESULT;

/**
 * Free the bone collection.
 *
 * You don't typically need this function, unless you created a bone collection outside the scope
 * of a bArmature. Normally bone collections are owned (and thus managed) by the armature.
 *
 * \see ANIM_armature_bonecoll_remove
 *
 * \param do_id_user_count whether to update user counts for IDs referenced from IDProperties of
 * the bone collection. Needs to be false when freeing an evaluated copy, true otherwise.
 */
void ANIM_bonecoll_free(BoneCollection *bcoll, bool do_id_user_count = true);

/**
 * Recalculate the armature & bone runtime data.
 *
 * TODO: move to BKE?
 */
void ANIM_armature_runtime_refresh(bArmature *armature);

/**
 * Free armature & bone runtime data.
 * TODO: move to BKE?
 */
void ANIM_armature_runtime_free(bArmature *armature);

/**
 * Add a new bone collection to the given armature.
 *
 * \param parent_index: Index into the Armature's `collections_array`. -1 adds it
 * as a root (i.e. parentless) collection.
 *
 * The Armature owns the returned pointer.
 */
BoneCollection *ANIM_armature_bonecoll_new(bArmature *armature,
                                           const char *name,
                                           int parent_index = -1);

/**
 * Add a bone collection to the Armature.
 *
 * If `anchor` is null or isn't found, this inserts the copy at the start
 * of the collection array.
 *
 * NOTE: this should not typically be used. It is only used by the library overrides system to
 * apply override operations.
 */
BoneCollection *ANIM_armature_bonecoll_insert_copy_after(bArmature *armature_dst,
                                                         const bArmature *armature_src,
                                                         const BoneCollection *anchor_in_dst,
                                                         const BoneCollection *bcoll_to_copy);

/**
 * Remove the bone collection at `index` from the armature.
 */
void ANIM_armature_bonecoll_remove_from_index(bArmature *armature, const int index);

/**
 * Remove a bone collection from the armature.
 */
void ANIM_armature_bonecoll_remove(bArmature *armature, BoneCollection *bcoll);

/**
 * Set the given bone collection as the active one.
 *
 * Pass `nullptr` to clear the active bone collection.
 *
 * The bone collection MUST already be owned by this armature. If it is not,
 * this function will simply clear the active bone collection.
 */
void ANIM_armature_bonecoll_active_set(bArmature *armature, BoneCollection *bcoll);

/**
 * Set the bone collection with the given index as the active one.
 *
 * Pass -1 to clear the active bone collection.
 */
void ANIM_armature_bonecoll_active_index_set(bArmature *armature, int bone_collection_index);

/**
 * Set the bone collection with the given name as the active one.
 *
 * Pass an empty name to clear the active bone collection. A non-existent name will also cause the
 * active bone collection to be cleared.
 */
void ANIM_armature_bonecoll_active_name_set(bArmature *armature, const char *name);

/**
 * Refresh the Armature runtime info about the active bone collection.
 *
 * The ground truth for the active bone collection is the collection's name,
 * whereas the runtime info also contains the active collection's index and
 * pointer. This function updates the runtime info to point to the named
 * collection. If that named collection cannot be found, the name will be
 * cleared.
 */
void ANIM_armature_bonecoll_active_runtime_refresh(bArmature *armature);

/**
 * Determine whether the given bone collection is editable.
 *
 * Bone collections are editable when they are local, so either on a local Armature or added to a
 * linked Armature via a library override in the local file.
 */
bool ANIM_armature_bonecoll_is_editable(const bArmature *armature, const BoneCollection *bcoll);

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
 *
 * \note This function is limited to moving between siblings of the bone
 * collection at `from_index`.
 *
 * \note This function ensures that the element at index `from_index` (before
 * the call) will end up at `to_index` (after the call). The element at
 * `to_index` before the call will shift towards `from_index`; in other words,
 * depending on the direction of movement, the moved element will end up either
 * before or after that one.
 *
 * TODO: add ASCII-art illustration of left & right movement.
 *
 * \see blender::animrig::armature_bonecoll_move_to_parent() to move bone
 * collections between different parents.
 */
bool ANIM_armature_bonecoll_move_to_index(bArmature *armature, int from_index, int to_index);

enum class MoveLocation {
  Before, /* Move to before the item at the given index. */
  After,  /* Move to after the item at the given index. */
};

int ANIM_armature_bonecoll_move_before_after_index(bArmature *armature,
                                                   int from_index,
                                                   int to_index,
                                                   MoveLocation before_after);

/**
 * Move the bone collection by \a step places up/down.
 *
 * \return whether the move actually happened.
 *
 * \note This function is limited to moving between siblings of the bone
 * collection at `from_index`.
 *
 * \see blender::animrig::armature_bonecoll_move_to_parent() to move bone
 * collections between different parents.
 */
bool ANIM_armature_bonecoll_move(bArmature *armature, BoneCollection *bcoll, int step);

BoneCollection *ANIM_armature_bonecoll_get_by_name(bArmature *armature,
                                                   const char *name) ATTR_WARN_UNUSED_RESULT;

/** Scan the bone collections to find the one with the given name.
 *
 * \return the index of the bone collection, or -1 if not found.
 */
int ANIM_armature_bonecoll_get_index_by_name(bArmature *armature,
                                             const char *name) ATTR_WARN_UNUSED_RESULT;

void ANIM_armature_bonecoll_name_set(bArmature *armature, BoneCollection *bcoll, const char *name);

/**
 * Show this bone collection.
 *
 * This marks the bone collection as 'visible'. Whether it is effectively
 * visible also depends on the visibility state of its ancestors. */
void ANIM_bonecoll_show(bArmature *armature, BoneCollection *bcoll);

/**
 * Hide this bone collection.
 *
 * This marks the bone collection as 'hidden'. This also effectively hides its descendants,
 * regardless of their visibility state. */
void ANIM_bonecoll_hide(bArmature *armature, BoneCollection *bcoll);

/**
 * Show or hide this bone collection.
 *
 * Calling this with a hard-coded `is_visible` parameter is equivalent to
 * calling the dedicated show/hide functions. Prefer the dedicated functions for
 * clarity.
 *
 * \see ANIM_bonecoll_show
 * \see ANIM_bonecoll_hide
 */
void ANIM_armature_bonecoll_is_visible_set(bArmature *armature,
                                           BoneCollection *bcoll,
                                           bool is_visible);

/**
 * Set or clear this bone collection's solo flag.
 */
void ANIM_armature_bonecoll_solo_set(bArmature *armature, BoneCollection *bcoll, bool is_solo);

/**
 * Refresh the ARM_BCOLL_SOLO_ACTIVE flag.
 */
void ANIM_armature_refresh_solo_active(bArmature *armature);

/**
 * Determine whether this bone collection is visible, taking into account the visibility of its
 * ancestors and the "solo" flags that are in use.
 */
bool ANIM_armature_bonecoll_is_visible_effectively(const bArmature *armature,
                                                   const BoneCollection *bcoll);

/**
 * Expand or collapse a bone collection in the tree view.
 */
void ANIM_armature_bonecoll_is_expanded_set(BoneCollection *bcoll, bool is_expanded);

/**
 * Assign the bone to the bone collection.
 *
 * No-op if the bone is already a member of the collection.
 *
 * \return true if the bone was actually assigned, false if not (f.e. when it already was assigned
 * previously).
 */
bool ANIM_armature_bonecoll_assign(BoneCollection *bcoll, Bone *bone);
bool ANIM_armature_bonecoll_assign_editbone(BoneCollection *bcoll, EditBone *ebone);
bool ANIM_armature_bonecoll_assign_and_move(BoneCollection *bcoll, Bone *bone);
bool ANIM_armature_bonecoll_assign_and_move_editbone(BoneCollection *bcoll, EditBone *ebone);
bool ANIM_armature_bonecoll_unassign(BoneCollection *bcoll, Bone *bone);
bool ANIM_armature_bonecoll_unassign_editbone(BoneCollection *bcoll, EditBone *ebone);
void ANIM_armature_bonecoll_unassign_all(Bone *bone);
void ANIM_armature_bonecoll_unassign_all_editbone(EditBone *ebone);

/* Assign the edit bone to the armature's active collection. */
void ANIM_armature_bonecoll_assign_active(const bArmature *armature, EditBone *ebone);

/**
 * Return whether the Armature's active bone is assigned to the given bone collection.
 */
bool ANIM_armature_bonecoll_contains_active_bone(const bArmature *armature,
                                                 const BoneCollection *bcoll);

/**
 * Reconstruct the bone collection memberships, based on the bone runtime data.
 *
 * This is needed to transition out of armature edit mode. That removes all bones, and
 * recreates them from the edit-bones.
 */
void ANIM_armature_bonecoll_reconstruct(bArmature *armature);

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
bool ANIM_bone_in_visible_collection(const bArmature *armature, const Bone *bone);

inline bool ANIM_bone_is_visible(const bArmature *armature, const Bone *bone)
{
  const bool bone_itself_visible = (bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0;
  return bone_itself_visible && ANIM_bone_in_visible_collection(armature, bone);
}

bool ANIM_bonecoll_is_visible_editbone(const bArmature *armature, const EditBone *ebone);

inline bool ANIM_bone_is_visible_editbone(const bArmature *armature, const EditBone *ebone)
{
  const bool bone_itself_visible = (ebone->flag & BONE_HIDDEN_A) == 0;
  return bone_itself_visible && ANIM_bonecoll_is_visible_editbone(armature, ebone);
}

inline bool ANIM_bonecoll_is_visible_pchan(const bArmature *armature, const bPoseChannel *pchan)
{
  return ANIM_bone_in_visible_collection(armature, pchan->bone);
}

inline bool ANIM_bonecoll_is_visible_actbone(const bArmature *armature)
{
  return ANIM_bone_in_visible_collection(armature, armature->act_bone);
}

void ANIM_armature_bonecoll_show_all(bArmature *armature);
void ANIM_armature_bonecoll_hide_all(bArmature *armature);

void ANIM_armature_bonecoll_show_from_bone(bArmature *armature, const Bone *bone);
void ANIM_armature_bonecoll_show_from_ebone(bArmature *armature, const EditBone *ebone);
void ANIM_armature_bonecoll_show_from_pchan(bArmature *armature, const bPoseChannel *pchan);

namespace blender::animrig {

/**
 * Return the index of the given collection in the armature's collection array,
 * or -1 if not found.
 */
int armature_bonecoll_find_index(const bArmature *armature, const ::BoneCollection *bcoll);

/**
 * Return the index of the given bone collection's parent, or -1 if it has no parent.
 */
int armature_bonecoll_find_parent_index(const bArmature *armature, int bcoll_index);

/**
 * Find the child number of this bone collection.
 *
 * This is the offset of this collection relative to the parent's first child.
 * In other words, the first child has number 0, second child has number 1, etc.
 *
 * This requires a scan of the array, hence the function is called 'find' and not 'get'.
 */
int armature_bonecoll_child_number_find(const bArmature *armature, const ::BoneCollection *bcoll);

/**
 * Move this bone collection to a new child number.
 *
 * \return the new absolute index of the bone collection, or -1 if the new child number was not
 * valid.
 *
 * \see armature_bonecoll_child_number_find
 */
int armature_bonecoll_child_number_set(bArmature *armature,
                                       ::BoneCollection *bcoll,
                                       int new_child_number);

bool armature_bonecoll_is_root(const bArmature *armature, int bcoll_index);

bool armature_bonecoll_is_child_of(const bArmature *armature,
                                   int potential_parent_index,
                                   int potential_child_index);

bool armature_bonecoll_is_descendant_of(const bArmature *armature,
                                        int potential_parent_index,
                                        int potential_descendant_index);

bool bonecoll_has_children(const BoneCollection *bcoll);

/**
 * For each bone collection in the destination armature, copy its #BONE_COLLECTION_EXPANDED flag
 * from the corresponding bone collection in the source armature.
 *
 * This is used in the handling of undo steps, to ensure that undo'ing does _not_
 * modify this flag.
 */
void bonecolls_copy_expanded_flag(Span<BoneCollection *> bcolls_dest,
                                  Span<const BoneCollection *> bcolls_source);

/**
 * Move a bone collection from one parent to another.
 *
 * \param from_bcoll_index: Index of the bone collection to move.
 * \param to_child_num: Gap index of where to insert the collection; 0 to make it
 * the first child, and parent->child_count to make it the last child. -1 also
 * works as an indicator for the last child, as that makes it possible to call
 * this function without requiring the caller to find the BoneCollection* of the
 * parent.
 * \param from_parent_index: Index of its current parent (-1 if it is a root collection).
 * \param to_parent_index: Index of the new parent (-1 if it is to become a root collection).
 * \return the collection's new index in the collections_array.
 */
int armature_bonecoll_move_to_parent(bArmature *armature,
                                     int from_bcoll_index,
                                     int to_child_num,
                                     int from_parent_index,
                                     int to_parent_index);

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
