/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BKE_fcurve.h"

#include "ED_keyframing.hh"

#include "DNA_anim_types.h"

#include "BLI_math_vector_types.hh"

namespace blender::bke::tests {

/* Epsilon for floating point comparisons. */
static const float EPSILON = 1e-7f;

TEST(evaluate_fcurve, EmptyFCurve)
{
  FCurve *fcu = BKE_fcurve_create();
  EXPECT_EQ(evaluate_fcurve(fcu, 47.0f), 0.0f);
  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, OnKeys)
{
  FCurve *fcu = BKE_fcurve_create();

  insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 3.0f, 19.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);

  EXPECT_NEAR(evaluate_fcurve(fcu, 1.0f), 7.0f, EPSILON);  /* hits 'on or before first' function */
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.0f), 13.0f, EPSILON); /* hits 'between' function */
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.0f), 19.0f, EPSILON); /* hits 'on or after last' function */

  /* Also test within a specific time epsilon of the keys, as this was an issue in #39207.
   * This epsilon is just slightly smaller than the epsilon given to
   * BKE_fcurve_bezt_binarysearch_index_ex() in fcurve_eval_between_keyframes(), so it should hit
   * the "exact" code path. */
  float time_epsilon = 0.00008f;
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.0f - time_epsilon), 13.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.0f + time_epsilon), 13.0f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, InterpolationConstant)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  fcu->bezt[0].ipo = BEZT_IPO_CONST;
  fcu->bezt[1].ipo = BEZT_IPO_CONST;

  EXPECT_NEAR(evaluate_fcurve(fcu, 1.25f), 7.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.50f), 7.0f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, InterpolationLinear)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  fcu->bezt[0].ipo = BEZT_IPO_LIN;
  fcu->bezt[1].ipo = BEZT_IPO_LIN;

  EXPECT_NEAR(evaluate_fcurve(fcu, 1.25f), 8.5f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.50f), 10.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.75f), 11.5f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, InterpolationBezier)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  EXPECT_EQ(fcu->bezt[0].ipo, BEZT_IPO_BEZ);
  EXPECT_EQ(fcu->bezt[1].ipo, BEZT_IPO_BEZ);

  /* Test with default handles. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.25f), 7.8297067f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.50f), 10.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.75f), 12.170294f, EPSILON);

  /* Test with modified handles. */
  fcu->bezt[0].vec[0][0] = 0.71855f; /* left handle X */
  fcu->bezt[0].vec[0][1] = 6.22482f; /* left handle Y */
  fcu->bezt[0].vec[2][0] = 1.35148f; /* right handle X */
  fcu->bezt[0].vec[2][1] = 7.96806f; /* right handle Y */

  fcu->bezt[1].vec[0][0] = 1.66667f; /* left handle X */
  fcu->bezt[1].vec[0][1] = 10.4136f; /* left handle Y */
  fcu->bezt[1].vec[2][0] = 2.33333f; /* right handle X */
  fcu->bezt[1].vec[2][1] = 15.5864f; /* right handle Y */

  EXPECT_NEAR(evaluate_fcurve(fcu, 1.25f), 7.945497f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.50f), 9.3495407f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.75f), 11.088551f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, InterpolationBounce)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  fcu->bezt[0].ipo = BEZT_IPO_BOUNCE;
  fcu->bezt[1].ipo = BEZT_IPO_BOUNCE;

  fcu->bezt[0].easing = BEZT_IPO_EASE_IN;
  fcu->bezt[1].easing = BEZT_IPO_EASE_AUTO;

  EXPECT_NEAR(evaluate_fcurve(fcu, 1.4f), 8.3649998f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.5f), 8.4062500f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.8f), 11.184999f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, ExtrapolationLinearKeys)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);
  fcu->bezt[0].ipo = BEZT_IPO_LIN;
  fcu->bezt[1].ipo = BEZT_IPO_LIN;

  fcu->extend = FCURVE_EXTRAPOLATE_LINEAR;
  /* Before first keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 5.5f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.50f), 4.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -1.50f), -8.0f, EPSILON);
  /* After last keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 17.5f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 22.0f, EPSILON);

  fcu->extend = FCURVE_EXTRAPOLATE_CONSTANT;
  /* Before first keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 7.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -1.50f), 7.0f, EPSILON);
  /* After last keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 13.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 13.0f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, ExtrapolationBezierKeys)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  fcu->bezt[0].vec[0][0] = 0.71855f; /* left handle X */
  fcu->bezt[0].vec[0][1] = 6.22482f; /* left handle Y */
  fcu->bezt[0].vec[2][0] = 1.35148f; /* right handle X */
  fcu->bezt[0].vec[2][1] = 7.96806f; /* right handle Y */

  fcu->bezt[1].vec[0][0] = 1.66667f; /* left handle X */
  fcu->bezt[1].vec[0][1] = 10.4136f; /* left handle Y */
  fcu->bezt[1].vec[2][0] = 2.33333f; /* right handle X */
  fcu->bezt[1].vec[2][1] = 15.5864f; /* right handle Y */

  fcu->extend = FCURVE_EXTRAPOLATE_LINEAR;
  /* Before first keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 6.3114409f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -0.50f), 2.8686447f, EPSILON);
  /* After last keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 18.81946f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 24.63892f, EPSILON);

  fcu->extend = FCURVE_EXTRAPOLATE_CONSTANT;
  /* Before first keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 7.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -1.50f), 7.0f, EPSILON);
  /* After last keyframe. */
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 13.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 13.0f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(fcurve_subdivide, BKE_fcurve_bezt_subdivide_handles)
{
  FCurve *fcu = BKE_fcurve_create();

  /* Insert two keyframes and set handles to something non-default. */
  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 0.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 13.0f, 2.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  fcu->bezt[0].h1 = fcu->bezt[0].h2 = HD_FREE;
  fcu->bezt[0].vec[0][0] = -5.0f;
  fcu->bezt[0].vec[0][1] = 0.0f;
  fcu->bezt[0].vec[2][0] = 2.0f;
  fcu->bezt[0].vec[2][1] = 4.0f;

  fcu->bezt[1].h1 = fcu->bezt[1].h2 = HD_FREE;
  fcu->bezt[1].vec[0][0] = 13.0f;
  fcu->bezt[1].vec[0][1] = -2.0f;
  fcu->bezt[1].vec[2][0] = 16.0f;
  fcu->bezt[1].vec[2][1] = -3.0f;

  /* Create new keyframe point with defaults from insert_vert_fcurve(). */
  BezTriple beztr;
  const float x = 7.375f; /* at this X-coord, the FCurve should evaluate to 1.000f. */
  const float y = 1.000f;
  beztr.vec[0][0] = x - 1.0f;
  beztr.vec[0][1] = y;
  beztr.vec[1][0] = x;
  beztr.vec[1][1] = y;
  beztr.vec[2][0] = x + 1.0f;
  beztr.vec[2][1] = y;
  beztr.h1 = beztr.h2 = HD_AUTO_ANIM;
  beztr.ipo = BEZT_IPO_BEZ;

  /* This should update the existing handles as well as the new BezTriple. */
  float y_delta;
  BKE_fcurve_bezt_subdivide_handles(&beztr, &fcu->bezt[0], &fcu->bezt[1], &y_delta);

  EXPECT_FLOAT_EQ(y_delta, 0.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[0][0], -5.0f); /* Left handle should not be touched. */
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[0][1], 0.0f);
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[1][0], 1.0f); /* Coordinates should not be touched. */
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[1][1], 0.0f);
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[2][0], 1.5f); /* Right handle should be updated. */
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[2][1], 2.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][0], 13.0f); /* Left handle should be updated. */
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][1], 0.0f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], 13.0f); /* Coordinates should not be touched. */
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], 2.0f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][0], 16.0f); /* Right handle should not be touched */
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][1], -3.0f);

  EXPECT_FLOAT_EQ(beztr.vec[0][0], 4.5f); /* Left handle should be updated. */
  EXPECT_FLOAT_EQ(beztr.vec[0][1], 1.5f);
  EXPECT_FLOAT_EQ(beztr.vec[1][0], 7.375f); /* Coordinates should not be touched. */
  EXPECT_FLOAT_EQ(beztr.vec[1][1], 1.0f);
  EXPECT_FLOAT_EQ(beztr.vec[2][0], 10.250); /* Right handle should be updated. */
  EXPECT_FLOAT_EQ(beztr.vec[2][1], 0.5);

  BKE_fcurve_free(fcu);
}

