/* SPDX-FileCopyrightText: 2020-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <map>
#include <memory>

#include "GHOST_Xr_intern.hh"

class GHOST_XrContext;
class GHOST_XrSwapchain;
struct GHOST_XrDrawInfo;
struct OpenXRSessionData;

class GHOST_XrSession {
 public:
  enum LifeExpectancy {
    SESSION_KEEP_ALIVE,
    SESSION_DESTROY,
  };

  GHOST_XrSession(GHOST_XrContext &xr_context);
  ~GHOST_XrSession();

  void start(const GHOST_XrSessionBeginInfo *begin_info);
  void requestEnd();

  LifeExpectancy handleStateChangeEvent(const XrEventDataSessionStateChanged &lifecycle);

  bool isRunning() const;
  bool needsUpsideDownDrawing() const;

  void unbindGraphicsContext(); /* Public so context can ensure it's unbound as needed. */

  void draw(void *draw_customdata);

  /** Action functions to be called pre-session start.
   * NOTE: The "destroy" functions can also be called post-session start. */
  bool createActionSet(const GHOST_XrActionSetInfo &info);
  void destroyActionSet(const char *action_set_name);
  bool createActions(const char *action_set_name, uint32_t count, const GHOST_XrActionInfo *infos);
  void destroyActions(const char *action_set_name,
                      uint32_t count,
                      const char *const *action_names);
  bool createActionBindings(const char *action_set_name,
                            uint32_t count,
                            const GHOST_XrActionProfileInfo *infos);
  void destroyActionBindings(const char *action_set_name,
                             uint32_t count,
                             const char *const *action_names,
                             const char *const *profile_paths);
  bool attachActionSets();

  /**
   * Action functions to be called post-session start.
   * \param action_set_name: When `nullptr`, all attached action sets will be synced.
   */
  bool syncActions(const char *action_set_name = nullptr);
  bool applyHapticAction(const char *action_set_name,
                         const char *action_name,
                         const char *subaction_path,
                         const int64_t &duration,
                         const float &frequency,
                         const float &amplitude);
  void stopHapticAction(const char *action_set_name,
                        const char *action_name,
                        const char *subaction_path);

  /* Custom data (owned by Blender, not GHOST) accessors. */
  void *getActionSetCustomdata(const char *action_set_name);
  void *getActionCustomdata(const char *action_set_name, const char *action_name);
  uint32_t getActionCount(const char *action_set_name);
  void getActionCustomdataArray(const char *action_set_name, void **r_customdata_array);

  /** Controller model functions. */
  bool loadControllerModel(const char *subaction_path);
  void unloadControllerModel(const char *subaction_path);
  bool updateControllerModelComponents(const char *subaction_path);
  bool getControllerModelData(const char *subaction_path, GHOST_XrControllerModelData &r_data);

 private:
  /** Pointer back to context managing this session. Would be nice to avoid, but needed to access
   * custom callbacks set before session start. */
  class GHOST_XrContext *m_context;

  std::unique_ptr<OpenXRSessionData> m_oxr; /* Could use stack, but PImpl is preferable. */

  /** Active Ghost graphic context. Owned by Blender, not GHOST. */
  class GHOST_Context *m_gpu_ctx = nullptr;
  std::unique_ptr<class GHOST_IXrGraphicsBinding> m_gpu_binding;

  /** Rendering information. Set when drawing starts. */
  std::unique_ptr<GHOST_XrDrawInfo> m_draw_info;

  void initSystem();
  void beginSession();
  void endSession();

  void bindGraphicsContext();

  void prepareDrawing();
  XrCompositionLayerProjection drawLayer(
      std::vector<XrCompositionLayerProjectionView> &r_proj_layer_views, void *draw_customdata);
  void drawView(GHOST_XrSwapchain &swapchain,
                XrCompositionLayerProjectionView &r_proj_layer_view,
                XrSpaceLocation &view_location,
                XrView &view,
                uint32_t view_idx,
                void *draw_customdata);
  void beginFrameDrawing();
  void endFrameDrawing(std::vector<XrCompositionLayerBaseHeader *> &layers);
};
