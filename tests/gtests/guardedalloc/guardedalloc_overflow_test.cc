/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

/* We expect to abort on integer overflow, to prevent possible exploits. */
#ifdef _WIN32
#define ABORT_PREDICATE ::testing::ExitedWithCode(3)
#else
#define ABORT_PREDICATE ::testing::KilledBySignal(SIGABRT)
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

TEST(guardedalloc, LockfreeIntegerOverflow)
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

TEST(guardedalloc, GuardedIntegerOverflow)
{
	MEM_use_guarded_allocator();

	MallocArray(1, SIZE_MAX);
	CallocArray(SIZE_MAX, 1);
	MallocArray(SIZE_MAX / 2, 2);
	CallocArray(SIZE_MAX / 1234567, 1234567);

	EXPECT_EXIT(MallocArray(SIZE_MAX, 2), ABORT_PREDICATE, "");
	EXPECT_EXIT(CallocArray(7, SIZE_MAX), ABORT_PREDICATE, "");
	EXPECT_EXIT(MallocArray(SIZE_MAX, 12345567), ABORT_PREDICATE, "");
	EXPECT_EXIT(CallocArray(SIZE_MAX, SIZE_MAX), ABORT_PREDICATE, "");
}

