/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "dummy_framebuffer.hh"

namespace blender::gpu {

class DummyContext : public Context {
 public:
  DummyContext()
  {
    back_left = active_fb = new DummyFrameBuffer("DummyFramebuffer");
  }
  void activate() override {}
  void deactivate() override {}
  void begin_frame() override {}
  void end_frame() override {}

  void flush() override {}
  void finish() override {}

  void memory_statistics_get(int * /*r_total_mem*/, int * /*r_free_mem*/) override {}

  void debug_group_begin(const char *, int) override {}
  void debug_group_end() override {}
  bool debug_capture_begin(const char * /*title*/) override
  {
    return false;
  }
  void debug_capture_end() override {}
  void *debug_capture_scope_create(const char * /*name*/) override
  {
    return nullptr;
  }
  bool debug_capture_scope_begin(void * /*scope*/) override
  {
    return false;
  }
  void debug_capture_scope_end(void * /*scope*/) override {}
};

}  // namespace blender::gpu
