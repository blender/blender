/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_deform.hh"

#include "DNA_meshdata_types.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(vertex_weights_normalize, EmptyWeights)
{
  /* Just making sure we don't crash on a vertex with no weights. */

  /* Some compilers don't like zero-length arrays, so we just do nullptr here as
   * a stand-in. */
  MDeformWeight *weights = nullptr;

  MDeformVert vert = {weights, 0, 0};

  BKE_defvert_normalize_lock_map(vert, {}, {});
}

TEST(vertex_weights_normalize, SingleWeight)
{
  MDeformWeight weights[1];
  weights[0].def_nr = 0;
  MDeformVert vert = {weights, 1, 0};

  /* Excluded from normalized set: shouldn't be touched. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_lock_map(vert, {false}, {false});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);

  /* Locked: shouldn't be touched. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_lock_map(vert, {true}, {true});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);

  /* Unlocked: should get normalized to 1.0. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_lock_map(vert, {true}, {false});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* An empty "subset" flag list should be equivalent to everything being included. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_lock_map(vert, {}, {false});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* An empty "locked" flag list should be equivalent to everything being unlocked. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_lock_map(vert, {true}, {});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* Zero weight: single-group vertices are special cased for some reason to be
   * set to 1.0. */
  weights[0].weight = 0.0;
  BKE_defvert_normalize_lock_map(vert, {}, {});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* Zero weight locked: shouldn't be touched. */
  weights[0].weight = 0.0;
  BKE_defvert_normalize_lock_map(vert, {}, {true});
  EXPECT_FLOAT_EQ(0.0, weights[0].weight);
}

TEST(vertex_weights_normalize, TwoWeights)
{
  MDeformWeight weights[2];
  weights[0].def_nr = 0;
  weights[1].def_nr = 1;
  MDeformVert vert = {weights, 2, 0};

  /* Both excluded from normalized set: shouldn't be touched. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_lock_map(vert, {false, false}, {false, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.25, weights[1].weight);

  /* One included: included one should be set to 1.0. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_lock_map(vert, {false, true}, {false, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(1.0, weights[1].weight);

  /* Both included: should be normalized together. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_lock_map(vert, {true, true}, {false, false});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);
  EXPECT_FLOAT_EQ(0.5, weights[1].weight);

  /* All flag arrays being empty should mean: included and unlocked. So this
   * should behave as a simple normalization across both groups. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_lock_map(vert, {}, {});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);
  EXPECT_FLOAT_EQ(0.5, weights[1].weight);

  /* Both included but locked: shouldn't be touched. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_lock_map(vert, {}, {true, true});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.25, weights[1].weight);

  /* Only one locked: locked shouldn't be touched, unlocked should pick up the
   * slack for normalization. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_lock_map(vert, {}, {true, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.75, weights[1].weight);

  /* Zero weight: shouldn't be touched. */
  weights[0].weight = 0.0;
  weights[1].weight = 0.0;
  BKE_defvert_normalize_lock_map(vert, {}, {false, false});
  EXPECT_FLOAT_EQ(0.0, weights[0].weight);
  EXPECT_FLOAT_EQ(0.0, weights[1].weight);
}

}  // namespace blender::bke::tests
