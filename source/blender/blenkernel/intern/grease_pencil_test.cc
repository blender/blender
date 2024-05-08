/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_string.h"

#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

using namespace blender::bke::greasepencil;

namespace blender::bke::greasepencil::tests {

/* --------------------------------------------------------------------------------------------- */
/* Grease Pencil ID Tests. */

/* NOTE: Using a struct with constructor and destructor instead of a fixture here, to have all the
 * tests in the same group (`greasepencil`). */
struct GreasePencilIDTestContext {
  Main *bmain = nullptr;

  GreasePencilIDTestContext()
  {
    BKE_idtype_init();
    bmain = BKE_main_new();
  }
  ~GreasePencilIDTestContext()
  {
    BKE_main_free(bmain);
  }
};

TEST(greasepencil, create_grease_pencil_id)
{
  GreasePencilIDTestContext ctx;

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(BKE_id_new(ctx.bmain, ID_GP, "GP"));
  EXPECT_EQ(grease_pencil.drawings().size(), 0);
  EXPECT_EQ(grease_pencil.root_group().num_nodes_total(), 0);
}

/* --------------------------------------------------------------------------------------------- */
/* Drawing Array Tests. */

TEST(greasepencil, add_empty_drawings)
{
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(BKE_id_new(ctx.bmain, ID_GP, "GP"));
  grease_pencil.add_empty_drawings(3);
  EXPECT_EQ(grease_pencil.drawings().size(), 3);
}

TEST(greasepencil, remove_drawings)
{
  GreasePencilIDTestContext ctx;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(BKE_id_new(ctx.bmain, ID_GP, "GP"));
  grease_pencil.add_empty_drawings(3);

  GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(grease_pencil.drawing(1));
  drawing->wrap().strokes_for_write().resize(0, 10);

  Layer &layer1 = grease_pencil.add_layer("Layer1");
  Layer &layer2 = grease_pencil.add_layer("Layer2");

  layer1.add_frame(0, 0);
  layer1.add_frame(10, 1);
  layer1.add_frame(20, 2);

  layer2.add_frame(0, 1);
  drawing->wrap().add_user();

  grease_pencil.remove_frames(layer1, {10});
  grease_pencil.remove_frames(layer2, {0});
  EXPECT_EQ(grease_pencil.drawings().size(), 2);

  static int expected_frames_size[] = {2, 0};
  static int expected_frames_pairs_layer0[][2] = {{0, 0}, {20, 1}};

  Span<const Layer *> layers = grease_pencil.layers();
  EXPECT_EQ(layers[0]->frames().size(), expected_frames_size[0]);
  EXPECT_EQ(layers[1]->frames().size(), expected_frames_size[1]);
  EXPECT_EQ(layers[0]->frames().lookup(expected_frames_pairs_layer0[0][0]).drawing_index,
            expected_frames_pairs_layer0[0][1]);
  EXPECT_EQ(layers[0]->frames().lookup(expected_frames_pairs_layer0[1][0]).drawing_index,
            expected_frames_pairs_layer0[1][1]);
}

/* --------------------------------------------------------------------------------------------- */
/* Layer Tree Tests. */

struct GreasePencilHelper : public ::GreasePencil {
  GreasePencilHelper()
  {
    this->root_group_ptr = MEM_new<greasepencil::LayerGroup>(__func__);
    this->active_node = nullptr;

    CustomData_reset(&this->layers_data);

    this->runtime = MEM_new<GreasePencilRuntime>(__func__);
  }

  ~GreasePencilHelper()
  {
    CustomData_free(&this->layers_data, this->layers().size());
    MEM_delete(&this->root_group());
    MEM_delete(this->runtime);
    this->runtime = nullptr;
  }
};

TEST(greasepencil, layer_tree_empty)
{
  GreasePencilHelper grease_pencil;
  EXPECT_EQ(grease_pencil.root_group().num_nodes_total(), 0);
}

TEST(greasepencil, layer_tree_build_simple)
{
  GreasePencilHelper grease_pencil;

  LayerGroup &group = grease_pencil.add_layer_group(grease_pencil.root_group(), "Group1");
  grease_pencil.add_layer(group, "Layer1");
  grease_pencil.add_layer(group, "Layer2");
  EXPECT_EQ(grease_pencil.root_group().num_nodes_total(), 3);
}

struct GreasePencilLayerTreeExample {
  StringRefNull names[7] = {"Group1", "Layer1", "Layer2", "Group2", "Layer3", "Layer4", "Layer5"};
  const bool is_layer[7] = {false, true, true, false, true, true, true};
  GreasePencilHelper grease_pencil;

