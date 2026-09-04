/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_gtest_base.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid.hh"

#include "FN_field.hh"
#include "FN_field_evaluation.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/Grid.h>
#endif

#include "testing/testing.h"

namespace blender::bke::tests {

using nodes::Bundle;
using nodes::BundlePtr;
using nodes::GList;
using nodes::GListPtr;
using nodes::List;
using nodes::ListPtr;

class SocketValueVariantTest : public BlenderGTestBase {};

TEST_F(SocketValueVariantTest, SimpleInt)
{
  SocketValueVariant s;
  {
    int &x = s.ensure_type<int>();
    x = 5;
  }
  {
    const int *x = s.get_if<int>();
    EXPECT_NE(x, nullptr);
    EXPECT_EQ(*x, 5);
  }
}

TEST_F(SocketValueVariantTest, IntToFloat)
{
  SocketValueVariant s;
  {
    float &x = s.ensure_type<float>();
    x = 5.3f;
  }
  {
    const int *x = s.get_if<int>();
    EXPECT_EQ(x, nullptr);
  }
  {
    int &x = s.ensure_type<int>();
    EXPECT_EQ(x, 5);
  }
}

TEST_F(SocketValueVariantTest, IntToIntField)
{
  SocketValueVariant s;
  s.ensure_type<int>() = 23;
  const fn::Field<int> &f = s.ensure_type<fn::Field<int>>();
  EXPECT_FALSE(f.depends_on_input());
  EXPECT_EQ(fn::evaluate_constant_field(f), 23);
}

TEST_F(SocketValueVariantTest, IntToFloatField)
{
  SocketValueVariant s;
  s.ensure_type<int>() = 23;
  const fn::Field<float> &f = s.ensure_type<fn::Field<float>>();
  EXPECT_FALSE(f.depends_on_input());
  EXPECT_EQ(fn::evaluate_constant_field(f), 23.0f);
}

TEST_F(SocketValueVariantTest, IntToGField)
{
  SocketValueVariant s;
  s.ensure_type<int>() = 23;
  const fn::GField &f = s.ensure_type<fn::GField>();
  EXPECT_FALSE(f.depends_on_input());
  EXPECT_TRUE(f.cpp_type().is<int>());
  EXPECT_EQ(fn::evaluate_constant_field(f.typed<int>()), 23);
}

TEST_F(SocketValueVariantTest, ConstantIntFieldToInt)
{
  SocketValueVariant s;
  s.ensure_type<fn::Field<int>>() = fn::Field<int>(42);
  const int &v = s.ensure_type<int>();
  EXPECT_EQ(v, 42);
}

TEST_F(SocketValueVariantTest, ConstantIntFieldToFloat)
{
  SocketValueVariant s;
  s.ensure_type<fn::Field<int>>() = fn::Field<int>(42);
  const float &v = s.ensure_type<float>();
  EXPECT_EQ(v, 42.0f);
}

TEST_F(SocketValueVariantTest, ConstIntFieldToFloatField)
{
  SocketValueVariant s;
  s.ensure_type<fn::Field<int>>() = fn::Field<int>(42);
  const fn::Field<float> &f = s.ensure_type<fn::Field<float>>();
  EXPECT_FALSE(f.depends_on_input());
  EXPECT_EQ(fn::evaluate_constant_field(f), 42.0f);
}

TEST_F(SocketValueVariantTest, IndexFieldToFloatField)
{
  SocketValueVariant s;
  s.ensure_type<fn::Field<int>>() = fn::IndexFieldInput::get_field();
  const fn::Field<float> &f = s.ensure_type<fn::Field<float>>();
  EXPECT_TRUE(f.depends_on_input());

  fn::FieldContext context;
  fn::FieldEvaluator evaluator{context, 5};
  evaluator.add(f);
  evaluator.evaluate();
  const VArray<float> values = evaluator.get_evaluated<float>(0);
  EXPECT_EQ(values.size(), 5);
  EXPECT_EQ(values[0], 0.0f);
  EXPECT_EQ(values[1], 1.0f);
  EXPECT_EQ(values[2], 2.0f);
  EXPECT_EQ(values[3], 3.0f);
  EXPECT_EQ(values[4], 4.0f);
}

TEST_F(SocketValueVariantTest, SimpleList)
{
  SocketValueVariant s;
  s.ensure_type<ListPtr<int>>() = List<int>::from_container(Vector<int>{1, 2, 3, 4, 5});
  VArray<int> values = s.ensure_type<ListPtr<int>>()->varray();
  EXPECT_EQ(values.size(), 5);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 3);
  EXPECT_EQ(values[3], 4);
  EXPECT_EQ(values[4], 5);
}

