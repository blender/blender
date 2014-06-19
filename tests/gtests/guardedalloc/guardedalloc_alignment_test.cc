/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#define CHECK_ALIGNMENT(ptr, align) EXPECT_EQ(0, (size_t)ptr % align)

namespace {

void DoBasicAlignmentChecks(const int alignment)
{
	int *foo, *bar;

	foo = (int *) MEM_mallocN_aligned(sizeof(int) * 10, alignment, "test");
	CHECK_ALIGNMENT(foo, alignment);

	bar = (int *) MEM_dupallocN(foo);
	CHECK_ALIGNMENT(bar, alignment);
	MEM_freeN(bar);

	foo = (int *) MEM_reallocN(foo, sizeof(int) * 5);
	CHECK_ALIGNMENT(foo, alignment);

	foo = (int *) MEM_recallocN(foo, sizeof(int) * 5);
	CHECK_ALIGNMENT(foo, alignment);

	MEM_freeN(foo);
}

}  // namespace

TEST(guardedalloc, LockfreeAlignedAlloc16)
{
	DoBasicAlignmentChecks(16);
}

TEST(guardedalloc, GuardedAlignedAlloc16)
{
	MEM_use_guarded_allocator();
	DoBasicAlignmentChecks(16);
}

// On Apple we currently support 16 bit alignment only.
// Harmless for Blender, but would be nice to support
// eventually.
#ifndef __APPLE__
TEST(guardedalloc, GuardedAlignedAlloc32)
{
	MEM_use_guarded_allocator();
	DoBasicAlignmentChecks(16);
}
#endif
