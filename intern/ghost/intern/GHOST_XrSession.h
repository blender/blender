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

#pragma once

#include <map>
#include <memory>

#include "GHOST_Xr_intern.h"

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
   * Note: The "destroy" functions can also be called post-session start. */
  bool createActionSet(const GHOST_XrActionSetInfo &info);
  void destroyActionSet(const char *action_set_name);
  bool createActions(const char *action_set_name, uint32_t count, const GHOST_XrActionInfo *infos);
  void destroyActions(const char *action_set_name,
                      uint32_t count,
                      const char *const *action_names);
  bool createActionSpaces(const char *action_set_name,
                          uint32_t count,
                          const GHOST_XrActionSpaceInfo *infos);
  void destroyActionSpaces(const char *action_set_name,
                           uint32_t count,
                           const GHOST_XrActionSpaceInfo *infos);
  bool createActionBindings(const char *action_set_name,
                            uint32_t count,
                            const GHOST_XrActionProfileInfo *infos);
  void destroyActionBindings(const char *action_set_name,
                             uint32_t count,
                             const GHOST_XrActionProfileInfo *infos);
  bool attachActionSets();

  /** Action functions to be called post-session start. */
  bool syncActions(
      const char *action_set_name = nullptr); /* If action_set_name is nullptr, all attached
                                               * action sets will be synced. */
  bool applyHapticAction(const char *action_set_name,
                         const char *action_name,
                         const GHOST_TInt64 &duration,
                         const float &frequency,
                         const float &amplitude);
  void stopHapticAction(const char *action_set_name, const char *action_name);

  /* Custom data (owned by Blender, not GHOST) accessors. */
  void *getActionSetCustomdata(const char *action_set_name);
  void *getActionCustomdata(const char *action_set_name, const char *action_name);

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
                void *draw_customdata);
  void beginFrameDrawing();
  void endFrameDrawing(std::vector<XrCompositionLayerBaseHeader *> &layers);
};
