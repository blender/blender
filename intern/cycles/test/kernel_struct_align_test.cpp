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

#include "kernel/kernel_types.h"

CCL_NAMESPACE_BEGIN

#define TEST_IS_ALIGNED(name) ((sizeof(name) % 16) == 0)

TEST(kernel_struct_align, KernelCamera)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelCamera));
}

TEST(kernel_struct_align, KernelFilm)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelFilm));
}

TEST(kernel_struct_align, KernelBackground)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelBackground));
}

TEST(kernel_struct_align, KernelIntegrator)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelIntegrator));
}

TEST(kernel_struct_align, KernelBVH)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelBVH));
}

TEST(kernel_struct_align, KernelCurves)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelCurves));
}

TEST(kernel_struct_align, KernelTables)
{
	EXPECT_TRUE(TEST_IS_ALIGNED(KernelTables));
}

CCL_NAMESPACE_END