  GreasePencilLayerTreeExample()
  {
    LayerGroup &group = grease_pencil.add_layer_group(grease_pencil.root_group(), names[0]);
    grease_pencil.add_layer(group, names[1]);
    grease_pencil.add_layer(group, names[2]);

    LayerGroup &group2 = grease_pencil.add_layer_group(group, names[3]);
    grease_pencil.add_layer(group2, names[4]);
    grease_pencil.add_layer(group2, names[5]);

    grease_pencil.add_layer(names[6]);
  }
};

TEST(greasepencil, layer_tree_pre_order_iteration)
{
  GreasePencilLayerTreeExample ex;

  Span<const TreeNode *> children = ex.grease_pencil.nodes();
  for (const int i : children.index_range()) {
    const TreeNode &child = *children[i];
    EXPECT_STREQ(child.name().data(), ex.names[i].data());
  }
}

TEST(greasepencil, layer_tree_pre_order_iteration2)
{
  GreasePencilLayerTreeExample ex;

  Span<const Layer *> layers = ex.grease_pencil.layers();
  char name[64];
  for (const int i : layers.index_range()) {
    const Layer &layer = *layers[i];
    SNPRINTF(name, "%s%d", "Layer", i + 1);
    EXPECT_STREQ(layer.name().data(), name);
  }
}

TEST(greasepencil, layer_tree_total_size)
{
  GreasePencilLayerTreeExample ex;
  EXPECT_EQ(ex.grease_pencil.root_group().num_nodes_total(), 7);
}

TEST(greasepencil, layer_tree_node_types)
{
  GreasePencilLayerTreeExample ex;
  Span<const TreeNode *> children = ex.grease_pencil.nodes();
  for (const int i : children.index_range()) {
    const TreeNode &child = *children[i];
    EXPECT_EQ(child.is_layer(), ex.is_layer[i]);
    EXPECT_EQ(child.is_group(), !ex.is_layer[i]);
  }
}

TEST(greasepencil, layer_tree_is_child_of)
{
  GreasePencilLayerTreeExample ex;

  EXPECT_FALSE(ex.grease_pencil.root_group().is_child_of(ex.grease_pencil.root_group()));

  const LayerGroup &group1 = ex.grease_pencil.find_node_by_name("Group1")->as_group();
  const LayerGroup &group2 = ex.grease_pencil.find_node_by_name("Group2")->as_group();
  const Layer &layer1 = ex.grease_pencil.find_node_by_name("Layer1")->as_layer();
  const Layer &layer3 = ex.grease_pencil.find_node_by_name("Layer3")->as_layer();
  const Layer &layer5 = ex.grease_pencil.find_node_by_name("Layer5")->as_layer();

  EXPECT_TRUE(layer1.is_child_of(ex.grease_pencil.root_group()));
  EXPECT_TRUE(layer1.is_child_of(group1));
  EXPECT_TRUE(layer3.is_child_of(group1));
  EXPECT_FALSE(layer5.is_child_of(group1));

  EXPECT_TRUE(layer3.is_child_of(group2));
  EXPECT_FALSE(layer1.is_child_of(group2));

  EXPECT_TRUE(layer5.is_child_of(ex.grease_pencil.root_group()));
}

/* --------------------------------------------------------------------------------------------- */
/* Frames Tests. */

struct GreasePencilLayerFramesExample {
  /**
   *               | | | | | | | | | | |1|1|1|1|1|1|1|
   * Scene Frame:  |0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|...
   * Drawing:      [#0       ][#1      ]   [#2     ]
   */
  const FramesMapKey sorted_keys[5] = {0, 5, 10, 12, 16};
  GreasePencilFrame sorted_values[5] = {{0}, {1}, {-1}, {2}, {-1}};
  Layer layer;

