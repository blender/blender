/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_batch.hh"

namespace blender::gpu {

class DummyBatch : public Batch {
 public:
  void draw(int /*vertex_first*/,
            int /*vertex_count*/,
            int /*instance_first*/,
            int /*instance_count*/) override
  {
  }
  void draw_indirect(GPUStorageBuf * /*indirect_buf*/, intptr_t /*offset*/) override {}
  void multi_draw_indirect(GPUStorageBuf * /*indirect_buf*/,
                           int /*count*/,
                           intptr_t /*offset*/,
                           intptr_t /*stride*/) override
  {
  }
};

}  // namespace blender::gpu
