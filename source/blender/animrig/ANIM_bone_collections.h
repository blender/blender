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
struct bPoseChannel;
struct EditBone;

/**
 * Armature/Bone Layer abstractions. These functions are intended as the sole
 * accessors for `bone->layer`, `armature->layer`, etc. to get a grip on which
 * queries & operations are performed.
 *
 * The functions are named "bonecoll" (short for "bone collection"), as that's
 * the soon-to-be-introduced replacement for armature layers. This API is the
 * first step towards replacement.
 */

inline bool ANIM_bonecoll_is_visible(const struct bArmature *armature, const struct Bone *bone)
{
  return armature->layer & bone->layer;
}

inline bool ANIM_bone_is_visible(const struct bArmature *armature, const struct Bone *bone)
{
  const bool bone_itself_visible = (bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0;
  return bone_itself_visible && ANIM_bonecoll_is_visible(armature, bone);
}

inline bool ANIM_bonecoll_is_visible_editbone(const struct bArmature *armature,
                                              const struct EditBone *ebone)
{
  return armature->layer & ebone->layer;
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

void ANIM_armature_enable_layers(struct bArmature *armature, const int layers);
void ANIM_armature_disable_all_layers(struct bArmature *armature);
void ANIM_bone_set_layer_ebone(struct EditBone *ebone, int layer);
void ANIM_bone_set_ebone_layer_from_armature(struct EditBone *ebone,
                                             const struct bArmature *armature);
void ANIM_armature_ensure_first_layer_enabled(struct bArmature *armature);
void ANIM_armature_ensure_layer_enabled_from_bone(struct bArmature *armature,
                                                  const struct Bone *bone);
void ANIM_armature_ensure_layer_enabled_from_ebone(struct bArmature *armature,
                                                   const struct EditBone *ebone);
void ANIM_armature_ensure_layer_enabled_from_pchan(struct bArmature *armature,
                                                   const struct bPoseChannel *pchan);
#ifdef __cplusplus
}
#endif
