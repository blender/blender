/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2020, Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_span.hh"

namespace blender::gpu {

typedef enum GPUQueryType {
  GPU_QUERY_OCCLUSION = 0,
} GPUQueryType;

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
