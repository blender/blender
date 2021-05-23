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
 * The Original Code is Copyright (C) 2020 by Blender Foundation.
 */
#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

namespace blender::bke::tests {

struct LibIDMainSortTestContext {
  Main *bmain;
};

static void test_lib_id_main_sort_init(LibIDMainSortTestContext *ctx)
{
  ctx->bmain = BKE_main_new();
}

static void test_lib_id_main_sort_free(LibIDMainSortTestContext *ctx)
{
  BKE_main_free(ctx->bmain);
}

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
  LibIDMainSortTestContext ctx = {nullptr};
  test_lib_id_main_sort_init(&ctx);
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));
  EXPECT_TRUE(ctx.bmain->objects.first == id_a);
  EXPECT_TRUE(ctx.bmain->objects.last == id_c);
  test_lib_id_main_sort_check_order({id_a, id_b, id_c});

  test_lib_id_main_sort_free(&ctx);
}

TEST(lib_id_main_sort, linked_ids_1)
{
  LibIDMainSortTestContext ctx = {nullptr};
  test_lib_id_main_sort_init(&ctx);
  EXPECT_TRUE(BLI_listbase_is_empty(&ctx.bmain->libraries));

  Library *lib_a = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_A"));
  Library *lib_b = static_cast<Library *>(BKE_id_new(ctx.bmain, ID_LI, "LI_B"));
  ID *id_c = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_C"));
  ID *id_a = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_A"));
  ID *id_b = static_cast<ID *>(BKE_id_new(ctx.bmain, ID_OB, "OB_B"));

  id_a->lib = lib_a;
  id_sort_by_name(&ctx.bmain->objects, id_a, nullptr);
  id_b->lib = lib_a;
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  id_a->lib = lib_b;
  id_sort_by_name(&ctx.bmain->objects, id_a, nullptr);
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_a);
  test_lib_id_main_sort_check_order({id_c, id_b, id_a});

  id_b->lib = lib_b;
  id_sort_by_name(&ctx.bmain->objects, id_b, nullptr);
  EXPECT_TRUE(ctx.bmain->objects.first == id_c);
  EXPECT_TRUE(ctx.bmain->objects.last == id_b);
  test_lib_id_main_sort_check_order({id_c, id_a, id_b});

  test_lib_id_main_sort_free(&ctx);
}

}  // namespace blender::bke::tests
