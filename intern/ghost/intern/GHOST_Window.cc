/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_Window.hh"

#include "GHOST_ContextNone.hh"

#include <cassert>

GHOST_Window::GHOST_Window(uint32_t width,
                           uint32_t height,
                           GHOST_TWindowState state,
                           const GHOST_ContextParams &context_params,
                           const bool /*exclusive*/)
    : drawing_context_type_(GHOST_kDrawingContextTypeNone),
      user_data_(nullptr),
      cursor_visible_(true),
      cursor_grab_(GHOST_kGrabDisable),
      cursor_grab_axis_(GHOST_kAxisNone),
      cursor_grab_init_pos_{0, 0},
      cursor_grab_accum_pos_{0, 0},
      cursor_shape_(GHOST_kStandardCursorDefault),
      progress_bar_visible_(false),
      can_accept_drag_operation_(false),
      is_unsaved_changes_(false),
      window_decoration_style_flags_(GHOST_kDecorationNone),
      window_decoration_style_settings_(),
      want_context_params_(context_params),
      native_pixel_size_(1.0f),
      context_(nullptr)

{
  const GHOST_ContextParams context_params_none = GHOST_CONTEXT_PARAMS_NONE;
  context_ = new GHOST_ContextNone(context_params_none);

  full_screen_ = state == GHOST_kWindowStateFullScreen;
  if (full_screen_) {
    full_screen_width_ = width;
    full_screen_height_ = height;
  }
}

GHOST_Window::~GHOST_Window()
{
  delete context_;
}

void *GHOST_Window::getOSWindow() const
{
  return nullptr;
}

GHOST_TWindowDecorationStyleFlags GHOST_Window::getWindowDecorationStyleFlags()
{
  return window_decoration_style_flags_;
}

void GHOST_Window::setWindowDecorationStyleFlags(GHOST_TWindowDecorationStyleFlags style_flags)
{
  window_decoration_style_flags_ = style_flags;
}

void GHOST_Window::setWindowDecorationStyleSettings(
    GHOST_WindowDecorationStyleSettings decoration_settings)
{
  window_decoration_style_settings_ = decoration_settings;
}

GHOST_TSuccess GHOST_Window::setDrawingContextType(GHOST_TDrawingContextType type)
{
  if (type != drawing_context_type_) {
    delete context_;
    context_ = nullptr;

    if (type != GHOST_kDrawingContextTypeNone) {
      context_ = newDrawingContext(type);
    }
    if (context_ != nullptr) {
      drawing_context_type_ = type;
    }
    else {
      context_ = new GHOST_ContextNone(want_context_params_);
      drawing_context_type_ = GHOST_kDrawingContextTypeNone;
    }

    return (type == drawing_context_type_) ? GHOST_kSuccess : GHOST_kFailure;
  }
  return GHOST_kSuccess;
}

GHOST_IContext *GHOST_Window::getDrawingContext()
{
  return context_;
}

GHOST_TSuccess GHOST_Window::swapBufferAcquire()
{
  return context_->swapBufferAcquire();
}
GHOST_TSuccess GHOST_Window::swapBufferRelease()
{
  return context_->swapBufferRelease();
}

GHOST_TSuccess GHOST_Window::setSwapInterval(int interval)
{
  return context_->setSwapInterval(interval);
}

GHOST_TSuccess GHOST_Window::getSwapInterval(int &interval_out)
{
  return context_->getSwapInterval(interval_out);
}

GHOST_Context *GHOST_Window::getContext()
{
  return context_;
}

uint GHOST_Window::getDefaultFramebuffer()
{
  return (context_) ? context_->getDefaultFramebuffer() : 0;
}

#ifdef WITH_VULKAN_BACKEND
GHOST_TSuccess GHOST_Window::getVulkanSwapChainFormat(GHOST_VulkanSwapChainData *r_swap_chain_data)
{
  return context_->getVulkanSwapChainFormat(r_swap_chain_data);
}
#endif

GHOST_TSuccess GHOST_Window::activateDrawingContext()
{
  return context_->activateDrawingContext();
}

GHOST_TSuccess GHOST_Window::updateDrawingContext()
{
  return context_->updateDrawingContext();
}

