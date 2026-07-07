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
  /**
   * \brief return the swapchain format selected by the graphic binding.
   *
   * The GPUFormat is the result of the call to #GHOST_IXrGraphicsBinding.chooseSwapchainFormat.
   * The selection is kept as there is no 1-to-1 mapping with GHOST_TXrSwapchainFormat.
   *
   * \todo we should consider to refactor GHOST_TXrSwapchainFormat as it limits color management
   * and actual data formats. Perhaps using a struct.
   */
  int64_t getGPUFormat() const;
  bool isBufferSRGB() const;

 private:
  std::unique_ptr<OpenXRSwapchainData> oxr_; /* Could use stack, but PImpl is preferable. */
  int32_t image_width_, image_height_;
  GHOST_TXrSwapchainFormat format_;
  int64_t gpu_format_;
  bool is_srgb_buffer_ = false;
};
