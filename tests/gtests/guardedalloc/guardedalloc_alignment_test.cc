/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#define CHECK_ALIGNMENT(ptr, align) EXPECT_EQ((size_t)ptr % align, 0)

namespace {

void DoBasicAlignmentChecks(const int alignment)
{
  int *foo, *bar;

  foo = (int *)MEM_mallocN_aligned(sizeof(int) * 10, alignment, "test");
  CHECK_ALIGNMENT(foo, alignment);

  bar = (int *)MEM_dupallocN(foo);
  CHECK_ALIGNMENT(bar, alignment);
  MEM_freeN(bar);

  foo = (int *)MEM_reallocN(foo, sizeof(int) * 5);
  CHECK_ALIGNMENT(foo, alignment);

  foo = (int *)MEM_recallocN(foo, sizeof(int) * 5);
  CHECK_ALIGNMENT(foo, alignment);

  MEM_freeN(foo);
}

}  // namespace

TEST(guardedalloc, LockfreeAlignedAlloc1)
{
  DoBasicAlignmentChecks(1);
}

TEST(guardedalloc, GuardedAlignedAlloc1)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(1);
}

TEST(guardedalloc, LockfreeAlignedAlloc2)
{
  DoBasicAlignmentChecks(2);
}

TEST(guardedalloc, GuardedAlignedAlloc2)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(2);
}

TEST(guardedalloc, LockfreeAlignedAlloc4)
{
  DoBasicAlignmentChecks(4);
}

TEST(guardedalloc, GuardedAlignedAlloc4)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(4);
}

TEST(guardedalloc, LockfreeAlignedAlloc8)
{
  DoBasicAlignmentChecks(8);
}

TEST(guardedalloc, GuardedAlignedAlloc8)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(8);
}

TEST(guardedalloc, LockfreeAlignedAlloc16)
{
  DoBasicAlignmentChecks(16);
}

TEST(guardedalloc, GuardedAlignedAlloc16)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(16);
}

TEST(guardedalloc, LockfreeAlignedAlloc32)
{
  DoBasicAlignmentChecks(32);
}

TEST(guardedalloc, GuardedAlignedAlloc32)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(32);
}

TEST(guardedalloc, LockfreeAlignedAlloc256)
{
  DoBasicAlignmentChecks(256);
}

TEST(guardedalloc, GuardedAlignedAlloc256)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(256);
}

TEST(guardedalloc, LockfreeAlignedAlloc512)
{
  DoBasicAlignmentChecks(512);
}

TEST(guardedalloc, GuardedAlignedAlloc512)
{
  MEM_use_guarded_allocator();
  DoBasicAlignmentChecks(512);
}
