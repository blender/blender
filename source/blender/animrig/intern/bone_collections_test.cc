/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "ANIM_bone_collections.hh"
#include "intern/bone_collections_internal.hh"

#include "testing/testing.h"

namespace blender::animrig::tests {

TEST(ANIM_bone_collections, bonecoll_new_free)
{
  BoneCollection *bcoll = ANIM_bonecoll_new("some name");
  EXPECT_NE(nullptr, bcoll);
  EXPECT_EQ("some name", std::string(bcoll->name));
  EXPECT_TRUE(BLI_listbase_is_empty(&bcoll->bones));
  EXPECT_EQ(BONE_COLLECTION_VISIBLE | BONE_COLLECTION_SELECTABLE |
                BONE_COLLECTION_ANCESTORS_VISIBLE,
            bcoll->flags);
  ANIM_bonecoll_free(bcoll);
}

TEST(ANIM_bone_collections, bonecoll_default_name)
{
  {
    BoneCollection *bcoll = ANIM_bonecoll_new("");
    EXPECT_EQ(DATA_("Bones"), std::string(bcoll->name));
    ANIM_bonecoll_free(bcoll);
  }

  {
    BoneCollection *bcoll = ANIM_bonecoll_new(nullptr);
    EXPECT_EQ(DATA_("Bones"), std::string(bcoll->name));
    ANIM_bonecoll_free(bcoll);
  }
}

class ArmatureBoneCollections : public testing::Test {
 protected:
  bArmature arm = {};
  Bone bone1 = {}, bone2 = {}, bone3 = {};
  Main *bmain;

  void SetUp() override
  {
    bmain = BKE_main_new();
    G_MAIN = bmain;
    STRNCPY(arm.id.name, "ARArmature");
    STRNCPY(bone1.name, "bone1");
    STRNCPY(bone2.name, "bone2");
    STRNCPY(bone3.name, "bone3");

    BLI_addtail(&arm.bonebase, &bone1);    /* bone1 is root bone. */
    BLI_addtail(&arm.bonebase, &bone2);    /* bone2 is root bone. */
    BLI_addtail(&bone2.childbase, &bone3); /* bone3 has bone2 as parent. */

    BKE_armature_bone_hash_make(&arm);
  }

  void TearDown() override
  {
    /* Avoid freeing the bones, as they are part of this struct and not owned by
     * the armature. */
    BLI_listbase_clear(&arm.bonebase);

    BKE_idtype_init();
    BKE_libblock_free_datablock(&arm.id, 0);

    BKE_main_free(bmain);
    G_MAIN = nullptr;
  }
};

TEST_F(ArmatureBoneCollections, armature_owned_collections)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "collection");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "collection");

  EXPECT_EQ(std::string("collection"), std::string(bcoll1->name));
  EXPECT_EQ(std::string("collection.001"), std::string(bcoll2->name));

  ANIM_armature_bonecoll_remove(&arm, bcoll1);
  ANIM_armature_bonecoll_remove(&arm, bcoll2);
}

TEST_F(ArmatureBoneCollections, collection_hierarchy_creation)
{
  /* Implicit root: */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "wortel");
  /* Explicit root: */
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "wortel", -1);

  ASSERT_EQ(0, armature_bonecoll_find_index(&arm, bcoll_root_0));
  ASSERT_EQ(1, armature_bonecoll_find_index(&arm, bcoll_root_1));

  /* Child of bcoll at index 0: */
  BoneCollection *bcoll_child_of_0 = ANIM_armature_bonecoll_new(&arm, "koter", 0);
  /* Child of bcoll at index 1: */
  BoneCollection *bcoll_child_of_1 = ANIM_armature_bonecoll_new(&arm, "koter", 1);

  ASSERT_EQ(4, arm.collection_array_num);
  EXPECT_EQ(0, armature_bonecoll_find_index(&arm, bcoll_root_0));
  EXPECT_EQ(1, armature_bonecoll_find_index(&arm, bcoll_root_1));
  EXPECT_EQ(2, armature_bonecoll_find_index(&arm, bcoll_child_of_0));
  EXPECT_EQ(3, armature_bonecoll_find_index(&arm, bcoll_child_of_1));

  /* Add another child of bcoll_root_0, which should push bcoll_child_of_1 further down the array.
   */
  BoneCollection *bcoll_another_child_of_0 = ANIM_armature_bonecoll_new(&arm, "koter", 0);
  ASSERT_EQ(5, arm.collection_array_num);
  EXPECT_EQ(0, armature_bonecoll_find_index(&arm, bcoll_root_0));
  EXPECT_EQ(1, armature_bonecoll_find_index(&arm, bcoll_root_1));
  EXPECT_EQ(2, armature_bonecoll_find_index(&arm, bcoll_child_of_0));
  EXPECT_EQ(3, armature_bonecoll_find_index(&arm, bcoll_another_child_of_0));
  EXPECT_EQ(4, armature_bonecoll_find_index(&arm, bcoll_child_of_1));

  /* Make sure the names remain unique within the entire Armature, and not just between siblings
   * (i.e. a unique 'path' is not strong enough). */
  EXPECT_EQ(std::string("wortel"), std::string(bcoll_root_0->name));
  EXPECT_EQ(std::string("wortel.001"), std::string(bcoll_root_1->name));
  EXPECT_EQ(std::string("koter"), std::string(bcoll_child_of_0->name));
  EXPECT_EQ(std::string("koter.001"), std::string(bcoll_child_of_1->name));
  EXPECT_EQ(std::string("koter.002"), std::string(bcoll_another_child_of_0->name));

  /* Test the internal hierarchy bookkeeping. */
  EXPECT_EQ(2, arm.collection_root_count);
  EXPECT_EQ(2, bcoll_root_0->child_count);
  EXPECT_EQ(1, bcoll_root_1->child_count);
  EXPECT_EQ(0, bcoll_child_of_0->child_count);
  EXPECT_EQ(0, bcoll_another_child_of_0->child_count);
  EXPECT_EQ(0, bcoll_child_of_1->child_count);

  EXPECT_EQ(2, bcoll_root_0->child_index);
  EXPECT_EQ(4, bcoll_root_1->child_index);
  EXPECT_EQ(0, bcoll_child_of_0->child_index);
  EXPECT_EQ(0, bcoll_another_child_of_0->child_index);
  EXPECT_EQ(0, bcoll_child_of_1->child_index);

  /* TODO: test with deeper hierarchy. */
}

