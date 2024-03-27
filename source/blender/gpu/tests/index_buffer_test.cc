/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "GPU_index_buffer.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

static void test_index_buffer_subbuilders()
{
  const uint num_subbuilders = 10;
  const uint verts_per_subbuilders = 100;
  const uint vertex_len = num_subbuilders * verts_per_subbuilders;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, vertex_len, vertex_len);

  GPUIndexBufBuilder subbuilders[num_subbuilders];
  for (int subbuilder_index = 0; subbuilder_index < num_subbuilders; subbuilder_index++) {
    memcpy(&subbuilders[subbuilder_index], &builder, sizeof(builder));
  }

  for (int subbuilder_index = 0; subbuilder_index < num_subbuilders; subbuilder_index++) {
    GPUIndexBufBuilder &subbuilder = subbuilders[subbuilder_index];
    for (int subbuilder_vert_index = 0; subbuilder_vert_index < verts_per_subbuilders;
         subbuilder_vert_index++)
    {
      int vert_index_to_update = subbuilder_index * verts_per_subbuilders + subbuilder_vert_index;
      GPU_indexbuf_set_point_vert(&subbuilder, vert_index_to_update, vert_index_to_update);
    }
  }

  for (int subbuilder_index = 0; subbuilder_index < num_subbuilders; subbuilder_index++) {
    EXPECT_EQ(builder.index_len, subbuilder_index * verts_per_subbuilders);
    GPU_indexbuf_join(&builder, &subbuilders[subbuilder_index]);
    EXPECT_EQ(builder.index_len, (subbuilder_index + 1) * verts_per_subbuilders);
  }

  IndexBuf *index_buffer = GPU_indexbuf_build(&builder);
  EXPECT_NE(index_buffer, nullptr);
  GPU_INDEXBUF_DISCARD_SAFE(index_buffer);
}

GPU_TEST(index_buffer_subbuilders)

}  // namespace blender::gpu::tests
