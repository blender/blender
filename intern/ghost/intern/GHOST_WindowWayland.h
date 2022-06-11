/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Declaration of GHOST_WindowWayland class.
 */

#pragma once

#include "GHOST_Window.h"

#include <unordered_set>
#include <vector>

class GHOST_SystemWayland;

struct output_t;
struct window_t;
struct wl_surface;

class GHOST_WindowWayland : public GHOST_Window {
 public:
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor cursorShape) override;

  GHOST_WindowWayland(GHOST_SystemWayland *system,
                      const char *title,
                      int32_t left,
                      int32_t top,
                      uint32_t width,
                      uint32_t height,
                      GHOST_TWindowState state,
                      const GHOST_IWindow *parentWindow,
                      GHOST_TDrawingContextType type,
                      const bool is_dialog,
                      const bool stereoVisual,
                      const bool exclusive);

  ~GHOST_WindowWayland() override;

  uint16_t getDPIHint() override;

  GHOST_TSuccess close();

  GHOST_TSuccess activate();

  GHOST_TSuccess deactivate();

  GHOST_TSuccess notify_size();

  wl_surface *surface() const;

  std::vector<const output_t *> &outputs();

  output_t *output_find_by_wl(struct wl_output *output);

  bool outputs_changed_update_scale();

  uint16_t dpi();

  int scale();

 protected:
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode) override;

  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) override;

  GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                            uint8_t *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor) override;

  void setTitle(const char *title) override;

  std::string getTitle() const override;

  void getWindowBounds(GHOST_Rect &bounds) const override;

  void getClientBounds(GHOST_Rect &bounds) const override;

  GHOST_TSuccess setClientWidth(uint32_t width) override;

  GHOST_TSuccess setClientHeight(uint32_t height) override;

  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height) override;

  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const override;

  GHOST_TSuccess setWindowCursorVisibility(bool visible) override;

  GHOST_TSuccess setState(GHOST_TWindowState state) override;

  GHOST_TWindowState getState() const override;

  GHOST_TSuccess invalidate() override;

  GHOST_TSuccess setOrder(GHOST_TWindowOrder order) override;

  GHOST_TSuccess beginFullScreen() const override;

  GHOST_TSuccess endFullScreen() const override;

  bool isDialog() const override;

#ifdef GHOST_OPENGL_ALPHA
  void setOpaque() const;
#endif

 private:
  GHOST_SystemWayland *m_system;
  struct window_t *w;
  std::string title;

  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type) override;
};