TEST_F(SocketValueVariantTest, IntListToFloatList)
{
  SocketValueVariant s;
  s.ensure_type<ListPtr<int>>() = List<int>::from_container(Vector<int>{1, 2, 3, 4, 5});
  VArray<float> values = s.ensure_type<ListPtr<float>>()->varray();
  EXPECT_EQ(values.size(), 5);
  EXPECT_EQ(values[0], 1.0f);
  EXPECT_EQ(values[1], 2.0f);
  EXPECT_EQ(values[2], 3.0f);
  EXPECT_EQ(values[3], 4.0f);
  EXPECT_EQ(values[4], 5.0f);
}

TEST_F(SocketValueVariantTest, IntToIntList)
{
  SocketValueVariant s;
  s.ensure_type<int>() = 42;
  const ListPtr<int> &list = s.ensure_type<ListPtr<int>>();
  /* Implicit conversion from single value to list is not allowed. */
  EXPECT_FALSE(list);
}

TEST_F(SocketValueVariantTest, SimpleString)
{
  SocketValueVariant s;
  s.ensure_type<std::string>() = "Hello World!";
  EXPECT_EQ(s.ensure_type<std::string>(), "Hello World!");
}

TEST_F(SocketValueVariantTest, SimpleBundle)
{
  SocketValueVariant s;
  s.ensure_type<BundlePtr>() = BundlePtr(Bundle::create());
  {
    Bundle &b = s.ensure_type<BundlePtr>().ensure_mutable_inplace();
    b.add("A"_ustr, 42);
    b.add("B"_ustr, 42.0f);
  }
  {
    const BundlePtr &b = s.ensure_type<BundlePtr>();
    EXPECT_EQ(b->lookup("A"_ustr)->as<int>(), 42);
    EXPECT_EQ(b->lookup("B"_ustr)->as<float>(), 42.0f);
  }
}

#ifdef WITH_OPENVDB

TEST_F(SocketValueVariantTest, SimpleVolumeGrid)
{
  SocketValueVariant s;
  s.ensure_type<GVolumeGrid>() = GVolumeGrid(openvdb::FloatGrid::create());
  const GVolumeGrid &grid = s.ensure_type<GVolumeGrid>();
  EXPECT_EQ(grid->active_tiles(), 0);
}

TEST_F(SocketValueVariantTest, SimpleFloatVolumeGrid)
{
  SocketValueVariant s;
  s.ensure_type<VolumeGrid<float>>() = VolumeGrid<float>(openvdb::FloatGrid::create());
  const VolumeGrid<float> &grid = s.ensure_type<VolumeGrid<float>>();
  EXPECT_EQ(grid->active_tiles(), 0);
}

TEST_F(SocketValueVariantTest, VolumeGridToSingle)
{
  SocketValueVariant s;
  s.ensure_type<GVolumeGrid>() = GVolumeGrid(openvdb::FloatGrid::create());
  const int &v = s.ensure_type<int>();
  /* There is no valid conversion, so this is 0 independent of the grid. */
  EXPECT_EQ(v, 0);
}

TEST_F(SocketValueVariantTest, SingleToVolumeGrid)
{
  SocketValueVariant s;
  s.ensure_type<int>() = 42;
  const GVolumeGrid &grid = s.ensure_type<GVolumeGrid>();
  /* These is no valid conversion from single to volume grid. */
  EXPECT_FALSE(grid);
}

TEST_F(SocketValueVariantTest, IntVolumeGridToFloatVolumeGrid)
{
  SocketValueVariant s;
  {
    std::shared_ptr<openvdb::Int32Grid> int_grid = std::make_shared<openvdb::Int32Grid>();
    int_grid->tree().root().setBackground(42, true);
    s.ensure_type<VolumeGrid<int>>() = VolumeGrid<int>(std::move(int_grid));
  }
  const VolumeGrid<float> &grid = s.ensure_type<VolumeGrid<float>>();
  VolumeTreeAccessToken tree_token;
  const openvdb::FloatGrid &float_grid = grid.grid(tree_token);
  EXPECT_EQ(float_grid.background(), 42.0f);
}

#endif

}  // namespace blender::bke::tests
