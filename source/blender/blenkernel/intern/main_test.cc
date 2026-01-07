/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "CLG_log.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_collection.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"

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
  Collection *coll = BKE_id_new<Collection>(bmain_src, "Coll_src");
  Object *ob = BKE_id_new<Object>(bmain_src, "Ob_src");
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
  Collection *coll_2 = BKE_id_new<Collection>(bmain_src, "Coll_src_2");
  Object *ob_2 = BKE_id_new<Object>(bmain_src, "Ob_src");
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

  BKE_id_new<Collection>(bmain_dst, "Coll_dst");

  Collection *coll_1 = BKE_id_new<Collection>(bmain_src, "Coll_src");
  Library *lib_src_1 = BKE_id_new<Library>(bmain_src, LIB_PATH);
  BKE_library_filepath_set(bmain_src, lib_src_1, LIB_PATH);
  Object *ob_1 = BKE_id_new_in_lib<Object>(bmain_src, lib_src_1, "Ob_src");
  BKE_collection_object_add(bmain_src, coll_1, ob_1);

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

  Collection *coll_2 = BKE_id_new<Collection>(bmain_src, "Coll_src_2");
  Library *lib_src_2 = BKE_id_new<Library>(bmain_src, LIB_PATH);
  BKE_library_filepath_set(bmain_src, lib_src_2, LIB_PATH);
  std::cout << lib_src_1->runtime->filepath_abs << "\n";
  std::cout << lib_src_2->runtime->filepath_abs << "\n";
  Object *ob_2 = BKE_id_new_in_lib<Object>(bmain_src, lib_src_2, "Ob_src_2");
  BKE_collection_object_add(bmain_src, coll_2, ob_2);
  Object *ob_2_2 = BKE_id_new_in_lib<Object>(bmain_src, lib_src_2, "Ob_src_2_2");
  BKE_collection_object_add(bmain_src, coll_2, ob_2_2);

  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(2, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  EXPECT_EQ(3, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(3, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(ob_1, bmain_dst->objects.first);
  EXPECT_EQ(ob_2_2, bmain_dst->objects.last);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.first);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(ob_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_2_2->id.lib, lib_src_1);
  EXPECT_EQ(3, reports.num_merged_ids);
  EXPECT_EQ(0, reports.num_unknown_ids);
  EXPECT_EQ(0, reports.num_remapped_ids);
  EXPECT_EQ(1, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);

  /* Use a relative library path. Since this is a different library, even though the object re-use
   * the same name, it should still be moved into `bmain_dst`. The library filepath should also be
   * updated and become relative the path of bmain_dst too. */
  bmain_src = BKE_main_new();
  STRNCPY(bmain_src->filepath, SRC_PATH);

  Collection *coll_3 = BKE_id_new<Collection>(bmain_src, "Coll_src_3");
  Library *lib_src_3 = BKE_id_new<Library>(bmain_src, LIB_PATH_RELATIVE);
  BKE_library_filepath_set(bmain_src, lib_src_3, LIB_PATH_RELATIVE);
  Object *ob_3 = BKE_id_new_in_lib<Object>(bmain_src, lib_src_3, "Ob_src");
  BKE_collection_object_add(bmain_src, coll_3, ob_3);

  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));
  EXPECT_TRUE(STREQ(lib_src_3->filepath, LIB_PATH_RELATIVE));
  EXPECT_TRUE(STREQ(lib_src_3->runtime->filepath_abs, LIB_PATH_RELATIVE_ABS_SRC));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(ob_1, bmain_dst->objects.first);
  EXPECT_EQ(ob_3, bmain_dst->objects.last);
  EXPECT_EQ(lib_src_3, bmain_dst->libraries.first);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.last);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(ob_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_2_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_3->id.lib, lib_src_3);
  EXPECT_FALSE(STREQ(lib_src_3->filepath, LIB_PATH_RELATIVE));
  EXPECT_TRUE(STREQ(lib_src_3->runtime->filepath_abs, LIB_PATH_RELATIVE_ABS_SRC));
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

  Library *lib_src_4 = BKE_id_new<Library>(bmain_src, DST_PATH);
  BKE_library_filepath_set(bmain_src, lib_src_4, DST_PATH);
  Collection *coll_4 = BKE_id_new_in_lib<Collection>(bmain_src, lib_src_4, "Coll_src");
  Object *ob_4 = BKE_id_new_in_lib<Object>(bmain_src, lib_src_4, "Ob_src_4");
  BKE_collection_object_add(bmain_src, coll_4, ob_4);

  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->collections));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->objects));
  EXPECT_EQ(1, BLI_listbase_count(&bmain_src->libraries));

  reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  /* `bmain_dst` is unchanged, since both `coll_4` and `ob_4` were defined as linked from
   * `bmain_dst`. */
  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->collections));
  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->objects));
  EXPECT_EQ(2, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_EQ(lib_src_3, bmain_dst->libraries.first);
  EXPECT_EQ(lib_src_1, bmain_dst->libraries.last);
  EXPECT_EQ(ob_1->id.lib, lib_src_1);
  EXPECT_EQ(ob_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_2_2->id.lib, lib_src_1);
  EXPECT_EQ(ob_3->id.lib, lib_src_3);
  EXPECT_EQ(0, reports.num_merged_ids);
  EXPECT_EQ(1, reports.num_unknown_ids);
  EXPECT_EQ(1, reports.num_remapped_ids);
  EXPECT_EQ(1, reports.num_remapped_libraries);
  EXPECT_EQ(nullptr, bmain_src);
}

