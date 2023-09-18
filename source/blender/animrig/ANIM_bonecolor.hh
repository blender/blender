/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief C++ part of the BoneColor DNA struct.
 */

#pragma once

#ifndef __cplusplus
#  error This is a C++ header.
#endif

#include "DNA_anim_types.h"

struct bPoseChannel;
struct ThemeWireColor;

namespace blender::animrig {

/** C++ wrapper for the DNA BoneColor struct. */
class BoneColor : public ::BoneColor {
 public:
  BoneColor();
  BoneColor(const BoneColor &other);
  ~BoneColor();

  const ThemeWireColor *effective_color() const;

  /* Support for storing in a blender::Set<BoneColor>.*/
  bool operator==(const BoneColor &other) const;
  bool operator!=(const BoneColor &other) const;
  uint64_t hash() const;
};

/**
 * Return the effective BoneColor of this pose bone.
 *
 * This returns the pose bone's own color, unless it's set to "default", then it defaults to the
 * armature bone color.
 */
BoneColor &ANIM_bonecolor_posebone_get(struct bPoseChannel *pose_bone);

};  // namespace blender::animrig
