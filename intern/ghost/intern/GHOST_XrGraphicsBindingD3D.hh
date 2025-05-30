/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */
#pragma once

#ifndef _WIN32
#  error "GHOST_XrGraphcisBindingD3D can only be compiled on Windows platforms."
#endif

#include "GHOST_ContextD3D.hh"
#include "GHOST_ContextWGL.hh"
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif
#include "GHOST_IXrGraphicsBinding.hh"

/**
 * Base class for bridging to an OpenXR platform that only supports Direct3D.
 *
 * OpenGL/Vulkan have their own specific implementations.
 */
class GHOST_XrGraphicsBindingD3D : public GHOST_IXrGraphicsBinding {
 public:
  GHOST_XrGraphicsBindingD3D();
  ~GHOST_XrGraphicsBindingD3D() override;

  /**
   * Check the version requirements to use OpenXR with the Vulkan backend.
   */
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

  void submitToSwapchainBegin() override {}
  void submitToSwapchainEnd() override {}

  bool needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const override;

 protected:
  /** Secondary DirectX 11 context used by OpenXR. */
  GHOST_ContextD3D *m_ghost_d3d_ctx = nullptr;

  std::list<std::vector<XrSwapchainImageD3D11KHR>> m_image_cache;
};

/**
 * OpenXR bridge between OpenGL and D3D.
 *
 * The D3D swapchain image is imported into OpenGL.
 */
class GHOST_XrGraphicsBindingOpenGLD3D : public GHOST_XrGraphicsBindingD3D {
 public:
  GHOST_XrGraphicsBindingOpenGLD3D(GHOST_Context &ghost_ctx);
  ~GHOST_XrGraphicsBindingOpenGLD3D();

  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override;

 private:
  /** Primary OpenGL context for Blender to use for drawing. */
  GHOST_ContextWGL &m_ghost_wgl_ctx;
  /** Handle to shared resource object. */
  GHOST_SharedOpenGLResource *m_shared_resource = nullptr;
};

#ifdef WITH_VULKAN_BACKEND
/**
 * OpenXR bridge between Vulkan and D3D.
 */
class GHOST_XrGraphicsBindingVulkanD3D : public GHOST_XrGraphicsBindingD3D {
 public:
  GHOST_XrGraphicsBindingVulkanD3D(GHOST_Context &ghost_ctx);
  ~GHOST_XrGraphicsBindingVulkanD3D();

  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override;

 private:
  /** Primary Vulkan context for Blender to use for drawing. */
  GHOST_ContextVK &m_ghost_ctx;
};
#endif
