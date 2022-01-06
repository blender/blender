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

#include "BLI_vector.hh"

#include "gpu_query.hh"

#include "glew-mx.h"

namespace blender::gpu {

class GLQueryPool : public QueryPool {
 private:
  /** Contains queries object handles. */
  Vector<GLuint> query_ids_;
  /** Type of this query pool. */
  GPUQueryType type_;
  /** Associated GL type. */
  GLenum gl_type_;
  /** Number of queries that have been issued since last initialization.
   * Should be equal to query_ids_.size(). */
  uint32_t query_issued_;
  /** Can only be initialized once. */
  bool initialized_ = false;

 public:
  ~GLQueryPool();

  void init(GPUQueryType type) override;

  void begin_query() override;
  void end_query() override;

  void get_occlusion_result(MutableSpan<uint32_t> r_values) override;
};

static inline GLenum to_gl(GPUQueryType type)
{
  if (type == GPU_QUERY_OCCLUSION) {
    /* TODO(fclem): try with GL_ANY_SAMPLES_PASSED​. */
    return GL_SAMPLES_PASSED;
  }
  BLI_assert(0);
  return GL_SAMPLES_PASSED;
}

}  // namespace blender::gpu