  GreasePencilLayerFramesExample()
  {
    for (int i = 0; i < 5; i++) {
      layer.frames_for_write().add(this->sorted_keys[i], this->sorted_values[i]);
    }
  }
};

TEST(greasepencil, frame_is_null)
{
  GreasePencilLayerFramesExample ex;
  EXPECT_TRUE(ex.layer.frames().lookup(10).is_null());
}

TEST(greasepencil, drawing_index_at)
{
  GreasePencilLayerFramesExample ex;
  EXPECT_EQ(ex.layer.drawing_index_at(-100), -1);
  EXPECT_EQ(ex.layer.drawing_index_at(100), -1);
  EXPECT_EQ(ex.layer.drawing_index_at(0), 0);
  EXPECT_EQ(ex.layer.drawing_index_at(1), 0);
  EXPECT_EQ(ex.layer.drawing_index_at(5), 1);
}

TEST(greasepencil, add_frame)
{
  GreasePencilLayerFramesExample ex;
  EXPECT_FALSE(ex.layer.add_frame(0, 3));
  EXPECT_TRUE(ex.layer.add_frame(10, 3));
  EXPECT_EQ(ex.layer.drawing_index_at(10), 3);
  EXPECT_EQ(ex.layer.drawing_index_at(11), 3);
  EXPECT_EQ(ex.layer.drawing_index_at(12), 2);
}

TEST(greasepencil, add_frame_duration_fail)
{
  GreasePencilLayerFramesExample ex;
  EXPECT_FALSE(ex.layer.add_frame(0, 3, 10));
}

TEST(greasepencil, add_frame_duration_override_start_null_frame)
{
  GreasePencilLayerFramesExample ex;
  EXPECT_TRUE(ex.layer.add_frame(10, 3, 2));
  EXPECT_EQ(ex.layer.drawing_index_at(10), 3);
  EXPECT_EQ(ex.layer.drawing_index_at(11), 3);
  EXPECT_EQ(ex.layer.drawing_index_at(12), 2);
}

TEST(greasepencil, add_frame_duration_check_duration)
{
  GreasePencilLayerFramesExample ex;
  EXPECT_TRUE(ex.layer.add_frame(17, 3, 10));
  Span<FramesMapKey> sorted_keys = ex.layer.sorted_keys();
  EXPECT_EQ(sorted_keys.size(), 7);
  EXPECT_EQ(sorted_keys[6] - sorted_keys[5], 10);
}

TEST(greasepencil, add_frame_duration_override_null_frames)
{
  Layer layer;
  layer.frames_for_write().add(0, {1});
  layer.frames_for_write().add(1, {-1});
  layer.frames_for_write().add(2, {-1});
  layer.frames_for_write().add(3, {-1});

  EXPECT_TRUE(layer.add_frame(1, 3, 10));
  EXPECT_EQ(layer.drawing_index_at(0), 1);
  EXPECT_EQ(layer.drawing_index_at(1), 3);
  EXPECT_EQ(layer.drawing_index_at(11), -1);
  Span<FramesMapKey> sorted_keys = layer.sorted_keys();
  EXPECT_EQ(sorted_keys.size(), 3);
  EXPECT_EQ(sorted_keys[0], 0);
  EXPECT_EQ(sorted_keys[1], 1);
  EXPECT_EQ(sorted_keys[2], 11);
}

TEST(greasepencil, remove_frame_single)
{
  Layer layer;
  layer.add_frame(0, 1);
  layer.remove_frame(0);
  EXPECT_EQ(layer.frames().size(), 0);
}

TEST(greasepencil, remove_frame_first)
{
  Layer layer;
  layer.add_frame(0, 1);
  layer.add_frame(5, 2);
  layer.remove_frame(0);
  EXPECT_EQ(layer.frames().size(), 1);
  EXPECT_EQ(layer.frames().lookup(5).drawing_index, 2);
}

TEST(greasepencil, remove_frame_last)
{
  Layer layer;
  layer.add_frame(0, 1);
  layer.add_frame(5, 2);
  layer.remove_frame(5);
  EXPECT_EQ(layer.frames().size(), 1);
  EXPECT_EQ(layer.frames().lookup(0).drawing_index, 1);
}

TEST(greasepencil, remove_frame_implicit_hold)
{
  Layer layer;
  layer.add_frame(0, 1, 4);
  layer.add_frame(5, 2);
  layer.remove_frame(5);
  EXPECT_EQ(layer.frames().size(), 2);
  EXPECT_EQ(layer.frames().lookup(0).drawing_index, 1);
  EXPECT_TRUE(layer.frames().lookup(4).is_null());
}

TEST(greasepencil, remove_frame_fixed_duration_end)
{
  Layer layer;
  layer.add_frame(0, 1, 5);
  layer.add_frame(5, 2);
  layer.remove_frame(0);
  EXPECT_EQ(layer.frames().size(), 1);
  EXPECT_EQ(layer.frames().lookup(5).drawing_index, 2);
}

TEST(greasepencil, remove_frame_fixed_duration_overwrite_end)
{
  Layer layer;
  layer.add_frame(0, 1, 5);
  layer.add_frame(5, 2);
  layer.remove_frame(5);
  EXPECT_EQ(layer.frames().size(), 2);
  EXPECT_EQ(layer.frames().lookup(0).drawing_index, 1);
  EXPECT_TRUE(layer.frames().lookup(5).is_null());
}

TEST(greasepencil, remove_drawings_no_change)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(
      BKE_id_new_nomain(ID_GP, "Grease Pencil test"));

  grease_pencil->add_empty_drawings(3);

  Layer &layer_a = grease_pencil->add_layer("LayerA");
  Layer &layer_b = grease_pencil->add_layer("LayerB");
  layer_b.add_frame(10, 0);
  layer_b.add_frame(20, 1);
  layer_b.add_frame(30, 2);

