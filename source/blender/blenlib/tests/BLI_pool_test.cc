/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_exception_safety_test_utils.hh"
#include "BLI_pool.hh"
#include "BLI_strict_flags.h"
#include "testing/testing.h"

namespace blender::tests {

TEST(pool, DefaultConstructor)
{
  Pool<int> pool;
  EXPECT_EQ(pool.size(), 0);
}

TEST(pool, Allocation)
{
  Vector<int *> ptrs;
  Pool<int> pool;
  for (int i = 0; i < 100; i++) {
    ptrs.append(&pool.construct(i));
  }
  EXPECT_EQ(pool.size(), 100);

  for (int *ptr : ptrs) {
    pool.destruct(*ptr);
  }
  EXPECT_EQ(pool.size(), 0);
}

TEST(pool, Reuse)
{
  Vector<int *> ptrs;
  Pool<int> pool;
  for (int i = 0; i < 32; i++) {
    ptrs.append(&pool.construct(i));
  }

  int *freed_ptr = ptrs[6];
  pool.destruct(*freed_ptr);

  ptrs[6] = &pool.construct(0);

  EXPECT_EQ(ptrs[6], freed_ptr);

  for (int *ptr : ptrs) {
    pool.destruct(*ptr);
  }
}

}  // namespace blender::tests
