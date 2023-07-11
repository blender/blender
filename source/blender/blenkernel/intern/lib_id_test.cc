/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_main_namemap.h"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

namespace blender::bke::tests {

struct LibIDMainSortTestContext {
  Main *bmain = nullptr;

  LibIDMainSortTestContext()
  {
    BKE_idtype_init();
    bmain = BKE_main_new();
  }
  ~LibIDMainSortTestContext()
  {
    BKE_main_free(bmain);
  }
};

static void test_lib_id_main_sort_check_order(std::initializer_list<ID *> list)
{
  ID *prev_id = nullptr;
  for (ID *id : list) {
    EXPECT_EQ(id->prev, prev_id);
    if (prev_id != nullptr) {
      EXPECT_EQ(prev_id->next, id);
    }
    prev_id = id;
  }
  EXPECT_EQ(prev_id->next, nullptr);
}

TEST(lib_id_main_sort, local_ids_1)
{
  LibIDMainSortTestContext ctx;
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));
  EXPECT_TRUE(ctx.bmain->objects.first == id_a);
  EXPECT_TRUE(ctx.bmain->objects.last == id_c);
  test_lib_id_main_sort_check_order({id_a, id_b, id_c});

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

static void change_lib(Main *bmain, ID *id, Library *lib)
{
  if (id->lib == lib) {
    return;
  }
  BKE_main_namemap_remove_name(bmain, id, id->name + 2);
  id->lib = lib;
  BKE_main_namemap_get_name(bmain, id, id->name + 2, false);
}

static void change_name(Main *bmain, ID *id, const char *name)
{
  BKE_main_namemap_remove_name(bmain, id, id->name + 2);
  BLI_strncpy(id->name + 2, name, MAX_NAME);
  BKE_id_new_name_validate(bmain, &bmain->objects, id, nullptr, true);
}

TEST(lib_id_main_sort, linked_ids_1)
{
  LibIDMainSortTestContext ctx;
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  Library *lib_a = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_A"));
  Library *lib_b = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_B"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));

  change_lib(ctx.bmain, id_a, lib_a);
  id_sort_by_name(&ctx.bmain->objects, id_a, nullptr);
  change_lib(ctx.bmain, id_b, lib_a);
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  change_lib(ctx.bmain, id_a, lib_b);
  id_sort_by_name(&ctx.bmain->objects, id_a, nullptr);
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_a);
  test_lib_id_main_sort_check_order({id_c, id_b, id_a});

  change_lib(ctx.bmain, id_b, lib_b);
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, local_ids_1)
{
  LibIDMainSortTestContext ctx;
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));
  test_lib_id_main_sort_check_order({id_a, id_b, id_c});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  change_name(ctx.bmain, id_c, "OB_A");

  EXPECT_STREQ(id_c->name + 2, "OB_A.001");
  EXPECT_STREQ(id_a->name + 2, "OB_A");
  EXPECT_TRUE(ctx.bmain->objects.first == id_a);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_a, id_c, id_b});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, linked_ids_1)
{
  LibIDMainSortTestContext ctx;
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  Library *lib_a = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_A"));
  Library *lib_b = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_B"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  change_lib(ctx.bmain, id_a, lib_a);
  id_sort_by_name(&ctx.bmain->objects, id_a, nullptr);
  change_lib(ctx.bmain, id_b, lib_a);
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);

  change_name(ctx.bmain, id_b, "OB_A");
  EXPECT_STREQ(id_b->name + 2, "OB_A.001");
  EXPECT_STREQ(id_a->name + 2, "OB_A");
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  change_lib(ctx.bmain, id_b, lib_b);
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);
  change_name(ctx.bmain, id_b, "OB_A");
  EXPECT_STREQ(id_b->name + 2, "OB_A");
  EXPECT_STREQ(id_a->name + 2, "OB_A");
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

static void change_name_global(Main *bmain, ID *id, const char *name)
{
  BKE_main_namemap_remove_name(bmain, id, id->name + 2);
  BLI_strncpy(id->name + 2, name, MAX_NAME);

  BKE_main_namemap_get_name(bmain, id, id->name + 2, true);

  id_sort_by_name(&bmain->objects, id, nullptr);
}

