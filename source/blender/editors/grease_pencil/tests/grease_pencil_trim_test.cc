/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

namespace blender::ed::greasepencil::tests {

static bke::CurvesGeometry create_test_curves(const Span<int> offsets,
                                              const Span<float2> positions_2d,
                                              const Span<bool> cyclic)
{
  BLI_assert(!offsets.is_empty());
  const int curves_num = offsets.size() - 1;
  BLI_assert(cyclic.size() == curves_num);
  const int points_num = offsets.last();

  bke::CurvesGeometry curves(points_num, curves_num);
  curves.offsets_for_write().copy_from(offsets);
  curves.cyclic_for_write().copy_from(cyclic);

  MutableSpan<float3> positions = curves.positions_for_write();
  for (const int i : curves.points_range()) {
    positions[i] = float3(positions_2d[i], 0.0f);
  }

  return curves;
}

static void expect_near_positions(const Span<float3> actual, const Span<float2> expected)
{
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() != actual.size()) {
    return;
  }

  for (const int64_t i : expected.index_range()) {
    EXPECT_NEAR(expected[i].x, actual[i].x, 1e-4) << "X mismatch at index " << i;
    EXPECT_NEAR(expected[i].y, actual[i].y, 1e-4) << "Y mismatch at index " << i;
  }
}

