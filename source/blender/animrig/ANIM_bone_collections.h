/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to deal with Armature collections (i.e. the successor of bone layers).
 */

#pragma once

#include <stdbool.h>

#include "BLI_math_bits.h"

#include "BKE_armature.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * \see #ANIM_armature_bonecoll_new
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
 * NOTE: this should only be used when the runtime structs on the Armature and Bones are still
 * empty. Any data allocated there will NOT be freed.
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
struct BoneCollection *ANIM_armature_bonecoll_new(struct bArmature *armature, const char *name);

/**
 * Add a bone collection to the Armature.
 *
 * NOTE: this should not typically be used. It is only used by the library overrides system to
 * apply override operations.
 */
struct BoneCollection *ANIM_armature_bonecoll_insert_copy_after(
    struct bArmature *armature,
    struct BoneCollection *anchor,
    const struct BoneCollection *bcoll_to_copy);

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
 * Determine whether the given bone collection is editable.
 *
 * Bone collections are editable when they are local, so either on a local Armature or added to a
 * linked Armature via a library override in the local file.
 */
bool ANIM_armature_bonecoll_is_editable(const struct bArmature *armature,
                                        const struct BoneCollection *bcoll);

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
bool ANIM_armature_bonecoll_unassign(struct BoneCollection *bcoll, struct Bone *bone);
void ANIM_armature_bonecoll_unassign_all(struct Bone *bone);
bool ANIM_armature_bonecoll_unassign_editbone(struct BoneCollection *bcoll,
                                              struct EditBone *ebone);

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

/* Only used by the Collada I/O code: */
void ANIM_armature_enable_layers(struct bArmature *armature, const int layers);
void ANIM_bone_set_layer_ebone(struct EditBone *ebone, int layer);

void ANIM_armature_bonecoll_show_from_bone(struct bArmature *armature, const struct Bone *bone);
void ANIM_armature_bonecoll_show_from_ebone(struct bArmature *armature,
                                            const struct EditBone *ebone);
void ANIM_armature_bonecoll_show_from_pchan(struct bArmature *armature,
                                            const struct bPoseChannel *pchan);
#ifdef __cplusplus
}
#endif
