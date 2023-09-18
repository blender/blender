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

struct ThemeWireColor;

namespace blender::animrig {

/** C++ wrapper for the DNA BoneColor struct. */
class BoneColor : public ::BoneColor {
 public:
  BoneColor();
  BoneColor(const BoneColor &other);
  ~BoneColor();

  const ThemeWireColor *effective_color() const;
};

};  // namespace blender::animrig
