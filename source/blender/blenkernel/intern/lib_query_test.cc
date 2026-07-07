/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "CLG_log.h"

#include "GHOST_ISystemPaths.hh"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"

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

#include "NOD_defaults.hh"

#include "IMB_imbuf.hh"

#include "BLI_index_range.hh"
#include "BLI_set.hh"

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
    GHOST_ISystemPaths::dispose();
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

  WholeIDTestData(const bool generate_cached_bmain_relations = false)
  {
    this->scene = BKE_scene_add(this->bmain, "IDLibQueryScene");
    CTX_data_scene_set(this->C, this->scene);

    this->object = BKE_object_add_only_object(this->bmain, OB_MESH, "IDLibQueryObject");
    this->target = BKE_object_add_only_object(this->bmain, OB_EMPTY, "IDLibQueryTarget");

    this->mesh = BKE_mesh_add(this->bmain, "IDLibQueryMesh");
    this->object->data = id_cast<ID *>(this->mesh);

    BKE_collection_object_add(this->bmain, this->scene->master_collection, this->object);
    BKE_collection_object_add(this->bmain, this->scene->master_collection, this->target);

    if (generate_cached_bmain_relations) {
      BKE_main_relations_create(this->bmain, 0);
    }
  }
};

class IDSubDataTestData : public WholeIDTestData {
 public:
  bNode *node = nullptr;

