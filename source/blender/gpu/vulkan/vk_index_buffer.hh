/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_index_buffer_private.hh"

#include "vk_buffer.hh"

namespace blender::gpu {

class VKIndexBuffer : public IndexBuf {
  VKBuffer buffer_;

 public:
  void upload_data() override;

  void bind_as_ssbo(uint binding) override;

  void read(uint32_t *data) const override;

  void update_sub(uint start, uint len, const void *data) override;

  VkBuffer vk_handle()
  {
    return buffer_.vk_handle();
  }

 private:
  void strip_restart_indices() override;
  void allocate(VKContext &context);
};

}  // namespace blender::gpu
