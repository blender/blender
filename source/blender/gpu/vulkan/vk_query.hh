/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_vector.hh"

#include "gpu_query.hh"

#include "vk_common.hh"

namespace blender::gpu {

class VKQueryPool : public QueryPool {
  const uint32_t query_chunk_len_ = 256;
  Vector<VkQueryPool> vk_query_pools_;
  VkQueryType vk_query_type_;
  uint32_t queries_issued_ = 0;

 protected:
  ~VKQueryPool();

 public:
  void init(GPUQueryType type) override;
  void begin_query() override;
  void end_query() override;
  void get_occlusion_result(MutableSpan<uint32_t> r_values) override;

 private:
  uint32_t query_index_in_pool() const;
};

}  // namespace blender::gpu