TEST(fcurve_active_keyframe, ActiveKeyframe)
{
  FCurve *fcu = BKE_fcurve_create();

  /* There should be no active keyframe with no points. */
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE);

  /* Check that adding new points sets the active index. */
  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.5f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 8.0f, 15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), 1);
  EXPECT_EQ(insert_vert_fcurve(fcu, 14.0f, 8.2f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 2);
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), 2);

  /* Check clearing the index. */
  BKE_fcurve_active_keyframe_set(fcu, nullptr);
  EXPECT_EQ(fcu->active_keyframe_index, FCURVE_ACTIVE_KEYFRAME_NONE);
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE);

  /* Check a "normal" action. */
  fcu->bezt[2].f2 |= SELECT;
  BKE_fcurve_active_keyframe_set(fcu, &fcu->bezt[2]);
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), 2);

  /* Check setting an unselected keyframe as active. */
  fcu->bezt[2].f1 = fcu->bezt[2].f2 = fcu->bezt[2].f3 = 0;
  EXPECT_BLI_ASSERT(BKE_fcurve_active_keyframe_set(fcu, &fcu->bezt[2]),
                    "active keyframe must be selected");
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE);

  /* Check out of bounds (lower). */
  BKE_fcurve_active_keyframe_set(fcu, fcu->bezt - 20);
  EXPECT_EQ(fcu->active_keyframe_index, FCURVE_ACTIVE_KEYFRAME_NONE)
      << "Setting out-of-bounds value via the API should result in valid active_keyframe_index";
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE);

  fcu->active_keyframe_index = -20;
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE)
      << "Even with active_keyframe_index out of bounds, getting it via the API should produce a "
         "valid value";

  /* Check out of bounds (higher). */
  BKE_fcurve_active_keyframe_set(fcu, fcu->bezt + 4);
  EXPECT_EQ(fcu->active_keyframe_index, FCURVE_ACTIVE_KEYFRAME_NONE)
      << "Setting out-of-bounds value via the API should result in valid active_keyframe_index";
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE);

  fcu->active_keyframe_index = fcu->totvert;
  EXPECT_EQ(BKE_fcurve_active_keyframe_index(fcu), FCURVE_ACTIVE_KEYFRAME_NONE)
      << "Even with active_keyframe_index out of bounds, getting it via the API should produce a "
         "valid value";

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_keyframe_move_value_with_handles)
{
  FCurve *fcu = BKE_fcurve_create();

  insert_vert_fcurve(fcu, 1.0f, 7.5f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 8.0f, 15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 14.0f, 8.2f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][0], 5.2671194f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][1], 15.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], 8.0f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], 15.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][0], 10.342469f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][1], 15.0f);

  BKE_fcurve_keyframe_move_value_with_handles(&fcu->bezt[1], 47.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][0], 5.2671194f) << "Left handle should not move in time";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][1], 47.0f) << "Left handle value should have been updated";

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], 8.0f) << "Frame should not move in time";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], 47.0f) << "Frame value should have been updated";

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][0], 10.342469f) << "Right handle should not move in time";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][1], 47.0f) << "Right handle value should have been updated";

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_keyframe_move_time_with_handles)
{
  FCurve *fcu = BKE_fcurve_create();

  insert_vert_fcurve(fcu, 1.0f, 7.5f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 8.0f, 15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 14.0f, 8.2f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][0], 5.2671194f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][1], 15.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], 8.0f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], 15.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][0], 10.342469f);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][1], 15.0f);

  BKE_fcurve_keyframe_move_time_with_handles(&fcu->bezt[1], 47.0f);

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][0], 44.2671194f) << "Left handle time should be updated";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[0][1], 15.0f) << "Left handle should not move in value";

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], 47.0f) << "Frame time should have been updated";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], 15.0f) << "Frame should not move in value";

  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][0], 49.342469f) << "Right handle time should be updated";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[2][1], 15.0f) << "Right handle should not move in value";

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_calc_range)
{
  FCurve *fcu = BKE_fcurve_create();

  insert_vert_fcurve(fcu, 1.0f, 7.5f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 4.0f, -15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 8.0f, 15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 14.0f, 8.2f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 18.2f, -20.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);

  for (int i = 0; i < fcu->totvert; i++) {
    fcu->bezt[i].f1 &= ~SELECT;
    fcu->bezt[i].f2 &= ~SELECT;
    fcu->bezt[i].f3 &= ~SELECT;
  }

  float min, max;
  bool success;

  /* All keys. */
  success = BKE_fcurve_calc_range(fcu, &min, &max, false);
  EXPECT_TRUE(success) << "A non-empty FCurve should have a range.";
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[1][0], min);
  EXPECT_FLOAT_EQ(fcu->bezt[4].vec[1][0], max);

  /* Only selected. */
  success = BKE_fcurve_calc_range(fcu, &min, &max, true);
  EXPECT_FALSE(success)
      << "Using selected keyframes only should not find a range if nothing is selected.";

  fcu->bezt[1].f2 |= SELECT;
  fcu->bezt[3].f2 |= SELECT;

  success = BKE_fcurve_calc_range(fcu, &min, &max, true);
  EXPECT_TRUE(success) << "Range of selected keyframes should have been found.";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], min);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[1][0], max);

  /* Curve samples. */
  const int sample_start = 1;
  const int sample_end = 20;
  fcurve_store_samples(fcu, nullptr, sample_start, sample_end, fcurve_samplingcb_evalcurve);

  success = BKE_fcurve_calc_range(fcu, &min, &max, true);
  EXPECT_TRUE(success) << "FCurve samples should have a range.";

  EXPECT_FLOAT_EQ(sample_start, min);
  EXPECT_FLOAT_EQ(sample_end, max);

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_calc_bounds)
{
  FCurve *fcu = BKE_fcurve_create();

  insert_vert_fcurve(fcu, 1.0f, 7.5f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 4.0f, -15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 8.0f, 15.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 14.0f, 8.2f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
  insert_vert_fcurve(fcu, 18.2f, -20.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);

  for (int i = 0; i < fcu->totvert; i++) {
    fcu->bezt[i].f1 &= ~SELECT;
    fcu->bezt[i].f2 &= ~SELECT;
    fcu->bezt[i].f3 &= ~SELECT;
  }

  fcu->bezt[0].vec[0][0] = -5.0f;
  fcu->bezt[4].vec[2][0] = 25.0f;

  rctf bounds;
  bool success;

  /* All keys. */
  success = BKE_fcurve_calc_bounds(fcu,
                                   false /* select only */,
                                   false /* include handles */,
                                   nullptr /* frame range */,
                                   &bounds);
  EXPECT_TRUE(success) << "A non-empty FCurve should have bounds.";
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[1][0], bounds.xmin);
  EXPECT_FLOAT_EQ(fcu->bezt[4].vec[1][0], bounds.xmax);
  EXPECT_FLOAT_EQ(fcu->bezt[4].vec[1][1], bounds.ymin);
  EXPECT_FLOAT_EQ(fcu->bezt[2].vec[1][1], bounds.ymax);

  /* Only selected. */
  success = BKE_fcurve_calc_bounds(fcu,
                                   true /* select only */,
                                   false /* include handles */,
                                   nullptr /* frame range */,
                                   &bounds);
  EXPECT_FALSE(success)
      << "Using selected keyframes only should not find bounds if nothing is selected.";

  fcu->bezt[1].f2 |= SELECT;
  fcu->bezt[3].f2 |= SELECT;

  success = BKE_fcurve_calc_bounds(fcu,
                                   true /* select only */,
                                   false /* include handles */,
                                   nullptr /* frame range */,
                                   &bounds);
  EXPECT_TRUE(success) << "Selected keys should have been found.";
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][0], bounds.xmin);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[1][0], bounds.xmax);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], bounds.ymin);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[1][1], bounds.ymax);

  /* Including handles. */
  success = BKE_fcurve_calc_bounds(fcu,
                                   false /* select only */,
                                   true /* include handles */,
                                   nullptr /* frame range */,
                                   &bounds);
  EXPECT_TRUE(success) << "A non-empty FCurve should have bounds including handles.";
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[0][0], bounds.xmin);
  EXPECT_FLOAT_EQ(fcu->bezt[4].vec[2][0], bounds.xmax);
  EXPECT_FLOAT_EQ(fcu->bezt[4].vec[1][1], bounds.ymin);
  EXPECT_FLOAT_EQ(fcu->bezt[2].vec[1][1], bounds.ymax);

  /* Range. */
  float range[2];

  range[0] = 25;
  range[1] = 30;
  success = BKE_fcurve_calc_bounds(
      fcu, false /* select only */, false /* include handles */, range /* frame range */, &bounds);
  EXPECT_FALSE(success) << "A frame range outside the range of keyframes should not find bounds.";

  range[0] = 0;
  range[1] = 18.2f;
  success = BKE_fcurve_calc_bounds(
      fcu, false /* select only */, false /* include handles */, range /* frame range */, &bounds);
  EXPECT_TRUE(success) << "A frame range within the range of keyframes should find bounds.";
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[1][0], bounds.xmin);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[1][0], bounds.xmax);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], bounds.ymin);
  EXPECT_FLOAT_EQ(fcu->bezt[2].vec[1][1], bounds.ymax);

  /* Range and handles. */
  success = BKE_fcurve_calc_bounds(
      fcu, false /* select only */, true /* include handles */, range /* frame range */, &bounds);
  EXPECT_TRUE(success)
      << "A frame range within the range of keyframes should find bounds with handles.";
  EXPECT_FLOAT_EQ(fcu->bezt[0].vec[0][0], bounds.xmin);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[2][0], bounds.xmax);
  EXPECT_FLOAT_EQ(fcu->bezt[1].vec[1][1], bounds.ymin);
  EXPECT_FLOAT_EQ(fcu->bezt[2].vec[1][1], bounds.ymax);

  /* Range, handles and only selection. */
  range[0] = 8.0f;
  range[1] = 18.2f;
  success = BKE_fcurve_calc_bounds(
      fcu, true /* select only */, true /* include handles */, range /* frame range */, &bounds);
  EXPECT_TRUE(success)
      << "A frame range within the range of keyframes should find bounds of selected keyframes.";
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[0][0], bounds.xmin);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[2][0], bounds.xmax);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[2][1], bounds.ymin);
  EXPECT_FLOAT_EQ(fcu->bezt[3].vec[0][1], bounds.ymax);

  /* Curve samples. */
  const int sample_start = 1;
  const int sample_end = 20;
  fcurve_store_samples(fcu, nullptr, sample_start, sample_end, fcurve_samplingcb_evalcurve);

  success = BKE_fcurve_calc_bounds(fcu,
                                   false /* select only */,
                                   false /* include handles */,
                                   nullptr /* frame range */,
                                   &bounds);
  EXPECT_TRUE(success) << "FCurve samples should have a range.";

  EXPECT_FLOAT_EQ(sample_start, bounds.xmin);
  EXPECT_FLOAT_EQ(sample_end, bounds.xmax);
  EXPECT_FLOAT_EQ(-20.0f, bounds.ymin);
  EXPECT_FLOAT_EQ(15.0f, bounds.ymax);

  range[0] = 8.0f;
  range[1] = 20.0f;
  success = BKE_fcurve_calc_bounds(
      fcu, false /* select only */, false /* include handles */, range /* frame range */, &bounds);
  EXPECT_TRUE(success) << "FCurve samples should have a range.";

  EXPECT_FLOAT_EQ(range[0], bounds.xmin);
  EXPECT_FLOAT_EQ(range[1], bounds.xmax);
  EXPECT_FLOAT_EQ(-20.0f, bounds.ymin);
  EXPECT_FLOAT_EQ(15.0f, bounds.ymax);

  range[0] = 20.1f;
  range[1] = 30.0f;
  success = BKE_fcurve_calc_bounds(
      fcu, false /* select only */, false /* include handles */, range /* frame range */, &bounds);
  EXPECT_FALSE(success)
      << "A frame range outside the range of keyframe samples should not have bounds.";

  BKE_fcurve_free(fcu);
}

