/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "CLG_log.h"

/* Allow using `Scene->nodetree` because it's still relevant for backward compatibility. */
#define DNA_DEPRECATED_ALLOW
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_scene.hh"

#include "IMB_imbuf.hh"

#include "ED_node_c.hh"

#include "RNA_define.hh"

namespace blender::nodes::tests {

class NodeTest : public ::testing::Test {

 protected:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
    RNA_init();
    blender::bke::node_system_init();
    BKE_appdir_init();
    IMB_init();
    BKE_materials_init();
  }

  static void TearDownTestSuite()
  {
    BKE_materials_exit();
    bke::node_system_exit();
    RNA_exit();
    BKE_appdir_exit();
    IMB_exit();
    CLG_exit();
  }

  struct IteratorResult {
    Vector<bNodeTree *> node_trees;
    Vector<ID *> ids;
  };

  IteratorResult get_node_trees(Main *bmain)
  {
    IteratorResult iter_result;

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      iter_result.node_trees.append(ntree);
      iter_result.ids.append(id);
    }
    FOREACH_NODETREE_END;

    return iter_result;
  };
};

class TestData {
 public:
  Main *bmain = nullptr;
  bContext *C = nullptr;

  TestData()
  {
    if (bmain == nullptr) {
      bmain = BKE_main_new();
      G.main = bmain;
    }

    if (C == nullptr) {
      C = CTX_create();
      CTX_data_main_set(C, bmain);
    }
  }

  ~TestData()
  {
    if (bmain != nullptr) {
      BKE_main_free(bmain);
      bmain = nullptr;
      G.main = nullptr;
    }

    if (C != nullptr) {
      CTX_free(C);
      C = nullptr;
    }
  }
};

TEST_F(NodeTest, tree_iterator_empty)
{
  TestData context;

  IteratorResult iter_result = this->get_node_trees(context.bmain);

  EXPECT_EQ(iter_result.node_trees.size(), 0);
  EXPECT_EQ(iter_result.ids.size(), 0);
}

TEST_F(NodeTest, tree_iterator_1_mat)
{
  TestData context;

  Material *material = BKE_material_add(context.bmain, "Material");
  ED_node_shader_default(context.C, context.bmain, &material->id);

  IteratorResult iter_result = this->get_node_trees(context.bmain);

  ASSERT_EQ(iter_result.node_trees.size(), 1);
  ASSERT_EQ(iter_result.ids.size(), 1);

  EXPECT_EQ(GS(iter_result.ids[0]->name), ID_MA);
}

TEST_F(NodeTest, tree_iterator_scene_no_tree)
{
  TestData context;

  Material *material = BKE_material_add(context.bmain, "Material");
  ED_node_shader_default(context.C, context.bmain, &material->id);

  BKE_scene_add(context.bmain, "Scene");

  IteratorResult iter_result = this->get_node_trees(context.bmain);

  ASSERT_EQ(iter_result.node_trees.size(), 1);
  ASSERT_EQ(iter_result.ids.size(), 1);

  EXPECT_EQ(GS(iter_result.ids[0]->name), ID_MA);
}

TEST_F(NodeTest, tree_iterator_1mat_1scene)
{
  TestData context;
  const char SCENE_NAME[MAX_ID_NAME] = "Scene for testing";

  Material *material = BKE_material_add(context.bmain, "Material");
  ED_node_shader_default(context.C, context.bmain, &material->id);

  Scene *scene = BKE_scene_add(context.bmain, SCENE_NAME);
  /* Embedded compositing trees are deprecated, but still relevant for versioning/backward
   * compatibility. */
  scene->nodetree = bke::node_tree_add_tree_embedded(
      context.bmain, &scene->id, "compositing nodetree", "CompositorNodeTree");

  IteratorResult iter_result = this->get_node_trees(context.bmain);

  ASSERT_EQ(iter_result.node_trees.size(), 2);
  ASSERT_EQ(iter_result.ids.size(), 2);

  EXPECT_EQ(GS(iter_result.ids[1]->name), ID_MA);
  EXPECT_EQ(GS(iter_result.ids[0]->name), ID_SCE);
  EXPECT_STREQ(iter_result.ids[0]->name + 2, SCENE_NAME);

  /* `scene->nodetree` is not managed by the scene anymore, i.e. `scene_free_data()` doesn't free
   * its embedded node-trees, so we need to free it manually here. */
  bke::node_tree_free_embedded_tree(scene->nodetree);
  MEM_freeN(scene->nodetree);
  scene->nodetree = nullptr;
}

