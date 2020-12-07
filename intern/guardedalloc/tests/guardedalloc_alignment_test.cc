/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"
#include "guardedalloc_test_base.h"

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

TEST_F(LockFreeAllocatorTest, MEM_mallocN_aligned)
{
  DoBasicAlignmentChecks(1);
  DoBasicAlignmentChecks(2);
  DoBasicAlignmentChecks(4);
  DoBasicAlignmentChecks(8);
  DoBasicAlignmentChecks(16);
  DoBasicAlignmentChecks(32);
  DoBasicAlignmentChecks(256);
  DoBasicAlignmentChecks(512);
}

TEST_F(GuardedAllocatorTest, MEM_mallocN_aligned)
{
  DoBasicAlignmentChecks(1);
  DoBasicAlignmentChecks(2);
  DoBasicAlignmentChecks(4);
  DoBasicAlignmentChecks(8);
  DoBasicAlignmentChecks(16);
  DoBasicAlignmentChecks(32);
  DoBasicAlignmentChecks(256);
  DoBasicAlignmentChecks(512);
}
