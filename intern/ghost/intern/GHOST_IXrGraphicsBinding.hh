/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "GHOST_Types.h"
#include "GHOST_Xr_openxr_includes.hh"

class GHOST_IXrGraphicsBinding {
 public:
  union {
#if defined(WITH_GHOST_X11)
#  if defined(WITH_OPENGL_BACKEND)
    XrGraphicsBindingEGLMNDX egl;
    XrGraphicsBindingOpenGLXlibKHR glx;
#  endif
#endif
#if defined(WIN32)
#  if defined(WITH_OPENGL_BACKEND)
    XrGraphicsBindingOpenGLWin32KHR wgl;
#  endif
    XrGraphicsBindingD3D11KHR d3d11;
#endif
#if defined(WITH_GHOST_WAYLAND)
#  if defined(WITH_OPENGL_BACKEND)
    XrGraphicsBindingOpenGLWaylandKHR wl;
#  endif
#endif
#ifdef WITH_VULKAN_BACKEND
    XrGraphicsBindingVulkanKHR vk;
#endif
#ifdef WITH_METAL_BACKEND
    XrGraphicsBindingMetalKHR metal;
#endif
  } oxr_binding;

  virtual ~GHOST_IXrGraphicsBinding() = default;

  /**
   * \brief load the functions for the graphics binding.
   *
   * A graphic binding uses an extension. These extensions provide graphics platform specific
   * functions. This function is called before #checkVersionRequirements allowing to preload the
   * function pointers.
   *
   * \param instance: XrInstance where to retrieve the functions from.
   *
   * \returns true when the extensions functions were successfully loaded, return false when
   *    the extension functions could not be loaded or would not allow to use the binding.
   *
   * \note These functions should be stored as instance variable as they can differ between
   * XrSessions.
   */
  virtual bool loadExtensionFunctions(XrInstance instance) = 0;

  /**
   * Does __not__ require this object to be initialized (can be called prior to
   * #initFromGhostContext). It's actually meant to be called first.
   *
   * \param r_requirement_info: Return argument to retrieve an informal string on the requirements.
   * to be met. Useful for error/debug messages.
   */
  virtual bool checkVersionRequirements(class GHOST_Context &ghost_ctx,
                                        XrInstance instance,
                                        XrSystemId system_id,
                                        std::string *r_requirement_info) const = 0;
  virtual void initFromGhostContext(class GHOST_Context &ghost_ctx,
                                    XrInstance instance,
                                    XrSystemId system_id) = 0;
  virtual std::optional<int64_t> chooseSwapchainFormat(const std::vector<int64_t> &runtime_formats,
                                                       GHOST_TXrSwapchainFormat &r_format,
                                                       bool &r_is_rgb_format) const = 0;
  virtual std::vector<XrSwapchainImageBaseHeader *> createSwapchainImages(
      uint32_t image_count) = 0;
  virtual void submitToSwapchainBegin() = 0;
  virtual void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                                      const GHOST_XrDrawViewInfo &draw_info) = 0;
  virtual void submitToSwapchainEnd() = 0;
  virtual bool needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const = 0;

 protected:
  /* Use GHOST_XrGraphicsBindingCreateFromType! */
  GHOST_IXrGraphicsBinding() = default;
};

std::unique_ptr<GHOST_IXrGraphicsBinding> GHOST_XrGraphicsBindingCreateFromType(
    GHOST_TXrGraphicsBinding type, GHOST_Context &context);