TEST_F(ArmatureBoneCollections, collection_hierarchy_removal)
{
  /* Set up a small hierarchy. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r1_child0 = ANIM_armature_bonecoll_new(&arm, "r1_child0", 1);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(5, arm.collection_array[1]->child_index);
  ASSERT_EQ(0, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);

  ASSERT_EQ(3, arm.collection_array[0]->child_count);
  ASSERT_EQ(1, arm.collection_array[1]->child_count);
  ASSERT_EQ(0, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);

  /* Remove the middle child of root_0. */
  ANIM_armature_bonecoll_remove(&arm, bcoll_r0_child1);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(5, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(4, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);

  EXPECT_EQ(2, arm.collection_array[0]->child_count);
  EXPECT_EQ(1, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);

  /* Remove the first child of root_0. */
  ANIM_armature_bonecoll_remove(&arm, bcoll_r0_child0);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(4, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[3]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(3, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);

  EXPECT_EQ(1, arm.collection_array[0]->child_count);
  EXPECT_EQ(1, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);

  /* Remove root_1 itself, which should make its only child a new root. */
  ANIM_armature_bonecoll_remove(&arm, bcoll_root_1);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(3, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[2]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);

  EXPECT_EQ(1, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
}

TEST_F(ArmatureBoneCollections, collection_hierarchy_removal__more_complex_remove_inner_child)
{
  /* Set up a slightly bigger hierarchy. Contrary to the other tests these are
   * actually declared in array order. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);
  BoneCollection *bcoll_r0c0_child0 = ANIM_armature_bonecoll_new(&arm, "r0c0_child0", 2);
  BoneCollection *bcoll_r0c0_child1 = ANIM_armature_bonecoll_new(&arm, "r0c0_child1", 2);
  BoneCollection *bcoll_r0c0_child2 = ANIM_armature_bonecoll_new(&arm, "r0c0_child2", 2);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(8, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name); /* Children of root_0. */
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r0c0_child0->name, arm.collection_array[5]->name); /* Kids of r0_child0. */
  ASSERT_STREQ(bcoll_r0c0_child1->name, arm.collection_array[6]->name);
  ASSERT_STREQ(bcoll_r0c0_child2->name, arm.collection_array[7]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(0, arm.collection_array[1]->child_index);
  ASSERT_EQ(5, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);
  ASSERT_EQ(0, arm.collection_array[6]->child_index);
  ASSERT_EQ(0, arm.collection_array[7]->child_index);

  ASSERT_EQ(3, arm.collection_array[0]->child_count);
  ASSERT_EQ(0, arm.collection_array[1]->child_count);
  ASSERT_EQ(3, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);
  ASSERT_EQ(0, arm.collection_array[6]->child_count);
  ASSERT_EQ(0, arm.collection_array[7]->child_count);

  /* Remove bcoll_r0_child0, which should make all of its children a child of root_0. */
  ANIM_armature_bonecoll_remove(&arm, bcoll_r0_child0);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0c0_child0->name, arm.collection_array[2]->name); /* Children of root_0. */
  EXPECT_STREQ(bcoll_r0c0_child1->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0c0_child2->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[5]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[6]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  EXPECT_EQ(5, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  /* Remove root_0, which should make all of its children new roots. */
  ANIM_armature_bonecoll_remove(&arm, bcoll_root_0);

  ASSERT_EQ(6, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_r0c0_child0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_r0c0_child1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0c0_child2->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[5]->name);

  EXPECT_EQ(0, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(0, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
}

TEST_F(ArmatureBoneCollections, collection_hierarchy_removal__more_complex_remove_root)
{
  /* Set up a slightly bigger hierarchy. Contrary to the other tests these are
   * actually declared in array order. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);
  BoneCollection *bcoll_r0c0_child0 = ANIM_armature_bonecoll_new(&arm, "r0c0_child0", 2);
  BoneCollection *bcoll_r0c0_child1 = ANIM_armature_bonecoll_new(&arm, "r0c0_child1", 2);
  BoneCollection *bcoll_r0c0_child2 = ANIM_armature_bonecoll_new(&arm, "r0c0_child2", 2);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(8, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name); /* Children of root_0. */
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r0c0_child0->name, arm.collection_array[5]->name); /* Kids of r0_child0. */
  ASSERT_STREQ(bcoll_r0c0_child1->name, arm.collection_array[6]->name);
  ASSERT_STREQ(bcoll_r0c0_child2->name, arm.collection_array[7]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(0, arm.collection_array[1]->child_index);
  ASSERT_EQ(5, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);
  ASSERT_EQ(0, arm.collection_array[6]->child_index);
  ASSERT_EQ(0, arm.collection_array[7]->child_index);

  ASSERT_EQ(3, arm.collection_array[0]->child_count);
  ASSERT_EQ(0, arm.collection_array[1]->child_count);
  ASSERT_EQ(3, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);
  ASSERT_EQ(0, arm.collection_array[6]->child_count);
  ASSERT_EQ(0, arm.collection_array[7]->child_count);

  /* Remove root_0, which should make all of its children new roots. */
  ANIM_armature_bonecoll_remove(&arm, bcoll_root_0);

  ASSERT_EQ(4, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[2]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0c0_child0->name, arm.collection_array[4]->name); /* Kids of r0_child0. */
  ASSERT_STREQ(bcoll_r0c0_child1->name, arm.collection_array[5]->name);
  ASSERT_STREQ(bcoll_r0c0_child2->name, arm.collection_array[6]->name);

  EXPECT_EQ(4, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  EXPECT_EQ(3, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);
}

TEST_F(ArmatureBoneCollections, find_parent_index)
{
  /* Set up a small hierarchy. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r1_child0 = ANIM_armature_bonecoll_new(&arm, "r1_child0", 1);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0c0_child0 = ANIM_armature_bonecoll_new(&arm, "r0c0_child0", 2);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r0c0_child0->name, arm.collection_array[5]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(4, arm.collection_array[1]->child_index);
  ASSERT_EQ(5, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);

  ASSERT_EQ(2, arm.collection_array[0]->child_count);
  ASSERT_EQ(1, arm.collection_array[1]->child_count);
  ASSERT_EQ(1, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);

  EXPECT_EQ(-1, armature_bonecoll_find_parent_index(&arm, -1));
  EXPECT_EQ(-1, armature_bonecoll_find_parent_index(&arm, 500000));

  EXPECT_EQ(-1, armature_bonecoll_find_parent_index(&arm, 0));
  EXPECT_EQ(-1, armature_bonecoll_find_parent_index(&arm, 1));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 2));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 3));
  EXPECT_EQ(1, armature_bonecoll_find_parent_index(&arm, 4));
  EXPECT_EQ(2, armature_bonecoll_find_parent_index(&arm, 5));
}

TEST_F(ArmatureBoneCollections, collection_hierarchy_visibility)
{
  /* Set up a small hierarchy. */
  BoneCollection *bcoll_root0 = ANIM_armature_bonecoll_new(&arm, "root0");
  BoneCollection *bcoll_root1 = ANIM_armature_bonecoll_new(&arm, "root1");
  const int root0_index = armature_bonecoll_find_index(&arm, bcoll_root0);
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", root0_index);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", root0_index);
  const int child0_index = armature_bonecoll_find_index(&arm, bcoll_r0_child0);
  BoneCollection *bcoll_c0_child0 = ANIM_armature_bonecoll_new(&arm, "c0_child0", child0_index);

  /* Initially, all bone collections should be marked as visible. */
  EXPECT_TRUE(bcoll_root0->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_root1->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child0->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child1->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_c0_child0->flags & BONE_COLLECTION_VISIBLE);

  /* Initially, all bone collections should have visible ancestors. */
  EXPECT_TRUE(bcoll_root0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_root1->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child1->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_c0_child0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);

  /* Mark root_0 as invisible, this should also update its children. */
  ANIM_bonecoll_hide(&arm, bcoll_root0);

  EXPECT_FALSE(bcoll_root0->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_root1->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child0->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child1->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_c0_child0->flags & BONE_COLLECTION_VISIBLE);

  EXPECT_TRUE(bcoll_root0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_root1->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_FALSE(bcoll_r0_child0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_FALSE(bcoll_r0_child1->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_FALSE(bcoll_c0_child0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);

  /* Move r0_child0 to root1, that should change its BONE_COLLECTION_ANCESTORS_VISIBLE */
  const int root1_index = armature_bonecoll_find_index(&arm, bcoll_root1);
  armature_bonecoll_move_to_parent(&arm, child0_index, 0, root0_index, root1_index);

  EXPECT_FALSE(bcoll_root0->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_root1->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child0->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child1->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_TRUE(bcoll_c0_child0->flags & BONE_COLLECTION_VISIBLE);

  EXPECT_TRUE(bcoll_root0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_root1->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
  EXPECT_TRUE(bcoll_r0_child0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE)
      << "The child that was moved to a visible root should be affected";
  EXPECT_FALSE(bcoll_r0_child1->flags & BONE_COLLECTION_ANCESTORS_VISIBLE)
      << "The child that wasn't moved should not be affected.";
  EXPECT_TRUE(bcoll_c0_child0->flags & BONE_COLLECTION_ANCESTORS_VISIBLE)
      << "The grandchild that was indirectly moved to a visible root should be affected";

  /* Add a new child to root0, it should have the right flags. */
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", root0_index);
  EXPECT_TRUE(bcoll_r0_child2->flags & BONE_COLLECTION_VISIBLE);
  EXPECT_FALSE(bcoll_r0_child2->flags & BONE_COLLECTION_ANCESTORS_VISIBLE);
}

TEST_F(ArmatureBoneCollections, bones_assign_unassign)
{
  BoneCollection *bcoll = ANIM_armature_bonecoll_new(&arm, "collection");

  ANIM_armature_bonecoll_assign(bcoll, &bone1);
  ANIM_armature_bonecoll_assign(bcoll, &bone2);

  ASSERT_EQ(2, BLI_listbase_count(&bcoll->bones)) << "expecting two bones in collection";
  EXPECT_EQ(&bone1, static_cast<BoneCollectionMember *>(BLI_findlink(&bcoll->bones, 0))->bone);
  EXPECT_EQ(&bone2, static_cast<BoneCollectionMember *>(BLI_findlink(&bcoll->bones, 1))->bone);

  EXPECT_EQ(bcoll, static_cast<BoneCollectionReference *>(bone1.runtime.collections.first)->bcoll)
      << "expecting back-reference to collection in bone1 runtime data";
  EXPECT_EQ(bcoll, static_cast<BoneCollectionReference *>(bone2.runtime.collections.first)->bcoll)
      << "expecting back-reference to collection in bone2 runtime data";

  ANIM_armature_bonecoll_unassign(bcoll, &bone1);
  ANIM_armature_bonecoll_unassign(bcoll, &bone2);

  EXPECT_EQ(0, BLI_listbase_count(&bone1.runtime.collections))
      << "expecting back-references in bone1 runtime data to be cleared when unassigned";
  EXPECT_EQ(0, BLI_listbase_count(&bone2.runtime.collections))
      << "expecting back-references in bone2 runtime data to be cleared when unassigned";

  ANIM_armature_bonecoll_remove(&arm, bcoll);
}

TEST_F(ArmatureBoneCollections, bones_assign_remove)
{
  BoneCollection *bcoll = ANIM_armature_bonecoll_new(&arm, "collection");

  ANIM_armature_bonecoll_assign(bcoll, &bone1);
  ANIM_armature_bonecoll_assign(bcoll, &bone2);
  ANIM_armature_bonecoll_remove(&arm, bcoll);

  EXPECT_EQ(0, BLI_listbase_count(&bone1.runtime.collections))
      << "expecting back-references in bone1 runtime data to be cleared when the collection is "
         "removed";
  EXPECT_EQ(0, BLI_listbase_count(&bone2.runtime.collections))
      << "expecting back-references in bone2 runtime data to be cleared when the collection is "
         "removed";
}

TEST_F(ArmatureBoneCollections, active_set_clear_by_pointer)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "Bones 1");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "Bones 2");
  BoneCollection *bcoll3 = ANIM_bonecoll_new("Alien Bones");

  ANIM_armature_bonecoll_active_set(&arm, bcoll1);
  EXPECT_EQ(0, arm.runtime.active_collection_index);
  EXPECT_EQ(bcoll1, arm.runtime.active_collection);
  EXPECT_STREQ(bcoll1->name, arm.active_collection_name);

  ANIM_armature_bonecoll_active_set(&arm, nullptr);
  EXPECT_EQ(-1, arm.runtime.active_collection_index);
  EXPECT_EQ(nullptr, arm.runtime.active_collection);
  EXPECT_STREQ("", arm.active_collection_name);

  ANIM_armature_bonecoll_active_set(&arm, bcoll2);
  EXPECT_EQ(1, arm.runtime.active_collection_index);
  EXPECT_EQ(bcoll2, arm.runtime.active_collection);
  EXPECT_STREQ(bcoll2->name, arm.active_collection_name);

  ANIM_armature_bonecoll_active_set(&arm, bcoll3);
  EXPECT_EQ(-1, arm.runtime.active_collection_index);
  EXPECT_EQ(nullptr, arm.runtime.active_collection);
  EXPECT_STREQ("", arm.active_collection_name);

  ANIM_bonecoll_free(bcoll3);
}

TEST_F(ArmatureBoneCollections, active_set_clear_by_index)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "Bones 1");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "Bones 2");

  ANIM_armature_bonecoll_active_index_set(&arm, 0);
  EXPECT_EQ(0, arm.runtime.active_collection_index);
  EXPECT_EQ(bcoll1, arm.runtime.active_collection);
  EXPECT_STREQ(bcoll1->name, arm.active_collection_name);

  ANIM_armature_bonecoll_active_index_set(&arm, -1);
  EXPECT_EQ(-1, arm.runtime.active_collection_index);
  EXPECT_EQ(nullptr, arm.runtime.active_collection);
  EXPECT_STREQ("", arm.active_collection_name);

  ANIM_armature_bonecoll_active_index_set(&arm, 1);
  EXPECT_EQ(1, arm.runtime.active_collection_index);
  EXPECT_EQ(bcoll2, arm.runtime.active_collection);
  EXPECT_STREQ(bcoll2->name, arm.active_collection_name);

  ANIM_armature_bonecoll_active_index_set(&arm, 47);
  EXPECT_EQ(-1, arm.runtime.active_collection_index);
  EXPECT_EQ(nullptr, arm.runtime.active_collection);
  EXPECT_STREQ("", arm.active_collection_name);
}

TEST_F(ArmatureBoneCollections, bcoll_is_editable)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "Bones 1");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "Bones 2");

  EXPECT_EQ(0, bcoll1->flags & BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL);
  EXPECT_EQ(0, bcoll2->flags & BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL);

  EXPECT_TRUE(ANIM_armature_bonecoll_is_editable(&arm, bcoll1))
      << "Expecting local armature to be editable";

  /* Fake that the armature is linked from another blend file. */
  Library fake_lib = {};
  arm.id.lib = &fake_lib;
  EXPECT_FALSE(ANIM_armature_bonecoll_is_editable(&arm, bcoll1))
      << "Expecting local armature to not be editable";

  /* Fake that the armature is an override, but linked from another blend file. */
  IDOverrideLibrary fake_override = {};
  bArmature fake_reference = {};
  fake_override.reference = &fake_reference.id;
  arm.id.override_library = &fake_override;
  EXPECT_FALSE(ANIM_armature_bonecoll_is_editable(&arm, bcoll1))
      << "Expecting linked armature override to not be editable";

  /* Fake that the armature is a local override. */
  arm.id.lib = nullptr;
  bcoll2->flags |= BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL;
  EXPECT_FALSE(ANIM_armature_bonecoll_is_editable(&arm, bcoll1))
      << "Expecting linked bone collection on local armature override to not be editable";
  EXPECT_TRUE(ANIM_armature_bonecoll_is_editable(&arm, bcoll2))
      << "Expecting local bone collection on local armature override to be editable";
}

TEST_F(ArmatureBoneCollections, bcoll_move_to_index__roots)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "collection");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "collection");
  BoneCollection *bcoll3 = ANIM_armature_bonecoll_new(&arm, "collection");
  BoneCollection *bcoll4 = ANIM_armature_bonecoll_new(&arm, "collection");

  EXPECT_EQ(arm.collection_array[0], bcoll1);
  EXPECT_EQ(arm.collection_array[1], bcoll2);
  EXPECT_EQ(arm.collection_array[2], bcoll3);
  EXPECT_EQ(arm.collection_array[3], bcoll4);

  EXPECT_TRUE(ANIM_armature_bonecoll_move_to_index(&arm, 2, 1));

  EXPECT_EQ(arm.collection_array[0], bcoll1);
  EXPECT_EQ(arm.collection_array[1], bcoll3);
  EXPECT_EQ(arm.collection_array[2], bcoll2);
  EXPECT_EQ(arm.collection_array[3], bcoll4);

  EXPECT_TRUE(ANIM_armature_bonecoll_move_to_index(&arm, 0, 3));

  EXPECT_EQ(arm.collection_array[0], bcoll3);
  EXPECT_EQ(arm.collection_array[1], bcoll2);
  EXPECT_EQ(arm.collection_array[2], bcoll4);
  EXPECT_EQ(arm.collection_array[3], bcoll1);

  /* Out of bounds should not be accepted. */
  EXPECT_FALSE(ANIM_armature_bonecoll_move_to_index(&arm, 0, 327));

  EXPECT_EQ(arm.collection_array[0], bcoll3);
  EXPECT_EQ(arm.collection_array[1], bcoll2);
  EXPECT_EQ(arm.collection_array[2], bcoll4);
  EXPECT_EQ(arm.collection_array[3], bcoll1);
}

