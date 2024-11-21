/* SPDX-FileCopyrightText: 2024 Blender Authors
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
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "ED_grease_pencil.hh"

#include <iostream>

namespace blender::ed::greasepencil::tests {

struct GreasePencilIDTestContext {
  Main *bmain = nullptr;
  GreasePencil *grease_pencil = nullptr;

  GreasePencilIDTestContext()
  {
    BKE_idtype_init();
    this->bmain = BKE_main_new();
    this->grease_pencil = static_cast<GreasePencil *>(BKE_id_new(this->bmain, ID_GP, "GP"));
  }
  ~GreasePencilIDTestContext()
  {
    BKE_main_free(bmain);
  }
};

TEST(grease_pencil_merge, merge_simple)
{
  using namespace bke::greasepencil;
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *ctx.grease_pencil;

  Layer &layer1 = grease_pencil.add_layer("Layer1");
  Layer &layer2 = grease_pencil.add_layer("Layer2");

  grease_pencil.insert_frame(layer1, 0);

  grease_pencil.insert_frame(layer2, 0);
  grease_pencil.insert_frame(layer2, 2);

  GreasePencil *merged_grease_pencil = BKE_grease_pencil_new_nomain();

  /* Merge Layer1 and Layer2. */
  Array<Vector<int>> src_layer_indices_by_dst_layer({{0, 1}});
  ed::greasepencil::merge_layers(
      grease_pencil, src_layer_indices_by_dst_layer, *merged_grease_pencil);

  EXPECT_EQ(merged_grease_pencil->layers().size(), 1);
  EXPECT_EQ(merged_grease_pencil->layer(0).frames().size(), 2);
  EXPECT_STREQ(merged_grease_pencil->layer(0).name().c_str(), "Layer1");

  BKE_id_free(nullptr, merged_grease_pencil);
}

TEST(grease_pencil_merge, merge_in_same_group)
{
  using namespace bke::greasepencil;
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *ctx.grease_pencil;

  LayerGroup &group1 = grease_pencil.add_layer_group(grease_pencil.root_group(), "Group1");
  LayerGroup &group2 = grease_pencil.add_layer_group(group1, "Group2");
  LayerGroup &group3 = grease_pencil.add_layer_group(grease_pencil.root_group(), "Group3");

  Layer &layer1 = grease_pencil.add_layer("Layer1");
  Layer &layer2 = grease_pencil.add_layer(group1, "Layer2");
  Layer &layer3 = grease_pencil.add_layer(group2, "Layer3");
  Layer &layer4 = grease_pencil.add_layer(group2, "Layer4");
  grease_pencil.add_layer(group3, "Layer5");

  grease_pencil.insert_frame(layer1, 0);

  grease_pencil.insert_frame(layer2, 0);
  grease_pencil.insert_frame(layer2, 2);

  grease_pencil.insert_frame(layer3, 0);
  grease_pencil.insert_frame(layer3, 3);

  grease_pencil.insert_frame(layer4, 1);
  grease_pencil.insert_frame(layer4, 3);

  GreasePencil *merged_grease_pencil = BKE_grease_pencil_new_nomain();
  BKE_grease_pencil_copy_parameters(grease_pencil, *merged_grease_pencil);

  /* Merge "Layer3" and "Layer4" */
  Array<Vector<int>> src_layer_indices_by_dst_layer({{0, 1}, {2}, {3}, {4}});
  ed::greasepencil::merge_layers(
      grease_pencil, src_layer_indices_by_dst_layer, *merged_grease_pencil);

  EXPECT_EQ(merged_grease_pencil->layers().size(), 4);

  Array<std::string> expected_layer_names({"Layer3", "Layer2", "Layer5", "Layer1"});
  for (const int i : merged_grease_pencil->layers().index_range()) {
    const Layer &layer = merged_grease_pencil->layer(i);
    EXPECT_STREQ(layer.name().c_str(), expected_layer_names[i].c_str());
  }

  Array<int> expected_layer3_keyframes({0, 1, 3});
  for (const int i : merged_grease_pencil->layer(0).sorted_keys().index_range()) {
    const int key = merged_grease_pencil->layer(0).sorted_keys()[i];
    EXPECT_EQ(key, expected_layer3_keyframes[i]);
  }

  BKE_id_free(nullptr, merged_grease_pencil);
}

