/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

#include "BLI_array_utils.hh"
#include "GEO_merge_curves.hh"

#include "testing/testing.h"

using namespace blender::bke;

namespace blender::geometry::tests {

static bke::CurvesGeometry create_test_curves(Span<int> offsets, Span<bool> cyclic)
{
  BLI_assert(!offsets.is_empty());
  const int curves_num = offsets.size() - 1;
  BLI_assert(cyclic.size() == curves_num);
  const int points_num = offsets.last();

  bke::CurvesGeometry curves(points_num, curves_num);
  curves.offsets_for_write().copy_from(offsets);
  curves.cyclic_for_write().copy_from(cyclic);

  /* Attribute storing original indices to test point remapping. */
  SpanAttributeWriter<int> test_indices_writer =
      curves.attributes_for_write().lookup_or_add_for_write_span<int>(
          "test_index", bke::AttrDomain::Point, bke::AttributeInitConstruct());
  array_utils::fill_index_range(test_indices_writer.span);
  test_indices_writer.finish();

  return curves;
}

TEST(merge_curves, NoConnections)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  Array<int> connect_to_curve(4, -1);
  Array<bool> flip_direction(4, false);

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 4);
  EXPECT_EQ_SPAN(Span({0, 3, 6, 9, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, true, true, false}), cyclic);
}

TEST(merge_curves, ConnectSingleCurve)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  Array<int> connect_to_curve = {-1, -1, -1, 1};
  Array<bool> flip_direction(4, false);

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 3);
  EXPECT_EQ_SPAN(Span({0, 3, 6, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, true, false}), cyclic);
  EXPECT_EQ_SPAN(Span({0, 1, 2, 6, 7, 8, 9, 10, 11, 3, 4, 5}), dst_indices);
}

TEST(merge_curves, ReverseCurves)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  Array<int> connect_to_curve = {-1, -1, -1, -1};
  Array<bool> flip_direction = {false, true, false, true};

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 4);
  EXPECT_EQ_SPAN(Span({0, 3, 6, 9, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, true, true, false}), cyclic);
  EXPECT_EQ_SPAN(Span({0, 1, 2, 5, 4, 3, 6, 7, 8, 11, 10, 9}), dst_indices);
}

TEST(merge_curves, ConnectAndReverseCurves)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  Array<int> connect_to_curve = {3, 0, -1, -1};
  Array<bool> flip_direction = {true, false, true, false};

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 2);
  EXPECT_EQ_SPAN(Span({0, 9, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, true}), cyclic);
  EXPECT_EQ_SPAN(Span({3, 4, 5, 2, 1, 0, 9, 10, 11, 8, 7, 6}), dst_indices);
}

TEST(merge_curves, CyclicConnection)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  Array<int> connect_to_curve = {-1, 3, -1, 1};
  Array<bool> flip_direction(4, false);

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 3);
  EXPECT_EQ_SPAN(Span({0, 3, 9, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, true, true}), cyclic);
  EXPECT_EQ_SPAN(Span({0, 1, 2, 3, 4, 5, 9, 10, 11, 6, 7, 8}), dst_indices);
}

TEST(merge_curves, SelfConnectCurve)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, false, false, false});

  Array<int> connect_to_curve = {-1, 1, 2, -1};
  Array<bool> flip_direction(4, false);

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 4);
  EXPECT_EQ_SPAN(Span({0, 3, 6, 9, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, true, true, false}), cyclic);
  EXPECT_EQ_SPAN(Span({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}), dst_indices);
}

TEST(merge_curves, MergeAll)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  Array<int> connect_to_curve = {2, 0, 3, 1};
  Array<bool> flip_direction(4, false);

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 1);
  EXPECT_EQ_SPAN(Span({0, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({true}), cyclic);
  EXPECT_EQ_SPAN(Span({0, 1, 2, 6, 7, 8, 9, 10, 11, 3, 4, 5}), dst_indices);
}

TEST(merge_curves, Branching)
{
  bke::CurvesGeometry src_curves = create_test_curves({0, 3, 6, 9, 12},
                                                      {false, true, true, false});

  /* Multiple curves connect to curve 2, one connection is ignored. */
  Array<int> connect_to_curve = {2, 2, -1, -1};
  Array<bool> flip_direction(4, false);

  bke::CurvesGeometry dst_curves = geometry::curves_merge_endpoints(
      src_curves, connect_to_curve, flip_direction, {});
  const VArraySpan<bool> cyclic = dst_curves.cyclic();
  const VArraySpan<int> dst_indices = *dst_curves.attributes().lookup<int>("test_index");

  EXPECT_EQ(dst_curves.points_num(), 12);
  EXPECT_EQ(dst_curves.curves_num(), 3);
  EXPECT_EQ_SPAN(Span({0, 6, 9, 12}), dst_curves.offsets());
  EXPECT_EQ_SPAN(Span({false, false, false}), cyclic);
  EXPECT_EQ_SPAN(Span({0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11}), dst_indices);
}

}  // namespace blender::geometry::tests
