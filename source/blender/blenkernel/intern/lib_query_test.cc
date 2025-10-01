/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "CLG_log.h"

#include "GHOST_Path-api.hh"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "RNA_define.hh"

#include "BKE_appdir.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh.h"
#include "BKE_node.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "IMB_imbuf.hh"

#include "ED_node.hh"

namespace blender::bke::tests {

class TestData {
 public:
  Main *bmain = nullptr;
  bContext *C = nullptr;

  TestData()
  {
    if (this->bmain == nullptr) {
      this->bmain = BKE_main_new();
      G.main = this->bmain;
    }

    if (this->C == nullptr) {
      this->C = CTX_create();
      CTX_data_main_set(this->C, this->bmain);
    }
  }

  ~TestData()
  {
    if (this->bmain != nullptr) {
      BKE_main_free(this->bmain);
      this->bmain = nullptr;
      G.main = nullptr;
    }

    if (this->C != nullptr) {
      CTX_free(this->C);
      this->C = nullptr;
    }
  }
};

class LibQueryTest : public ::testing::Test {

 protected:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
    RNA_init();
    bke::node_system_init();
    BKE_appdir_init();
    IMB_init();
    BKE_materials_init();
  }

  static void TearDownTestSuite()
  {
    BKE_materials_exit();
    bke::node_system_exit();
    RNA_exit();
    IMB_exit();
    BKE_appdir_exit();
    GHOST_DisposeSystemPaths();
    CLG_exit();
  }
};

class WholeIDTestData : public TestData {
 public:
  Scene *scene = nullptr;
  Object *object = nullptr;
  Object *target = nullptr;
  Mesh *mesh = nullptr;
  Material *material = nullptr;

  WholeIDTestData()
  {
    this->scene = BKE_scene_add(this->bmain, "IDLibQueryScene");
    CTX_data_scene_set(this->C, this->scene);

    this->object = BKE_object_add_only_object(this->bmain, OB_MESH, "IDLibQueryObject");
    this->target = BKE_object_add_only_object(this->bmain, OB_EMPTY, "IDLibQueryTarget");

    this->mesh = BKE_mesh_add(this->bmain, "IDLibQueryMesh");
    this->object->data = this->mesh;

    BKE_collection_object_add(this->bmain, this->scene->master_collection, this->object);
    BKE_collection_object_add(this->bmain, this->scene->master_collection, this->target);
  }
};

class IDSubDataTestData : public WholeIDTestData {
 public:
  bNode *node = nullptr;

  IDSubDataTestData()
  {
    /* Add a material that contains an embedded nodetree and assign a custom property to one of
     * its nodes. */
    this->material = BKE_material_add(this->bmain, "Material");
    ED_node_shader_default(this->C, this->bmain, &this->material->id);

    BKE_object_material_assign(
        this->bmain, this->object, this->material, this->object->actcol, BKE_MAT_ASSIGN_OBJECT);

    this->node = static_cast<bNode *>(this->material->nodetree->nodes.first);

    this->node->prop = bke::idprop::create_group("Node Custom Properties").release();
    IDP_AddToGroup(this->node->prop,
                   bke::idprop::create("ID Pointer", &this->target->id).release());
  }
  ~IDSubDataTestData()
  {
    BKE_id_free(this->bmain, &this->material->id);
  }
};

/* -------------------------------------------------------------------- */
/** \name Query Tests
 * \{ */

TEST_F(LibQueryTest, libquery_basic)
{
  WholeIDTestData context;

  ASSERT_NE(context.scene, nullptr);
  ASSERT_NE(context.object, nullptr);
  ASSERT_NE(context.target, nullptr);
  ASSERT_NE(context.mesh, nullptr);

  /* Reset all ID user-count to 0. */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    id_iter->us = 0;
  }
  FOREACH_MAIN_ID_END;

  /* Set an invalid user-count value to IDs directly used by the scene.
   * This includes these used by its embedded IDs, like the master collection, and the scene itself
   * (through the loop-back pointers of embedded IDs to their owner). */
  auto set_count = [](LibraryIDLinkCallbackData *cb_data) -> int {
    if (*(cb_data->id_pointer)) {
      (*(cb_data->id_pointer))->us = 42;
    }
    return IDWALK_RET_NOP;
  };
  BKE_library_foreach_ID_link(
      context.bmain, &context.scene->id, set_count, nullptr, IDWALK_READONLY);
  EXPECT_EQ(context.scene->id.us, 42);
  EXPECT_EQ(context.object->id.us, 42);
  EXPECT_EQ(context.target->id.us, 42);
  EXPECT_EQ(context.mesh->id.us, 0);

  /* Clear object's obdata mesh pointer. */
  auto clear_mesh_pointer = [](LibraryIDLinkCallbackData *cb_data) -> int {
    WholeIDTestData *test_data = static_cast<WholeIDTestData *>(cb_data->user_data);
    if (*(cb_data->id_pointer) == &test_data->mesh->id) {
      *(cb_data->id_pointer) = nullptr;
    }
    return IDWALK_RET_NOP;
  };
  BKE_library_foreach_ID_link(
      context.bmain, &context.object->id, clear_mesh_pointer, &context, IDWALK_NOP);
  EXPECT_EQ(context.object->data, nullptr);