  IDSubDataTestData(const bool generate_cached_bmain_relations = false)
  {
    /* Add a material that contains an embedded nodetree and assign a custom property to one of
     * its nodes. */
    this->material = BKE_material_add(this->bmain, "Material");
    nodes::node_tree_shader_default(this->C, this->bmain, &this->material->id);

    BKE_object_material_assign(
        this->bmain, this->object, this->material, this->object->actcol, BKE_MAT_ASSIGN_OBJECT);

    this->node = static_cast<bNode *>(this->material->nodetree->nodes.first);

    this->node->prop = bke::idprop::create_group("Node Custom Properties").release();
    IDP_AddToGroup(this->node->prop,
                   bke::idprop::create("ID Pointer", &this->target->id).release());

    if (generate_cached_bmain_relations) {
      BKE_main_relations_create(this->bmain, 0);
    }
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
  /* Behavior should remain strictly identical, between foreach_id iterations over 'raw' ID data,
   * or the cached relationships stored in Main::relations. */
  WholeIDTestData context_raw = {false};
  WholeIDTestData context_cached = {true};

  for (const WholeIDTestData *context_p : {&context_raw, &context_cached}) {
    const WholeIDTestData &context = *context_p;

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
     * This includes these used by its embedded IDs, like the master collection, and the scene
     * itself (through the loop-back pointers of embedded IDs to their owner). */
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
    auto clear_mesh_pointer = [&context](LibraryIDLinkCallbackData *cb_data) -> int {
      if (*(cb_data->id_pointer) == &context.mesh->id) {
        *(cb_data->id_pointer) = nullptr;
      }
      return IDWALK_RET_NOP;
    };
    BKE_library_foreach_ID_link(
        context.bmain, &context.object->id, clear_mesh_pointer, nullptr, IDWALK_NOP);
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
}

TEST_F(LibQueryTest, libquery_recursive)
{
  /* Behavior should remain strictly identical, between foreach_id iterations over 'raw' ID data,
   * or the cached relationships stored in Main::relations. */
  IDSubDataTestData context_raw = {false};
  IDSubDataTestData context_cached = {true};

  for (const IDSubDataTestData *context_p : {&context_raw, &context_cached}) {
    const IDSubDataTestData &context = *context_p;

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
     * (because of the loop-back pointer from the embedded master collection to its scene owner).
     */
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
    /* Scene's master collection, and scene's compositor node IDProperty. Note that object
     * constraint is _not_ a reference-counting usage. */
    EXPECT_EQ(context.target->id.us, 2);
    EXPECT_EQ(context.mesh->id.us, 1);
  }
}

TEST_F(LibQueryTest, libquery_subdata)
{
  /* Behavior should remain strictly identical, between foreach_id iterations over 'raw' ID data,
   * or the cached relationships stored in Main::relations. */
  IDSubDataTestData context_raw = {false};
  IDSubDataTestData context_cached = {true};

  for (const IDSubDataTestData *context_p : {&context_raw, &context_cached}) {
    const IDSubDataTestData &context = *context_p;

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
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unused IDs detection tests.
 * \{ */

class UnusedIDsTestData : public TestData {
 public:
  Library *library = nullptr;
  Scene *scene = nullptr;
  World *world = nullptr;
  Object *object = nullptr;
  Object *target = nullptr;
  Mesh *mesh = nullptr;

  /* IDs that must never be detected as unused. */
  Set<ID *> used_ids;
  /* IDs that are 'immediately' unused (i.e. have `0` users refcounting). */
  Set<ID *> unused_ids;
  /* IDs that must only be detected as unused in the recursive search. */
  Set<ID *> unused_recursive_ids;

  UnusedIDsTestData()
  {
    /* Used data. */
    this->library = BKE_id_new<Library>(this->bmain, "IDLibQueryLibrary");
    used_ids.add(&this->library->id);
    id_us_ensure_real(&this->library->id);

    this->scene = BKE_scene_add(this->bmain, "IDLibQueryScene");
    used_ids.add(&this->scene->id);
    id_us_ensure_real(&this->scene->id);
    CTX_data_scene_set(this->C, this->scene);
    this->world = BKE_id_new<World>(this->bmain, "IDLibQueryWorld");
    used_ids.add(&this->world->id);
    scene->world = this->world;

    this->object = BKE_object_add_only_object(this->bmain, OB_MESH, "IDLibQueryObject");
    used_ids.add(&this->object->id);
    this->target = BKE_object_add_only_object(this->bmain, OB_EMPTY, "IDLibQueryTarget");
    used_ids.add(&this->target->id);

    this->mesh = BKE_id_new_in_lib<Mesh>(this->bmain, this->library, "IDLibQueryMesh");
    used_ids.add(&this->mesh->id);
    this->object->data = id_cast<ID *>(this->mesh);

    BKE_collection_object_add(this->bmain, this->scene->master_collection, this->object);
    BKE_collection_object_add(this->bmain, this->scene->master_collection, this->target);

    /* Unused data. */
    /* 'dummy' unused data. */
    Mesh *me0 = BKE_id_new_in_lib<Mesh>(this->bmain, this->library, "IDLibQueryMeshUnused");
    unused_ids.add(&me0->id);
    World *world0 = BKE_id_new<World>(this->bmain, "IDLibQueryWorldUnused");
    unused_ids.add(&world0->id);

    /* 'Fake User' IDs are always considered as used. */
    Object *ob0 = BKE_object_add_only_object(this->bmain, OB_EMPTY, "IDLibQueryObjectUnused");
    id_fake_user_set(&ob0->id);
    used_ids.add(&ob0->id);

    /* Libraries are never considered as unused by this code (`IDTYPE_FLAGS_NEVER_UNUSED`). */
    Library *li0 = BKE_id_new<Library>(this->bmain, "IDLibQueryLibraryUnused");
    used_ids.add(&li0->id);
    id_us_ensure_real(&li0->id);

    auto make_islands = [this](Library *lib_1, Library *lib_2) -> void {
      /* Disconnected sub-graphs of IDs. */
      Mesh *me1 = BKE_id_new_in_lib<Mesh>(this->bmain, lib_2, "IDLibQueryMeshUnused_Island_1");
      unused_recursive_ids.add(&me1->id);
      Mesh *me2 = BKE_id_new_in_lib<Mesh>(this->bmain, lib_1, "IDLibQueryMeshUnused2_Island_1");
      unused_recursive_ids.add(&me2->id);
      World *world1 = BKE_id_new_in_lib<World>(
          this->bmain, lib_2, "IDLibQueryWorldUnused_Island_1");
      unused_recursive_ids.add(&world1->id);
      Object *ob1 = BKE_id_new_in_lib<Object>(
          this->bmain, lib_1, "IDLibQueryObjectUnused_Island_1");
      ob1->type = OB_MESH;
      unused_recursive_ids.add(&ob1->id);
      /* Create the internal links keeping this isolated group 'used' from a refcounting
       * perspective. */
      ob1->data = id_cast<ID *>(me1);
      IDProperty *ob1_idp = IDP_EnsureProperties(&ob1->id);
      IDP_AddToGroup(ob1_idp, bke::idprop::create("Ob1ToMesh1", &me1->id).release());
      IDP_AddToGroup(ob1_idp, bke::idprop::create("Ob1ToMesh2", &me2->id).release());
      IDProperty *me2_idp = IDP_EnsureProperties(&me2->id);
      IDP_AddToGroup(me2_idp, bke::idprop::create("Mesh1ToWorld1", &world1->id).release());
      IDProperty *world1_idp = IDP_EnsureProperties(&world1->id);
      IDP_AddToGroup(world1_idp, bke::idprop::create("World1ToOb1", &ob1->id).release());
    };
    /* Disconnected graph of all local IDs. */
    make_islands(nullptr, nullptr);
    /* Disconnected graph of all linked IDs. */
    make_islands(this->library, this->library);
    /* Disconnected graph of mixed local and linked IDs. */
    make_islands(nullptr, this->library);

    /* Ensure all ID refcountings are valid. */
    BKE_main_id_refcount_recompute(this->bmain, false);
  }
};

TEST_F(LibQueryTest, libquery_unused_ids_direct)
{
  UnusedIDsTestData context;

  ASSERT_NE(context.library, nullptr);
  ASSERT_NE(context.scene, nullptr);
  ASSERT_NE(context.world, nullptr);
  ASSERT_NE(context.object, nullptr);
  ASSERT_NE(context.target, nullptr);
  ASSERT_NE(context.mesh, nullptr);

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    EXPECT_TRUE(context.used_ids.contains(id_iter) || context.unused_ids.contains(id_iter) ||
                context.unused_recursive_ids.contains(id_iter));
  }
  FOREACH_MAIN_ID_END;

  /* "Detect nothing" case. */
  LibQueryUnusedIDsData data;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);
  for (const int64_t index : IndexRange(INDEX_ID_MAX)) {
    EXPECT_EQ(data.num_local[index], 0);
    EXPECT_EQ(data.num_linked[index], 0);
    EXPECT_EQ(data.num_total[index], 0);
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;

  /* "Detect local directly unused IDs" case. */
  data = {};
  data.do_local_ids = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);

  for (const int64_t index : IndexRange(INDEX_ID_MAX)) {
    EXPECT_EQ(data.num_linked[index], 0);
  }
  EXPECT_EQ(data.num_local[INDEX_ID_WO], 1);
  EXPECT_EQ(data.num_local[INDEX_ID_WO], data.num_total[INDEX_ID_WO]);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL], 1);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL], data.num_total[INDEX_ID_NULL]);

  for (ID *id_iter : context.unused_ids) {
    EXPECT_TRUE(((id_iter->tag & ID_TAG_DOIT) != 0) != ID_IS_LINKED(id_iter));
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    if (!ID_IS_LINKED(id_iter) && context.unused_ids.contains(id_iter)) {
      continue;
    }
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;

  /* "Detect linked directly unused IDs" case. */
  data = {};
  data.do_linked_ids = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);

  for (const int64_t index : IndexRange(INDEX_ID_MAX)) {
    EXPECT_EQ(data.num_local[index], 0);
  }
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], data.num_total[INDEX_ID_ME]);
  EXPECT_EQ(data.num_linked[INDEX_ID_NULL], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_NULL], data.num_total[INDEX_ID_NULL]);

  for (ID *id_iter : context.unused_ids) {
    EXPECT_TRUE(((id_iter->tag & ID_TAG_DOIT) != 0) == ID_IS_LINKED(id_iter));
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    if (ID_IS_LINKED(id_iter) && context.unused_ids.contains(id_iter)) {
      continue;
    }
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;

  /* "Detect local & linked directly unused IDs" case. */
  data = {};
  data.do_local_ids = true;
  data.do_linked_ids = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);

  EXPECT_EQ(data.num_local[INDEX_ID_WO], 1);
  EXPECT_EQ(data.num_local[INDEX_ID_WO], data.num_total[INDEX_ID_WO]);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], data.num_total[INDEX_ID_ME]);
  EXPECT_EQ(data.num_linked[INDEX_ID_NULL], 1);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL] + data.num_linked[INDEX_ID_NULL],
            data.num_total[INDEX_ID_NULL]);

  for (ID *id_iter : context.unused_ids) {
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) != 0);
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    if (context.unused_ids.contains(id_iter)) {
      continue;
    }
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;
}

