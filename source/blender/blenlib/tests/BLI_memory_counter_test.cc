/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_memory_counter.hh"

#include "testing/testing.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender::tests {

TEST(memory_counter, Simple)
{
  MemoryCount memory_count;
  MemoryCounter memory{memory_count};
  EXPECT_EQ(memory_count.total_bytes, 0);
  memory.add(10);
  EXPECT_EQ(memory_count.total_bytes, 10);
  memory.add(10);
  EXPECT_EQ(memory_count.total_bytes, 20);

  const int alloc_size = 100;
  void *data1 = MEM_mallocN(alloc_size, __func__);
  void *data2 = MEM_mallocN(alloc_size, __func__);
  const ImplicitSharingPtr sharing_info1{implicit_sharing::info_for_mem_free(data1)};
  const ImplicitSharingPtr sharing_info2{implicit_sharing::info_for_mem_free(data2)};

  memory.add_shared(sharing_info1.get(), alloc_size);
  EXPECT_EQ(memory_count.total_bytes, 120);

  memory.add_shared(sharing_info1.get(), [&](MemoryCounter & /*shared_memory*/) { FAIL(); });
  EXPECT_EQ(memory_count.total_bytes, 120);

  memory.add_shared(sharing_info2.get(),
                    [&](MemoryCounter &shared_memory) { shared_memory.add(alloc_size); });
  EXPECT_EQ(memory_count.total_bytes, 220);

  memory.add_shared(nullptr, 1000);
  EXPECT_EQ(memory_count.total_bytes, 1220);

  memory.add_shared(nullptr, 1000);
  EXPECT_EQ(memory_count.total_bytes, 2220);
}

}  // namespace blender::tests