TEST(lib_id_main_global_unique_name, linked_ids_1)
{
  LibIDMainSortTestContext ctx;
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  Library *lib_a = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_A"));
  Library *lib_b = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_B"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  change_lib(ctx.bmain, id_a, lib_a);
  id_sort_by_name(&ctx.bmain->objects, id_a, nullptr);
  change_lib(ctx.bmain, id_b, lib_b);
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);

  change_name_global(ctx.bmain, id_b, "OB_A");
  EXPECT_NE(ctx.bmain->name_map_global, nullptr);
  EXPECT_STREQ(id_b->name + 2, "OB_A.001");
  EXPECT_STREQ(id_a->name + 2, "OB_A");
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  change_lib(ctx.bmain, id_b, lib_a);
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);
  change_name_global(ctx.bmain, id_b, "OB_C");
  EXPECT_STREQ(id_b->name + 2, "OB_C.001");
  EXPECT_STREQ(id_a->name + 2, "OB_A");
  EXPECT_STREQ(id_c->name + 2, "OB_C");
  change_name_global(ctx.bmain, id_a, "OB_C");
  EXPECT_STREQ(id_b->name + 2, "OB_C.001");
  EXPECT_STREQ(id_a->name + 2, "OB_C.002");
  EXPECT_STREQ(id_c->name + 2, "OB_C");
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_a);
  test_lib_id_main_sort_check_order({id_c, id_b, id_a});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  change_name(ctx.bmain, id_b, "OB_C");
  EXPECT_STREQ(id_b->name + 2, "OB_C");
  EXPECT_STREQ(id_a->name + 2, "OB_C.002");
  EXPECT_STREQ(id_c->name + 2, "OB_C");
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_a);
  test_lib_id_main_sort_check_order({id_c, id_b, id_a});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));
}

