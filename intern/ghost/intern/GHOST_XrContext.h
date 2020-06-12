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
 */

/** \file
 * \ingroup GHOST
 */

#ifndef __GHOST_XRCONTEXT_H__
#define __GHOST_XRCONTEXT_H__

#include <memory>
#include <vector>

#include "GHOST_Xr_intern.h"

#include "GHOST_IXrContext.h"

struct OpenXRInstanceData;

struct GHOST_XrCustomFuncs {
  /** Function to retrieve (possibly create) a graphics context. */
  GHOST_XrGraphicsContextBindFn gpu_ctx_bind_fn = nullptr;
  /** Function to release (possibly free) a graphics context. */
  GHOST_XrGraphicsContextUnbindFn gpu_ctx_unbind_fn = nullptr;

  GHOST_XrSessionExitFn session_exit_fn = nullptr;
  void *session_exit_customdata = nullptr;

  /** Custom per-view draw function for Blender side drawing. */
  GHOST_XrDrawViewFn draw_view_fn = nullptr;
};

/**
 * In some occasions, runtime specific handling is needed, e.g. to work around runtime bugs.
 */
enum GHOST_TXrOpenXRRuntimeID {
  OPENXR_RUNTIME_MONADO,
  OPENXR_RUNTIME_OCULUS,
  OPENXR_RUNTIME_STEAMVR,
  OPENXR_RUNTIME_WMR, /* Windows Mixed Reality */

  OPENXR_RUNTIME_UNKNOWN
};

/**
 * \brief Main GHOST container to manage OpenXR through.
 *
 * Creating a context using #GHOST_XrContextCreate involves dynamically connecting to the OpenXR
 * runtime, likely reading the OS OpenXR configuration (i.e. active_runtime.json). So this is
 * something that should better be done using lazy-initialization.
 */
class GHOST_XrContext : public GHOST_IXrContext {
 public:
  GHOST_XrContext(const GHOST_XrContextCreateInfo *create_info);
  ~GHOST_XrContext();
  void initialize(const GHOST_XrContextCreateInfo *create_info);

  void startSession(const GHOST_XrSessionBeginInfo *begin_info) override;
  void endSession() override;
  bool isSessionRunning() const override;
  void drawSessionViews(void *draw_customdata) override;

  static void setErrorHandler(GHOST_XrErrorHandlerFn handler_fn, void *customdata);
  void dispatchErrorMessage(const class GHOST_XrException *exception) const override;

  void setGraphicsContextBindFuncs(GHOST_XrGraphicsContextBindFn bind_fn,
                                   GHOST_XrGraphicsContextUnbindFn unbind_fn) override;
  void setDrawViewFunc(GHOST_XrDrawViewFn draw_view_fn) override;
  bool needsUpsideDownDrawing() const override;

  void handleSessionStateChange(const XrEventDataSessionStateChanged *lifecycle);

  GHOST_TXrOpenXRRuntimeID getOpenXRRuntimeID() const;
  const GHOST_XrCustomFuncs &getCustomFuncs() const;
  GHOST_TXrGraphicsBinding getGraphicsBindingType() const;
  XrInstance getInstance() const;
  bool isDebugMode() const;
  bool isDebugTimeMode() const;

 private:
  static GHOST_XrErrorHandlerFn s_error_handler;
  static void *s_error_handler_customdata;

  std::unique_ptr<OpenXRInstanceData> m_oxr;

  GHOST_TXrOpenXRRuntimeID m_runtime_id = OPENXR_RUNTIME_UNKNOWN;

  /* The active GHOST XR Session. Null while no session runs. */
  std::unique_ptr<class GHOST_XrSession> m_session;

  /** Active graphics binding type. */
  GHOST_TXrGraphicsBinding m_gpu_binding_type = GHOST_kXrGraphicsUnknown;

  /** Names of enabled extensions. */
  std::vector<const char *> m_enabled_extensions;
  /** Names of enabled API-layers. */
  std::vector<const char *> m_enabled_layers;

  GHOST_XrCustomFuncs m_custom_funcs;

  /** Enable debug message prints and OpenXR API validation layers. */
  bool m_debug = false;
  bool m_debug_time = false;

  void createOpenXRInstance();
  void storeInstanceProperties();
  void initDebugMessenger();

  void printInstanceInfo();
  void printAvailableAPILayersAndExtensionsInfo();
  void printExtensionsAndAPILayersToEnable();

  void initApiLayers();
  void initExtensions();
  void initExtensionsEx(std::vector<XrExtensionProperties> &extensions, const char *layer_name);
  void getAPILayersToEnable(std::vector<const char *> &r_ext_names);
  void getExtensionsToEnable(std::vector<const char *> &r_ext_names);
  GHOST_TXrGraphicsBinding determineGraphicsBindingTypeToEnable(
      const GHOST_XrContextCreateInfo *create_info);
};

#endif  // __GHOST_XRCONTEXT_H__
