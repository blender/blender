/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Iterators for armatures.
 */

#pragma once

#ifndef __cplusplus
#  error This is a C++ header.
#endif

#include "DNA_armature_types.h"

#include "BLI_listbase_wrapper.hh"

namespace blender::animrig {
/**
 * Call `callback(bone)` for each bone in the list of bones.
 *
 * Bones are visited in depth-first order.
 *
 * TODO: extend the callback with a `bool` return value to indicate whether the
 * loop should continue or stop.
 */
template<typename CB>
static void ANIM_armature_foreach_bone(ListBase /* Bone */ *bones, CB callback)
{
  for (Bone *bone : blender::ListBaseWrapper<Bone>(bones)) {
    callback(bone);
    ANIM_armature_foreach_bone(&bone->childbase, callback);
  }
}

/**
 * Call `callback(bone)` for each bone in the list of bones.
 *
 * Bones are visited in depth-first order.
 *
 * TODO: extend the callback with a `bool` return value to indicate whether the
 * loop should continue or stop.
 */
template<typename CB>
static void ANIM_armature_foreach_bone(const ListBase /* Bone */ *bones, CB callback)
{
  for (const Bone *bone : blender::ConstListBaseWrapper<Bone>(bones)) {
    callback(bone);
    ANIM_armature_foreach_bone(&bone->childbase, callback);
  }
}

};  // namespace blender::animrig
