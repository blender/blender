/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 by Blender Foundation.
 */
#include "testing/testing.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "BKE_fcurve.h"

#include "ED_keyframing.h"

#include "DNA_anim_types.h"
}

// Epsilon for floating point comparisons.
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

  EXPECT_NEAR(evaluate_fcurve(fcu, 1.0f), 7.0f, EPSILON);   // hits 'on or before first' function
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.0f), 13.0f, EPSILON);  // hits 'between' function
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.0f), 19.0f, EPSILON);  // hits 'on or after last' function

  /* Also test within a specific time epsilon of the keys, as this was an issue in T39207.
   * This epsilon is just slightly smaller than the epsilon given to binarysearch_bezt_index_ex()
   * in fcurve_eval_between_keyframes(), so it should hit the "exact" code path. */
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

  // Test with default handles.
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.25f), 7.8297067f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.50f), 10.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 1.75f), 12.170294f, EPSILON);

  // Test with modified handles.
  fcu->bezt[0].vec[0][0] = 0.71855f;  // left handle X
  fcu->bezt[0].vec[0][1] = 6.22482f;  // left handle Y
  fcu->bezt[0].vec[2][0] = 1.35148f;  // right handle X
  fcu->bezt[0].vec[2][1] = 7.96806f;  // right handle Y

  fcu->bezt[1].vec[0][0] = 1.66667f;  // left handle X
  fcu->bezt[1].vec[0][1] = 10.4136f;  // left handle Y
  fcu->bezt[1].vec[2][0] = 2.33333f;  // right handle X
  fcu->bezt[1].vec[2][1] = 15.5864f;  // right handle Y

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
  // Before first keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 5.5f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.50f), 4.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -1.50f), -8.0f, EPSILON);
  // After last keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 17.5f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 22.0f, EPSILON);

  fcu->extend = FCURVE_EXTRAPOLATE_CONSTANT;
  // Before first keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 7.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -1.50f), 7.0f, EPSILON);
  // After last keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 13.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 13.0f, EPSILON);

  BKE_fcurve_free(fcu);
}

TEST(evaluate_fcurve, ExtrapolationBezierKeys)
{
  FCurve *fcu = BKE_fcurve_create();

  EXPECT_EQ(insert_vert_fcurve(fcu, 1.0f, 7.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 0);
  EXPECT_EQ(insert_vert_fcurve(fcu, 2.0f, 13.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF), 1);

  fcu->bezt[0].vec[0][0] = 0.71855f;  // left handle X
  fcu->bezt[0].vec[0][1] = 6.22482f;  // left handle Y
  fcu->bezt[0].vec[2][0] = 1.35148f;  // right handle X
  fcu->bezt[0].vec[2][1] = 7.96806f;  // right handle Y

  fcu->bezt[1].vec[0][0] = 1.66667f;  // left handle X
  fcu->bezt[1].vec[0][1] = 10.4136f;  // left handle Y
  fcu->bezt[1].vec[2][0] = 2.33333f;  // right handle X
  fcu->bezt[1].vec[2][1] = 15.5864f;  // right handle Y

  fcu->extend = FCURVE_EXTRAPOLATE_LINEAR;
  // Before first keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 6.3114409f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -0.50f), 2.8686447f, EPSILON);
  // After last keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 18.81946f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 24.63892f, EPSILON);

  fcu->extend = FCURVE_EXTRAPOLATE_CONSTANT;
  // Before first keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 0.75f), 7.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, -1.50f), 7.0f, EPSILON);
  // After last keyframe.
  EXPECT_NEAR(evaluate_fcurve(fcu, 2.75f), 13.0f, EPSILON);
  EXPECT_NEAR(evaluate_fcurve(fcu, 3.50f), 13.0f, EPSILON);

  BKE_fcurve_free(fcu);
}