TEST_F(ArmatureBoneCollections, bcoll_move_to_index__siblings)
{
  BoneCollection *root = ANIM_armature_bonecoll_new(&arm, "root");
  BoneCollection *child0 = ANIM_armature_bonecoll_new(&arm, "child0", 0);
  BoneCollection *child1 = ANIM_armature_bonecoll_new(&arm, "child1", 0);
  BoneCollection *child2 = ANIM_armature_bonecoll_new(&arm, "child2", 0);
  BoneCollection *child1_0 = ANIM_armature_bonecoll_new(&arm, "child1_0", 2);

  ASSERT_STREQ(root->name, arm.collection_array[0]->name);
  ASSERT_STREQ(child0->name, arm.collection_array[1]->name);
  ASSERT_STREQ(child1->name, arm.collection_array[2]->name);
  ASSERT_STREQ(child2->name, arm.collection_array[3]->name);
  ASSERT_STREQ(child1_0->name, arm.collection_array[4]->name);

  /* Move child2 to child0, i.e. a move 'to the left'. */
  EXPECT_TRUE(ANIM_armature_bonecoll_move_to_index(&arm, 3, 1));

  EXPECT_STREQ(root->name, arm.collection_array[0]->name);
  EXPECT_STREQ(child2->name, arm.collection_array[1]->name);
  EXPECT_STREQ(child0->name, arm.collection_array[2]->name);
  EXPECT_STREQ(child1->name, arm.collection_array[3]->name);
  EXPECT_STREQ(child1_0->name, arm.collection_array[4]->name);

  /* Move child2 to child1, i.e. a move 'to the right'. */
  EXPECT_TRUE(ANIM_armature_bonecoll_move_to_index(&arm, 1, 3));

  EXPECT_STREQ(root->name, arm.collection_array[0]->name);
  EXPECT_STREQ(child0->name, arm.collection_array[1]->name);
  EXPECT_STREQ(child1->name, arm.collection_array[2]->name);
  EXPECT_STREQ(child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(child1_0->name, arm.collection_array[4]->name);

  /* Move child2 to root, should not be allowed. */
  EXPECT_FALSE(ANIM_armature_bonecoll_move_to_index(&arm, 3, 0));

  EXPECT_STREQ(root->name, arm.collection_array[0]->name);
  EXPECT_STREQ(child0->name, arm.collection_array[1]->name);
  EXPECT_STREQ(child1->name, arm.collection_array[2]->name);
  EXPECT_STREQ(child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(child1_0->name, arm.collection_array[4]->name);

  /* Move child1_0 to child_2, should not be allowed. */
  EXPECT_FALSE(ANIM_armature_bonecoll_move_to_index(&arm, 4, 3));

  EXPECT_STREQ(root->name, arm.collection_array[0]->name);
  EXPECT_STREQ(child0->name, arm.collection_array[1]->name);
  EXPECT_STREQ(child1->name, arm.collection_array[2]->name);
  EXPECT_STREQ(child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(child1_0->name, arm.collection_array[4]->name);
}

TEST_F(ArmatureBoneCollections, bcoll_move_to_parent)
{
  /* Set up a small hierarchy. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r1_child0 = ANIM_armature_bonecoll_new(&arm, "r1_child0", 1);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(5, arm.collection_array[1]->child_index);
  ASSERT_EQ(0, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);

  ASSERT_EQ(3, arm.collection_array[0]->child_count);
  ASSERT_EQ(1, arm.collection_array[1]->child_count);
  ASSERT_EQ(0, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);

  /* Move the middle child of root_0 to root_1. */
  EXPECT_EQ(5, armature_bonecoll_move_to_parent(&arm, 3, bcoll_root_1->child_count, 0, 1));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[5]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(4, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(2, arm.collection_array[0]->child_count);
  EXPECT_EQ(2, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);

  /* Move the first child of root_1 to root_0. This shouldn't change its index. */
  EXPECT_EQ(4, armature_bonecoll_move_to_parent(&arm, 4, bcoll_root_0->child_count, 1, 0));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[5]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(5, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(3, arm.collection_array[0]->child_count);
  EXPECT_EQ(1, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);

  /* Move the final child of root_1 to root_0. This shouldn't change its index
   * again, but leave root_1 without children. */
  EXPECT_EQ(5, armature_bonecoll_move_to_parent(&arm, 5, bcoll_root_0->child_count, 1, 0));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[5]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(4, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);

  /* Move the first child of root_0 (bcoll_r0_child0) to bcoll_r0_child2. */
  EXPECT_EQ(5, armature_bonecoll_move_to_parent(&arm, 2, bcoll_r0_child2->child_count, 0, 3));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[5]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(5, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(3, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(1, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
}

TEST_F(ArmatureBoneCollections, bcoll_move_to_parent__root_unroot)
{
  /* Set up a small hierarchy. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r1_child0 = ANIM_armature_bonecoll_new(&arm, "r1_child0", 1);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(5, arm.collection_array[1]->child_index);
  ASSERT_EQ(0, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);

  ASSERT_EQ(3, arm.collection_array[0]->child_count);
  ASSERT_EQ(1, arm.collection_array[1]->child_count);
  ASSERT_EQ(0, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);

  /* Make a leaf node (bcoll_r0_child1) a root. */
  EXPECT_EQ(2, armature_bonecoll_move_to_parent(&arm, 3, arm.collection_root_count, 0, -1));

  ASSERT_EQ(3, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[2]->name); /* Became a root. */
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  EXPECT_EQ(3, arm.collection_array[0]->child_index);
  EXPECT_EQ(5, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(2, arm.collection_array[0]->child_count);
  EXPECT_EQ(1, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);

  /* Make a root node (root_1) a child of root_0. */
  EXPECT_EQ(4, armature_bonecoll_move_to_parent(&arm, 1, bcoll_root_0->child_count, -1, 0));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(6, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[1]->name); /* Actually a root. */
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[4]->name); /* Became a child. */
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  EXPECT_EQ(2, arm.collection_array[0]->child_index);
  EXPECT_EQ(0, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(5, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  EXPECT_EQ(3, arm.collection_array[0]->child_count);
  EXPECT_EQ(0, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(1, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);

  /* TODO: test with circular parenthood. */
}

TEST_F(ArmatureBoneCollections, bcoll_move_to_parent__within_siblings)
{
  /* Set up a small hierarchy. */
  auto *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  auto *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  auto *bcoll_r1_child0 = ANIM_armature_bonecoll_new(&arm, "r1_child0", 1);
  auto *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  auto *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  auto *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);
  auto *bcoll_r0_child3 = ANIM_armature_bonecoll_new(&arm, "r0_child3", 0);

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r1_child0->name, arm.collection_array[2]->name); /* Children root_1. */
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[3]->name); /* Children root_0. */
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[5]->name);
  ASSERT_STREQ(bcoll_r0_child3->name, arm.collection_array[6]->name);

  ASSERT_EQ(3, arm.collection_array[0]->child_index);
  ASSERT_EQ(2, arm.collection_array[1]->child_index);
  ASSERT_EQ(0, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);
  ASSERT_EQ(0, arm.collection_array[6]->child_index);

  ASSERT_EQ(4, arm.collection_array[0]->child_count);
  ASSERT_EQ(1, arm.collection_array[1]->child_count);
  ASSERT_EQ(0, arm.collection_array[2]->child_count);
  ASSERT_EQ(0, arm.collection_array[3]->child_count);
  ASSERT_EQ(0, arm.collection_array[4]->child_count);
  ASSERT_EQ(0, arm.collection_array[5]->child_count);
  ASSERT_EQ(0, arm.collection_array[6]->child_count);

  /* First half of the test, move 3 children from root_1 to root_0. */

  /* Move r0_child0 to become 1st child of root_1, before r1_child0. */
  EXPECT_EQ(2,
            armature_bonecoll_move_to_parent(&arm,
                                             3, /* From index. */
                                             0, /* To child number. */
                                             0, /* From parent. */
                                             1  /* To parent. */
                                             ));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name); /* Children root_1. */
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[4]->name); /* Children root_0. */
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[5]->name);
  EXPECT_STREQ(bcoll_r0_child3->name, arm.collection_array[6]->name);

  EXPECT_EQ(4, arm.collection_array[0]->child_index);
  EXPECT_EQ(2, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_index);

  EXPECT_EQ(3, arm.collection_array[0]->child_count);
  EXPECT_EQ(2, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  /* Move r0_child1 to become the 2nd child of root_1. */
  EXPECT_EQ(3,
            armature_bonecoll_move_to_parent(&arm,
                                             4, /* From index. */
                                             1, /* To child number. */
                                             0, /* From parent. */
                                             1  /* To parent. */
                                             ));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name); /* Children root_1. */
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[5]->name); /* Children root_0. */
  EXPECT_STREQ(bcoll_r0_child3->name, arm.collection_array[6]->name);

  EXPECT_EQ(5, arm.collection_array[0]->child_index);
  EXPECT_EQ(2, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_index);

  EXPECT_EQ(2, arm.collection_array[0]->child_count);
  EXPECT_EQ(3, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  /* Move r0_child3 to become the last child of root_1. */
  EXPECT_EQ(5,
            armature_bonecoll_move_to_parent(&arm,
                                             6, /* From index. */
                                             3, /* To child number. */
                                             0, /* From parent. */
                                             1  /* To parent. */
                                             ));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name); /* Children root_1. */
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child3->name, arm.collection_array[5]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[6]->name); /* Children root_0. */

  EXPECT_EQ(6, arm.collection_array[0]->child_index);
  EXPECT_EQ(2, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_index);

  EXPECT_EQ(1, arm.collection_array[0]->child_count);
  EXPECT_EQ(4, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  /* 2nd half of the test: move the children back to root_0 to test moving in
   * the other direction. */

  /* Move r0_child3 to become the first child of root_0. */
  EXPECT_EQ(5,
            armature_bonecoll_move_to_parent(&arm,
                                             5, /* From index. */
                                             0, /* To child number. */
                                             1, /* From parent. */
                                             0  /* To parent. */
                                             ));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name); /* Children root_1. */
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child3->name, arm.collection_array[5]->name); /* Children root_0. */
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[6]->name);

  EXPECT_EQ(5, arm.collection_array[0]->child_index);
  EXPECT_EQ(2, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_index);

  EXPECT_EQ(2, arm.collection_array[0]->child_count);
  EXPECT_EQ(3, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  /* Move r0_child0 to become the last child of root_0. */
  EXPECT_EQ(6,
            armature_bonecoll_move_to_parent(&arm,
                                             2, /* From index. */
                                             2, /* To child number. */
                                             1, /* From parent. */
                                             0  /* To parent. */
                                             ));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[2]->name); /* Children root_1. */
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0_child3->name, arm.collection_array[4]->name); /* Children root_0. */
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[5]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[6]->name);

  EXPECT_EQ(4, arm.collection_array[0]->child_index);
  EXPECT_EQ(2, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_index);

  EXPECT_EQ(3, arm.collection_array[0]->child_count);
  EXPECT_EQ(2, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);

  /* Move r0_child1 to become the 3rd child of root_0. */
  EXPECT_EQ(5,
            armature_bonecoll_move_to_parent(&arm,
                                             2, /* From index. */
                                             2, /* To child number. */
                                             1, /* From parent. */
                                             0  /* To parent. */
                                             ));

  ASSERT_EQ(2, arm.collection_root_count);
  ASSERT_EQ(7, arm.collection_array_num);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[2]->name); /* Children root_1. */
  EXPECT_STREQ(bcoll_r0_child3->name, arm.collection_array[3]->name); /* Children root_0. */
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[5]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[6]->name);

  EXPECT_EQ(3, arm.collection_array[0]->child_index);
  EXPECT_EQ(2, arm.collection_array[1]->child_index);
  EXPECT_EQ(0, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
  EXPECT_EQ(0, arm.collection_array[6]->child_index);

  EXPECT_EQ(4, arm.collection_array[0]->child_count);
  EXPECT_EQ(1, arm.collection_array[1]->child_count);
  EXPECT_EQ(0, arm.collection_array[2]->child_count);
  EXPECT_EQ(0, arm.collection_array[3]->child_count);
  EXPECT_EQ(0, arm.collection_array[4]->child_count);
  EXPECT_EQ(0, arm.collection_array[5]->child_count);
  EXPECT_EQ(0, arm.collection_array[6]->child_count);
}