GHOST_TSuccess GHOST_Window::releaseNativeHandles()
{
  return context_->releaseNativeHandles();
}

GHOST_TSuccess GHOST_Window::setCursorVisibility(bool visible)
{
  if (setWindowCursorVisibility(visible)) {
    cursor_visible_ = visible;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::setCursorGrab(GHOST_TGrabCursorMode mode,
                                           GHOST_TAxisFlag wrap_axis,
                                           GHOST_Rect *bounds,
                                           int32_t mouse_ungrab_xy[2])
{
  if (cursor_grab_ == mode) {
    return GHOST_kSuccess;
  }
  /* Override with new location. */
  if (mouse_ungrab_xy) {
    assert(mode == GHOST_kGrabDisable);
    cursor_grab_init_pos_[0] = mouse_ungrab_xy[0];
    cursor_grab_init_pos_[1] = mouse_ungrab_xy[1];
  }

  if (setWindowCursorGrab(mode)) {

    if (mode == GHOST_kGrabDisable) {
      cursor_grab_bounds_.l_ = cursor_grab_bounds_.r_ = -1;
    }
    else if (bounds) {
      cursor_grab_bounds_ = *bounds;
    }
    else { /* if bounds not defined, use window */
      getClientBounds(cursor_grab_bounds_);
    }
    cursor_grab_ = mode;
    cursor_grab_axis_ = wrap_axis;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::getCursorGrabBounds(GHOST_Rect &bounds) const
{
  if (!(cursor_grab_ == GHOST_kGrabWrap || cursor_grab_ == GHOST_kGrabHide)) {
    return GHOST_kFailure;
  }
  bounds = cursor_grab_bounds_;
  return (bounds.l_ == -1 && bounds.r_ == -1) ? GHOST_kFailure : GHOST_kSuccess;
}

void GHOST_Window::getCursorGrabState(GHOST_TGrabCursorMode &mode,
                                      GHOST_TAxisFlag &wrap_axis,
                                      GHOST_Rect &bounds,
                                      bool &use_software_cursor)
{
  mode = cursor_grab_;
  if (cursor_grab_ == GHOST_kGrabWrap) {
    bounds = cursor_grab_bounds_;
    wrap_axis = cursor_grab_axis_;
  }
  else {
    bounds.l_ = -1;
    bounds.r_ = -1;
    bounds.t_ = -1;
    bounds.b_ = -1;
    wrap_axis = GHOST_kAxisNone;
  }
  use_software_cursor = (cursor_grab_ != GHOST_kGrabDisable) ? getCursorGrabUseSoftwareDisplay() :
                                                               false;
}

bool GHOST_Window::getCursorGrabUseSoftwareDisplay()
{
  /* Sub-classes may override, by default don't use software cursor. */
  return false;
}

GHOST_TSuccess GHOST_Window::setCursorShape(GHOST_TStandardCursor cursor_shape)
{
  if (setWindowCursorShape(cursor_shape)) {
    cursor_shape_ = cursor_shape;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::setCustomCursorShape(const uint8_t *bitmap,
                                                  const uint8_t *mask,
                                                  const int size[2],
                                                  const int hot_spot[2],
                                                  bool can_invert_color)
{
  if (setWindowCustomCursorShape(bitmap, mask, size, hot_spot, can_invert_color)) {
    cursor_shape_ = GHOST_kStandardCursorCustom;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::setCustomCursorGenerator(GHOST_CursorGenerator *cursor_generator)
{
  if (setWindowCustomCursorGenerator(cursor_generator)) {
    cursor_shape_ = GHOST_kStandardCursorCustom;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::getCursorBitmap(GHOST_CursorBitmapRef * /*bitmap*/)
{
  /* Sub-classes may override. */
  return GHOST_kFailure;
}

void GHOST_Window::setAcceptDragOperation(bool can_accept)
{
  can_accept_drag_operation_ = can_accept;
}

bool GHOST_Window::canAcceptDragOperation() const
{
  return can_accept_drag_operation_;
}

GHOST_TSuccess GHOST_Window::setModifiedState(bool is_unsaved_changes)
{
  is_unsaved_changes_ = is_unsaved_changes;

  return GHOST_kSuccess;
}

bool GHOST_Window::getModifiedState()
{
  return is_unsaved_changes_;
}
