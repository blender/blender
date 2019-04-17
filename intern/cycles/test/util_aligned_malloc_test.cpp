/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testing/testing.h"

#include "util/util_aligned_malloc.h"

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
