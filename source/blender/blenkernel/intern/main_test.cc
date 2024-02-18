/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_collection.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"

namespace blender::bke::tests {

class BMainTest : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }
  static void TearDownTestSuite()
  {
    CLG_exit();
  }
};

class BMainMergeTest : public BMainTest {
 public:
  void SetUp() override
  {
    bmain_src = BKE_main_new();
    bmain_dst = BKE_main_new();
  }

  void TearDown() override
  {
    if (bmain_src) {
      BKE_main_free(bmain_src);
    }
    if (bmain_dst) {
      BKE_main_free(bmain_dst);
    }
  }

  Main *bmain_src;
  Main *bmain_dst;
};

TEST_F(BMainMergeTest, basics)
{
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_dst->libraries));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_dst->collections));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_dst->objects));

  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_src->libraries));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_src->collections));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_src->objects));

  BKE_id_new(bmain_dst, ID_GR, "Coll_dst");
  Collection *coll = static_cast<Collection *>(BKE_id_new(bmain_src, ID_GR, "Coll_src"));
  Object *ob = static_cast<Object *>(BKE_id_new(bmain_src, ID_OB, "Ob_src"));
  BKE_collection_object_add(bmain_src, coll, ob);

  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(0, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));

  MainMergeReport reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(2, reports.num_merged_ids);
  EXPECT_EQ(0, reports.num_unknown_ids);
  EXPECT_EQ(0, reports.num_remapped_ids);
  EXPECT_EQ(0, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);

  bmain_src = BKE_main_new();
  Collection *coll_2 = static_cast<Collection *>(BKE_id_new(bmain_src, ID_GR, "Coll_src_2"));
  Object *ob_2 = static_cast<Object *>(BKE_id_new(bmain_src, ID_OB, "Ob_src"));
  BKE_collection_object_add(bmain_src, coll_2, ob_2);

  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  /* The second `Ob_src` object in `bmain_src` cannot be merged in `bmain_dst`, since its name
   * would collide with the first object. */
  EXPECT_EQ(3, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(1, reports.num_merged_ids);
  EXPECT_EQ(0, reports.num_unknown_ids);
  EXPECT_EQ(1, reports.num_remapped_ids);
  EXPECT_EQ(0, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);

  /* `Coll_src_2` should have been remapped to using `Ob_src` in `bmain_dst`, instead of `Ob_src`
   * in `bmain_src`. */
  EXPECT_EQ(1, BLI_listbase_count(&coll_2->gobject));
  EXPECT_EQ(ob, static_cast<CollectionObject *>(coll_2->gobject.first)->ob);
}

