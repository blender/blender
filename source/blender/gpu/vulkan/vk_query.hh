/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_query.hh"

namespace blender::gpu {

class VKQueryPool : public QueryPool {
 public:
  void init(GPUQueryType type) override;
  void begin_query() override;
  void end_query() override;
  void get_occlusion_result(MutableSpan<uint32_t> r_values) override;
};

}  // namespace blender::gpu