TEST_F(ArmatureBoneCollections, internal__bonecolls_rotate_block)
{
  /* Set up a small hierarchy. */
  BoneCollection *bcoll_root_0 = ANIM_armature_bonecoll_new(&arm, "root_0");
  BoneCollection *bcoll_root_1 = ANIM_armature_bonecoll_new(&arm, "root_1");
  BoneCollection *bcoll_r0_child0 = ANIM_armature_bonecoll_new(&arm, "r0_child0", 0);
  BoneCollection *bcoll_r1_child0 = ANIM_armature_bonecoll_new(&arm, "r1_child0", 1);
  BoneCollection *bcoll_r0_child1 = ANIM_armature_bonecoll_new(&arm, "r0_child1", 0);
  BoneCollection *bcoll_r0_child2 = ANIM_armature_bonecoll_new(&arm, "r0_child2", 0);

  /* The tests below compare the collection names, instead of their pointers, so
   * that we get human-readable messages on failure. */

  /* Unnecessary assertions, just to make it easier to understand in which order
   * the array starts out. */
  ASSERT_EQ(6, arm.collection_array_num);
  ASSERT_STREQ(bcoll_root_0->name, arm.collection_array[0]->name);
  ASSERT_STREQ(bcoll_root_1->name, arm.collection_array[1]->name);
  ASSERT_STREQ(bcoll_r0_child0->name, arm.collection_array[2]->name);
  ASSERT_STREQ(bcoll_r0_child1->name, arm.collection_array[3]->name);
  ASSERT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  ASSERT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  ASSERT_EQ(2, arm.collection_array[0]->child_index);
  ASSERT_EQ(5, arm.collection_array[1]->child_index);
  ASSERT_EQ(0, arm.collection_array[2]->child_index);
  ASSERT_EQ(0, arm.collection_array[3]->child_index);
  ASSERT_EQ(0, arm.collection_array[4]->child_index);
  ASSERT_EQ(0, arm.collection_array[5]->child_index);

  /* Move [0,1,2] to [1,2,3]. */
  internal::bonecolls_rotate_block(&arm, 0, 3, 1);
  ASSERT_EQ(6, arm.collection_array_num) << "array size should not change";
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[5]->name);

  EXPECT_EQ(0, arm.collection_array[0]->child_index);
  EXPECT_EQ(3, arm.collection_array[1]->child_index);
  EXPECT_EQ(5, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);

  /* Move [4,5] to [3,4]. */
  internal::bonecolls_rotate_block(&arm, 4, 2, -1);
  ASSERT_EQ(6, arm.collection_array_num) << "array size should not change";
  EXPECT_STREQ(bcoll_r0_child1->name, arm.collection_array[0]->name);
  EXPECT_STREQ(bcoll_root_0->name, arm.collection_array[1]->name);
  EXPECT_STREQ(bcoll_root_1->name, arm.collection_array[2]->name);
  EXPECT_STREQ(bcoll_r0_child2->name, arm.collection_array[3]->name);
  EXPECT_STREQ(bcoll_r1_child0->name, arm.collection_array[4]->name);
  EXPECT_STREQ(bcoll_r0_child0->name, arm.collection_array[5]->name);

  EXPECT_EQ(0, arm.collection_array[0]->child_index);
  EXPECT_EQ(3, arm.collection_array[1]->child_index);
  EXPECT_EQ(4, arm.collection_array[2]->child_index);
  EXPECT_EQ(0, arm.collection_array[3]->child_index);
  EXPECT_EQ(0, arm.collection_array[4]->child_index);
  EXPECT_EQ(0, arm.collection_array[5]->child_index);
}

