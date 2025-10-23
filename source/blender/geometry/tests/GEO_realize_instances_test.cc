/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_array_utils.hh"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_idtype.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"

#include "DNA_curves_types.h"

#include "GEO_realize_instances.hh"

#include "CLG_log.h"

#include "testing/testing.h"

using namespace blender::bke;

namespace blender::geometry::tests {

class RealizeInstancesTest : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }
};

static void create_test_curves(bke::CurvesGeometry &curves, Span<int> offsets)
{
  BLI_assert(!offsets.is_empty());
  const int curves_num = offsets.size() - 1;
  const int points_num = offsets.last();

  curves.resize(points_num, curves_num);
  curves.offsets_for_write().copy_from(offsets);
  curves.update_curve_types();

  /* Attribute storing original indices to test point remapping. */
  SpanAttributeWriter<int> test_indices_writer =
      curves.attributes_for_write().lookup_or_add_for_write_span<int>(
          "test_index", bke::AttrDomain::Point, bke::AttributeInitConstruct());
  array_utils::fill_index_range(test_indices_writer.span);
  test_indices_writer.finish();
}

/* Regression test for builtin curve attributes:
 * The attribute can be added with arbitrary type/domain on instances,
 * but is built-in and restricted on curves, which will not allow writing it
 * to the realized curves geometry. #142163 */
TEST_F(RealizeInstancesTest, InstanceAttributeToBuiltinCurvesAttribute)
{
  Curves *curves_id = BKE_id_new_nomain<Curves>("TestCurves");
  create_test_curves(curves_id->geometry.wrap(), {0, 3});
  bke::GeometrySet curves_geometry = GeometrySet::from_curves(curves_id);

  Instances *instances = new Instances();
  const int handle = instances->add_reference(bke::InstanceReference{curves_geometry});
  /* The issue only occurs with 2 or more instances. In case of a single instance the code takes a
   * special path that does not run cause this problem. */
  instances->add_instance(handle, float4x4::identity());
  instances->add_instance(handle, float4x4::identity());
  /* This attribute will be converted to the point domain, where it is invalid on curves. */
  instances->attributes_for_write().add<float>(
      "curve_type", AttrDomain::Instance, AttributeInitDefaultValue());
  bke::GeometrySet instances_geometry = GeometrySet::from_instances(instances);

  geometry::RealizeInstancesOptions options;
  options.realize_instance_attributes = true;
  GeometrySet realized_geometry_set =
      geometry::realize_instances(instances_geometry, options).geometry;
}

}  // namespace blender::geometry::tests