TEST_F(BMainMergeTest, link_lib_packed)
{
  constexpr char LIB_PATH[] = ABS_ROOT "tmp" SEP_STR "lib" SEP_STR "lib.blend";
  bool is_archive_lib_new = false;

  auto create_packed_object = [&is_archive_lib_new](Main &bmain,
                                                    Library &owner_lib,
                                                    StringRefNull ob_name,
                                                    IDHash ob_deep_hash) -> Object * {
    Object *ob_packed = BKE_id_new_in_lib<Object>(&bmain, &owner_lib, ob_name.c_str());
    ob_packed->id.deep_hash = ob_deep_hash;
    Library *archive_lib = bke::library::ensure_archive_library(
        bmain, ob_packed->id, owner_lib, ob_packed->id.deep_hash, is_archive_lib_new);
    BKE_main_namemap_remove_id(bmain, ob_packed->id);
    ob_packed->id.lib = archive_lib;
    ob_packed->id.flag |= ID_FLAG_LINKED_AND_PACKED;
    BKE_main_namemap_get_unique_name(bmain, ob_packed->id, BKE_id_name(ob_packed->id));
    return ob_packed;
  };

  /* Three packed IDs in source Main, two with same names, one with different name, leading to two
   * different archive libraries. */
  Library *lib_src = BKE_id_new<Library>(bmain_src, LIB_PATH);
  BKE_library_filepath_set(bmain_src, lib_src, LIB_PATH);

  Object *ob_src_linked = BKE_id_new_in_lib<Object>(bmain_src, lib_src, "Ob_linked");

  Object *ob_src_packed = create_packed_object(*bmain_src, *lib_src, "Ob_packed", {1});
  EXPECT_TRUE(is_archive_lib_new);
  Object *ob_src_packed_same_name = create_packed_object(*bmain_src, *lib_src, "Ob_packed", {2});
  EXPECT_TRUE(is_archive_lib_new);
  Object *ob_src_packed_diff_name = create_packed_object(
      *bmain_src, *lib_src, "Ob_packed_second_name", {3});
  EXPECT_FALSE(is_archive_lib_new);

  /* Two packed IDs in destination Main before the merge, with same names as the two first in
   * source Main, one sharing the same deep_hash (so being identical data), the second with another
   * deep hash. */
  Library *lib_dst = BKE_id_new<Library>(bmain_dst, LIB_PATH);
  BKE_library_filepath_set(bmain_dst, lib_dst, LIB_PATH);

  Object *ob_dst_packed = create_packed_object(*bmain_dst, *lib_dst, "Ob_packed", {1});
  EXPECT_TRUE(is_archive_lib_new);
  Object *ob_dst_packed_same_name = create_packed_object(*bmain_dst, *lib_dst, "Ob_packed", {4});
  EXPECT_TRUE(is_archive_lib_new);

  MainMergeReport reports = {};
  BKE_main_merge(bmain_dst, &bmain_src, reports);

  /* Part of the packed IDs in `bmain_src` already existed in `bmain_dst`, so these are re-used.
   * The others are moved over, which will also create a new archive library in `bmain_dst`. */
  EXPECT_EQ(4, BLI_listbase_count(&bmain_dst->libraries));
  EXPECT_TRUE((static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 0))->flag &
               LIBRARY_FLAG_IS_ARCHIVE) == 0);
  EXPECT_EQ(static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 0)), lib_dst);
  EXPECT_TRUE((static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 1))->flag &
               LIBRARY_FLAG_IS_ARCHIVE) != 0);
  EXPECT_EQ(static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 1))->archive_parent_library,
            lib_dst);
  EXPECT_TRUE((static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 2))->flag &
               LIBRARY_FLAG_IS_ARCHIVE) != 0);
  EXPECT_EQ(static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 2))->archive_parent_library,
            lib_dst);
  EXPECT_TRUE((static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 3))->flag &
               LIBRARY_FLAG_IS_ARCHIVE) != 0);
  EXPECT_EQ(static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 3))->archive_parent_library,
            lib_dst);

  EXPECT_EQ(5, BLI_listbase_count(&bmain_dst->objects));
  /* ob_src_packed is identical to ob_dst_packed (same deep hash), so it has been discarded. */
  UNUSED_VARS(ob_src_packed);
  EXPECT_EQ(ob_dst_packed, BLI_findlink(&bmain_dst->objects, 0));
  EXPECT_TRUE(ID_IS_PACKED(&ob_dst_packed->id));
  /* ob_src_packed_diff_name has no name collision with any other packed objects, so it is added to
   * the first archive library in bmain_dst, and therefore is the second object. */
  EXPECT_EQ(ob_src_packed_diff_name->id.lib, ob_dst_packed->id.lib);
  EXPECT_EQ(ob_src_packed_diff_name, BLI_findlink(&bmain_dst->objects, 1));
  EXPECT_TRUE(ID_IS_PACKED(&ob_src_packed_diff_name->id));
  /* ob_dst_packed_same_name was the second packed ID in bmain_dst, requiring its own archive
   * library, so it is now third. */
  EXPECT_EQ(ob_dst_packed_same_name, BLI_findlink(&bmain_dst->objects, 2));
  EXPECT_TRUE(ID_IS_PACKED(&ob_dst_packed_same_name->id));
  /* ob_src_linked is added to the first, regular existing lib_dst library in bmain_dst, as it
   * matches its original source library in bmain_src. Since it's the first ID for lib_dst in
   * bmain_dst, it is added after all packed IDs belonging to the first two archive libraries
   * pre-existing in bmain_dst. */
  EXPECT_EQ(ob_src_linked->id.lib, static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 0)));
  EXPECT_EQ(ob_src_linked, BLI_findlink(&bmain_dst->objects, 3));
  EXPECT_FALSE(ID_IS_PACKED(&ob_src_linked->id));
  /* ob_src_packed_same_name name conflicts with ob_dst_packed and ob_dst_packed_same_name names,
   * so it is added in a new archive library by the merge operation. This makes it the last object,
   * using the last library, in bmain_dst. */
  EXPECT_EQ(ob_src_packed_same_name->id.lib,
            static_cast<Library *>(BLI_findlink(&bmain_dst->libraries, 3)));
  EXPECT_EQ(ob_src_packed_same_name, BLI_findlink(&bmain_dst->objects, 4));
  EXPECT_TRUE(ID_IS_PACKED(&ob_src_packed_same_name->id));

  EXPECT_EQ(nullptr, bmain_src);
}

}  // namespace blender::bke::tests
