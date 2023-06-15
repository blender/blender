/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/aligned_malloc.h"

#define CHECK_ALIGNMENT(ptr, align) EXPECT_EQ((size_t)ptr % align, 0)

CCL_NAMESPACE_BEGIN

TEST(util_aligned_malloc, aligned_malloc_16)
{
  int *mem = (int *)util_aligned_malloc(sizeof(int), 16);
  CHECK_ALIGNMENT(mem, 16);
  util_aligned_free(mem);
}

/* On Apple we currently only support 16 bytes alignment. */
#ifndef __APPLE__
TEST(util_aligned_malloc, aligned_malloc_32)
{
  int *mem = (int *)util_aligned_malloc(sizeof(int), 32);
  CHECK_ALIGNMENT(mem, 32);
  util_aligned_free(mem);
}
#endif /* __APPLE__ */

CCL_NAMESPACE_END