TEST_F(NodeTest, tree_iterator_1mat_3scenes)
{
  TestData context;
  const char SCENE_NAME_1[MAX_ID_NAME] = "Scene 1";
  const char SCENE_NAME_2[MAX_ID_NAME] = "Scene 2";
  const char SCENE_NAME_3[MAX_ID_NAME] = "Scene 3";
  const char NTREE_NAME[MAX_NAME] = "Test Composisiting Nodetree";
  /* Name is hard-coded in #ED_node_shader_default(). */
  const char MATERIAL_NTREE_NAME[MAX_NAME] = "Shader Nodetree";

  Material *material = BKE_material_add(context.bmain, "Material");
  ED_node_shader_default(context.C, context.bmain, &material->id);

  BKE_scene_add(context.bmain, SCENE_NAME_1);
  /* Note: no node tree for scene 1. */

  Scene *scene2 = BKE_scene_add(context.bmain, SCENE_NAME_2);
  scene2->nodetree = bke::node_tree_add_tree_embedded(
      context.bmain, &scene2->id, NTREE_NAME, "CompositorNodeTree");

  BKE_scene_add(context.bmain, SCENE_NAME_3);
  /* Also no node tree for scene 3. */

  IteratorResult iter_result = this->get_node_trees(context.bmain);

  ASSERT_EQ(iter_result.node_trees.size(), 2);
  ASSERT_EQ(iter_result.ids.size(), 2);

  /* Expect that scenes with no node-trees don't have side effects for node trees. */
  EXPECT_EQ(GS(iter_result.ids[0]->name), ID_SCE);
  EXPECT_STREQ(iter_result.ids[0]->name + 2, SCENE_NAME_2);
  EXPECT_STREQ(iter_result.node_trees[0]->id.name + 2, NTREE_NAME);

  EXPECT_EQ(GS(iter_result.ids[1]->name), ID_MA);
  EXPECT_STREQ(iter_result.node_trees[1]->id.name + 2, MATERIAL_NTREE_NAME);

  /* `scene->nodetree` is not managed by the scene anymore, i.e. `scene_free_data()` doesn't free
   * its embedded node-trees, so we need to free it manually here. */
  bke::node_tree_free_embedded_tree(scene2->nodetree);
  MEM_freeN(scene2->nodetree);
  scene2->nodetree = nullptr;
}

TEST_F(NodeTest, tree_iterator_1mat_1scene_2compositing_trees)
{
  TestData context;
  const char SCENE_NAME_1[MAX_ID_NAME - 2] = "Scene 1";
  const char NTREE_NAME_1[MAX_ID_NAME - 2] = "Test Composisiting Node Tree 1";
  const char NTREE_NAME_2[MAX_ID_NAME - 2] = "Test Composisiting Node Tree 2";
  const char MATERIAL_NTREE_NAME[MAX_NAME] = "Shader Nodetree";

  Material *material = BKE_material_add(context.bmain, "Material");
  ED_node_shader_default(context.C, context.bmain, &material->id);

  BKE_scene_add(context.bmain, SCENE_NAME_1);

  bke::node_tree_add_tree(context.bmain, NTREE_NAME_1, "CompositorNodeTree");
  bke::node_tree_add_tree(context.bmain, NTREE_NAME_2, "CompositorNodeTree");

  IteratorResult iter_result = this->get_node_trees(context.bmain);

  ASSERT_EQ(iter_result.node_trees.size(), 3);
  ASSERT_EQ(iter_result.ids.size(), 3);

  /* Iterator should return 2 compositing node trees and no scene node tree. */
  EXPECT_EQ(GS(iter_result.ids[0]->name), ID_NT);
  EXPECT_STREQ(iter_result.ids[0]->name + 2, NTREE_NAME_1);
  EXPECT_FALSE((iter_result.ids[0]->flag & ID_FLAG_EMBEDDED_DATA));

  EXPECT_EQ(GS(iter_result.ids[1]->name), ID_NT);
  EXPECT_STREQ(iter_result.ids[1]->name + 2, NTREE_NAME_2);
  EXPECT_FALSE((iter_result.ids[1]->flag & ID_FLAG_EMBEDDED_DATA));

  EXPECT_EQ(GS(iter_result.ids[2]->name), ID_MA);
  EXPECT_STREQ(iter_result.node_trees[2]->id.name + 2, MATERIAL_NTREE_NAME);
}

}  // namespace blender::nodes::tests