TEST_F(BMainMergeTest, linked_data)
{
#ifdef WIN32
#  define ABS_ROOT "C:" SEP_STR
#else
#  define ABS_ROOT SEP_STR
#endif
  constexpr char DST_PATH[] = ABS_ROOT "tmp" SEP_STR "dst" SEP_STR "dst.blend";
  constexpr char SRC_PATH[] = ABS_ROOT "tmp" SEP_STR "src" SEP_STR "src.blend";
  constexpr char LIB_PATH[] = ABS_ROOT "tmp" SEP_STR "lib" SEP_STR "lib.blend";

  constexpr char LIB_PATH_RELATIVE[] = "//lib" SEP_STR "lib.blend";
  constexpr char LIB_PATH_RELATIVE_ABS_SRC[] = ABS_ROOT "tmp" SEP_STR "src" SEP_STR "lib" SEP_STR
                                                        "lib.blend";

  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_dst->libraries));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_dst->collections));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_dst->objects));

  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_src->libraries));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_src->collections));
  EXPECT_TRUE(BLI_listbase_is_empty(&bmain_src->objects));

  STRNCPY(bmain_dst->filepath, DST_PATH);
  STRNCPY(bmain_src->filepath, SRC_PATH);

  BKE_id_new(bmain_dst, ID_GR, "Coll_dst");

  Collection *coll_1 = static_cast<Collection *>(BKE_id_new(bmain_src, ID_GR, "Coll_src"));
  Object *ob_1 = static_cast<Object *>(BKE_id_new(bmain_src, ID_OB, "Ob_src"));
  BKE_collection_object_add(bmain_src, coll_1, ob_1);
  Library *lib_src_1 = static_cast<Library *>(BKE_id_new(bmain_src, ID_LI, LIB_PATH));
  BKE_library_filepath_set(bmain_src, lib_src_1, LIB_PATH);
  ob_1->id.lib = lib_src_1;

  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(0, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(0, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));

  MainMergeReport reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(ob_1, bmain_dst->objects.first);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.first);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(3, reports.num_merged_ids);
  EXPECT_EQ(0, reports.num_unknown_ids);
  EXPECT_EQ(0, reports.num_remapped_ids);
  EXPECT_EQ(0, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);

  /* Try another merge, with the same library path - second library should be skipped, destination
   * merge should still have only one library ID. */
  bmain_src = BKE_main_new();
  STRNCPY(bmain_src->filepath, SRC_PATH);

  Collection *coll_2 = static_cast<Collection *>(BKE_id_new(bmain_src, ID_GR, "Coll_src_2"));
  Object *ob_2 = static_cast<Object *>(BKE_id_new(bmain_src, ID_OB, "Ob_src_2"));
  BKE_collection_object_add(bmain_src, coll_2, ob_2);
  Library *lib_src_2 = static_cast<Library *>(BKE_id_new(bmain_src, ID_LI, LIB_PATH));
  BKE_library_filepath_set(bmain_src, lib_src_2, LIB_PATH);
  std::cout << lib_src_1->filepath_abs << "\n";
  std::cout << lib_src_2->filepath_abs << "\n";
  ob_2->id.lib = lib_src_2;

  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  EXPECT_EQ(3, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(ob_1, bmain_dst->objects.first);
  EXPECT_EQ(ob_2, bmain_dst->objects.last);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.first);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(ob_2->id.lib, lib_src_1);
  EXPECT_EQ(2, reports.num_merged_ids);
  EXPECT_EQ(0, reports.num_unknown_ids);
  EXPECT_EQ(0, reports.num_remapped_ids);
  EXPECT_EQ(1, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);

  /* Use a relative library path. Since this is a different library, even though the object re-use
   * the same name, it should still be moved into `bmain_dst`. The library filepath should also be
   * updated and become relative the path of bmain_dst too. */
  bmain_src = BKE_main_new();
  STRNCPY(bmain_src->filepath, SRC_PATH);

  Collection *coll_3 = static_cast<Collection *>(BKE_id_new(bmain_src, ID_GR, "Coll_src_3"));
  Object *ob_3 = static_cast<Object *>(BKE_id_new(bmain_src, ID_OB, "Ob_src"));
  BKE_collection_object_add(bmain_src, coll_3, ob_3);
  Library *lib_src_3 = static_cast<Library *>(BKE_id_new(bmain_src, ID_LI, LIB_PATH_RELATIVE));
  BKE_library_filepath_set(bmain_src, lib_src_3, LIB_PATH_RELATIVE);
  ob_3->id.lib = lib_src_3;

  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));
  EXPECT_TRUE(STREQ(lib_src_3->filepath, LIB_PATH_RELATIVE));
  EXPECT_TRUE(STREQ(lib_src_3->filepath_abs, LIB_PATH_RELATIVE_ABS_SRC));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(3, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(ob_1, bmain_dst->objects.first);
  EXPECT_EQ(ob_3, bmain_dst->objects.last);
  EXPECT_EQ(lib_src_3, bmain_dst->libraries.first);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.last);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(ob_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_3->id.lib, lib_src_3);
  EXPECT_FALSE(STREQ(lib_src_3->filepath, LIB_PATH_RELATIVE));
  EXPECT_TRUE(STREQ(lib_src_3->filepath_abs, LIB_PATH_RELATIVE_ABS_SRC));
  EXPECT_EQ(3, reports.num_merged_ids);
  EXPECT_EQ(0, reports.num_unknown_ids);
  EXPECT_EQ(0, reports.num_remapped_ids);
  EXPECT_EQ(0, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);

  /* Try another merge, with the library path set to the path of the destination bmain. That source
   * library should also be skipped, and the 'linked' object in source bmain should become a local
   * object in destination bmain. */
  bmain_src = BKE_main_new();
  STRNCPY(bmain_src->filepath, SRC_PATH);

  Collection *coll_4 = static_cast<Collection *>(BKE_id_new(bmain_src, ID_GR, "Coll_src"));
  Object *ob_4 = static_cast<Object *>(BKE_id_new(bmain_src, ID_OB, "Ob_src_4"));
  BKE_collection_object_add(bmain_src, coll_4, ob_4);
  Library *lib_src_4 = static_cast<Library *>(BKE_id_new(bmain_src, ID_LI, DST_PATH));
  BKE_library_filepath_set(bmain_src, lib_src_4, DST_PATH);
  coll_4->id.lib = lib_src_4;
  ob_4->id.lib = lib_src_4;

  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  /* `bmain_dst` is unchanged, since both `coll_4` and `ob_4` were defined as linked from
   * `bmain_dst`. */
  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(3, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(lib_src_3, bmain_dst->libraries.first);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.last);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(ob_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_3->id.lib, lib_src_3);
  EXPECT_EQ(0, reports.num_merged_ids);
  EXPECT_EQ(1, reports.num_unknown_ids);
  EXPECT_EQ(1, reports.num_remapped_ids);
  EXPECT_EQ(1, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);
}

}  // namespace blender::bke::tests
