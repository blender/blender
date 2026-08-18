/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gl_work_in_flight.hh"

namespace blender::gpu {

GLWorkInFlight::GLWorkInFlight(unsigned int max_in_flight)
{
  async_fences_.resize(max_in_flight);
  async_fences_.fill(0);
}

GLWorkInFlight::~GLWorkInFlight()
{
  delete_fences();
}

void GLWorkInFlight::delete_fences()
{
  for (GLsync fence : async_fences_) {
    if (fence != 0) {
      glDeleteSync(fence);
    }
  }
}

void GLWorkInFlight::reset()
{
  delete_fences();
  async_fences_.fill(0);
  work_index_ = 0;
}

void GLWorkInFlight::begin_work()
{
  GLsync async_fence_prev = async_fences_[work_index_];
  if (async_fence_prev != 0) {
    while (glClientWaitSync(async_fence_prev, GL_SYNC_FLUSH_COMMANDS_BIT, 1000) ==
           GL_TIMEOUT_EXPIRED)
    {
      /* Repeat until the work has finished. */
    }
  }
}

void GLWorkInFlight::end_work()
{
  async_fences_[work_index_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  work_index_ = (work_index_ + 1) % async_fences_.size();
}

}  // namespace blender::gpu