TEST_F(LibQueryTest, libquery_unused_ids_recursive)
{
  UnusedIDsTestData context;

  ASSERT_NE(context.library, nullptr);
  ASSERT_NE(context.scene, nullptr);
  ASSERT_NE(context.world, nullptr);
  ASSERT_NE(context.object, nullptr);
  ASSERT_NE(context.target, nullptr);
  ASSERT_NE(context.mesh, nullptr);

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    EXPECT_TRUE(context.used_ids.contains(id_iter) || context.unused_ids.contains(id_iter) ||
                context.unused_recursive_ids.contains(id_iter));
  }
  FOREACH_MAIN_ID_END;

  /* "Detect nothing" case. */
  LibQueryUnusedIDsData data;
  data.do_recursive = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);
  for (const int64_t index : IndexRange(INDEX_ID_MAX)) {
    EXPECT_EQ(data.num_local[index], 0);
    EXPECT_EQ(data.num_linked[index], 0);
    EXPECT_EQ(data.num_total[index], 0);
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;

  /* "Detect all local unused IDs" case. */
  data = {};
  data.do_recursive = true;
  data.do_local_ids = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);

  for (const int64_t index : IndexRange(INDEX_ID_MAX)) {
    EXPECT_EQ(data.num_linked[index], 0);
  }
  /* Data from the directly unused IDs, and the disconnected graphs. Note that the mixed graph is
   * not detected here, as some of its IDs are linked, therefore not considered, which generates
   * valid usages for the local IDs of that group. */
  EXPECT_EQ(data.num_local[INDEX_ID_WO], 2);
  EXPECT_EQ(data.num_local[INDEX_ID_WO], data.num_total[INDEX_ID_WO]);
  EXPECT_EQ(data.num_local[INDEX_ID_ME], 2);
  EXPECT_EQ(data.num_local[INDEX_ID_ME], data.num_total[INDEX_ID_ME]);
  EXPECT_EQ(data.num_local[INDEX_ID_OB], 1);
  EXPECT_EQ(data.num_local[INDEX_ID_OB], data.num_total[INDEX_ID_OB]);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL], 5);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL], data.num_total[INDEX_ID_NULL]);
  for (ID *id_iter : context.unused_ids) {
    /* All directly unused local IDs must be tagged, linked ones must remain untagged. */
    EXPECT_TRUE(((id_iter->tag & ID_TAG_DOIT) != 0) != ID_IS_LINKED(id_iter));
  }
  for (ID *id_iter : context.unused_recursive_ids) {
    /* All unused linked IDs must not be tagged. Some local ones also remain untagged. */
    EXPECT_TRUE(((id_iter->tag & ID_TAG_DOIT) == 0) || !ID_IS_LINKED(id_iter));
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    if (!ID_IS_LINKED(id_iter) && context.unused_ids.contains(id_iter)) {
      continue;
    }
    if (!ID_IS_LINKED(id_iter) && context.unused_recursive_ids.contains(id_iter)) {
      continue;
    }
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;

  /* "Detect all linked unused IDs" case. */
  data = {};
  data.do_recursive = true;
  data.do_linked_ids = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);

  for (const int64_t index : IndexRange(INDEX_ID_MAX)) {
    EXPECT_EQ(data.num_local[index], 0);
  }
  /* Data from the directly unused IDs, and the disconnected graphs. Note that the mixed graph is
   * not detected here, as some of its IDs are local, therefore not considered, which generates
   * valid usages for the linked IDs of that group. */
  EXPECT_EQ(data.num_linked[INDEX_ID_WO], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_WO], data.num_total[INDEX_ID_WO]);
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], 3);
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], data.num_total[INDEX_ID_ME]);
  EXPECT_EQ(data.num_linked[INDEX_ID_OB], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_OB], data.num_total[INDEX_ID_OB]);
  EXPECT_EQ(data.num_linked[INDEX_ID_NULL], 5);
  EXPECT_EQ(data.num_linked[INDEX_ID_NULL], data.num_total[INDEX_ID_NULL]);
  for (ID *id_iter : context.unused_ids) {
    /* All directly unused linked IDs must be tagged, local ones must remain untagged. */
    EXPECT_TRUE(((id_iter->tag & ID_TAG_DOIT) != 0) == ID_IS_LINKED(id_iter));
  }
  for (ID *id_iter : context.unused_recursive_ids) {
    /* All unused local IDs must not be tagged. Some linked ones also remain untagged. */
    EXPECT_TRUE(((id_iter->tag & ID_TAG_DOIT) == 0) || ID_IS_LINKED(id_iter));
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    if (ID_IS_LINKED(id_iter) && context.unused_ids.contains(id_iter)) {
      continue;
    }
    if (ID_IS_LINKED(id_iter) && context.unused_recursive_ids.contains(id_iter)) {
      continue;
    }
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;

  /* "Detect all local & linked unused IDs" case. */
  data = {};
  data.do_recursive = true;
  data.do_local_ids = true;
  data.do_linked_ids = true;

  BKE_lib_query_unused_ids_tag(context.bmain, ID_TAG_DOIT, data);

  /* Data from the directly unused IDs, and the disconnected graphs. Here IDs from all three
   * disconnected graphs must be tagged. */
  EXPECT_EQ(data.num_local[INDEX_ID_WO], 2);
  EXPECT_EQ(data.num_local[INDEX_ID_ME], 3);
  EXPECT_EQ(data.num_local[INDEX_ID_OB], 2);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL], 7);
  EXPECT_EQ(data.num_linked[INDEX_ID_WO], 2);
  EXPECT_EQ(data.num_linked[INDEX_ID_ME], 4);
  EXPECT_EQ(data.num_linked[INDEX_ID_OB], 1);
  EXPECT_EQ(data.num_linked[INDEX_ID_NULL], 7);

  EXPECT_EQ(data.num_local[INDEX_ID_WO] + data.num_linked[INDEX_ID_WO],
            data.num_total[INDEX_ID_WO]);
  EXPECT_EQ(data.num_local[INDEX_ID_ME] + data.num_linked[INDEX_ID_ME],
            data.num_total[INDEX_ID_ME]);
  EXPECT_EQ(data.num_local[INDEX_ID_OB] + data.num_linked[INDEX_ID_OB],
            data.num_total[INDEX_ID_OB]);
  EXPECT_EQ(data.num_local[INDEX_ID_NULL] + data.num_linked[INDEX_ID_NULL],
            data.num_total[INDEX_ID_NULL]);

  for (ID *id_iter : context.unused_ids) {
    /* All directly unused IDs must be tagged. */
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) != 0);
  }
  for (ID *id_iter : context.unused_recursive_ids) {
    /* All indirectly unused IDs must be tagged. */
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) != 0);
  }
  FOREACH_MAIN_ID_BEGIN (context.bmain, id_iter) {
    if (context.unused_ids.contains(id_iter)) {
      continue;
    }
    if (context.unused_recursive_ids.contains(id_iter)) {
      continue;
    }
    EXPECT_TRUE((id_iter->tag & ID_TAG_DOIT) == 0);
  }
  FOREACH_MAIN_ID_END;
}

/** \} */

}  // namespace blender::bke::tests
