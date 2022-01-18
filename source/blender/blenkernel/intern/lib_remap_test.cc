/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2022 by Blender Foundation.
 */
#include "testing/testing.h"

#include "BLI_utildefines.h"

#include "CLG_log.h"

#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"

#include "ED_node.h"

#include "MEM_guardedalloc.h"

namespace blender::bke::tests {

class TestData {
 public:
  Main *bmain = nullptr;
  struct bContext *C = nullptr;

  virtual void setup()
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

  virtual void teardown()
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

class SceneTestData : public TestData {
 public:
  Scene *scene = nullptr;
  void setup() override
  {
    TestData::setup();
    scene = BKE_scene_add(bmain, "IDRemapScene");
    CTX_data_scene_set(C, scene);
  }
};

class CompositorTestData : public SceneTestData {
 public:
  bNodeTree *compositor_nodetree = nullptr;
  void setup() override
  {
    SceneTestData::setup();
    ED_node_composit_default(C, scene);
    compositor_nodetree = scene->nodetree;
  }
};

class MeshTestData : public TestData {
 public:
  Mesh *mesh = nullptr;

  void setup() override
  {
    TestData::setup();
    mesh = BKE_mesh_add(bmain, nullptr);
  }
};

class TwoMeshesTestData : public MeshTestData {
 public:
  Mesh *other_mesh = nullptr;

  void setup() override
  {
    MeshTestData::setup();
    other_mesh = BKE_mesh_add(bmain, nullptr);
  }
};

class MeshObjectTestData : public MeshTestData {
 public:
  Object *object;
  void setup() override
  {
    MeshTestData::setup();

    object = BKE_object_add_only_object(bmain, OB_MESH, nullptr);
    object->data = mesh;
  }
};

template<typename TestData> class Context {
 public:
  TestData test_data;

  Context()
  {
    CLG_init();
    BKE_idtype_init();
    RNA_init();
    BKE_node_system_init();
    BKE_appdir_init();
    IMB_init();

    test_data.setup();
  }

  ~Context()
  {
    test_data.teardown();

    BKE_node_system_exit();
    RNA_exit();
    IMB_exit();
    BKE_appdir_exit();
    CLG_exit();
  }
};

/* -------------------------------------------------------------------- */
/** \name Embedded IDs
 * \{ */

TEST(lib_remap, embedded_ids_can_not_be_remapped)
{
  Context<CompositorTestData> context;
  bNodeTree *other_tree = static_cast<bNodeTree *>(BKE_id_new_nomain(ID_NT, nullptr));

  EXPECT_NE(context.test_data.scene, nullptr);
  EXPECT_NE(context.test_data.compositor_nodetree, nullptr);
  EXPECT_EQ(context.test_data.compositor_nodetree, context.test_data.scene->nodetree);

  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.compositor_nodetree, other_tree, 0);

  EXPECT_EQ(context.test_data.compositor_nodetree, context.test_data.scene->nodetree);
  EXPECT_NE(context.test_data.scene->nodetree, other_tree);

  BKE_id_free(nullptr, other_tree);
}

TEST(lib_remap, embedded_ids_can_not_be_deleted)
{
  Context<CompositorTestData> context;

  EXPECT_NE(context.test_data.scene, nullptr);
  EXPECT_NE(context.test_data.compositor_nodetree, nullptr);
  EXPECT_EQ(context.test_data.compositor_nodetree, context.test_data.scene->nodetree);

  BKE_libblock_remap(context.test_data.bmain,
                     context.test_data.compositor_nodetree,
                     nullptr,
                     ID_REMAP_SKIP_NEVER_NULL_USAGE);

  EXPECT_EQ(context.test_data.compositor_nodetree, context.test_data.scene->nodetree);
  EXPECT_NE(context.test_data.scene->nodetree, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remap to self
 * \{ */

TEST(lib_remap, delete_when_remap_to_self_not_allowed)
{
  Context<TwoMeshesTestData> context;

  EXPECT_NE(context.test_data.mesh, nullptr);
  EXPECT_NE(context.test_data.other_mesh, nullptr);
  context.test_data.mesh->texcomesh = context.test_data.other_mesh;

  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.other_mesh, context.test_data.mesh, 0);

  EXPECT_EQ(context.test_data.mesh->texcomesh, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name User Reference Counting
 * \{ */

TEST(lib_remap, users_are_decreased_when_not_skipping_never_null)
{
  Context<MeshObjectTestData> context;

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);
  EXPECT_EQ(context.test_data.mesh->id.us, 1);

  /* This is an invalid situation, test case tests this in between value until we have a better
   * solution. */
  BKE_libblock_remap(context.test_data.bmain, context.test_data.mesh, nullptr, 0);
  EXPECT_EQ(context.test_data.mesh->id.us, 0);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_NE(context.test_data.object->data, nullptr);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);
}

TEST(lib_remap, users_are_same_when_skipping_never_null)
{
  Context<MeshObjectTestData> context;

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);
  EXPECT_EQ(context.test_data.mesh->id.us, 1);

  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.mesh, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.mesh->id.us, 1);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_NE(context.test_data.object->data, nullptr);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Never Null
 * \{ */

TEST(lib_remap, do_not_delete_when_cannot_unset)
{
  Context<MeshObjectTestData> context;

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);

  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.mesh, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_NE(context.test_data.object->data, nullptr);
}

TEST(lib_remap, force_never_null_usage)
{
  Context<MeshObjectTestData> context;

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);

  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.mesh, nullptr, ID_REMAP_FORCE_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.object->data, nullptr);
}

TEST(lib_remap, never_null_usage_flag_not_requested_on_delete)
{
  Context<MeshObjectTestData> context;

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);

  /* Never null usage isn't requested so the flag should not be set.*/
  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.mesh, nullptr, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_NE(context.test_data.object->data, nullptr);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);
}

TEST(lib_remap, never_null_usage_flag_requested_on_delete)
{
  Context<MeshObjectTestData> context;

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);

  /* Never null usage is requested so the flag should be set. */
  BKE_libblock_remap(context.test_data.bmain,
                     context.test_data.mesh,
                     nullptr,
                     ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_FLAG_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_NE(context.test_data.object->data, nullptr);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, LIB_TAG_DOIT);
}

TEST(lib_remap, never_null_usage_flag_not_requested_on_remap)
{
  Context<MeshObjectTestData> context;
  Mesh *other_mesh = BKE_mesh_add(context.test_data.bmain, nullptr);

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);

  /* Never null usage isn't requested so the flag should not be set.*/
  BKE_libblock_remap(
      context.test_data.bmain, context.test_data.mesh, other_mesh, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.object->data, other_mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);
}

TEST(lib_remap, never_null_usage_flag_requested_on_remap)
{
  Context<MeshObjectTestData> context;
  Mesh *other_mesh = BKE_mesh_add(context.test_data.bmain, nullptr);

  EXPECT_NE(context.test_data.object, nullptr);
  EXPECT_EQ(context.test_data.object->data, context.test_data.mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, 0);

  /* Never null usage is requested so the flag should be set. */
  BKE_libblock_remap(context.test_data.bmain,
                     context.test_data.mesh,
                     other_mesh,
                     ID_REMAP_SKIP_NEVER_NULL_USAGE | ID_REMAP_FLAG_NEVER_NULL_USAGE);
  EXPECT_EQ(context.test_data.object->data, other_mesh);
  EXPECT_EQ(context.test_data.object->id.tag & LIB_TAG_DOIT, LIB_TAG_DOIT);
}

/** \} */

}  // namespace blender::bke::tests
