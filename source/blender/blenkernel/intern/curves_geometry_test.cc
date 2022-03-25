/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_curves.hh"

#include "testing/testing.h"

namespace blender::bke::tests {

static CurvesGeometry create_basic_curves(const int points_size, const int curves_size)
{
  CurvesGeometry curves(points_size, curves_size);

  const int curve_length = points_size / curves_size;
  for (const int i : curves.curves_range()) {
    curves.offsets()[i] = points_size * curve_length;
  }
  curves.offsets().last() = points_size;

  for (const int i : curves.points_range()) {
    curves.positions()[i] = {float(i), float(i % curve_length), 0.0f};
  }

  return curves;
}

TEST(curves_geometry, Empty)
{
  CurvesGeometry empty(0, 0);
  empty.cyclic();
  float3 min;
  float3 max;
  EXPECT_FALSE(empty.bounds_min_max(min, max));
}

TEST(curves_geometry, Move)
{
  CurvesGeometry curves = create_basic_curves(100, 10);

  const int *offsets_data = curves.offsets().data();
  const float3 *positions_data = curves.positions().data();

  CurvesGeometry other = std::move(curves);

  /* The old curves should be empty, and the offsets are expected to be null. */
  EXPECT_EQ(curves.points_size(), 0);       /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(curves.curve_offsets, nullptr); /* NOLINT: bugprone-use-after-move */

  /* Just a basic check that the new curves work okay. */
  float3 min;
  float3 max;
  EXPECT_TRUE(other.bounds_min_max(min, max));

  curves = std::move(other);

  CurvesGeometry second_other(std::move(curves));

  /* The data should not have been reallocated ever. */
  EXPECT_EQ(second_other.positions().data(), positions_data);
  EXPECT_EQ(second_other.offsets().data(), offsets_data);
}

TEST(curves_geometry, CatmullRomEvaluation)
{
  CurvesGeometry curves(4, 1);
  curves.curve_types().fill(CURVE_TYPE_CATMULL_ROM);
  curves.resolution().fill(12);
  curves.offsets().last() = 4;
  curves.cyclic().fill(false);

  MutableSpan<float3> positions = curves.positions();
  positions[0] = {1, 1, 0};
  positions[1] = {0, 1, 0};
  positions[2] = {0, 0, 0};
  positions[3] = {-1, 0, 0};

  Span<float3> evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_1{{
      {1, 1, 0},
      {0.948495, 1.00318, 0},
      {0.87963, 1.01157, 0},
      {0.796875, 1.02344, 0},
      {0.703704, 1.03704, 0},
      {0.603588, 1.05064, 0},
      {0.5, 1.0625, 0},
      {0.396412, 1.07089, 0},
      {0.296296, 1.07407, 0},
      {0.203125, 1.07031, 0},
      {0.12037, 1.05787, 0},
      {0.0515046, 1.03501, 0},
      {0, 1, 0},
      {-0.0318287, 0.948495, 0},
      {-0.0462963, 0.87963, 0},
      {-0.046875, 0.796875, 0},
      {-0.037037, 0.703704, 0},
      {-0.0202546, 0.603588, 0},
      {0, 0.5, 0},
      {0.0202546, 0.396412, 0},
      {0.037037, 0.296296, 0},
      {0.046875, 0.203125, 0},
      {0.0462963, 0.12037, 0},
      {0.0318287, 0.0515046, 0},
      {0, 0, 0},
      {-0.0515046, -0.0350116, 0},
      {-0.12037, -0.0578704, 0},
      {-0.203125, -0.0703125, 0},
      {-0.296296, -0.0740741, 0},
      {-0.396412, -0.0708912, 0},
      {-0.5, -0.0625, 0},
      {-0.603588, -0.0506366, 0},
      {-0.703704, -0.037037, 0},
      {-0.796875, -0.0234375, 0},
      {-0.87963, -0.0115741, 0},
      {-0.948495, -0.00318287, 0},
      {-1, 0, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_1[i], 1e-5f);
  }

  /* Changing the positions shouldn't cause the evaluated positions array to be reallocated. */
  curves.tag_positions_changed();
  curves.evaluated_positions();
  EXPECT_EQ(curves.evaluated_positions().data(), evaluated_positions.data());

  /* Call recalculation (which shouldn't happen because low-level accessors don't tag caches). */
  EXPECT_EQ(evaluated_positions[12].x, 0.0f);
  EXPECT_EQ(evaluated_positions[12].y, 1.0f);

  positions[0] = {1, 0, 0};
  positions[1] = {1, 1, 0};
  positions[2] = {0, 1, 0};
  positions[3] = {0, 0, 0};
  curves.cyclic().fill(true);

  /* Tag topology changed because the new cyclic value is different. */
  curves.tag_topology_changed();

  /* Retrieve the data again since the size should be larger than last time (one more segment). */
  evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_2{{
      {1, 0, 0},
      {1.03819, 0.0515046, 0},
      {1.06944, 0.12037, 0},
      {1.09375, 0.203125, 0},
      {1.11111, 0.296296, 0},
      {1.12153, 0.396412, 0},
      {1.125, 0.5, 0},
      {1.12153, 0.603588, 0},
      {1.11111, 0.703704, 0},
      {1.09375, 0.796875, 0},
      {1.06944, 0.87963, 0},
      {1.03819, 0.948495, 0},
      {1, 1, 0},
      {0.948495, 1.03819, 0},
      {0.87963, 1.06944, 0},
      {0.796875, 1.09375, 0},
      {0.703704, 1.11111, 0},
      {0.603588, 1.12153, 0},
      {0.5, 1.125, 0},
      {0.396412, 1.12153, 0},
      {0.296296, 1.11111, 0},
      {0.203125, 1.09375, 0},
      {0.12037, 1.06944, 0},
      {0.0515046, 1.03819, 0},
      {0, 1, 0},
      {-0.0381944, 0.948495, 0},
      {-0.0694444, 0.87963, 0},
      {-0.09375, 0.796875, 0},
      {-0.111111, 0.703704, 0},
      {-0.121528, 0.603588, 0},
      {-0.125, 0.5, 0},
      {-0.121528, 0.396412, 0},
      {-0.111111, 0.296296, 0},
      {-0.09375, 0.203125, 0},
      {-0.0694444, 0.12037, 0},
      {-0.0381944, 0.0515046, 0},
      {0, 0, 0},
      {0.0515046, -0.0381944, 0},
      {0.12037, -0.0694444, 0},
      {0.203125, -0.09375, 0},
      {0.296296, -0.111111, 0},
      {0.396412, -0.121528, 0},
      {0.5, -0.125, 0},
      {0.603588, -0.121528, 0},
      {0.703704, -0.111111, 0},
      {0.796875, -0.09375, 0},
      {0.87963, -0.0694444, 0},
      {0.948495, -0.0381944, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_2[i], 1e-5f);
  }
}

TEST(curves_geometry, CatmullRomTwoPointCyclic)
{
  CurvesGeometry curves(2, 1);
  curves.curve_types().fill(CURVE_TYPE_CATMULL_ROM);
  curves.resolution().fill(12);
  curves.offsets().last() = 2;
  curves.cyclic().fill(true);

  /* The cyclic value should be ignored when there are only two control points. There should
   * be 12 evaluated points for the single segment and an extra for the last point. */
  EXPECT_EQ(curves.evaluated_points_size(), 13);
}

TEST(curves_geometry, BezierPositionEvaluation)
{
  CurvesGeometry curves(2, 1);
  curves.curve_types().fill(CURVE_TYPE_BEZIER);
  curves.resolution().fill(12);
  curves.offsets().last() = 2;

  MutableSpan<float3> handles_left = curves.handle_positions_left();
  MutableSpan<float3> handles_right = curves.handle_positions_right();
  MutableSpan<float3> positions = curves.positions();
  positions.first() = {-1, 0, 0};
  positions.last() = {1, 0, 0};
  handles_right.first() = {-0.5f, 0.5f, 0.0f};
  handles_left.last() = {0, 0, 0};

  /* Dangling handles shouldn't be used in a non-cyclic curve. */
  handles_left.first() = {100, 100, 100};
  handles_right.last() = {100, 100, 100};

  Span<float3> evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_1{{
      {-1, 0, 0},
      {-0.874711, 0.105035, 0},
      {-0.747685, 0.173611, 0},
      {-0.617188, 0.210937, 0},
      {-0.481481, 0.222222, 0},
      {-0.338831, 0.212674, 0},
      {-0.1875, 0.1875, 0},
      {-0.0257524, 0.15191, 0},
      {0.148148, 0.111111, 0},
      {0.335937, 0.0703125, 0},
      {0.539352, 0.0347222, 0},
      {0.760127, 0.00954859, 0},
      {1, 0, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_1[i], 1e-5f);
  }

  curves.resize(4, 2);
  curves.curve_types().fill(CURVE_TYPE_BEZIER);
  curves.resolution().fill(9);
  curves.offsets().last() = 4;
  handles_left = curves.handle_positions_left();
  handles_right = curves.handle_positions_right();
  positions = curves.positions();
  positions[2] = {-1, 1, 0};
  positions[3] = {1, 1, 0};
  handles_right[2] = {-0.5f, 1.5f, 0.0f};
  handles_left[3] = {0, 1, 0};

  /* Dangling handles shouldn't be used in a non-cyclic curve. */
  handles_left[2] = {-100, -100, -100};
  handles_right[3] = {-100, -100, -100};

  evaluated_positions = curves.evaluated_positions();
  EXPECT_EQ(evaluated_positions.size(), 20);
  static const Array<float3> result_2{{
      {-1, 0, 0},
      {-0.832647, 0.131687, 0},
      {-0.66118, 0.201646, 0},
      {-0.481481, 0.222222, 0},
      {-0.289438, 0.205761, 0},
      {-0.0809327, 0.164609, 0},
      {0.148148, 0.111111, 0},
      {0.40192, 0.0576133, 0},
      {0.684499, 0.016461, 0},
      {1, 0, 0},
      {-1, 1, 0},
      {-0.832647, 1.13169, 0},
      {-0.66118, 1.20165, 0},
      {-0.481481, 1.22222, 0},
      {-0.289438, 1.20576, 0},
      {-0.0809327, 1.16461, 0},
      {0.148148, 1.11111, 0},
      {0.40192, 1.05761, 0},
      {0.684499, 1.01646, 0},
      {1, 1, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_2[i], 1e-5f);
  }
}

TEST(curves_geometry, NURBSEvaluation)
{
  CurvesGeometry curves(4, 1);
  curves.curve_types().fill(CURVE_TYPE_NURBS);
  curves.resolution().fill(10);
  curves.offsets().last() = 4;

  MutableSpan<float3> positions = curves.positions();
  positions[0] = {1, 1, 0};
  positions[1] = {0, 1, 0};
  positions[2] = {0, 0, 0};
  positions[3] = {-1, 0, 0};

  Span<float3> evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_1{{
      {0.166667, 0.833333, 0},    {0.150006, 0.815511, 0},   {0.134453, 0.796582, 0},
      {0.119924, 0.776627, 0},    {0.106339, 0.75573, 0},    {0.0936146, 0.733972, 0},
      {0.0816693, 0.711434, 0},   {0.0704211, 0.6882, 0},    {0.0597879, 0.66435, 0},
      {0.0496877, 0.639968, 0},   {0.0400385, 0.615134, 0},  {0.0307584, 0.589931, 0},
      {0.0217653, 0.564442, 0},   {0.0129772, 0.538747, 0},  {0.00431208, 0.512929, 0},
      {-0.00431208, 0.487071, 0}, {-0.0129772, 0.461253, 0}, {-0.0217653, 0.435558, 0},
      {-0.0307584, 0.410069, 0},  {-0.0400385, 0.384866, 0}, {-0.0496877, 0.360032, 0},
      {-0.0597878, 0.33565, 0},   {-0.0704211, 0.3118, 0},   {-0.0816693, 0.288566, 0},
      {-0.0936146, 0.266028, 0},  {-0.106339, 0.24427, 0},   {-0.119924, 0.223373, 0},
      {-0.134453, 0.203418, 0},   {-0.150006, 0.184489, 0},  {-0.166667, 0.166667, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_1[i], 1e-5f);
  }

  /* Test a cyclic curve. */
  curves.cyclic().fill(true);
  curves.tag_topology_changed();
  evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_2{{
      {0.166667, 0.833333, 0},   {0.121333, 0.778667, 0},
      {0.084, 0.716, 0},         {0.0526667, 0.647333, 0},
      {0.0253333, 0.574667, 0},  {0, 0.5, 0},
      {-0.0253333, 0.425333, 0}, {-0.0526667, 0.352667, 0},
      {-0.084, 0.284, 0},        {-0.121333, 0.221333, 0},
      {-0.166667, 0.166667, 0},  {-0.221, 0.121667, 0},
      {-0.281333, 0.0866667, 0}, {-0.343667, 0.0616666, 0},
      {-0.404, 0.0466667, 0},    {-0.458333, 0.0416667, 0},
      {-0.502667, 0.0466667, 0}, {-0.533, 0.0616666, 0},
      {-0.545333, 0.0866667, 0}, {-0.535667, 0.121667, 0},
      {-0.5, 0.166667, 0},       {-0.436, 0.221334, 0},
      {-0.348, 0.284, 0},        {-0.242, 0.352667, 0},
      {-0.124, 0.425333, 0},     {0, 0.5, 0},
      {0.124, 0.574667, 0},      {0.242, 0.647333, 0},
      {0.348, 0.716, 0},         {0.436, 0.778667, 0},
      {0.5, 0.833333, 0},        {0.535667, 0.878334, 0},
      {0.545333, 0.913333, 0},   {0.533, 0.938333, 0},
      {0.502667, 0.953333, 0},   {0.458333, 0.958333, 0},
      {0.404, 0.953333, 0},      {0.343667, 0.938333, 0},
      {0.281333, 0.913333, 0},   {0.221, 0.878333, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_2[i], 1e-5f);
  }

  /* Test a circular cyclic curve with weights. */
  positions[0] = {1, 0, 0};
  positions[1] = {1, 1, 0};
  positions[2] = {0, 1, 0};
  positions[3] = {0, 0, 0};
  curves.nurbs_weights().fill(1.0f);
  curves.nurbs_weights()[0] = 4.0f;
  curves.tag_positions_changed();
  static const Array<float3> result_3{{
      {0.888889, 0.555556, 0},  {0.837792, 0.643703, 0},  {0.773885, 0.727176, 0},
      {0.698961, 0.800967, 0},  {0.616125, 0.860409, 0},  {0.529412, 0.901961, 0},
      {0.443152, 0.923773, 0},  {0.361289, 0.925835, 0},  {0.286853, 0.909695, 0},
      {0.221722, 0.877894, 0},  {0.166667, 0.833333, 0},  {0.122106, 0.778278, 0},
      {0.0903055, 0.713148, 0}, {0.0741654, 0.638711, 0}, {0.0762274, 0.556847, 0},
      {0.0980392, 0.470588, 0}, {0.139591, 0.383875, 0},  {0.199032, 0.301039, 0},
      {0.272824, 0.226114, 0},  {0.356297, 0.162208, 0},  {0.444444, 0.111111, 0},
      {0.531911, 0.0731388, 0}, {0.612554, 0.0468976, 0}, {0.683378, 0.0301622, 0},
      {0.74391, 0.0207962, 0},  {0.794872, 0.017094, 0},  {0.837411, 0.017839, 0},
      {0.872706, 0.0222583, 0}, {0.901798, 0.0299677, 0}, {0.925515, 0.0409445, 0},
      {0.944444, 0.0555556, 0}, {0.959056, 0.0744855, 0}, {0.970032, 0.0982019, 0},
      {0.977742, 0.127294, 0},  {0.982161, 0.162589, 0},  {0.982906, 0.205128, 0},
      {0.979204, 0.256091, 0},  {0.969838, 0.316622, 0},  {0.953102, 0.387446, 0},
      {0.926861, 0.468089, 0},
  }};
  evaluated_positions = curves.evaluated_positions();
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_3[i], 1e-5f);
  }
}

}  // namespace blender::bke::tests