TEST(grease_pencil_merge, merge_in_different_group)
{
  using namespace bke::greasepencil;
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *ctx.grease_pencil;

  LayerGroup &group1 = grease_pencil.add_layer_group(grease_pencil.root_group(), "Group1");
  LayerGroup &group2 = grease_pencil.add_layer_group(group1, "Group2");
  LayerGroup &group3 = grease_pencil.add_layer_group(grease_pencil.root_group(), "Group3");
  LayerGroup &group4 = grease_pencil.add_layer_group(group2, "Group4");
  LayerGroup &group5 = grease_pencil.add_layer_group(group4, "Group5");
  LayerGroup &group6 = grease_pencil.add_layer_group(group1, "Group6");

  Layer &layer1 = grease_pencil.add_layer("Layer1");
  Layer &layer2 = grease_pencil.add_layer(group6, "Layer2");
  Layer &layer3 = grease_pencil.add_layer(group5, "Layer3");
  Layer &layer4 = grease_pencil.add_layer(group2, "Layer4");
  grease_pencil.add_layer(group3, "Layer5");

  grease_pencil.insert_frame(layer1, 0);

  grease_pencil.insert_frame(layer2, 0);
  grease_pencil.insert_frame(layer2, 2);

  grease_pencil.insert_frame(layer3, 0);
  grease_pencil.insert_frame(layer3, 3);

  grease_pencil.insert_frame(layer4, 1);
  grease_pencil.insert_frame(layer4, 3);

  GreasePencil *merged_grease_pencil = BKE_grease_pencil_new_nomain();
  BKE_grease_pencil_copy_parameters(grease_pencil, *merged_grease_pencil);

  Array<Vector<int>> src_layer_indices_by_dst_layer({{0, 2}, {1}, {3}, {4}});
  ed::greasepencil::merge_layers(
      grease_pencil, src_layer_indices_by_dst_layer, *merged_grease_pencil);

  EXPECT_EQ(merged_grease_pencil->layers().size(), 4);

  TreeNode *node = merged_grease_pencil->find_node_by_name("Layer3");
  EXPECT_TRUE(node != nullptr);
  EXPECT_TRUE(node->is_layer());
  EXPECT_TRUE(node->parent_group() && node->parent_group()->name() == "Group1");

  EXPECT_EQ(node->as_layer().frames().size(), 3);

  Array<int> expected_layer3_keyframes({0, 2, 3});
  for (const int i : node->as_layer().sorted_keys().index_range()) {
    const int key = node->as_layer().sorted_keys()[i];
    EXPECT_EQ(key, expected_layer3_keyframes[i]);
  }

  Array<std::string> expected_layer_names({"Layer4", "Layer3", "Layer5", "Layer1"});
  for (const int i : merged_grease_pencil->layers().index_range()) {
    const Layer &layer = merged_grease_pencil->layer(i);
    EXPECT_STREQ(layer.name().c_str(), expected_layer_names[i].c_str());
  }

  BKE_id_free(nullptr, merged_grease_pencil);
}

TEST(grease_pencil_merge, merge_keyframes)
{
  using namespace bke::greasepencil;
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *ctx.grease_pencil;

  Layer &layer1 = grease_pencil.add_layer("Layer1");
  Layer &layer2 = grease_pencil.add_layer("Layer2");
  Layer &layer3 = grease_pencil.add_layer("Layer3");
  Layer &layer4 = grease_pencil.add_layer("Layer4");
  grease_pencil.add_layer("Layer5");

  Drawing *drawing = grease_pencil.insert_frame(layer1, 0);
  drawing->strokes_for_write().resize(10, 2);

  drawing = grease_pencil.insert_frame(layer2, 0);
  drawing->strokes_for_write().resize(20, 3);
  drawing = grease_pencil.insert_frame(layer2, 2);
  drawing->strokes_for_write().resize(30, 4);

  drawing = grease_pencil.insert_frame(layer3, 0);
  drawing->strokes_for_write().resize(40, 5);
  drawing = grease_pencil.insert_frame(layer3, 3);
  drawing->strokes_for_write().resize(50, 6);

  drawing = grease_pencil.insert_frame(layer4, 1);
  drawing->strokes_for_write().resize(60, 7);
  drawing = grease_pencil.insert_frame(layer4, 3);
  drawing->strokes_for_write().resize(70, 8);

  GreasePencil *merged_grease_pencil = BKE_grease_pencil_new_nomain();
  BKE_grease_pencil_copy_parameters(grease_pencil, *merged_grease_pencil);

  Array<Vector<int>> src_layer_indices_by_dst_layer({{0}, {1, 2}, {3}, {4}});
  ed::greasepencil::merge_layers(
      grease_pencil, src_layer_indices_by_dst_layer, *merged_grease_pencil);

  EXPECT_EQ(merged_grease_pencil->layers().size(), 4);

  Layer &expected_layer_1 = merged_grease_pencil->find_node_by_name("Layer1")->as_layer();
  EXPECT_EQ(merged_grease_pencil->get_drawing_at(expected_layer_1, 0)->strokes().points_num(), 10);

  Layer &expected_layer_2 = merged_grease_pencil->find_node_by_name("Layer2")->as_layer();
  EXPECT_EQ(merged_grease_pencil->get_drawing_at(expected_layer_2, 0)->strokes().points_num(), 60);

  Layer &expected_layer_4 = merged_grease_pencil->find_node_by_name("Layer4")->as_layer();
  EXPECT_EQ(merged_grease_pencil->get_drawing_at(expected_layer_4, 3)->strokes().points_num(), 70);

  BKE_id_free(nullptr, merged_grease_pencil);
}

TEST(grease_pencil_merge, merge_layer_attributes)
{
  using namespace bke;
  using namespace bke::greasepencil;
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *ctx.grease_pencil;

  grease_pencil.add_layer("Layer1");
  grease_pencil.add_layer("Layer2");
  grease_pencil.add_layer("Layer3");

  Array<float> test_float_values({4.2f, 1.0f, -12.0f});
  SpanAttributeWriter<float> test_attribute =
      grease_pencil.attributes_for_write().lookup_or_add_for_write_only_span<float>(
          "test", AttrDomain::Layer);
  test_attribute.span.copy_from(test_float_values);
  test_attribute.finish();

  GreasePencil *merged_grease_pencil = BKE_grease_pencil_new_nomain();

  /* Merge Layer1 and Layer2. */
  Array<Vector<int>> src_layer_indices_by_dst_layer({{0, 1}, {2}});
  ed::greasepencil::merge_layers(
      grease_pencil, src_layer_indices_by_dst_layer, *merged_grease_pencil);

  EXPECT_EQ(merged_grease_pencil->layers().size(), 2);

  VArray<float> merged_values = *merged_grease_pencil->attributes().lookup<float>("test");
  Array<float> expected_float_values({2.6, -12.0f});
  for (const int i : merged_grease_pencil->layers().index_range()) {
    EXPECT_FLOAT_EQ(merged_values[i], expected_float_values[i]);
  }

  BKE_id_free(nullptr, merged_grease_pencil);
}

}  // namespace blender::ed::greasepencil::tests
