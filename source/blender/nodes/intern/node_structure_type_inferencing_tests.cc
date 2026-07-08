/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BKE_global.hh"
#include "BKE_gtest_base.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "DNA_node_types.h"

namespace blender::nodes::tests {

class StructureTypeInferenceTest : public bke::BlenderGTestBase {};

class TestData {
 public:
  Main *bmain = nullptr;

  TestData()
  {
    bmain = BKE_main_new();
    G.main = bmain;
  }

  ~TestData()
  {
    BKE_main_free(bmain);
    G.main = nullptr;
  }
};

static bNode &add_field_boolean_node(bNodeTree &tree)
{
  return *bke::node_add_node(nullptr, tree, "GeometryNodeInputNamedAttribute"_ustr);
}

static bNode &add_switch_node(Main &bmain, bNodeTree &tree, const eNodeSocketDatatype socket_type)
{
  bNode &node = *bke::node_add_node(nullptr, tree, "GeometryNodeSwitch"_ustr);
  NodeSwitch &storage = *static_cast<NodeSwitch *>(node.storage);
  storage.input_type = socket_type;
  BKE_ntree_update_after_single_tree_change(bmain, tree);
  return node;
}

static StructureType infer_switch_output_structure_with_field_condition(
    const eNodeSocketDatatype socket_type)
{
  TestData data;
  bNodeTree &tree = *bke::node_tree_add_tree(data.bmain, "Test", "GeometryNodeTree");
  bNode &field_node = add_field_boolean_node(tree);
  bNode &switch_node = add_switch_node(*data.bmain, tree, socket_type);

  bke::node_add_link(tree,
                     field_node,
                     *field_node.output_by_identifier("Exists"_ustr),
                     switch_node,
                     *switch_node.input_by_identifier("Switch"_ustr));
  BKE_ntree_update_after_single_tree_change(*data.bmain, tree);

  return switch_node.output_by_identifier("Output"_ustr)->runtime->inferred_structure_type;
}

TEST_F(StructureTypeInferenceTest, GeometryOutputDoesNotBecomeField)
{
  EXPECT_EQ(infer_switch_output_structure_with_field_condition(SOCK_GEOMETRY),
            StructureType::Single);
}

TEST_F(StructureTypeInferenceTest, ValueOutputCanBecomeField)
{
  EXPECT_EQ(infer_switch_output_structure_with_field_condition(SOCK_FLOAT), StructureType::Field);
}

}  // namespace blender::nodes::tests
