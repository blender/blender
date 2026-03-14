/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_rna.hh"

#include "testing/testing.h"

namespace blender::animrig::tests {

TEST(ANIM_rna, is_rotation_path)
{
  EXPECT_TRUE(is_rotation_path("rotation_euler"));
  EXPECT_TRUE(is_rotation_path("pose.bones[\"test\"].rotation_euler"));

  EXPECT_FALSE(is_rotation_path("xrotation_euler"));
  EXPECT_FALSE(is_rotation_path("rotation_euler2"));
  EXPECT_FALSE(is_rotation_path("[\"rotation_euler\"]"));
  EXPECT_FALSE(is_rotation_path("pose.bones[\"test\"][\"rotation_euler\"]"));
}

TEST(ANIM_rna, rotation_mode_from_path)
{
  EXPECT_EQ(ROT_MODE_QUAT, get_rotation_mode_from_path("rotation_quaternion").value());
  EXPECT_EQ(ROT_MODE_EUL, get_rotation_mode_from_path("rotation_euler").value());
  EXPECT_EQ(ROT_MODE_EUL,
            get_rotation_mode_from_path("pose.bones[\"test\"].rotation_euler").value());
  EXPECT_EQ(ROT_MODE_AXISANGLE, get_rotation_mode_from_path("rotation_axis_angle").value());

  EXPECT_EQ(std::nullopt, get_rotation_mode_from_path("scale"));
  EXPECT_EQ(std::nullopt, get_rotation_mode_from_path("xrotation_euler"));
  EXPECT_EQ(std::nullopt, get_rotation_mode_from_path("rotation_euler2"));
}

}  // namespace blender::animrig::tests
