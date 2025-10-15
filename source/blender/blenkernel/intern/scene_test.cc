/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_scene.hh"

#include "DNA_scene_types.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(scene, frame_snap_by_seconds)
{
  Scene fake_scene = {};

  /* Regular 24 FPS snapping. */
  fake_scene.r.frs_sec = 24;
  fake_scene.r.frs_sec_base = 1.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 47));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 49));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 59));
  EXPECT_FLOAT_EQ(72.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 60));
  EXPECT_FLOAT_EQ(9984.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 10000.0));

  /* 12 FPS snapping by incrementing the base. */
  fake_scene.r.frs_sec = 24;
  fake_scene.r.frs_sec_base = 2.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 47));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 49));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 53));
  EXPECT_FLOAT_EQ(60.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 54));
  EXPECT_FLOAT_EQ(9996.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 10000.0));

  /* 0.1 FPS snapping to 2-second intervals. */
  fake_scene.r.frs_sec = 1;
  fake_scene.r.frs_sec_base = 10.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.0));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.1));
  EXPECT_FLOAT_EQ(48.2, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.2));
  EXPECT_FLOAT_EQ(10000.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 10000.0));
}

}  // namespace blender::bke::tests
