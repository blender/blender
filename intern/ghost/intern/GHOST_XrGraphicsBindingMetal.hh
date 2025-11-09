/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef __APPLE__
#  error "GHOST_XrGraphicsBindingMetal can be only compiled on macOS."
#endif

class GHOST_ContextMTL;

#include <list>
#include <vector>

#include "GHOST_IXrGraphicsBinding.hh"
#include "GHOST_Types.h"

class GHOST_XrGraphicsBindingMetal : public GHOST_IXrGraphicsBinding {
 public:
  GHOST_XrGraphicsBindingMetal(GHOST_Context &ghost_ctx);

  bool checkVersionRequirements(GHOST_Context &ghost_ctx,
                                XrInstance instance,
                                XrSystemId system_id,
                                std::string *r_requirement_info) const override;

  void initFromGhostContext(GHOST_Context &ghost_ctx,
                            XrInstance instance,
                            XrSystemId system_id) override;
  std::optional<int64_t> chooseSwapchainFormat(const std::vector<int64_t> &runtime_formats,
                                               GHOST_TXrSwapchainFormat &r_format,
                                               bool &r_is_srgb_format) const override;
  std::vector<XrSwapchainImageBaseHeader *> createSwapchainImages(uint32_t image_count) override;

  void submitToSwapchainBegin() override;
  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override;
  void submitToSwapchainEnd() override;

  bool needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const override;

 protected:
  GHOST_ContextMTL *ghost_metal_ctx_ = nullptr;
  std::list<std::vector<XrSwapchainImageMetalKHR>> image_cache_;
};
