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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "IO_abstract_hierarchy_iterator.h"

#include "testing/testing.h"

extern "C" {
#include "BLI_utildefines.h"
}

using namespace blender::io;

class HierarchyContextOrderTest : public testing::Test {
};

static Object *fake_pointer(int value)
{
  return static_cast<Object *>(POINTER_FROM_INT(value));
}

TEST_F(HierarchyContextOrderTest, ObjectPointerTest)
{
  HierarchyContext ctx_a = {0};
  ctx_a.object = fake_pointer(1);
  ctx_a.duplicator = nullptr;

  HierarchyContext ctx_b = {0};
  ctx_b.object = fake_pointer(2);
  ctx_b.duplicator = nullptr;

  EXPECT_LT(ctx_a, ctx_b);
  EXPECT_FALSE(ctx_b < ctx_a);
  EXPECT_FALSE(ctx_a < ctx_a);
}

TEST_F(HierarchyContextOrderTest, DuplicatorPointerTest)
{
  HierarchyContext ctx_a = {0};
  ctx_a.object = fake_pointer(1);
  ctx_a.duplicator = fake_pointer(1);
  ctx_a.export_name = "A";

  HierarchyContext ctx_b = {0};
  ctx_b.object = fake_pointer(1);
  ctx_b.duplicator = fake_pointer(1);
  ctx_b.export_name = "B";

  EXPECT_LT(ctx_a, ctx_b);
  EXPECT_FALSE(ctx_b < ctx_a);
  EXPECT_FALSE(ctx_a < ctx_a);
}

TEST_F(HierarchyContextOrderTest, ExportParentTest)
{
  HierarchyContext ctx_a = {0};
  ctx_a.object = fake_pointer(1);
  ctx_a.export_parent = fake_pointer(1);

  HierarchyContext ctx_b = {0};
  ctx_b.object = fake_pointer(1);
  ctx_b.export_parent = fake_pointer(2);

  EXPECT_LT(ctx_a, ctx_b);
  EXPECT_FALSE(ctx_b < ctx_a);
  EXPECT_FALSE(ctx_a < ctx_a);
}

TEST_F(HierarchyContextOrderTest, TransitiveTest)
{
  HierarchyContext ctx_a = {0};
  ctx_a.object = fake_pointer(1);
  ctx_a.export_parent = fake_pointer(1);
  ctx_a.duplicator = nullptr;
  ctx_a.export_name = "A";

  HierarchyContext ctx_b = {0};
  ctx_b.object = fake_pointer(2);
  ctx_b.export_parent = nullptr;
  ctx_b.duplicator = fake_pointer(1);
  ctx_b.export_name = "B";

  HierarchyContext ctx_c = {0};
  ctx_c.object = fake_pointer(2);
  ctx_c.export_parent = fake_pointer(2);
  ctx_c.duplicator = fake_pointer(1);
  ctx_c.export_name = "C";

  HierarchyContext ctx_d = {0};
  ctx_d.object = fake_pointer(2);
  ctx_d.export_parent = fake_pointer(3);
  ctx_d.duplicator = nullptr;
  ctx_d.export_name = "D";

  EXPECT_LT(ctx_a, ctx_b);
  EXPECT_LT(ctx_a, ctx_c);
  EXPECT_LT(ctx_a, ctx_d);
  EXPECT_LT(ctx_b, ctx_c);
  EXPECT_LT(ctx_b, ctx_d);
  EXPECT_LT(ctx_c, ctx_d);

  EXPECT_FALSE(ctx_b < ctx_a);
  EXPECT_FALSE(ctx_c < ctx_a);
  EXPECT_FALSE(ctx_d < ctx_a);
  EXPECT_FALSE(ctx_c < ctx_b);
  EXPECT_FALSE(ctx_d < ctx_b);
  EXPECT_FALSE(ctx_d < ctx_c);
}
