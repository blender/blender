/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_span.hh"

namespace blender::gpu {

#define QUERY_MIN_LEN 16

enum GPUQueryType {
  GPU_QUERY_OCCLUSION = 0,
};

class QueryPool {
 public:
  virtual ~QueryPool(){};

  /**
   * Will start and end the query at this index inside the pool. The pool will resize
   * automatically but does not support sparse allocation. So prefer using consecutive indices.
   */
  virtual void init(GPUQueryType type) = 0;

  /**
   * Will start and end the query at this index inside the pool.
   * The pool will resize automatically.
   */
  virtual void begin_query() = 0;
  virtual void end_query() = 0;

  /**
   * Must be fed with a buffer large enough to contain all the queries issued.
   * IMPORTANT: Result for each query can be either binary or represent the number of samples
   * drawn.
   */
  virtual void get_occlusion_result(MutableSpan<uint32_t> r_values) = 0;
};

}  // namespace blender::gpu