class ArmatureBoneCollectionsTestList : public testing::Test {
 protected:
  bArmature arm = {};

  BoneCollection *root = nullptr;
  BoneCollection *child0 = nullptr;
  BoneCollection *child1 = nullptr;
  BoneCollection *child2 = nullptr;
  BoneCollection *child1_0 = nullptr;

  void SetUp() override
  {
    STRNCPY(arm.id.name, "ARArmature");

    root = ANIM_armature_bonecoll_new(&arm, "root");
    child0 = ANIM_armature_bonecoll_new(&arm, "child0", 0);
    child1 = ANIM_armature_bonecoll_new(&arm, "child1", 0);
    child2 = ANIM_armature_bonecoll_new(&arm, "child2", 0);
    child1_0 = ANIM_armature_bonecoll_new(&arm, "child1_0", 2);

    ASSERT_STREQ(root->name, arm.collection_array[0]->name);
    ASSERT_STREQ(child0->name, arm.collection_array[1]->name);
    ASSERT_STREQ(child1->name, arm.collection_array[2]->name);
    ASSERT_STREQ(child2->name, arm.collection_array[3]->name);
    ASSERT_STREQ(child1_0->name, arm.collection_array[4]->name);
  }

  void TearDown() override
  {
    BKE_idtype_init();
    BKE_libblock_free_datablock(&arm.id, 0);
  }