static void set_key(FCurve *fcu, const int index, const float x, const float y)
{
  fcu->bezt[index].vec[0][0] = x - 0.5f;
  fcu->bezt[index].vec[1][0] = x;
  fcu->bezt[index].vec[2][0] = x + 0.5f;

  fcu->bezt[index].vec[0][1] = y;
  fcu->bezt[index].vec[1][1] = y;
  fcu->bezt[index].vec[2][1] = y;
}

static FCurve *testcurve_with_duplicates()
{
  /* Create a curve with some duplicate keys. The first ones are all with Y=1, the later repeats
   * increase Y-coordinates on every repeat. */
  FCurve *fcu = BKE_fcurve_create();
  ED_keyframes_add(fcu, 10); /* Avoid `insert_vert_fcurve`, that de-duplicates the keys. */
  set_key(fcu, 0, 1.0f, 1.0f);
  set_key(fcu, 1, 327.16f, 1.0f);
  set_key(fcu, 2, 7.0f, 1.0f);
  set_key(fcu, 3, 47.0f, 1.0f);
  set_key(fcu, 4, 7.0f, 2.0f);
  set_key(fcu, 5, 47.0f, 2.0f);
  set_key(fcu, 6, 47.0f + BEZT_BINARYSEARCH_THRESH, 3.0f);
  set_key(fcu, 7, 7.0f, 3.0f);
  set_key(fcu, 8, 3.0f, 1.0f);
  set_key(fcu, 9, 2.0f, 1.0f);
  return fcu;
}

