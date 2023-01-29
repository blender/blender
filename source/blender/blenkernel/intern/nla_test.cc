/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

#include "BKE_nla.h"

#include "DNA_anim_types.h"
#include "DNA_nla_types.h"

#include "MEM_guardedalloc.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(nla_strip, BKE_nlastrip_recalculate_blend)
{
  NlaStrip strip{};
  strip.blendin = 4.0;
  strip.blendout = 5.0;
  strip.start = 1;
  strip.end = 10;

  /* Scaling a strip up doesn't affect the blend in/out value */
  strip.end = 20;
  BKE_nlastrip_recalculate_blend(&strip);
  EXPECT_FLOAT_EQ(strip.blendin, 4.0);
  EXPECT_FLOAT_EQ(strip.blendout, 5.0);

  /* Scaling a strip down affects the blend-in value before the blend-out value  */
  strip.end = 7;
  BKE_nlastrip_recalculate_blend(&strip);
  EXPECT_FLOAT_EQ(strip.blendin, 1.0);
  EXPECT_FLOAT_EQ(strip.blendout, 5.0);

  /* Scaling a strip down to nothing updates the blend in/out values accordingly  */
  strip.end = 1.1;
  BKE_nlastrip_recalculate_blend(&strip);
  EXPECT_FLOAT_EQ(strip.blendin, 0.0);
  EXPECT_FLOAT_EQ(strip.blendout, 0.1);
}

}  // namespace blender::bke::tests