  testing::AssertionResult expect_bcolls(std::vector<std::string> expect_names)
  {
    std::vector<std::string> actual_names;
    for (const BoneCollection *bcoll : arm.collections_span()) {
      actual_names.emplace_back(bcoll->name);
    }

    if (expect_names == actual_names) {
      return testing::AssertionSuccess();
    }

    testing::AssertionResult failure = testing::AssertionFailure();

    failure << "Expected bone collections differ from actual ones:" << std::endl;

    /* This is what you get when C++ doesn't even have a standard library
     * function to do something like `expect_names.join(", ")`. */
    failure << "Expected collections: [";
    for (int i = 0; i < expect_names.size() - 1; i++) {
      failure << expect_names[i] << ", ";
    }
    failure << expect_names.back() << "]" << std::endl;

    failure << "Actual collections  : [";
    for (int i = 0; i < actual_names.size() - 1; i++) {
      failure << actual_names[i] << ", ";
    }
    failure << actual_names.back() << "]" << std::endl;

    internal::bonecolls_debug_list(&arm);

    return failure;
  }
};

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__before_first_sibling)
{
  /* Set the active index to be one of the affected bone collections. */
  ANIM_armature_bonecoll_active_name_set(&arm, "child2");
  ASSERT_EQ(3, arm.runtime.active_collection_index);

  EXPECT_EQ(1, ANIM_armature_bonecoll_move_before_after_index(&arm, 3, 1, MoveLocation::Before));
  EXPECT_TRUE(expect_bcolls({"root", "child2", "child0", "child1", "child1_0"}));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 1));

  /* The three indicators of the active collection should still be in sync. */
  EXPECT_EQ(1, arm.runtime.active_collection_index);
  EXPECT_EQ(child2, arm.runtime.active_collection);
  EXPECT_STREQ("child2", arm.active_collection_name);
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__after_first_sibling)
{
  EXPECT_EQ(2, ANIM_armature_bonecoll_move_before_after_index(&arm, 3, 1, MoveLocation::After));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child2", "child1", "child1_0"}));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 2));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__before_last_sibling)
{
  /* Set the active index to be one of the affected bone collections. */
  ANIM_armature_bonecoll_active_name_set(&arm, "child1");
  ASSERT_EQ(2, arm.runtime.active_collection_index);

  EXPECT_EQ(2, ANIM_armature_bonecoll_move_before_after_index(&arm, 1, 3, MoveLocation::Before));
  EXPECT_TRUE(expect_bcolls({"root", "child1", "child0", "child2", "child1_0"}));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 2));

  /* The three indicators of the active collection should still be in sync. */
  EXPECT_EQ(1, arm.runtime.active_collection_index);
  EXPECT_EQ(child1, arm.runtime.active_collection);
  EXPECT_STREQ("child1", arm.active_collection_name);
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__after_last_sibling)
{
  EXPECT_EQ(3, ANIM_armature_bonecoll_move_before_after_index(&arm, 1, 3, MoveLocation::After));
  EXPECT_TRUE(expect_bcolls({"root", "child1", "child2", "child0", "child1_0"}));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 3));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__other_parent_before__move_left)
{
  EXPECT_EQ(1, ANIM_armature_bonecoll_move_before_after_index(&arm, 4, 1, MoveLocation::Before));
  EXPECT_TRUE(expect_bcolls({"root", "child1_0", "child0", "child1", "child2"}));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 1));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__other_parent_after__move_left)
{
  EXPECT_EQ(2, ANIM_armature_bonecoll_move_before_after_index(&arm, 4, 1, MoveLocation::After));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1_0", "child1", "child2"}));
  EXPECT_EQ(0, armature_bonecoll_find_parent_index(&arm, 2));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__other_parent_before__move_right)
{
  EXPECT_EQ(3, ANIM_armature_bonecoll_move_before_after_index(&arm, 1, 4, MoveLocation::Before));
  EXPECT_TRUE(expect_bcolls({"root", "child1", "child2", "child0", "child1_0"}));
  EXPECT_EQ(1, armature_bonecoll_find_parent_index(&arm, 3));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__other_parent_after__move_right)
{
  EXPECT_EQ(4, ANIM_armature_bonecoll_move_before_after_index(&arm, 1, 4, MoveLocation::After));
  EXPECT_TRUE(expect_bcolls({"root", "child1", "child2", "child1_0", "child0"}));
  EXPECT_EQ(1, armature_bonecoll_find_parent_index(&arm, 4));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__to_root__before)
{
  EXPECT_EQ(0, ANIM_armature_bonecoll_move_before_after_index(&arm, 4, 0, MoveLocation::Before));
  EXPECT_TRUE(expect_bcolls({"child1_0", "root", "child0", "child1", "child2"}));
  EXPECT_EQ(-1, armature_bonecoll_find_parent_index(&arm, 0));
}

