/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_vector.hh"

#include "gpu_query.hh"
#include "mtl_context.hh"

namespace blender::gpu {

class MTLQueryPool : public QueryPool {
 private:
  /** Number of queries that have been issued since last initialization.
   * Should be equal to query_ids_.size(). */
  uint32_t query_issued_;
  /** Type of this query pool. */
  GPUQueryType type_;
  /** Can only be initialized once. */
  bool initialized_ = false;
  MTLVisibilityResultMode mtl_type_;
  Vector<gpu::MTLBuffer *> buffer_;

  void allocate();

 public:
  MTLQueryPool();
  ~MTLQueryPool();

  void init(GPUQueryType type) override;

  void begin_query() override;
  void end_query() override;

  void get_occlusion_result(MutableSpan<uint32_t> r_values) override;
};
}  // namespace blender::gpu
