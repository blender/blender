/* SPDX-FileCopyrightText: 2018-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "guardedalloc_test_base.h"

/* We expect to abort on integer overflow, to prevent possible exploits. */

#if defined(__GNUC__) && !defined(__clang__)
/* Disable since it's the purpose of this test. */
#  pragma GCC diagnostic ignored "-Walloc-size-larger-than="
#endif

namespace {

void MallocArray(size_t len, size_t size)
{
  void *mem = MEM_malloc_arrayN(len, size, "MallocArray");
  if (mem) {
    MEM_freeN(mem);
  }
}

void CallocArray(size_t len, size_t size)
{
  void *mem = MEM_calloc_arrayN(len, size, "CallocArray");
  if (mem) {
    MEM_freeN(mem);
  }
}

}  // namespace

TEST_F(LockFreeAllocatorTest, LockfreeIntegerOverflow)
{
  MallocArray(1, SIZE_MAX);
  CallocArray(SIZE_MAX, 1);
  MallocArray(SIZE_MAX / 2, 2);
  CallocArray(SIZE_MAX / 1234567, 1234567);

  EXPECT_EXIT(MallocArray(SIZE_MAX, 2), ABORT_PREDICATE, "");
  EXPECT_EXIT(CallocArray(7, SIZE_MAX), ABORT_PREDICATE, "");
  EXPECT_EXIT(MallocArray(SIZE_MAX, 12345567), ABORT_PREDICATE, "");
  EXPECT_EXIT(CallocArray(SIZE_MAX, SIZE_MAX), ABORT_PREDICATE, "");
}

TEST_F(GuardedAllocatorTest, GuardedIntegerOverflow)
{
  MallocArray(1, SIZE_MAX);
  CallocArray(SIZE_MAX, 1);
  MallocArray(SIZE_MAX / 2, 2);
  CallocArray(SIZE_MAX / 1234567, 1234567);

  EXPECT_EXIT(MallocArray(SIZE_MAX, 2), ABORT_PREDICATE, "");
  EXPECT_EXIT(CallocArray(7, SIZE_MAX), ABORT_PREDICATE, "");
  EXPECT_EXIT(MallocArray(SIZE_MAX, 12345567), ABORT_PREDICATE, "");
  EXPECT_EXIT(CallocArray(SIZE_MAX, SIZE_MAX), ABORT_PREDICATE, "");
}