TEST_F(ArmatureBoneCollectionsTestList, move_before_after_index__to_root__after)
{
  EXPECT_EQ(1, ANIM_armature_bonecoll_move_before_after_index(&arm, 4, 0, MoveLocation::After));
  EXPECT_TRUE(expect_bcolls({"root", "child1_0", "child0", "child1", "child2"}));
  EXPECT_EQ(-1, armature_bonecoll_find_parent_index(&arm, 1));
}

TEST_F(ArmatureBoneCollectionsTestList, child_number_set__roots)
{
  /* Test with only one root. */
  EXPECT_EQ(0, armature_bonecoll_child_number_set(&arm, root, 0));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));

  /* Move to "after the last child", which is the one root itself. */
  EXPECT_EQ(0, armature_bonecoll_child_number_set(&arm, root, -1));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));
  EXPECT_EQ(0, armature_bonecoll_child_number_set(&arm, root, 0));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));

  /* Going beyond the number of children is not allowed. */
  EXPECT_EQ(-1, armature_bonecoll_child_number_set(&arm, root, 1));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));

  /* Add two roots to be able to play. */
  ANIM_armature_bonecoll_new(&arm, "root1");
  ANIM_armature_bonecoll_new(&arm, "root2");
  EXPECT_TRUE(expect_bcolls({"root", "root1", "root2", "child0", "child1", "child2", "child1_0"}));

  /* Move the old root in between the two new ones. */
  EXPECT_EQ(1, armature_bonecoll_child_number_set(&arm, root, 1));
  EXPECT_TRUE(expect_bcolls({"root1", "root", "root2", "child0", "child1", "child2", "child1_0"}));

  /* And to the last one. */
  EXPECT_EQ(2, armature_bonecoll_child_number_set(&arm, root, 2));
  EXPECT_TRUE(expect_bcolls({"root1", "root2", "root", "child0", "child1", "child2", "child1_0"}));
}

TEST_F(ArmatureBoneCollectionsTestList, child_number_set__siblings)
{
  /* Move child0 to itself. */
  EXPECT_EQ(1, armature_bonecoll_child_number_set(&arm, child0, 0));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));

  /* Move child2 to itself. */
  EXPECT_EQ(3, armature_bonecoll_child_number_set(&arm, child2, -1));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));

  /* Going beyond the number of children is not allowed. */
  EXPECT_EQ(-1, armature_bonecoll_child_number_set(&arm, child0, 3));
  EXPECT_TRUE(expect_bcolls({"root", "child0", "child1", "child2", "child1_0"}));

  /* Move child0 to in between child1 and child2. */
  EXPECT_EQ(2, armature_bonecoll_child_number_set(&arm, child0, 1));
  EXPECT_TRUE(expect_bcolls({"root", "child1", "child0", "child2", "child1_0"}));

  /* Move child0 to the last spot. */
  EXPECT_EQ(3, armature_bonecoll_child_number_set(&arm, child0, 2));
  EXPECT_TRUE(expect_bcolls({"root", "child1", "child2", "child0", "child1_0"}));
}

