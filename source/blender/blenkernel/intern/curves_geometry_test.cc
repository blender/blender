/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
    curves.offsets_for_write()[i] = curve_length * i;
  }
  curves.offsets_for_write().last() = points_size;

  for (const int i : curves.points_range()) {
    curves.positions_for_write()[i] = {float(i), float(i % curve_length), 0.0f};
  }

  return curves;
}

TEST(curves_geometry, Empty)
{
  CurvesGeometry empty(0, 0);
  empty.cyclic();
  EXPECT_TRUE(empty.is_empty());
  EXPECT_FALSE(empty.bounds_min_max());
}

TEST(curves_geometry, Move)
{
  CurvesGeometry curves = create_basic_curves(100, 10);

  const int *offsets_data = curves.offsets().data();
  const float3 *positions_data = curves.positions().data();

  CurvesGeometry other = std::move(curves);

  /* The old curves should be empty, and the offsets are expected to be null. */
  EXPECT_TRUE(curves.is_empty());           /* NOLINT: bugprone-use-after-move */
  EXPECT_EQ(curves.curve_offsets, nullptr); /* NOLINT: bugprone-use-after-move */

  /* Just a basic check that the new curves work okay. */
  EXPECT_TRUE(other.bounds_min_max());

  curves = std::move(other);

  CurvesGeometry second_other(std::move(curves));

  /* The data should not have been reallocated ever. */
  EXPECT_EQ(second_other.positions().data(), positions_data);
  EXPECT_EQ(second_other.offsets().data(), offsets_data);
}

TEST(curves_geometry, TypeCount)
{
  CurvesGeometry curves = create_basic_curves(100, 10);
  curves.curve_types_for_write().copy_from({
      CURVE_TYPE_BEZIER,
      CURVE_TYPE_NURBS,
      CURVE_TYPE_NURBS,
      CURVE_TYPE_NURBS,
      CURVE_TYPE_CATMULL_ROM,
      CURVE_TYPE_CATMULL_ROM,
      CURVE_TYPE_CATMULL_ROM,
      CURVE_TYPE_POLY,
      CURVE_TYPE_POLY,
      CURVE_TYPE_POLY,
  });
  curves.update_curve_types();
  const std::array<int, CURVE_TYPES_NUM> &counts = curves.curve_type_counts();
  EXPECT_EQ(counts[CURVE_TYPE_CATMULL_ROM], 3);
  EXPECT_EQ(counts[CURVE_TYPE_POLY], 3);
  EXPECT_EQ(counts[CURVE_TYPE_BEZIER], 1);
  EXPECT_EQ(counts[CURVE_TYPE_NURBS], 3);
}

TEST(curves_geometry, CyclicOffsets)
{
  CurvesGeometry curves = create_basic_curves(100, 10);
  {
    EXPECT_FALSE(curves.has_cyclic_curve());
  }
  {
    curves.cyclic_for_write().fill(true);
    curves.tag_topology_changed();
    EXPECT_TRUE(curves.has_cyclic_curve());
  }
  {
    curves.cyclic_for_write().fill(false);
    curves.tag_topology_changed();
    EXPECT_FALSE(curves.has_cyclic_curve());
  }
  {
    curves.attributes_for_write().remove("cyclic");
    EXPECT_FALSE(curves.has_cyclic_curve());
  }
  {
    curves.cyclic_for_write().copy_from(
        {false, true, false, true, false, false, false, false, true, false});
    curves.tag_topology_changed();
    EXPECT_TRUE(curves.has_cyclic_curve());
  }
}

TEST(curves_geometry, InvalidResolution)
{
  CurvesGeometry curves = create_basic_curves(40, 4);
  curves.curve_types_for_write().copy_from({
      CURVE_TYPE_BEZIER,
      CURVE_TYPE_NURBS,
      CURVE_TYPE_CATMULL_ROM,
      CURVE_TYPE_POLY,
  });
  curves.update_curve_types();
  curves.resolution_for_write().fill(0);

  static const Array<int> expected_offsets{0, 10, 20, 30, 40};

  OffsetIndices<int> actual_offsets = curves.evaluated_points_by_curve();
  for (const int i : actual_offsets.index_range()) {
    EXPECT_EQ(expected_offsets[i], actual_offsets.data()[i]);
  }
}

