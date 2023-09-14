/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "ANIM_bone_collections.h"

#include "testing/testing.h"

namespace blender::animrig::tests {

TEST(ANIM_bone_collections, bonecoll_new_free)
{
  BoneCollection *bcoll = ANIM_bonecoll_new("some name");
  EXPECT_NE(nullptr, bcoll);
  EXPECT_EQ("some name", std::string(bcoll->name));
  EXPECT_TRUE(BLI_listbase_is_empty(&bcoll->bones));
  EXPECT_EQ(BONE_COLLECTION_VISIBLE | BONE_COLLECTION_SELECTABLE, bcoll->flags);
  ANIM_bonecoll_free(bcoll);
}

TEST(ANIM_bone_collections, bonecoll_default_name)
{
  {
    BoneCollection *bcoll = ANIM_bonecoll_new("");
    EXPECT_EQ("Bones", std::string(bcoll->name));
    ANIM_bonecoll_free(bcoll);
  }

  {
    BoneCollection *bcoll = ANIM_bonecoll_new(nullptr);
    EXPECT_EQ("Bones", std::string(bcoll->name));
    ANIM_bonecoll_free(bcoll);
  }
}

class ANIM_armature_bone_collections : public testing::Test {
 protected:
  bArmature arm;
  Bone bone1, bone2, bone3;

  void SetUp() override
  {
    memset(&arm, 0, sizeof(arm));
    memset(&bone1, 0, sizeof(Bone));
    memset(&bone2, 0, sizeof(Bone));
    memset(&bone3, 0, sizeof(Bone));

    STRNCPY(bone1.name, "bone1");
    STRNCPY(bone2.name, "bone2");
    STRNCPY(bone3.name, "bone3");

    BLI_addtail(&arm.bonebase, &bone1);    /* bone1 is root bone. */
    BLI_addtail(&arm.bonebase, &bone2);    /* bone2 is root bone. */
    BLI_addtail(&bone2.childbase, &bone3); /* bone3 has bone2 as parent. */
  }

  void TearDown() override
  {
    LISTBASE_FOREACH_BACKWARD_MUTABLE (BoneCollection *, bcoll, &arm.collections) {
      ANIM_armature_bonecoll_remove(&arm, bcoll);
    }
  }
};

TEST_F(ANIM_armature_bone_collections, armature_owned_collections)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "collection");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "collection");

  EXPECT_EQ(std::string("collection"), std::string(bcoll1->name));
  EXPECT_EQ(std::string("collection.001"), std::string(bcoll2->name));

  ANIM_armature_bonecoll_remove(&arm, bcoll1);
  ANIM_armature_bonecoll_remove(&arm, bcoll2);
}

TEST_F(ANIM_armature_bone_collections, bones_assign_unassign)
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

TEST_F(ANIM_armature_bone_collections, bones_assign_remove)
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

TEST_F(ANIM_armature_bone_collections, active_set_clear_by_pointer)
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

TEST_F(ANIM_armature_bone_collections, active_set_clear_by_index)
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

TEST_F(ANIM_armature_bone_collections, bcoll_is_editable)
{
  BoneCollection *bcoll1 = ANIM_armature_bonecoll_new(&arm, "Bones 1");
  BoneCollection *bcoll2 = ANIM_armature_bonecoll_new(&arm, "Bones 2");

  EXPECT_EQ(0, bcoll1->flags & BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL);
  EXPECT_EQ(0, bcoll2->flags & BONE_COLLECTION_OVERRIDE_LIBRARY_LOCAL);

  EXPECT_TRUE(ANIM_armature_bonecoll_is_editable(&arm, bcoll1))
      << "Expecting local armature to be editable";

  /* Fake that the armature is linked from another blend file. */
  Library fake_lib;
  arm.id.lib = &fake_lib;
  EXPECT_FALSE(ANIM_armature_bonecoll_is_editable(&arm, bcoll1))
      << "Expecting local armature to not be editable";

  /* Fake that the armature is an override, but linked from another blend file. */
  IDOverrideLibrary fake_override;
  bArmature fake_reference;
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

}  // namespace blender::animrig::tests