TEST_F(ArmatureBoneCollectionsTestList, bone_collection_solo)
{
  EXPECT_FALSE(arm.flag & ARM_BCOLL_SOLO_ACTIVE) << "By default no solo'ing should be active";

  /* Enable solo. */
  EXPECT_FALSE(child1->flags & BONE_COLLECTION_SOLO);
  ANIM_armature_bonecoll_solo_set(&arm, child1, true);
  EXPECT_TRUE(child1->flags & BONE_COLLECTION_SOLO);
  EXPECT_TRUE(arm.flag & ARM_BCOLL_SOLO_ACTIVE);

  /* Enable solo on another bone collection. */
  EXPECT_FALSE(child1_0->flags & BONE_COLLECTION_SOLO);
  ANIM_armature_bonecoll_solo_set(&arm, child1_0, true);
  EXPECT_TRUE(child1_0->flags & BONE_COLLECTION_SOLO);
  EXPECT_TRUE(arm.flag & ARM_BCOLL_SOLO_ACTIVE);

  /* Disable the first solo flag. */
  EXPECT_TRUE(child1->flags & BONE_COLLECTION_SOLO);
  ANIM_armature_bonecoll_solo_set(&arm, child1, false);
  EXPECT_FALSE(child1->flags & BONE_COLLECTION_SOLO);
  EXPECT_TRUE(arm.flag & ARM_BCOLL_SOLO_ACTIVE);

  /* Disable the second solo flag. This should also disable the ARM_BCOLL_SOLO_ACTIVE flag. */
  EXPECT_TRUE(child1_0->flags & BONE_COLLECTION_SOLO);
  ANIM_armature_bonecoll_solo_set(&arm, child1_0, false);
  EXPECT_FALSE(child1_0->flags & BONE_COLLECTION_SOLO);
  EXPECT_FALSE(arm.flag & ARM_BCOLL_SOLO_ACTIVE);
}

class ArmatureBoneCollectionsLiboverrides : public ArmatureBoneCollectionsTestList {
 protected:
  bArmature dst_arm = {};

  BoneCollection *dst_root = nullptr;
  BoneCollection *dst_child0 = nullptr;
  BoneCollection *dst_child1 = nullptr;
  BoneCollection *dst_child2 = nullptr;
  BoneCollection *dst_child1_0 = nullptr;

  void SetUp() override
  {
    ArmatureBoneCollectionsTestList::SetUp();

    /* TODO: make this clone `arm` into `dst_arm`, instead of assuming the below
     * code is still in sync with the super-class. */
    STRNCPY(dst_arm.id.name, "ARArmatureDST");

    dst_root = ANIM_armature_bonecoll_new(&dst_arm, "root");
    dst_child0 = ANIM_armature_bonecoll_new(&dst_arm, "child0", 0);
    dst_child1 = ANIM_armature_bonecoll_new(&dst_arm, "child1", 0);
    dst_child2 = ANIM_armature_bonecoll_new(&dst_arm, "child2", 0);
    dst_child1_0 = ANIM_armature_bonecoll_new(&dst_arm, "child1_0", 2);

    ASSERT_STREQ(dst_root->name, dst_arm.collection_array[0]->name);
    ASSERT_STREQ(dst_child0->name, dst_arm.collection_array[1]->name);
    ASSERT_STREQ(dst_child1->name, dst_arm.collection_array[2]->name);
    ASSERT_STREQ(dst_child2->name, dst_arm.collection_array[3]->name);
    ASSERT_STREQ(dst_child1_0->name, dst_arm.collection_array[4]->name);

    BKE_armature_bone_hash_make(&arm);
    BKE_armature_bone_hash_make(&dst_arm);
  }

  void TearDown() override
  {
    ArmatureBoneCollectionsTestList::TearDown();
    BKE_libblock_free_datablock(&dst_arm.id, 0);
  }
};

TEST_F(ArmatureBoneCollectionsLiboverrides, bcoll_insert_copy_after)
{
  /* Mimic that a new root, two children, and two grandchildren were added via library overrides.
   * These were saved in `arm`, and now need to be copied into `dst_arm`. */
  BoneCollection *src_root = ANIM_armature_bonecoll_new(&arm, "new_root");
  const int root_index = armature_bonecoll_find_index(&arm, src_root);
  BoneCollection *src_child1 = ANIM_armature_bonecoll_new(&arm, "new_child1", root_index);
  ANIM_armature_bonecoll_new(&arm, "new_child2", root_index);
  const int child1_index = armature_bonecoll_find_index(&arm, src_child1);
  ANIM_armature_bonecoll_new(&arm, "new_gchild1", child1_index);
  ANIM_armature_bonecoll_new(&arm, "new_gchild2", child1_index);

  /* Copy the root. This should be the only change that's recorded by a library override operation.
   * It should also copy the entire subtree of that root. */
  const BoneCollection *anchor = dst_arm.collection_array[0];
  ASSERT_STREQ("root", anchor->name);
  BoneCollection *copy_root = ANIM_armature_bonecoll_insert_copy_after(
      &dst_arm, &arm, anchor, src_root);

  /* Check the array order. */
  EXPECT_TRUE(expect_bcolls({"root",
                             "new_root",
                             "child0",
                             "child1",
                             "child2",
                             "child1_0",
                             "new_child1",
                             "new_child2",
                             "new_gchild1",
                             "new_gchild2"}));

  /* Check that the copied root is actually stored in the destination armature array. */
  const int new_root_index = armature_bonecoll_find_index(&dst_arm, copy_root);
  EXPECT_EQ(1, new_root_index);

  /* Check the hierarchy. */
  const int new_child1_index = ANIM_armature_bonecoll_get_index_by_name(&dst_arm, "new_child1");
  EXPECT_TRUE(armature_bonecoll_is_root(&dst_arm, new_root_index));
  EXPECT_TRUE(armature_bonecoll_is_child_of(&dst_arm, new_root_index, new_child1_index));
  EXPECT_TRUE(armature_bonecoll_is_child_of(
      &dst_arm, new_root_index, ANIM_armature_bonecoll_get_index_by_name(&dst_arm, "new_child2")));
  EXPECT_TRUE(armature_bonecoll_is_child_of(
      &dst_arm,
      new_child1_index,
      ANIM_armature_bonecoll_get_index_by_name(&dst_arm, "new_gchild1")));
  EXPECT_TRUE(armature_bonecoll_is_child_of(
      &dst_arm,
      new_child1_index,
      ANIM_armature_bonecoll_get_index_by_name(&dst_arm, "new_gchild2")));

  if (HasFailure()) {
    internal::bonecolls_debug_list(&dst_arm);
  }
}

}  // namespace blender::animrig::tests
