/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_bonecolor.hh"

#include "UI_resources.hh"

#include <cstring>

namespace blender::animrig {

BoneColor::BoneColor()
{
  this->palette_index = 0;
}
BoneColor::BoneColor(const BoneColor &other)
{
  this->palette_index = other.palette_index;
  std::memcpy(&this->custom, &other.custom, sizeof(this->custom));
}
BoneColor::~BoneColor() {}

const ThemeWireColor *BoneColor::effective_color() const
{
  const int8_t color_index = this->palette_index;
  if (color_index == 0) {
    return nullptr;
  }
  if (color_index < 0) {
    return &this->custom;
  }

  const bTheme *btheme = UI_GetTheme();
  return &btheme->tarm[(color_index - 1)];
}

};  // namespace blender::animrig
