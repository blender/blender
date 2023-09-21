/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_bonecolor.hh"

#include "BLI_hash.hh"

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

bool BoneColor::operator==(const BoneColor &other) const
{
  if (palette_index != other.palette_index) {
    return false;
  }
  if (palette_index == -1) {
    /* Explicitly compare each field, skipping the DNA padding fields. */
    /* TODO: maybe there is already a DNA-level-comparison function for this? */

    /* The last byte of the colors isn't used, but it's still in memory. The annoying thing is that
     * values are inconsistently either 0 or 255 depending on how the color was set, and there is
     * no way to influence this with the color picker in the GUI. So, just skip the last byte in
     * the comparisons. */
    return std::memcmp(custom.solid, other.custom.solid, sizeof(custom.solid) - 1) == 0 &&
           std::memcmp(custom.select, other.custom.select, sizeof(custom.select) - 1) == 0 &&
           std::memcmp(custom.active, other.custom.active, sizeof(custom.active) - 1) == 0 &&
           custom.flag == other.custom.flag;
  }
  return true;
}

bool BoneColor::operator!=(const BoneColor &other) const
{
  return !(*this == other);
}

uint64_t BoneColor::hash() const
{
  if (palette_index >= 0) {
    /* Theme colors are simple. */
    return get_default_hash(palette_index);
  }

  /* For custom colors, hash everything together. */

  /* The last byte of the color is skipped, as it is inconsistent (see note above). */
  const uint64_t hash_solid = get_default_hash_3(
      custom.solid[0], custom.solid[1], custom.solid[2]);
  const uint64_t hash_select = get_default_hash_3(
      custom.select[0], custom.select[1], custom.select[2]);
  const uint64_t hash_active = get_default_hash_3(
      custom.active[0], custom.active[1], custom.active[2]);
  return get_default_hash_4(hash_solid, hash_select, hash_active, custom.flag);
}

BoneColor &ANIM_bonecolor_posebone_get(bPoseChannel *pose_bone)
{
  if (pose_bone->color.palette_index == 0) {
    return pose_bone->bone->color.wrap();
  }
  return pose_bone->color.wrap();
}

};  // namespace blender::animrig
