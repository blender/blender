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

}  // namespace blender::bke::tests