TEST(grease_pencil_trim, trim_two_edges)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{-10, 50}, {10, 50}, {10, 10}, {50, 10}, {50, -10}, {-10, -10}};
  const Array<int> src_offsets = {0, 2, 4, 6, 8};
  const Array<bool> src_cyclic = {false, false, false, false};
  const Array<float2> screen_space_positions = {{20.0f, 0.0f},
                                                {20.0f, 60.0f},
                                                {40.0f, 0.0f},
                                                {40.0f, 60.0f},
                                                {0.0f, 20.0f},
                                                {60.0f, 20.0f},
                                                {0.0f, 40.0f},
                                                {60.0f, 40.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{20.0f, 20.0f},
                                            {20.0f, 60.0f},
                                            {40.0f, 20.0f},
                                            {40.0f, 60.0f},
                                            {20.0f, 20.0f},
                                            {60.0f, 20.0f},
                                            {20.0f, 40.0f},
                                            {60.0f, 40.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_sub_edges)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{10, 35}, {50, 35}, {50, 25}, {10, 25}};
  const Array<int> src_offsets = {0, 2, 4, 6, 8};
  const Array<bool> src_cyclic = {false, false, false, false};
  const Array<float2> screen_space_positions = {{20.0f, 0.0f},
                                                {20.0f, 60.0f},
                                                {40.0f, 0.0f},
                                                {40.0f, 60.0f},
                                                {0.0f, 20.0f},
                                                {60.0f, 20.0f},
                                                {0.0f, 40.0f},
                                                {60.0f, 40.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{20.0f, 0.0f},
                                            {20.0f, 20.0f},
                                            {20.0f, 40.0f},
                                            {20.0f, 60.0f},
                                            {40.0f, 0.0f},
                                            {40.0f, 20.0f},
                                            {40.0f, 40.0f},
                                            {40.0f, 60.0f},
                                            {0.0f, 20.0f},
                                            {60.0f, 20.0f},
                                            {0.0f, 40.0f},
                                            {60.0f, 40.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_plus_intersection)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{20, -10}, {20, 10}, {40, 10}, {40, -10}};
  const Array<int> src_offsets = {0, 4, 8};
  const Array<bool> src_cyclic = {false, false};
  const Array<float2> screen_space_positions = {{30.0f, 0.0f},
                                                {30.0f, 20.0f},
                                                {30.0f, 40.0f},
                                                {30.0f, 60.0f},
                                                {0.0f, 30.0f},
                                                {20.0f, 30.0f},
                                                {40.0f, 30.0f},
                                                {60.0f, 30.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{30.0f, 30.0f},
                                            {30.0f, 40.0f},
                                            {30.0f, 60.0f},
                                            {0.0f, 30.0f},
                                            {20.0f, 30.0f},
                                            {40.0f, 30.0f},
                                            {60.0f, 30.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_t_intersection_to_corner)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{-10, 20}, {10, 20}, {10, 40}, {-10, 40}};
  const Array<int> src_offsets = {0, 3, 7};
  const Array<bool> src_cyclic = {false, false};
  const Array<float2> screen_space_positions = {{30.0f, 0.0f},
                                                {30.0f, 20.0f},
                                                {30.0f, 30.0f},
                                                {0.0f, 30.0f},
                                                {20.0f, 30.0f},
                                                {40.0f, 30.0f},
                                                {60.0f, 30.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{30.0f, 0.0f},
                                            {30.0f, 20.0f},
                                            {30.0f, 30.0f},
                                            {30.0f, 30.0f},
                                            {40.0f, 30.0f},
                                            {60.0f, 30.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_t_intersection_line)
{
  using namespace bke::greasepencil;
  using namespace bke;
  const Array<int2> mcoords = {{20, -10}, {20, 10}, {40, 10}, {40, -10}};
  const Array<bool> src_cyclic = {false, false};

  /* Intersection at the start. */
  {
    const Array<int> src_offsets = {0, 2, 4};
    const Array<float2> screen_space_positions = {
        {30.0f, 30.0f}, {30.0f, 0.0f}, {0.0f, 30.0f}, {60.0f, 30.0f}};
    const bke::CurvesGeometry src = create_test_curves(
        src_offsets, screen_space_positions, src_cyclic);
    const bke::CurvesGeometry dst = trim::trim_curve_segments(
        src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

    const Array<float2> expected_positions = {{0.0f, 30.0f}, {60.0f, 30.0f}};
    expect_near_positions(dst.positions(), expected_positions);
  }

  /* Intersection at the end. */
  {
    const Array<int> src_offsets = {0, 2, 4};
    const Array<float2> screen_space_positions = {
        {30.0f, 0.0f}, {30.0f, 30.0f}, {0.0f, 30.0f}, {60.0f, 30.0f}};
    const bke::CurvesGeometry src = create_test_curves(
        src_offsets, screen_space_positions, src_cyclic);
    const bke::CurvesGeometry dst = trim::trim_curve_segments(
        src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

    const Array<float2> expected_positions = {{0.0f, 30.0f}, {60.0f, 30.0f}};
    expect_near_positions(dst.positions(), expected_positions);
  }
}

TEST(grease_pencil_trim, trim_figure_eight)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{40, 20}, {40, 40}, {60, 40}, {60, 20}};
  const Array<int> src_offsets = {0, 8};
  const Array<bool> src_cyclic = {true};
  const Array<float2> screen_space_positions = {{0.0f, 10.0f},
                                                {0.0f, 30.0f},
                                                {20.0f, 30.0f},
                                                {30.0f, 10.0f},
                                                {50.0f, 10.0f},
                                                {50.0f, 30.0f},
                                                {30.0f, 30.0f},
                                                {20.0f, 10.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{25.0f, 20.0f},
                                            {20.0f, 10.0f},
                                            {0.0f, 10.0f},
                                            {0.0f, 30.0f},
                                            {20.0f, 30.0f},
                                            {25.0f, 20.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_no_geometry)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{0, 0}, {0, 5}, {5, 5}, {5, 0}};
  const Array<int> src_offsets = {0, 2, 4};
  const Array<bool> src_cyclic = {false, false};
  const Array<float2> screen_space_positions = {
      {10.0f, 10.0f}, {50.0f, 10.0f}, {10.0f, 50.0f}, {50.0f, 50.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {
      {10.0f, 10.0f}, {50.0f, 10.0f}, {10.0f, 50.0f}, {50.0f, 50.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_no_geometry_loop)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{0, 0}, {0, 5}, {5, 5}, {5, 0}};
  const Array<int> src_offsets = {0, 3};
  const Array<bool> src_cyclic = {true};
  const Array<float2> screen_space_positions = {{10.0f, 10.0f}, {50.0f, 10.0f}, {10.0f, 50.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{10.0f, 10.0f}, {50.0f, 10.0f}, {10.0f, 50.0f}};
  expect_near_positions(dst.positions(), expected_positions);
  EXPECT_EQ(dst.cyclic()[0], true);
}

TEST(grease_pencil_trim, trim_cyclical_corner)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{20, 20}, {40, 20}, {20, 40}};
  const Array<int> src_offsets = {0, 3, 6};
  const Array<bool> src_cyclic = {false, true};
  const Array<float2> screen_space_positions = {{40.0f, 10.0f},
                                                {10.0f, 10.0f},
                                                {10.0f, 40.0f},
                                                {0.0f, 30.0f},
                                                {30.0f, 30.0f},
                                                {30.0f, 0.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {{40.0f, 10.0f},
                                            {10.0f, 10.0f},
                                            {10.0f, 40.0f},
                                            {30.0f, 10.0f},
                                            {30.0f, 0.0f},
                                            {0.0f, 30.0f},
                                            {10.0f, 30.0f}};
  expect_near_positions(dst.positions(), expected_positions);
}

TEST(grease_pencil_trim, trim_no_geometry_edge_end_intersection)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{20, 10}, {20, 20}, {30, 20}, {30, 10}};
  const Array<int> src_offsets = {0, 2, 5};
  const Array<bool> src_cyclic = {false, false};
  const Array<float2> screen_space_positions = {
      {0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 10.0f}, {10.0f, 20.0f}, {10.0f, 0.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {
      {0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 10.0f}, {10.0f, 20.0f}, {10.0f, 0.0f}};
  expect_near_positions(dst.positions(), expected_positions);
  EXPECT_EQ(dst.cyclic()[0], false);
  EXPECT_EQ(dst.cyclic()[1], false);
}

TEST(grease_pencil_trim, trim_no_geometry_cyclical_loop)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{40, 50}, {40, 40}, {50, 40}, {50, 50}};
  const Array<int> src_offsets = {0, 4, 6};
  const Array<bool> src_cyclic = {true, false};
  const Array<float2> screen_space_positions = {
      {0.0f, 0.0f}, {20.0f, 0.0f}, {20.0f, 20.0f}, {0.0f, 20.0f}, {10.0f, 10.0f}, {30.0f, 0.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {
      {0.0f, 0.0f}, {20.0f, 0.0f}, {20.0f, 20.0f}, {0.0f, 20.0f}, {10.0f, 10.0f}, {30.0f, 0.0f}};
  expect_near_positions(dst.positions(), expected_positions);
  EXPECT_EQ(dst.cyclic()[0], true);
  EXPECT_EQ(dst.cyclic()[1], false);
}

TEST(grease_pencil_trim, trim_no_geometry_point_intersection)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{40, 50}, {40, 40}, {50, 40}, {50, 50}};
  const Array<int> src_offsets = {0, 3, 6};
  const Array<bool> src_cyclic = {false, false};
  const Array<float2> screen_space_positions = {
      {20.0f, 20.0f}, {10.0f, 10.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}, {10.0f, 10.0f}, {0.0f, 0.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {
      {20.0f, 20.0f}, {10.0f, 10.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}, {10.0f, 10.0f}, {0.0f, 0.0f}};
  expect_near_positions(dst.positions(), expected_positions);
  EXPECT_EQ(dst.cyclic()[0], false);
  EXPECT_EQ(dst.cyclic()[1], false);
}

TEST(grease_pencil_trim, trim_no_geometry_self_intersection_degeneracy)
{
  using namespace bke::greasepencil;
  using namespace bke;

  const Array<int2> mcoords = {{40, 50}, {40, 40}, {50, 40}, {50, 50}};
  const Array<int> src_offsets = {0, 6};
  const Array<bool> src_cyclic = {true};
  const Array<float2> screen_space_positions = {
      {0.0f, 20.0f}, {10.0f, 10.0f}, {0.0f, 0.0f}, {0.0f, 20.0f}, {10.0f, 10.0f}, {0.0f, 0.0f}};
  const bke::CurvesGeometry src = create_test_curves(
      src_offsets, screen_space_positions, src_cyclic);
  const bke::CurvesGeometry dst = trim::trim_curve_segments(
      src, screen_space_positions, mcoords, src.curves_range(), src.curves_range(), true);

  const Array<float2> expected_positions = {
      {0.0f, 20.0f}, {10.0f, 10.0f}, {0.0f, 0.0f}, {0.0f, 20.0f}, {10.0f, 10.0f}, {0.0f, 0.0f}};
  expect_near_positions(dst.positions(), expected_positions);
  EXPECT_EQ(dst.cyclic()[0], true);
}

}  // namespace blender::ed::greasepencil::tests
