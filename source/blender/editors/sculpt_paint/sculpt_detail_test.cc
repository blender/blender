#include "sculpt_intern.hh"

#include "BKE_object_types.hh"

#include "testing/testing.h"

namespace blender::ed::sculpt_paint::dyntopo::detail_size::test {
constexpr float CONSTANT_DETAIL = 50.0f;
constexpr float BRUSH_RADIUS = 0.5f;
constexpr float PIXEL_RADIUS = 200;
constexpr float PIXEL_SIZE = 100;

TEST(Conversion, ConstantToBrushDetail)
{
  blender::bke::ObjectRuntime runtime;
  runtime.object_to_world = MatBase<float, 4, 4>::identity();

  Object ob;
  ob.runtime = &runtime;

  const float brush_percent = constant_to_brush_detail(CONSTANT_DETAIL, BRUSH_RADIUS, &ob);
  const float converted = brush_to_detail_size(brush_percent, BRUSH_RADIUS);

  const float expected = constant_to_detail_size(CONSTANT_DETAIL, &ob);
  EXPECT_FLOAT_EQ(expected, converted);
}
TEST(Conversion, ConstantToRelativeDetail)
{
  blender::bke::ObjectRuntime runtime;
  runtime.object_to_world = MatBase<float, 4, 4>::identity();

  Object ob;
  ob.runtime = &runtime;

  const float relative_detail = constant_to_relative_detail(
      CONSTANT_DETAIL, BRUSH_RADIUS, PIXEL_RADIUS, PIXEL_SIZE, &ob);
  const float converted = relative_to_detail_size(
      relative_detail, BRUSH_RADIUS, PIXEL_RADIUS, PIXEL_SIZE);

  const float expected = constant_to_detail_size(CONSTANT_DETAIL, &ob);
  EXPECT_FLOAT_EQ(expected, converted);
}
}  // namespace blender::ed::sculpt_paint::dyntopo::detail_size::test