  EXPECT_EQ(layer_a.frames().size(), 0);
  EXPECT_EQ(layer_b.frames().size(), 3);
  EXPECT_EQ(layer_b.frames().lookup(10).drawing_index, 0);
  EXPECT_EQ(layer_b.frames().lookup(20).drawing_index, 1);
  EXPECT_EQ(layer_b.frames().lookup(30).drawing_index, 2);
  /* Test DNA storage data too. */
  layer_a.prepare_for_dna_write();
  layer_b.prepare_for_dna_write();
  EXPECT_EQ(layer_a.frames_storage.num, 0);
  EXPECT_EQ(layer_b.frames_storage.num, 3);
  EXPECT_EQ(layer_b.frames_storage.values[0].drawing_index, 0);
  EXPECT_EQ(layer_b.frames_storage.values[1].drawing_index, 1);
  EXPECT_EQ(layer_b.frames_storage.values[2].drawing_index, 2);

  grease_pencil->remove_layer(layer_a);
  EXPECT_EQ(layer_b.frames().size(), 3);
  EXPECT_EQ(layer_b.frames().lookup(10).drawing_index, 0);
  EXPECT_EQ(layer_b.frames().lookup(20).drawing_index, 1);
  EXPECT_EQ(layer_b.frames().lookup(30).drawing_index, 2);
  /* Test DNA storage data too. */
  layer_b.prepare_for_dna_write();
  EXPECT_EQ(layer_b.frames_storage.num, 3);
  EXPECT_EQ(layer_b.frames_storage.values[0].drawing_index, 0);
  EXPECT_EQ(layer_b.frames_storage.values[1].drawing_index, 1);
  EXPECT_EQ(layer_b.frames_storage.values[2].drawing_index, 2);

  BKE_id_free(nullptr, grease_pencil);
}

TEST(greasepencil, remove_drawings_with_no_users)
{
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(
      BKE_id_new_nomain(ID_GP, "Grease Pencil test"));

  /* Test drawing index correctness: Removing users from drawings should remove those drawings, and
   * all index references should get updated to match the changed drawing indices. */

  grease_pencil->add_empty_drawings(5);

  Layer &layer_a = grease_pencil->add_layer("LayerA");
  layer_a.add_frame(10, 0);
  layer_a.add_frame(20, 1);
  layer_a.add_frame(30, 2);
  Layer &layer_b = grease_pencil->add_layer("LayerB");
  layer_b.add_frame(10, 3);
  layer_b.add_frame(30, 4);

  EXPECT_EQ(layer_a.frames().size(), 3);
  EXPECT_EQ(layer_a.frames().lookup(10).drawing_index, 0);
  EXPECT_EQ(layer_a.frames().lookup(20).drawing_index, 1);
  EXPECT_EQ(layer_a.frames().lookup(30).drawing_index, 2);
  EXPECT_EQ(layer_b.frames().size(), 2);
  EXPECT_EQ(layer_b.frames().lookup(10).drawing_index, 3);
  EXPECT_EQ(layer_b.frames().lookup(30).drawing_index, 4);
  /* Test DNA storage data too. */
  layer_a.prepare_for_dna_write();
  layer_b.prepare_for_dna_write();
  EXPECT_EQ(layer_a.frames_storage.num, 3);
  EXPECT_EQ(layer_a.frames_storage.values[0].drawing_index, 0);
  EXPECT_EQ(layer_a.frames_storage.values[1].drawing_index, 1);
  EXPECT_EQ(layer_a.frames_storage.values[2].drawing_index, 2);
  EXPECT_EQ(layer_b.frames_storage.num, 2);
  EXPECT_EQ(layer_b.frames_storage.values[0].drawing_index, 3);
  EXPECT_EQ(layer_b.frames_storage.values[1].drawing_index, 4);

  /* Drawings 0,1,2 get removed, drawings 3,4 move up (order changes). */
  grease_pencil->remove_layer(layer_a);
  EXPECT_EQ(layer_b.frames().size(), 2);
  EXPECT_EQ(layer_b.frames().lookup(10).drawing_index, 1);
  EXPECT_EQ(layer_b.frames().lookup(30).drawing_index, 0);
  /* Test DNA storage data too. */
  layer_b.prepare_for_dna_write();
  EXPECT_EQ(layer_b.frames_storage.num, 2);
  EXPECT_EQ(layer_b.frames_storage.values[0].drawing_index, 1);
  EXPECT_EQ(layer_b.frames_storage.values[1].drawing_index, 0);

  BKE_id_free(nullptr, grease_pencil);
}

}  // namespace blender::bke::greasepencil::tests
