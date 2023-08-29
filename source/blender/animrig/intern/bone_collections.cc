/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "DNA_armature_types.h"

#include "BLI_math_bits.h"

#include "ANIM_bone_collections.h"

/* ********************************* */
/* Armature Layers transitional API. */

void ANIM_armature_enable_layers(bArmature *armature, const int layers)
{
  armature->layer |= layers;
}

void ANIM_armature_disable_all_layers(bArmature *armature)
{
  armature->layer = 0;
}

void ANIM_bone_set_layer_ebone(EditBone *ebone, const int layer)
{
  ebone->layer = layer;
}

void ANIM_bone_set_ebone_layer_from_armature(EditBone *ebone, const bArmature *armature)
{
  ebone->layer = armature->layer;
}

void ANIM_armature_ensure_first_layer_enabled(bArmature *armature)
{
  armature->layer = 1;
}

void ANIM_armature_ensure_layer_enabled_from_bone(bArmature *armature, const Bone *bone)
{
  if (ANIM_bonecoll_is_visible(armature, bone)) {
    return;
  }
  armature->layer |= 1U << bitscan_forward_uint(bone->layer);
}

void ANIM_armature_ensure_layer_enabled_from_ebone(bArmature *armature, const EditBone *ebone)
{
  if (ANIM_bonecoll_is_visible_editbone(armature, ebone)) {
    return;
  }
  armature->layer |= 1U << bitscan_forward_uint(ebone->layer);
}

void ANIM_armature_ensure_layer_enabled_from_pchan(bArmature *armature, const bPoseChannel *pchan)
{
  ANIM_armature_ensure_layer_enabled_from_bone(armature, pchan->bone);
}
