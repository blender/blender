/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
                           const bool wantStereoVisual,
                           const bool /*exclusive*/)
    : m_drawingContextType(GHOST_kDrawingContextTypeNone),
      m_userData(nullptr),
      m_cursorVisible(true),
      m_cursorGrab(GHOST_kGrabDisable),
      m_cursorGrabAxis(GHOST_kAxisNone),
      m_cursorGrabInitPos{0, 0},
      m_cursorGrabAccumPos{0, 0},
      m_cursorShape(GHOST_kStandardCursorDefault),
      m_progressBarVisible(false),
      m_canAcceptDragOperation(false),
      m_isUnsavedChanges(false),
      m_wantStereoVisual(wantStereoVisual),
      m_nativePixelSize(1.0f),
      m_context(new GHOST_ContextNone(false))

{

  m_fullScreen = state == GHOST_kWindowStateFullScreen;
  if (m_fullScreen) {
    m_fullScreenWidth = width;
    m_fullScreenHeight = height;
  }
}

GHOST_Window::~GHOST_Window()
{
  delete m_context;
}

void *GHOST_Window::getOSWindow() const
{
  return nullptr;
}

GHOST_TSuccess GHOST_Window::setDrawingContextType(GHOST_TDrawingContextType type)
{
  if (type != m_drawingContextType) {
    delete m_context;
    m_context = nullptr;

    if (type != GHOST_kDrawingContextTypeNone) {
      m_context = newDrawingContext(type);
    }
    if (m_context != nullptr) {
      m_drawingContextType = type;
    }
    else {
      m_context = new GHOST_ContextNone(m_wantStereoVisual);
      m_drawingContextType = GHOST_kDrawingContextTypeNone;
    }

    return (type == m_drawingContextType) ? GHOST_kSuccess : GHOST_kFailure;
  }
  return GHOST_kSuccess;
}

GHOST_IContext *GHOST_Window::getDrawingContext()
{
  return m_context;
}

GHOST_TSuccess GHOST_Window::swapBuffers()
{
  return m_context->swapBuffers();
}

GHOST_TSuccess GHOST_Window::setSwapInterval(int interval)
{
  return m_context->setSwapInterval(interval);
}

GHOST_TSuccess GHOST_Window::getSwapInterval(int &intervalOut)
{
  return m_context->getSwapInterval(intervalOut);
}

GHOST_Context *GHOST_Window::getContext()
{
  return m_context;
}

uint GHOST_Window::getDefaultFramebuffer()
{
  return (m_context) ? m_context->getDefaultFramebuffer() : 0;
}

GHOST_TSuccess GHOST_Window::getVulkanBackbuffer(
    void *image, void *framebuffer, void *render_pass, void *extent, uint32_t *fb_id)
{
  return m_context->getVulkanBackbuffer(image, framebuffer, render_pass, extent, fb_id);
}

GHOST_TSuccess GHOST_Window::activateDrawingContext()
{
  return m_context->activateDrawingContext();
}

GHOST_TSuccess GHOST_Window::updateDrawingContext()
{
  return m_context->updateDrawingContext();
}

GHOST_TSuccess GHOST_Window::releaseNativeHandles()
{
  return m_context->releaseNativeHandles();
}

GHOST_TSuccess GHOST_Window::setCursorVisibility(bool visible)
{
  if (setWindowCursorVisibility(visible)) {
    m_cursorVisible = visible;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::setCursorGrab(GHOST_TGrabCursorMode mode,
                                           GHOST_TAxisFlag wrap_axis,
                                           GHOST_Rect *bounds,
                                           int32_t mouse_ungrab_xy[2])
{
  if (m_cursorGrab == mode) {
    return GHOST_kSuccess;
  }
  /* Override with new location. */
  if (mouse_ungrab_xy) {
    assert(mode == GHOST_kGrabDisable);
    m_cursorGrabInitPos[0] = mouse_ungrab_xy[0];
    m_cursorGrabInitPos[1] = mouse_ungrab_xy[1];
  }

  if (setWindowCursorGrab(mode)) {

    if (mode == GHOST_kGrabDisable) {
      m_cursorGrabBounds.m_l = m_cursorGrabBounds.m_r = -1;
    }
    else if (bounds) {
      m_cursorGrabBounds = *bounds;
    }
    else { /* if bounds not defined, use window */
      getClientBounds(m_cursorGrabBounds);
    }
    m_cursorGrab = mode;
    m_cursorGrabAxis = wrap_axis;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::getCursorGrabBounds(GHOST_Rect &bounds) const
{
  if (m_cursorGrab != GHOST_kGrabWrap) {
    return GHOST_kFailure;
  }
  bounds = m_cursorGrabBounds;
  return (bounds.m_l == -1 && bounds.m_r == -1) ? GHOST_kFailure : GHOST_kSuccess;
}

void GHOST_Window::getCursorGrabState(GHOST_TGrabCursorMode &mode,
                                      GHOST_TAxisFlag &wrap_axis,
                                      GHOST_Rect &bounds,
                                      bool &use_software_cursor)
{
  mode = m_cursorGrab;
  if (m_cursorGrab == GHOST_kGrabWrap) {
    bounds = m_cursorGrabBounds;
    wrap_axis = m_cursorGrabAxis;
  }
  else {
    bounds.m_l = -1;
    bounds.m_r = -1;
    bounds.m_t = -1;
    bounds.m_b = -1;
    wrap_axis = GHOST_kAxisNone;
  }
  use_software_cursor = (m_cursorGrab != GHOST_kGrabDisable) ? getCursorGrabUseSoftwareDisplay() :
                                                               false;
}

bool GHOST_Window::getCursorGrabUseSoftwareDisplay()
{
  /* Sub-classes may override, by default don't use software cursor. */
  return false;
}

GHOST_TSuccess GHOST_Window::setCursorShape(GHOST_TStandardCursor cursorShape)
{
  if (setWindowCursorShape(cursorShape)) {
    m_cursorShape = cursorShape;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::setCustomCursorShape(
    uint8_t *bitmap, uint8_t *mask, int sizex, int sizey, int hotX, int hotY, bool canInvertColor)
{
  if (setWindowCustomCursorShape(bitmap, mask, sizex, sizey, hotX, hotY, canInvertColor)) {
    m_cursorShape = GHOST_kStandardCursorCustom;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_Window::getCursorBitmap(GHOST_CursorBitmapRef * /*bitmap*/)
{
  /* Sub-classes may override. */
  return GHOST_kFailure;
}

void GHOST_Window::setAcceptDragOperation(bool canAccept)
{
  m_canAcceptDragOperation = canAccept;
}

bool GHOST_Window::canAcceptDragOperation() const
{
  return m_canAcceptDragOperation;
}

GHOST_TSuccess GHOST_Window::setModifiedState(bool isUnsavedChanges)
{
  m_isUnsavedChanges = isUnsavedChanges;

  return GHOST_kSuccess;
}

bool GHOST_Window::getModifiedState()
{
  return m_isUnsavedChanges;
}
