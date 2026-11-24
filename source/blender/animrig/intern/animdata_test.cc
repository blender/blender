/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_animdata.hh"

#include "BKE_gtest_base.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"

#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_world_types.h"

#include "testing/testing.h"

namespace blender::animrig::tests {

/* A text fixture AnimDataTest already exists in blenkernel, and apparently GTest stores these in a
 * global namespace somewhere. */
class AnimrigAnimDataTest : public bke::BlenderGTestBase {
 public:
  Main *bmain;
  Material *material;
  World *world;

  void SetUp() override
  {
    bmain = BKE_main_new();
    material = BKE_id_new<Material>(bmain, "MAMaterial");

    world = BKE_id_new<World>(bmain, "WOWorld");
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(AnimrigAnimDataTest, find_related_ids_material)
{
  /* Check material itself. */
  {
    const Vector<ID *> related = find_related_ids(*bmain, material->id);
    EXPECT_TRUE(related.contains(&material->id));
    EXPECT_TRUE(related.contains(&material->nodetree->id));
  }

  /* Check its node tree. */
  {
    const Vector<ID *> related = find_related_ids(*bmain, material->nodetree->id);
    EXPECT_TRUE(related.contains(&material->id));
    EXPECT_TRUE(related.contains(&material->nodetree->id));
  }
}

TEST_F(AnimrigAnimDataTest, find_related_ids_world)
{
  ASSERT_NE(world->nodetree, nullptr);

  /* Check world itself. */
  {
    const Vector<ID *> related = find_related_ids(*bmain, world->id);
    EXPECT_TRUE(related.contains(&world->id));
    EXPECT_TRUE(related.contains(&world->nodetree->id));
  }

  /* Check its node tree. */
  {
    const Vector<ID *> related = find_related_ids(*bmain, world->nodetree->id);
    EXPECT_TRUE(related.contains(&world->id));
    EXPECT_TRUE(related.contains(&world->nodetree->id));
  }
}

}  // namespace blender::animrig::tests
