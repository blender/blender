/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "ED_curves.hh"

#include "testing/testing.h"

namespace blender::ed::curves::tests {

static bke::CurvesGeometry create_curves(const Span<Vector<float3>> all_positions,
                                         const int order,
                                         const Set<int> &is_cyclic)
{
  Array<int> offsets(all_positions.size() + 1, 0);
  for (const int curve : all_positions.index_range()) {
    const Vector<float3> &curve_positions = all_positions[curve];
    offsets[curve + 1] = offsets[curve] + curve_positions.size();
  }

  bke::CurvesGeometry curves(offsets.last(), all_positions.size());

  curves.offsets_for_write().copy_from(offsets);
  const OffsetIndices points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<bool> cyclic = curves.cyclic_for_write();
  MutableSpan<int8_t> orders = curves.nurbs_orders_for_write();

  for (const int curve : all_positions.index_range()) {
    positions.slice(points_by_curve[curve]).copy_from(all_positions[curve]);
    cyclic[curve] = is_cyclic.contains(curve);
    orders[curve] = order;
  }

  curves.tag_topology_changed();
  return curves;
}

static bke::CurvesGeometry create_curves(const Vector<float3> positions,
                                         const int order,
                                         const Set<int> &is_cyclic)
{
  return create_curves(Span<Vector<float3>>(&positions, 1), order, is_cyclic);
}

TEST(curves_editors, DuplicatePointsTwoSingle)
{
  /* Two points from single curve. */
  const Vector<float3> expected_positions = {{-1.5, 0, 0}, {-1, 1, 0}, {1, 1, 0}, {1.5, 0, 0}};

  bke::CurvesGeometry curves = create_curves(expected_positions, 4, {});
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices(Array<int>{1, 2}.as_span(), memory);

  duplicate_points(curves, mask);

  EXPECT_TRUE(curves.curves_num() == 2);

  const Span<float3> positions = curves.positions();

  for (const int point : expected_positions.index_range()) {
    EXPECT_TRUE(positions[point] == expected_positions[point]);
  }

  EXPECT_TRUE(positions[4] == expected_positions[1]);
  EXPECT_TRUE(positions[5] == expected_positions[2]);
}

TEST(curves_editors, DuplicatePointsFourThree)
{
  /* Four points from three curves. One curve has one point. */
  const Vector<Vector<float3>> expected_positions = {
      {{-1.5, 0, 0}, {-1, 1, 0}, {1, 1, 0}, {1.5, 0, 0}},
      {{0, 0, 0}},
      {{-1.5, 0, 0}, {-1, 1, 0}, {1, 1, 0}, {1.5, 0, 0}, {1, -1, 0}}};

  bke::CurvesGeometry curves = create_curves(expected_positions, 4, {});
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices(Array<int>{0, 1, 4, 9}.as_span(), memory);

  duplicate_points(curves, mask);

  EXPECT_TRUE(curves.curves_num() == expected_positions.size() + 3);

  const Span<float3> positions = curves.positions();
  const OffsetIndices points_by_curve = curves.points_by_curve();

  for (const int curve : expected_positions.index_range()) {
    const Span<float3> expected_curve_positions = expected_positions[curve];
    const IndexRange points = points_by_curve[curve];
    for (const int point : expected_curve_positions.index_range()) {
      EXPECT_TRUE(positions[points[point]] == expected_curve_positions[point]);
    }
  }

  EXPECT_TRUE(positions[10] == expected_positions[0][0]);
  EXPECT_TRUE(positions[11] == expected_positions[0][1]);
  EXPECT_TRUE(positions[12] == expected_positions[1][0]);
  EXPECT_TRUE(positions[13] == expected_positions[2][4]);
}

TEST(curves_editors, DuplicatePointsTwoCyclic)
{
  /* Two points from cyclic curve. Points are on cycle. */
  const Vector<Vector<float3>> expected_positions = {
      {{-1.5, 0, 0}, {-1, 1, 0}, {1, 1, 0}, {1.5, 0, 0}},
      {{0, 0, 0}},
      {{1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {-1, 1, 0}},
      {{-1.5, 0, 0}, {-1, 1, 0}, {1, 1, 0}, {1.5, 0, 0}, {1, -1, 0}}};

  bke::CurvesGeometry curves = create_curves(expected_positions, 4, {2});
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices(Array<int>{5, 8}.as_span(), memory);

  duplicate_points(curves, mask);

  EXPECT_TRUE(curves.curves_num() == expected_positions.size() + 1);

  const Span<float3> positions = curves.positions();
  const OffsetIndices points_by_curve = curves.points_by_curve();

  for (const int curve : expected_positions.index_range()) {
    const Span<float3> expected_curve_positions = expected_positions[curve];
    const IndexRange points = points_by_curve[curve];
    for (const int point : expected_curve_positions.index_range()) {
      EXPECT_TRUE(positions[points[point]] == expected_curve_positions[point]);
    }
  }

  EXPECT_TRUE(positions[14] == expected_positions[2][3]);
  EXPECT_TRUE(positions[15] == expected_positions[2][0]);
}

}  // namespace blender::ed::curves::tests
