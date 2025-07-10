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

  BKE_defvert_normalize_ex(vert, {}, {}, {});
}

TEST(vertex_weights_normalize, SingleWeight)
{
  MDeformWeight weights[1];
  weights[0].def_nr = 0;
  MDeformVert vert = {weights, 1, 0};

  /* Excluded from normalized set: shouldn't be touched. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {false}, {false}, {false});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);

  /* Locked: shouldn't be touched. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {true}, {true}, {false});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);

  /* Unlocked: should get normalized to 1.0. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {true}, {false}, {false});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* Unlocked and soft-locked: should get normalized to 1.0. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {true}, {false}, {true});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* Locked and soft-locked: shouldn't be touched (locked takes precedent). */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {true}, {true}, {true});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);

  /* An empty "subset" flag list should be equivalent to everything being included. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {}, {false}, {false});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* An empty "locked" flag list should be equivalent to everything being unlocked. */
  weights[0].weight = 0.5;
  BKE_defvert_normalize_ex(vert, {true}, {}, {false});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* Zero weight: single-group vertices are special cased for some reason to be
   * set to 1.0. */
  weights[0].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {}, {});
  EXPECT_FLOAT_EQ(1.0, weights[0].weight);

  /* Zero weight locked: shouldn't be touched. */
  weights[0].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {true}, {});
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
  BKE_defvert_normalize_ex(vert, {false, false}, {false, false}, {false, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.25, weights[1].weight);

  /* One included: included one should be set to 1.0. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {false, true}, {false, false}, {false, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(1.0, weights[1].weight);

  /* Both included: should be normalized together. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {true, true}, {false, false}, {false, false});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);
  EXPECT_FLOAT_EQ(0.5, weights[1].weight);

  /* All flag arrays being empty should mean: included, unlocked, and not "just
   * set". So this should behave as a simple normalization across both groups. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {}, {}, {});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);
  EXPECT_FLOAT_EQ(0.5, weights[1].weight);

  /* Both included but locked: shouldn't be touched. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {}, {true, true}, {false, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.25, weights[1].weight);

  /* Only one locked: locked shouldn't be touched, unlocked should pick up the
   * slack for normalization. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {}, {true, false}, {false, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.75, weights[1].weight);

  /* Only one marked as soft-locked: soft-locked shouldn't be touched, the other
   * should pick up the slack for normalization. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {}, {false, false}, {true, false});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.75, weights[1].weight);

  /* One locked, the other marked as soft-locked: soft-locked should pick up the
   * slack for normalization. */
  weights[0].weight = 0.25;
  weights[1].weight = 0.25;
  BKE_defvert_normalize_ex(vert, {}, {true, false}, {false, true});
  EXPECT_FLOAT_EQ(0.25, weights[0].weight);
  EXPECT_FLOAT_EQ(0.75, weights[1].weight);

  /* Zero weight: shouldn't be touched. */
  weights[0].weight = 0.0;
  weights[1].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {false, false}, {false, false});
  EXPECT_FLOAT_EQ(0.0, weights[0].weight);
  EXPECT_FLOAT_EQ(0.0, weights[1].weight);

  /* Zero weight with one group soft-locked: soft-locked should pick up the slack. */
  weights[0].weight = 0.0;
  weights[1].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {false, false}, {false, true});
  EXPECT_FLOAT_EQ(0.0, weights[0].weight);
  EXPECT_FLOAT_EQ(1.0, weights[1].weight);

  /* Zero weight with both groups soft-locked: both should pick up the slack equally. */
  weights[0].weight = 0.0;
  weights[1].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {false, false}, {true, true});
  EXPECT_FLOAT_EQ(0.5, weights[0].weight);
  EXPECT_FLOAT_EQ(0.5, weights[1].weight);
}

TEST(vertex_weights_normalize, FourWeights)
{
  /* Note the out-of-order `def_nr`, which is part of this test. Further below,
   * we write the weights ordered to line up with the boolean arrays to make
   * things easier to follow. */
  MDeformWeight weights[4];
  weights[0].def_nr = 3;
  weights[1].def_nr = 0;
  weights[2].def_nr = 1;
  weights[3].def_nr = 2;
  MDeformVert vert = {weights, 4, 0};

  /* One locked, one soft-locked: the remaining two should pick up the slack. */
  weights[1].weight = 0.125;
  weights[2].weight = 0.125;
  weights[3].weight = 0.125;
  weights[0].weight = 0.0625;
  BKE_defvert_normalize_ex(vert, {}, {true, false, false, false}, {false, false, true, false});
  EXPECT_FLOAT_EQ(0.125, weights[1].weight);
  EXPECT_FLOAT_EQ(0.75 * 2.0 / 3.0, weights[2].weight);
  EXPECT_FLOAT_EQ(0.125, weights[3].weight);
  EXPECT_FLOAT_EQ(0.75 / 3.0, weights[0].weight);

  /* One locked, two soft-locked: the remaining one should pick up the slack. */
  weights[1].weight = 0.125;
  weights[2].weight = 0.125;
  weights[3].weight = 0.125;
  weights[0].weight = 0.125;
  BKE_defvert_normalize_ex(vert, {}, {true, false, false, false}, {false, true, true, false});
  EXPECT_FLOAT_EQ(0.125, weights[1].weight);
  EXPECT_FLOAT_EQ(0.125, weights[2].weight);
  EXPECT_FLOAT_EQ(0.125, weights[3].weight);
  EXPECT_FLOAT_EQ(0.625, weights[0].weight);

  /* One locked, one soft-locked, and the rest zero-weight: the soft-locked one
   * should pick up the slack. */
  weights[1].weight = 0.125;
  weights[2].weight = 0.0;
  weights[3].weight = 0.125;
  weights[0].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {true, false, false, false}, {false, false, true, false});
  EXPECT_FLOAT_EQ(0.125, weights[1].weight);
  EXPECT_FLOAT_EQ(0.0, weights[2].weight);
  EXPECT_FLOAT_EQ(0.875, weights[3].weight);
  EXPECT_FLOAT_EQ(0.0, weights[0].weight);

  /* One locked, two soft-locked, and the last zero-weight: the soft-locked ones
   * should pick up the slack. */
  weights[1].weight = 0.125;
  weights[2].weight = 0.125;
  weights[3].weight = 0.25;
  weights[0].weight = 0.0;
  BKE_defvert_normalize_ex(vert, {}, {true, false, false, false}, {false, true, true, false});
  EXPECT_FLOAT_EQ(0.125, weights[1].weight);
  EXPECT_FLOAT_EQ(0.875 / 3.0, weights[2].weight);
  EXPECT_FLOAT_EQ(0.875 * 2.0 / 3.0, weights[3].weight);
  EXPECT_FLOAT_EQ(0.0f, weights[0].weight);
}

}  // namespace blender::bke::tests