TEST(curves_geometry, CatmullRomEvaluation)
{
  CurvesGeometry curves(4, 1);
  curves.fill_curve_types(CURVE_TYPE_CATMULL_ROM);
  curves.resolution_for_write().fill(12);
  curves.offsets_for_write().last() = 4;
  curves.cyclic_for_write().fill(false);

  MutableSpan<float3> positions = curves.positions_for_write();
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
  curves.cyclic_for_write().fill(true);

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
  curves.fill_curve_types(CURVE_TYPE_CATMULL_ROM);
  curves.resolution_for_write().fill(12);
  curves.offsets_for_write().last() = 2;
  curves.cyclic_for_write().fill(true);

  /* The curve should still be cyclic when there are only two control points. */
  EXPECT_EQ(curves.evaluated_points_num(), 24);
}

TEST(curves_geometry, BezierPositionEvaluation)
{
  CurvesGeometry curves(2, 1);
  curves.fill_curve_types(CURVE_TYPE_BEZIER);
  curves.resolution_for_write().fill(12);
  curves.offsets_for_write().last() = 2;

  MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();
  MutableSpan<float3> positions = curves.positions_for_write();
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
  curves.fill_curve_types(CURVE_TYPE_BEZIER);
  curves.resolution_for_write().fill(9);
  curves.offsets_for_write().last() = 4;
  handles_left = curves.handle_positions_left_for_write();
  handles_right = curves.handle_positions_right_for_write();
  positions = curves.positions_for_write();
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
  curves.fill_curve_types(CURVE_TYPE_NURBS);
  curves.resolution_for_write().fill(10);
  curves.offsets_for_write().last() = 4;

  MutableSpan<float3> positions = curves.positions_for_write();
  positions[0] = {1, 1, 0};
  positions[1] = {0, 1, 0};
  positions[2] = {0, 0, 0};
  positions[3] = {-1, 0, 0};

  Span<float3> evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_1{{
      {0.166667, 0.833333, 0},
      {0.121333, 0.778667, 0},
      {0.084, 0.716, 0},
      {0.0526667, 0.647333, 0},
      {0.0253333, 0.574667, 0},
      {0, 0.5, 0},
      {-0.0253333, 0.425333, 0},
      {-0.0526667, 0.352667, 0},
      {-0.084, 0.284, 0},
      {-0.121333, 0.221333, 0},
      {-0.166667, 0.166667, 0},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_1[i], 1e-5f);
  }

  /* Test a cyclic curve. */
  curves.cyclic_for_write().fill(true);
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
  curves.nurbs_weights_for_write().fill(1.0f);
  curves.nurbs_weights_for_write()[0] = 4.0f;
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

TEST(curves_geometry, BezierGenericEvaluation)
{
  CurvesGeometry curves(3, 1);
  curves.fill_curve_types(CURVE_TYPE_BEZIER);
  curves.resolution_for_write().fill(8);
  curves.offsets_for_write().last() = 3;

  MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();
  MutableSpan<float3> positions = curves.positions_for_write();
  positions.first() = {-1, 0, 0};
  handles_right.first() = {-1, 1, 0};
  handles_left[1] = {0, 0, 0};
  positions[1] = {1, 0, 0};
  handles_right[1] = {2, 0, 0};
  handles_left.last() = {1, 1, 0};
  positions.last() = {2, 1, 0};

  /* Dangling handles shouldn't be used in a non-cyclic curve. */
  handles_left.first() = {100, 100, 100};
  handles_right.last() = {100, 100, 100};

  Span<float3> evaluated_positions = curves.evaluated_positions();
  static const Array<float3> result_1{{
      {-1.0f, 0.0f, 0.0f},
      {-0.955078f, 0.287109f, 0.0f},
      {-0.828125f, 0.421875f, 0.0f},
      {-0.630859f, 0.439453f, 0.0f},
      {-0.375f, 0.375f, 0.0f},
      {-0.0722656f, 0.263672f, 0.0f},
      {0.265625f, 0.140625f, 0.0f},
      {0.626953f, 0.0410156f, 0.0f},
      {1.0f, 0.0f, 0.0f},
      {1.28906f, 0.0429688f, 0.0f},
      {1.4375f, 0.15625f, 0.0f},
      {1.49219f, 0.316406f, 0.0f},
      {1.5f, 0.5f, 0.0f},
      {1.50781f, 0.683594f, 0.0f},
      {1.5625f, 0.84375f, 0.0f},
      {1.71094f, 0.957031f, 0.0f},
      {2.0f, 1.0f, 0.0f},
  }};
  for (const int i : evaluated_positions.index_range()) {
    EXPECT_V3_NEAR(evaluated_positions[i], result_1[i], 1e-5f);
  }

  Array<float> radii{{0.0f, 1.0f, 2.0f}};
  Array<float> evaluated_radii(17);
  curves.interpolate_to_evaluated(0, radii.as_span(), evaluated_radii.as_mutable_span());
  static const Array<float> result_2{{
      0.0f,
      0.125f,
      0.25f,
      0.375f,
      0.5f,
      0.625f,
      0.75f,
      0.875f,
      1.0f,
      1.125f,
      1.25f,
      1.375f,
      1.5f,
      1.625f,
      1.75f,
      1.875f,
      2.0f,
  }};
  for (const int i : evaluated_radii.index_range()) {
    EXPECT_NEAR(evaluated_radii[i], result_2[i], 1e-6f);
  }
}

/* -------------------------------------------------------------------- */
/** \name NURBS: Basis Cache Calculation
 * \{ */

TEST(curves_geometry, BasisCacheBezierSegmentDeg2)
{
  const int order = 3;
  const int point_count = 3;
  const int resolution = 3;
  const bool is_cyclic = false;

  const std::array<float, 6> knots_data{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
  const Span<float> knots = Span<float>(knots_data);

  /* Expectation */
  auto fn_Ni2_span = [](MutableSpan<float> Ni2, const float u) {
    const float nu = 1.0f - u;
    Ni2[0] = nu * nu;
    Ni2[1] = 2.0f * u * nu;
    Ni2[2] = u * u;
  };

  std::array<float, 12> expected_data;
  MutableSpan<float> expectation = MutableSpan<float>(expected_data);
  fn_Ni2_span(expectation.slice(0, 3), 0.0f);
  fn_Ni2_span(expectation.slice(3, 3), 1.0f / 3.0f);
  fn_Ni2_span(expectation.slice(6, 3), 2.0f / 3.0f);
  fn_Ni2_span(expectation.slice(9, 3), 1.0f);

  /* Test */
  const int evaluated_num = curves::nurbs::calculate_evaluated_num(
      point_count, order, is_cyclic, resolution, KnotsMode::NURBS_KNOT_MODE_CUSTOM, knots);
  EXPECT_EQ(evaluated_num, resolution + 1);

  curves::nurbs::BasisCache cache;
  curves::nurbs::calculate_basis_cache(point_count,
                                       evaluated_num,
                                       order,
                                       resolution,
                                       is_cyclic,
                                       KnotsMode::NURBS_KNOT_MODE_CUSTOM,
                                       knots,
                                       cache);
  EXPECT_EQ_SPAN<float>(expectation, cache.weights);
}

TEST(curves_geometry, BasisCacheNonUniformDeg2)
{
  const int order = 3;
  const int point_count = 8;
  const int resolution = 3;
  const bool is_cyclic = false;

  const std::array<float, 11> knots_data{
      0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 4.0f, 5.0f, 5.0f, 5.0f};
  const Span<float> knots = Span<float>(knots_data);

  /* Expectation */
  auto fn_Ni2_span0 = [](MutableSpan<float> Ni2, const float u) {
    Ni2[0] = square_f(1.0f - u);
    Ni2[1] = 2.0f * u - 1.5f * square_f(u);
    Ni2[2] = square_f(u) / 2.0f;
  };
  auto fn_Ni2_span1 = [](MutableSpan<float> Ni2, float u) {
    Ni2[0] = square_f(2.0f - u) / 2.0f;
    Ni2[1] = -1.5f + 3 * u - square_f(u);
    Ni2[2] = square_f(u - 1.0f) / 2.0f;
  };
  auto fn_Ni2_span2 = [](MutableSpan<float> Ni2, float u) {
    Ni2[0] = square_f(3.0f - u) / 2.0f;
    Ni2[1] = -5.5f + 5.0f * u - square_f(u);
    Ni2[2] = square_f(u - 2.0f) / 2.0f;
  };
  auto fn_Ni2_span3 = [](MutableSpan<float> Ni2, float u) {
    Ni2[0] = square_f(4.0f - u) / 2.0f;
    Ni2[1] = -16.0f + 10.0f * u - 1.5f * square_f(u);
    Ni2[2] = square_f(u - 3.0f);
  };
  auto fn_Ni2_span4 = [](MutableSpan<float> Ni2, float u) {
    Ni2[0] = square_f(5.0f - u);
    Ni2[1] = 2.0f * (u - 4.0f) * (5.0f - u);
    Ni2[2] = square_f(u - 4.0f);
  };

  std::array<float, 48> expected_data;
  MutableSpan<float> expectation = MutableSpan<float>(expected_data);
  for (int i = 0; i < 3; i++) {
    const float du = i / 3.0f;
    const int step = i * 3;
    fn_Ni2_span0(expectation.slice(step, 3), du);
    fn_Ni2_span1(expectation.slice(step + 9, 3), 1.0f + du);
    fn_Ni2_span2(expectation.slice(step + 18, 3), 2.0f + du);
    fn_Ni2_span3(expectation.slice(step + 27, 3), 3.0f + du);
    fn_Ni2_span4(expectation.slice(step + 36, 3), 4.0f + du);
  }
  fn_Ni2_span4(expectation.slice(45, 3), 5.0f);

  /* Test */
  const int evaluated_num = curves::nurbs::calculate_evaluated_num(
      point_count, order, is_cyclic, resolution, KnotsMode::NURBS_KNOT_MODE_CUSTOM, knots);
  EXPECT_EQ(evaluated_num, 5 * resolution + 1);

  curves::nurbs::BasisCache cache;
  curves::nurbs::calculate_basis_cache(point_count,
                                       evaluated_num,
                                       order,
                                       resolution,
                                       is_cyclic,
                                       KnotsMode::NURBS_KNOT_MODE_CUSTOM,
                                       knots,
                                       cache);
  EXPECT_NEAR_SPAN<float>(expectation, cache.weights, 1e-6f);
}

/** \} */

TEST(knot_vector, KnotVectorUniform)
{
  constexpr int8_t order = 5;
  constexpr int points_num = 7;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_NORMAL, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}), multiplicity);
}

