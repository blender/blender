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
    STRNCPY(bone1.name, "bone1");
    STRNCPY(bone2.name, "bone2");
    STRNCPY(bone3.name, "bone3");

    memset(&arm, 0, sizeof(arm));
    bone1.childbase = {nullptr, nullptr};
    bone2.childbase = {nullptr, nullptr};
    bone3.childbase = {nullptr, nullptr};

    BLI_addtail(&arm.bonebase, &bone1);    /* bone1 is root bone. */
    BLI_addtail(&arm.bonebase, &bone2);    /* bone2 is root bone. */
    BLI_addtail(&bone2.childbase, &bone3); /* bone3 has bone2 as parent. */
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

}  // namespace blender::animrig::tests