TEST(lib_id_main_unique_name, ids_sorted_by_default)
{
  LibIDMainSortTestContext ctx;

  ID *id_foo = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_bar = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Bar"));
  ID *id_baz = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Baz"));
  ID *id_yes = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Yes"));
  test_lib_id_main_sort_check_order({id_bar, id_baz, id_foo, id_yes});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

static ID *add_id_in_library(Main *bmain, const char *name, Library *lib)
{
  ID *id = static_cast<ID *>(BKE_id_new(bmain, ID_OB, name));
  change_lib(bmain, id, lib);
  id_sort_by_name(&bmain->objects, id, nullptr);
  return id;
}

TEST(lib_id_main_unique_name, ids_sorted_by_default_with_libraries)
{
  LibIDMainSortTestContext ctx;

  Library *lib_one = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LibOne"));
  Library *lib_two = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LibTwo"));

  ID *id_foo = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_bar = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Bar"));

  ID *id_l1c = add_id_in_library(ctx.bmain, "C", lib_one);
  ID *id_l2b = add_id_in_library(ctx.bmain, "B", lib_two);
  ID *id_l1a = add_id_in_library(ctx.bmain, "A", lib_one);

  ID *id_baz = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Baz"));
  ID *id_yes = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Yes"));

  test_lib_id_main_sort_check_order({id_bar, id_baz, id_foo, id_yes, id_l1a, id_l1c, id_l2b});

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, name_too_long_handling)
{
  LibIDMainSortTestContext ctx;
  const char *name_a = "Long_Name_That_Does_Not_Fit_Into_Max_Name_Limit_And_Should_Get_Truncated";
  const char *name_b = "Another_Long_Name_That_Does_Not_Fit_And_Has_A_Number_Suffix.123456";
  const char *name_c = "Name_That_Has_Too_Long_Number_Suffix.1234567890";

  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, name_a));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, name_b));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, name_c));

  EXPECT_STREQ(id_a->name + 2, "Long_Name_That_Does_Not_Fit_Into_Max_Name_Limit_And_Should_Get_");
  EXPECT_STREQ(id_b->name + 2, "Another_Long_Name_That_Does_Not_Fit_And_Has_A_Number_Suffix.123");
  EXPECT_STREQ(id_c->name + 2, "Name_That_Has_Too_Long_Number_Suffix.1234567890"); /* Unchanged */

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, create_equivalent_numeric_suffixes)
{
  LibIDMainSortTestContext ctx;

  /* Create names where many of their numeric suffixes are
   * the same number, yet the names are different and thus
   * should be allowed as-is. */
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.123"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.000"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.003"));
  ID *id_d = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.3"));
  ID *id_e = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.0"));
  ID *id_f = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo."));
  ID *id_g = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.0123"));
  ID *id_h = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_i = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.."));
  ID *id_j = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo..001"));
  ID *id_k = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo..000"));

  EXPECT_STREQ(id_a->name + 2, "Foo.123");
  EXPECT_STREQ(id_b->name + 2, "Foo.000");
  EXPECT_STREQ(id_c->name + 2, "Foo.003");
  EXPECT_STREQ(id_d->name + 2, "Foo.3");
  EXPECT_STREQ(id_e->name + 2, "Foo.0");
  EXPECT_STREQ(id_f->name + 2, "Foo.");
  EXPECT_STREQ(id_g->name + 2, "Foo.0123");
  EXPECT_STREQ(id_h->name + 2, "Foo");
  EXPECT_STREQ(id_i->name + 2, "Foo..");
  EXPECT_STREQ(id_j->name + 2, "Foo..001");
  EXPECT_STREQ(id_k->name + 2, "Foo..000");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  /* Now create their exact duplicates again, and check what happens. */
  id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.123"));
  id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.000"));
  id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.003"));
  id_d = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.3"));
  id_e = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.0"));
  id_f = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo."));
  id_g = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.0123"));
  id_h = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  id_i = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.."));
  id_j = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo..001"));
  id_k = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo..000"));

  EXPECT_STREQ(id_a->name + 2, "Foo.001");
  EXPECT_STREQ(id_b->name + 2, "Foo.002");
  EXPECT_STREQ(id_c->name + 2, "Foo.004");
  EXPECT_STREQ(id_d->name + 2, "Foo.005");
  EXPECT_STREQ(id_e->name + 2, "Foo.006");
  EXPECT_STREQ(id_f->name + 2, "Foo..002");
  EXPECT_STREQ(id_g->name + 2, "Foo.007");
  EXPECT_STREQ(id_h->name + 2, "Foo.008");
  EXPECT_STREQ(id_i->name + 2, "Foo...001");
  EXPECT_STREQ(id_j->name + 2, "Foo..003");
  EXPECT_STREQ(id_k->name + 2, "Foo..004");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, zero_suffix_is_never_assigned)
{
  LibIDMainSortTestContext ctx;

  /* Creating these should assign 002 to the first one, but the next
   * ones should start numbers starting from 1: 001 and 003. */
  ID *id_002 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.002"));
  ID *id_001 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.002"));
  ID *id_003 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.002"));

  EXPECT_STREQ(id_002->name + 2, "Foo.002");
  EXPECT_STREQ(id_001->name + 2, "Foo.001");
  EXPECT_STREQ(id_003->name + 2, "Foo.003");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, remove_after_dup_get_original_name)
{
  LibIDMainSortTestContext ctx;

  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));

  EXPECT_STREQ(id_a->name + 2, "Foo");
  EXPECT_STREQ(id_b->name + 2, "Foo.001");
  BKE_id_free(ctx.bmain, id_a);

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  EXPECT_STREQ(id_a->name + 2, "Foo");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, name_number_suffix_assignment)
{
  LibIDMainSortTestContext ctx;

  /* Create <1k objects first. */
  const int total_object_count = 1200;
  ID *ids[total_object_count] = {};
  for (int i = 0; i < total_object_count / 2; ++i) {
    ids[i] = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  }

  /* They should get assigned sequential numeric suffixes. */
  EXPECT_STREQ(ids[0]->name + 2, "Foo");
  EXPECT_STREQ(ids[1]->name + 2, "Foo.001");
  EXPECT_STREQ(ids[total_object_count / 2 - 1]->name + 2, "Foo.599");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  /* Free some of the objects. */
  BKE_id_free(ctx.bmain, ids[10]);
  BKE_id_free(ctx.bmain, ids[20]);
  BKE_id_free(ctx.bmain, ids[30]);

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  /* Create objects again; they should get suffixes that were just free'd up. */
  ID *id_010 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  EXPECT_STREQ(id_010->name + 2, "Foo.010");
  ID *id_020 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.123"));
  EXPECT_STREQ(id_020->name + 2, "Foo.020");
  /* Suffixes >1k do not get the "use the most proper free one" treatment. */
  ID *id_2000 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.2000"));
  EXPECT_STREQ(id_2000->name + 2, "Foo.2000");
  /* But smaller than 1k suffixes do get proper empty spots. */
  ID *id_030 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  EXPECT_STREQ(id_030->name + 2, "Foo.030");
  ID *id_600 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  EXPECT_STREQ(id_600->name + 2, "Foo.600");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  /* Max possible numeric suffix. */
  ID *id_max = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.999999999"));
  EXPECT_STREQ(id_max->name + 2, "Foo.999999999");
  /* Try with max. possible suffix again: will assign free suffix under 1k. */
  ID *id_max1 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.999999999"));
  EXPECT_STREQ(id_max1->name + 2, "Foo.601");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  /* Now create the rest of objects, to use all the suffixes up to 1k.
   * Once all the ones up to 1k are used, the logic will fall back to
   * "use largest number seen + 1", but the largest one is already the max
   * possible. So it will shorten the name part and restart the counter,
   * i.e. "Fo.001". */
  for (int i = total_object_count / 2; i < total_object_count; ++i) {
    ids[i] = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  }
  /* At this point creating "Foo" based objects will fall always
   * result in shortened name to "Fo". */
  ID *id_fo178 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  EXPECT_STREQ(id_fo178->name + 2, "Fo.178");
  ID *id_fo179 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.2000"));
  EXPECT_STREQ(id_fo179->name + 2, "Fo.179");
  ID *id_fo180 = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo.999999999"));
  EXPECT_STREQ(id_fo180->name + 2, "Fo.180");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, renames_with_duplicates)
{
  LibIDMainSortTestContext ctx;

  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Bar"));

  EXPECT_STREQ(id_a->name + 2, "Foo");
  EXPECT_STREQ(id_b->name + 2, "Foo.001");
  EXPECT_STREQ(id_c->name + 2, "Bar");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  BKE_libblock_rename(ctx.bmain, id_a, "Foo.002");
  EXPECT_STREQ(id_a->name + 2, "Foo.002");
  BKE_libblock_rename(ctx.bmain, id_b, "Bar");
  EXPECT_STREQ(id_b->name + 2, "Bar.001");
  BKE_libblock_rename(ctx.bmain, id_c, "Foo");
  EXPECT_STREQ(id_c->name + 2, "Foo");
  BKE_libblock_rename(ctx.bmain, id_b, "Bar");
  EXPECT_STREQ(id_b->name + 2, "Bar");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, names_are_unique_per_id_type)
{
  LibIDMainSortTestContext ctx;

  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_CA, "Foo"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "Foo"));

  EXPECT_STREQ(id_a->name + 2, "Foo");
  EXPECT_STREQ(id_b->name + 2, "Foo"); /* Different types (OB & CA) can have the same name. */
  EXPECT_STREQ(id_c->name + 2, "Foo.001");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

TEST(lib_id_main_unique_name, name_huge_number_suffix)
{
  LibIDMainSortTestContext ctx;

  /* Use numeric suffix that is really large: should come through
   * fine, since no duplicates with other names. */
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "SuperLong.1234567890"));
  EXPECT_STREQ(id_a->name + 2, "SuperLong.1234567890");
  /* Now create with the same name again: should get 001 suffix. */
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "SuperLong.1234567890"));
  EXPECT_STREQ(id_b->name + 2, "SuperLong.001");

  EXPECT_TRUE(BKE_main_namemap_validate(ctx.bmain));

  EXPECT_EQ(ctx.bmain->name_map_global, nullptr);
}

}  // namespace blender::bke::tests
