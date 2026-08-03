/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <ranges>

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_listbase.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BKE_collection.hh"
#include "BKE_gtest_base.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"

namespace blender::bke::tests {

/* TODO: Move here most tests from python `bl_id_management.py`, as some lower-level advanced
 * features are not exposed to RNA/bpy. */

class BMainNameMapTest : public bke::BlenderGTestBase {
 public:
  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    if (bmain) {
      BKE_main_free(bmain);
    }
  }

  Main *bmain;
};

TEST_F(BMainNameMapTest, global_map_uniqueness)
{
  EXPECT_TRUE(bmain->libraries.is_empty());
  EXPECT_TRUE(bmain->collections.is_empty());
  EXPECT_TRUE(bmain->objects.is_empty());

  Library *lib_1 = BKE_id_new<Library>(bmain, "Library");
  EXPECT_STREQ(BKE_id_name(lib_1->id), "Library");

  Library *lib_2 = BKE_id_new<Library>(bmain, "Library");
  EXPECT_STREQ(BKE_id_name(lib_2->id), "Library.001");

  Collection *coll_1 = BKE_id_new<Collection>(bmain, "IDName");
  EXPECT_STREQ(BKE_id_name(coll_1->id), "IDName");

  Collection *coll_2 = BKE_id_new<Collection>(bmain, "IDName");
  EXPECT_STREQ(BKE_id_name(coll_2->id), "IDName.001");

  Object *ob_1 = BKE_id_new<Object>(bmain, "IDName");
  EXPECT_STREQ(BKE_id_name(ob_1->id), "IDName");

  Object *ob_2 = BKE_id_new<Object>(bmain, "IDName");
  EXPECT_STREQ(BKE_id_name(ob_2->id), "IDName.001");

  Object *ob_1_lib_1 = BKE_id_new_in_lib<Object>(bmain, lib_1, "IDName");
  EXPECT_STREQ(BKE_id_name(ob_1_lib_1->id), "IDName");

  Object *ob_2_lib_1 = BKE_id_new_in_lib<Object>(bmain, lib_1, "IDName");
  EXPECT_STREQ(BKE_id_name(ob_2_lib_1->id), "IDName.001");

  Object *ob_1_lib_2 = BKE_id_new_in_lib<Object>(bmain, lib_2, "IDName");
  EXPECT_STREQ(BKE_id_name(ob_1_lib_2->id), "IDName");

  Object *ob_2_lib_2 = BKE_id_new_in_lib<Object>(bmain, lib_2, "IDName");
  EXPECT_STREQ(BKE_id_name(ob_2_lib_2->id), "IDName.001");

  Object *ob_3_lib_2 = BKE_id_new_in_lib<Object>(bmain, lib_2, "IDName.003");
  EXPECT_STREQ(BKE_id_name(ob_3_lib_2->id), "IDName.003");

  /* Generate a globally unique name for `ob_1_lib_1`. */
  BKE_main_namemap_remove_id(*bmain, ob_1_lib_1->id);
  // BLI_strncpy(ob_1_lib_1->id.name + 2, "IDName", MAX_ID_NAME - 2);
  BKE_main_global_namemap_get_unique_name(*bmain, ob_1_lib_1->id, BKE_id_name(ob_1_lib_1->id));
  id_sort_by_name(which_libbase(bmain, GS(ob_1_lib_1->id.name)), &ob_1_lib_1->id, nullptr);
  EXPECT_STREQ(BKE_id_name(ob_1_lib_1->id), "IDName.002");

  /* Generate a globally unique name for `ob_2_lib_1`. */
  BKE_main_namemap_remove_id(*bmain, ob_2_lib_1->id);
  // BLI_strncpy(ob_2_lib_1->id.name + 2, "IDName", MAX_ID_NAME - 2);
  BKE_main_global_namemap_get_unique_name(*bmain, ob_2_lib_1->id, BKE_id_name(ob_2_lib_1->id));
  id_sort_by_name(which_libbase(bmain, GS(ob_2_lib_1->id.name)), &ob_2_lib_1->id, nullptr);
  EXPECT_STREQ(BKE_id_name(ob_2_lib_1->id), "IDName.004");
}

}  // namespace blender::bke::tests
