/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_Context class.
 */

#pragma once

#include "GHOST_IContext.hh"
#include "GHOST_Types.h"

#include <cstdlib> /* For `nullptr`. */

class GHOST_Context : public GHOST_IContext {
 protected:
  static thread_local inline GHOST_Context *active_context_;

 public:
  /**
   * Constructor.
   * \param context_params: Parameters to use when initializing the context.
   */
  GHOST_Context(const GHOST_ContextParams &context_params) : context_params_(context_params) {}

  /**
   * Destructor.
   */
  ~GHOST_Context() override
  {
    if (active_context_ == this) {
      active_context_ = nullptr;
    }
  };

  /**
   * Returns the thread's currently active drawing context.
   */
  static inline GHOST_Context *getActiveDrawingContext()
  {
    return active_context_;
  }

  /** \copydoc #GHOST_IContext::swapBuffersAcquire */
  GHOST_TSuccess swapBufferAcquire() override = 0;

  /** \copydoc #GHOST_IContext::swapBuffers */
  GHOST_TSuccess swapBufferRelease() override = 0;

  /** \copydoc #GHOST_IContext::activateDrawingContext */
  GHOST_TSuccess activateDrawingContext() override = 0;

  /** \copydoc #GHOST_IContext::releaseDrawingContext */
  GHOST_TSuccess releaseDrawingContext() override = 0;

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  virtual GHOST_TSuccess initializeDrawingContext() = 0;

  /**
   * Updates the drawing context of this window. Needed
   * whenever the window is changed.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess updateDrawingContext()
  {
    return GHOST_kFailure;
  }

  /**
   * Checks if it is OK for a remove the native display
   * \return Indication as to whether removal has succeeded.
   */
  virtual GHOST_TSuccess releaseNativeHandles() = 0;

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess setSwapInterval(int /*interval*/)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param interval_out: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  virtual GHOST_TSuccess getSwapInterval(int & /*interval*/)
  {
    return GHOST_kFailure;
  }

  /**
   * Get user data.
   */
  void *getUserData()
  {
    return user_data_;
  }

  /**
   * Set user data (intended for the caller to use as needed).
   */
  void setUserData(void *user_data)
  {
    user_data_ = user_data;
  }

  /**
   * Stereo visual created. Only necessary for 'real' stereo support,
   * ie quad buffered stereo. This is not always possible, depends on
   * the graphics h/w
   */
  bool isStereoVisual() const
  {
    return context_params_.is_stereo_visual;
  }

  /** Get the VSync value. */
  virtual GHOST_TVSyncModes getVSync()
  {
    return context_params_.vsync;
  }

  /**
   * Returns if the context is rendered upside down compared to OpenGL.
   */
  virtual bool isUpsideDown() const
  {
    return false;
  }

  /** \copydoc #GHOST_IContext::getDefaultFramebuffer */
  unsigned int getDefaultFramebuffer() override
  {
    return 0;
  }

#ifdef WITH_VULKAN_BACKEND
  /** \copydoc #GHOST_IContext::getVulkanHandles */
  virtual GHOST_TSuccess getVulkanHandles(GHOST_VulkanHandles & /* r_handles */) override
  {
    return GHOST_kFailure;
  };
  /** \copydoc #GHOST_IContext::getVulkanSwapChainFormat */
  virtual GHOST_TSuccess getVulkanSwapChainFormat(
      GHOST_VulkanSwapChainData * /*r_swap_chain_data*/) override
  {
    return GHOST_kFailure;
  }

  /** \copydoc #GHOST_IContext::setVulkanSwapBuffersCallbacks */
  virtual GHOST_TSuccess setVulkanSwapBuffersCallbacks(
      std::function<void(const GHOST_VulkanSwapChainData *)> /*swap_buffer_draw_callback*/,
      std::function<void(void)> /*swap_buffer_acquired_callback*/,
      std::function<void(GHOST_VulkanOpenXRData *)> /*openxr_acquire_framebuffer_image_callback*/,
      std::function<void(GHOST_VulkanOpenXRData *)> /*openxr_release_framebuffer_image_callback*/)
      override
  {
    return GHOST_kFailure;
  }
#endif

 protected:
  GHOST_ContextParams context_params_;

  /** Caller specified, not for internal use. */
  void *user_data_ = nullptr;

#ifdef WITH_OPENGL_BACKEND
  static void initClearGL();
#endif

  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_Context")
};

#ifdef _WIN32
bool win32_chk(bool result, const char *file = nullptr, int line = 0, const char *text = nullptr);
bool win32_silent_chk(bool result);

#  ifndef NDEBUG
#    define WIN32_CHK(x) win32_chk((x), __FILE__, __LINE__, #x)
#  else
#    define WIN32_CHK(x) win32_chk(x)
#  endif

#  define WIN32_CHK_SILENT(x, silent) ((silent) ? win32_silent_chk(x) : WIN32_CHK(x))
#endif /* _WIN32 */