TEST(BKE_fcurve, sort_time_fcurve_stability)
{
  FCurve *fcu = testcurve_with_duplicates();
  ASSERT_EQ(fcu->totvert, 10);

  sort_time_fcurve(fcu);

  /* The sorting should be stable, i.e. retain the original order when the
   * X-coordinates are identical. */
  ASSERT_EQ(fcu->totvert, 10) << "sorting should not influence number of keys";
  EXPECT_V2_NEAR(fcu->bezt[0].vec[1], float2(1.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[1].vec[1], float2(2.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[2].vec[1], float2(3.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[3].vec[1], float2(7.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[4].vec[1], float2(7.0f, 2.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[5].vec[1], float2(7.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[6].vec[1], float2(47.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[7].vec[1], float2(47.0f, 2.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[8].vec[1], float2(47.0f + BEZT_BINARYSEARCH_THRESH, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[9].vec[1], float2(327.16f, 1.0f), 1e-3);

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_deduplicate_keys)
{
  FCurve *fcu = testcurve_with_duplicates();
  ASSERT_EQ(fcu->totvert, 10);
  sort_time_fcurve(fcu);

  BKE_fcurve_deduplicate_keys(fcu);
  ASSERT_GE(fcu->totvert, 6); /* Protect against out-of-bounds access. */
  EXPECT_EQ(fcu->totvert, 6); /* The actual expected value. */
  EXPECT_V2_NEAR(fcu->bezt[0].vec[1], float2(1.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[1].vec[1], float2(2.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[2].vec[1], float2(3.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[3].vec[1], float2(7.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[4].vec[1], float2(47.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[5].vec[1], float2(327.16f, 1.0f), 1e-3);

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_deduplicate_keys_edge_cases)
{
  FCurve *fcu = testcurve_with_duplicates();
  ASSERT_EQ(fcu->totvert, 10);

  /* Update the 2nd and 2nd-to-last keys to test the edge cases. */
  set_key(fcu, 0, 1, 1);
  set_key(fcu, 1, 1, 2);
  set_key(fcu, 8, 327.16f, 1);
  set_key(fcu, 9, 327.16f, 2);

  sort_time_fcurve(fcu);

  BKE_fcurve_deduplicate_keys(fcu);
  ASSERT_EQ(fcu->totvert, 4);
  EXPECT_V2_NEAR(fcu->bezt[0].vec[1], float2(1.0f, 2.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[1].vec[1], float2(7.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[2].vec[1], float2(47.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[3].vec[1], float2(327.16f, 2.0f), 1e-3);

  BKE_fcurve_free(fcu);
}

TEST(BKE_fcurve, BKE_fcurve_deduplicate_keys_prefer_whole_frames)
{
  FCurve *fcu = testcurve_with_duplicates();
  ASSERT_EQ(fcu->totvert, 10);

  /* Update the first key around 47.0 to be slightly before the frame. This gives us three keys on
   * 47-epsilon, 47, and 47+epsilon. The keys at index 5 and 6 already have this value, so the
   * `set_key` calls are unnecessary, but this way this test has a more local overview of the
   * situation under test. */
  set_key(fcu, 3, 47.0f - BEZT_BINARYSEARCH_THRESH, 1.0f);
  set_key(fcu, 5, 47.0f, 2.0f);
  set_key(fcu, 6, 47.0f + BEZT_BINARYSEARCH_THRESH, 3.0f);

  sort_time_fcurve(fcu);

  BKE_fcurve_deduplicate_keys(fcu);
  ASSERT_EQ(fcu->totvert, 6);
  EXPECT_V2_NEAR(fcu->bezt[0].vec[1], float2(1.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[1].vec[1], float2(2.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[2].vec[1], float2(3.0f, 1.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[3].vec[1], float2(7.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[4].vec[1], float2(47.0f, 3.0f), 1e-3);
  EXPECT_V2_NEAR(fcu->bezt[5].vec[1], float2(327.16f, 1.0f), 1e-3);

  BKE_fcurve_free(fcu);
}

}  // namespace blender::bke::tests