#if 0 /* Does not work. */
  /* Modifying data when IDWALK_READONLY is set is forbidden. */
  context.object->data = context.mesh;
  EXPECT_BLI_ASSERT(BKE_library_foreach_ID_link(context.bmain,
                                                &context.scene->id,
                                                clear_mesh_pointer,
                                                &context.test_data,
                                                IDWALK_READONLY),
                    "");
#endif
}

TEST_F(LibQueryTest, libquery_recursive)
{
  IDSubDataTestData context;

  EXPECT_NE(context.scene, nullptr);
  EXPECT_NE(context.object, nullptr);
  EXPECT_NE(context.target, nullptr);
  EXPECT_NE(context.mesh, nullptr);

  /* Reset all ID user-count to 0. */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    id_iter->us = 0;
  }
  FOREACH_MAIN_ID_END;

  /* Set an invalid user-count value to all IDs used by the scene, recursively.
   * Here, it should mean all IDs in Main, including the scene itself
   * (because of the loop-back pointer from the embedded master collection to its scene owner). */
  auto set_count = [](LibraryIDLinkCallbackData *cb_data) -> int {
    if (*(cb_data->id_pointer)) {
      (*(cb_data->id_pointer))->us = 42;
    }
    return IDWALK_RET_NOP;
  };
  BKE_library_foreach_ID_link(
      context.bmain, &context.scene->id, set_count, nullptr, IDWALK_RECURSE);
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    EXPECT_EQ(id_iter->us, 42);
  }
  FOREACH_MAIN_ID_END;

  /* Reset all ID user-count to 0. */
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    id_iter->us = 0;
  }
  FOREACH_MAIN_ID_END;

  /* Recompute valid user counts for all IDs used by the scene, recursively. */
  auto compute_count = [](LibraryIDLinkCallbackData *cb_data) -> int {
    if (*(cb_data->id_pointer) && (cb_data->cb_flag & IDWALK_CB_USER) != 0) {
      (*(cb_data->id_pointer))->us++;
    }
    return IDWALK_RET_NOP;
  };
  BKE_library_foreach_ID_link(
      context.bmain, &context.scene->id, compute_count, nullptr, IDWALK_RECURSE);
  EXPECT_EQ(context.scene->id.us, 0);
  EXPECT_EQ(context.object->id.us, 1);
  /* Scene's master collection, and scene's compositor node IDProperty. Note that object constraint
   * is _not_ a reference-counting usage. */
  EXPECT_EQ(context.target->id.us, 2);
  EXPECT_EQ(context.mesh->id.us, 1);
}

TEST_F(LibQueryTest, libquery_subdata)
{
  IDSubDataTestData context;

  ASSERT_NE(context.scene, nullptr);
  ASSERT_NE(context.object, nullptr);
  ASSERT_NE(context.target, nullptr);
  ASSERT_NE(context.mesh, nullptr);
  ASSERT_NE(context.material, nullptr);

  /* Reset all ID user-count to 0. */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    id_iter->us = 0;
  }
  FOREACH_MAIN_ID_END;

  /* Set an invalid user-count value to all IDs used by one of the material's nodes. */
  auto set_count = [](LibraryIDLinkCallbackData *cb_data) -> int {
    if (*(cb_data->id_pointer)) {
      (*(cb_data->id_pointer))->us = 42;
    }
    return IDWALK_RET_NOP;
  };
  auto node_foreach_id = [&context](LibraryForeachIDData *data) {
    bke::node_node_foreach_id(context.node, data);
  };

  BKE_library_foreach_subdata_id(context.bmain,
                                 &context.material->id,
                                 &context.material->nodetree->id,
                                 node_foreach_id,
                                 set_count,
                                 nullptr,
                                 IDWALK_NOP);

  EXPECT_EQ(context.scene->id.us, 0);
  EXPECT_EQ(context.object->id.us, 0);
  /* The material's node-tree input node IDProperty uses the target object. */
  EXPECT_EQ(context.target->id.us, 42);
  EXPECT_EQ(context.mesh->id.us, 0);
}

/** \} */

}  // namespace blender::bke::tests
