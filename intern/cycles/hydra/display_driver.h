/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"
#include "session/display_driver.h"
#include "util/thread.h"

#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/texture.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesDisplayDriver final : public CCL_NS::DisplayDriver {
 public:
  HdCyclesDisplayDriver(HdCyclesSession *renderParam, Hgi *hgi);
  ~HdCyclesDisplayDriver();

 private:
  void next_tile_begin() override;

  bool update_begin(const Params &params, int texture_width, int texture_height) override;
  void update_end() override;

  void flush() override;

  CCL_NS::half4 *map_texture_buffer() override;
  void unmap_texture_buffer() override;

  GraphicsInterop graphics_interop_get() override;

  void graphics_interop_activate() override;
  void graphics_interop_deactivate() override;

  void clear() override;

  void draw(const Params &params) override;

  void gl_context_create();
  bool gl_context_enable();
  void gl_context_disable();
  void gl_context_dispose();

  HdCyclesSession *const _renderParam;
  Hgi *const _hgi;

#ifdef _WIN32
  void *hdc_ = nullptr;
  void *gl_context_ = nullptr;
#endif
  CCL_NS::thread_mutex mutex_;

  PXR_NS::HgiTextureHandle texture_;
  unsigned int gl_pbo_id_ = 0;
  CCL_NS::int2 pbo_size_ = CCL_NS::make_int2(0, 0);
  bool need_update_ = false;
  std::atomic_bool need_clear_ = false;

  void *gl_render_sync_ = nullptr;
  void *gl_upload_sync_ = nullptr;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