TEST(knot_vector, KnotVectorUniformClamped)
{
  constexpr int8_t order = 3;
  constexpr int points_num = 7;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({3, 1, 1, 1, 1, 3}), multiplicity);
}

/* -------------------------------------------------------------------- */
/** \name Knot vector: KnotMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER
 * \{ */

TEST(knot_vector, KnotVectorBezierClampedSegmentDeg2)
{
  constexpr int8_t order = 3;
  constexpr int points_num = 3;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({3, 3}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierClampedSegmentDeg4)
{
  constexpr int8_t order = 5;
  constexpr int points_num = 5;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({5, 5}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierClampedDeg2)
{
  constexpr int8_t order = 3;
  constexpr int points_num = 9;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({3, 2, 2, 2, 3}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierClampedUnevenDeg2)
{
  constexpr int8_t order = 3;
  constexpr int points_num = 8;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({3, 2, 2, 4}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierClampedDeg4)
{
  constexpr int8_t order = 5;
  constexpr int points_num = 13;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({5, 4, 4, 5}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierClampedUnevenDeg4)
{
  constexpr int8_t order = 5;
  constexpr int points_num[4] = {12, 11, 10, 9};
  const std::array<std::array<int, 3>, 4> expectation = {std::array<int, 3>{5, 4, 8},
                                                         std::array<int, 3>{5, 4, 7},
                                                         std::array<int, 3>{5, 4, 6},
                                                         std::array<int, 3>{5, 4, 5}};

  for (int i = 0; i < expectation.size(); i++) {
    Vector<float> knots(curves::nurbs::knots_num(points_num[i], order, false));
    curves::nurbs::calculate_knots(
        points_num[i], KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, false, knots);

    const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
    EXPECT_EQ_SPAN<int>(Span(expectation[i]), multiplicity);
  }
}

TEST(knot_vector, KnotVectorCircleCyclicUnevenDeg2)
{
  constexpr int8_t order = 3;
  constexpr int points_num = 8;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, true));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, true, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({1, 2, 2, 2, 2, 2, 2}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierClampedCyclicUnevenDeg4)
{
  constexpr int8_t order = 5;
  constexpr int points_num[4] = {12, 11, 10, 9};
  const std::array<std::array<int, 6>, 4> expectation = {std::array<int, 6>{1, 4, 4, 4, 4, 4},
                                                         std::array<int, 6>{1, 4, 4, 3, 4, 4},
                                                         std::array<int, 6>{1, 4, 4, 2, 4, 4},
                                                         std::array<int, 6>{1, 4, 4, 1, 4, 4}};

  for (int i = 0; i < expectation.size(); i++) {
    Vector<float> knots(curves::nurbs::knots_num(points_num[i], order, true));
    curves::nurbs::calculate_knots(
        points_num[i], KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER, order, true, knots);

    const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
    EXPECT_EQ_SPAN<int>(Span(expectation[i]), multiplicity);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knot vector: KnotMode::NURBS_KNOT_MODE_BEZIER
 * \{ */

TEST(knot_vector, KnotVectorBezierSegmentDeg2)
{
  constexpr int8_t order = 4;
  constexpr int points_num = 4;

  Vector<float> knots(curves::nurbs::knots_num(points_num, order, false));
  curves::nurbs::calculate_knots(
      points_num, KnotsMode::NURBS_KNOT_MODE_BEZIER, order, false, knots);

  const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
  EXPECT_EQ_SPAN<int>(Span({2, 3, 3}), multiplicity);
}

TEST(knot_vector, KnotVectorBezierUnevenDeg2)
{
  constexpr int8_t order = 3;
  constexpr int points_num[4] = {8, 7, 6, 5};
  const std::array<std::array<int, 6>, 4> expectation = {std::array<int, 6>{2, 2, 2, 2, 2, 1},
                                                         std::array<int, 6>{2, 2, 2, 2, 2, -1},
                                                         std::array<int, 6>{2, 2, 2, 2, 1, -1},
                                                         std::array<int, 6>{2, 2, 2, 2, -1, -1}};

  for (int i = 0; i < expectation.size(); i++) {
    Vector<float> knots(curves::nurbs::knots_num(points_num[i], order, false));
    curves::nurbs::calculate_knots(
        points_num[i], KnotsMode::NURBS_KNOT_MODE_BEZIER, order, false, knots);

    const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
    EXPECT_EQ_SPAN<int>(Span(expectation[i].data(), multiplicity.size()), multiplicity);
  }
}

TEST(knot_vector, KnotVectorBezierUnevenDeg4)
{
  constexpr int8_t order = 5;
  constexpr int points_num[6] = {14, 13, 12, 11, 10, 9};
  const std::array<std::array<int, 6>, 6> expectation = {std::array<int, 6>{2, 4, 4, 4, 4, 1},
                                                         std::array<int, 6>{2, 4, 4, 4, 4, -1},
                                                         std::array<int, 6>{2, 4, 4, 4, 3, -1},
                                                         std::array<int, 6>{2, 4, 4, 4, 2, -1},
                                                         std::array<int, 6>{2, 4, 4, 4, 1, -1},
                                                         std::array<int, 6>{2, 4, 4, 4, -1, -1}};

  for (int i = 0; i < expectation.size(); i++) {
    Vector<float> knots(curves::nurbs::knots_num(points_num[i], order, false));
    curves::nurbs::calculate_knots(
        points_num[i], KnotsMode::NURBS_KNOT_MODE_BEZIER, order, false, knots);

    const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
    EXPECT_EQ_SPAN<int>(Span(expectation[i].data(), multiplicity.size()), multiplicity);
  }
}

TEST(knot_vector, KnotVectorBezierCyclicUnevenDeg4)
{
  constexpr int8_t order = 5;
  constexpr int points_num[4] = {12, 11, 10, 9};
  const std::array<std::array<int, 6>, 4> expectation = {std::array<int, 6>{2, 4, 4, 4, 4, 3},
                                                         std::array<int, 6>{2, 4, 4, 3, 4, 3},
                                                         std::array<int, 6>{2, 4, 4, 2, 4, 3},
                                                         std::array<int, 6>{2, 4, 5, 4, 3, -1}};

  for (int i = 0; i < expectation.size(); i++) {
    Vector<float> knots(curves::nurbs::knots_num(points_num[i], order, true));
    curves::nurbs::calculate_knots(
        points_num[i], KnotsMode::NURBS_KNOT_MODE_BEZIER, order, true, knots);

    const Vector<int> multiplicity = curves::nurbs::calculate_multiplicity_sequence(knots);
    EXPECT_EQ_SPAN<int>(Span(expectation[i].data(), multiplicity.size()), multiplicity);
  }
}

/** \} */

}  // namespace blender::bke::tests
