/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <memory>

struct OpenXRSwapchainData;

class GHOST_XrSwapchain {
 public:
  GHOST_XrSwapchain(GHOST_IXrGraphicsBinding &gpu_binding,
                    const XrSession &session,
                    const XrViewConfigurationView &view_config);
  GHOST_XrSwapchain(GHOST_XrSwapchain &&other);
  ~GHOST_XrSwapchain();

  XrSwapchainImageBaseHeader *acquireDrawableSwapchainImage();
  void releaseImage();

  void updateCompositionLayerProjectViewSubImage(XrSwapchainSubImage &r_sub_image);

  GHOST_TXrSwapchainFormat getFormat() const;
  bool isBufferSRGB() const;

 private:
  std::unique_ptr<OpenXRSwapchainData> oxr_; /* Could use stack, but PImpl is preferable. */
  int32_t image_width_, image_height_;
  GHOST_TXrSwapchainFormat format_;
  bool is_srgb_buffer_ = false;
};
