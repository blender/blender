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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_Window.h"

#include "GHOST_ContextNone.h"

#include <assert.h>

GHOST_Window::GHOST_Window(GHOST_TUns32 width,
                           GHOST_TUns32 height,
                           GHOST_TWindowState state,
                           const bool wantStereoVisual,
                           const bool /*exclusive*/)
    : m_drawingContextType(GHOST_kDrawingContextTypeNone),
      m_cursorVisible(true),
      m_cursorGrab(GHOST_kGrabDisable),
      m_cursorShape(GHOST_kStandardCursorDefault),
      m_wantStereoVisual(wantStereoVisual),
      m_context(new GHOST_ContextNone(false))
{
  m_isUnsavedChanges = false;
  m_canAcceptDragOperation = false;

  m_progressBarVisible = false;

  m_cursorGrabAccumPos[0] = 0;
  m_cursorGrabAccumPos[1] = 0;

  m_nativePixelSize = 1.0f;

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
  return NULL;
}

GHOST_TSuccess GHOST_Window::setDrawingContextType(GHOST_TDrawingContextType type)
{
  if (type != m_drawingContextType) {
    delete m_context;
    m_context = NULL;

    if (type != GHOST_kDrawingContextTypeNone)
      m_context = newDrawingContext(type);

    if (m_context != NULL) {
      m_drawingContextType = type;
    }
    else {
      m_context = new GHOST_ContextNone(m_wantStereoVisual);
      m_drawingContextType = GHOST_kDrawingContextTypeNone;
    }

    return (type == m_drawingContextType) ? GHOST_kSuccess : GHOST_kFailure;
  }
  else {
    return GHOST_kSuccess;
  }
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
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_Window::setCursorGrab(GHOST_TGrabCursorMode mode,
                                           GHOST_TAxisFlag wrap_axis,
                                           GHOST_Rect *bounds,
                                           GHOST_TInt32 mouse_ungrab_xy[2])
{
  if (m_cursorGrab == mode)
    return GHOST_kSuccess;

  /* override with new location */
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
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_Window::getCursorGrabBounds(GHOST_Rect &bounds)
{
  bounds = m_cursorGrabBounds;
  return (bounds.m_l == -1 && bounds.m_r == -1) ? GHOST_kFailure : GHOST_kSuccess;
}

GHOST_TSuccess GHOST_Window::setCursorShape(GHOST_TStandardCursor cursorShape)
{
  if (setWindowCursorShape(cursorShape)) {
    m_cursorShape = cursorShape;
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_Window::setCustomCursorShape(GHOST_TUns8 bitmap[16][2],
                                                  GHOST_TUns8 mask[16][2],
                                                  int hotX,
                                                  int hotY)
{
  return setCustomCursorShape(
      (GHOST_TUns8 *)bitmap, (GHOST_TUns8 *)mask, 16, 16, hotX, hotY, 0, 1);
}

GHOST_TSuccess GHOST_Window::setCustomCursorShape(GHOST_TUns8 *bitmap,
                                                  GHOST_TUns8 *mask,
                                                  int sizex,
                                                  int sizey,
                                                  int hotX,
                                                  int hotY,
                                                  int fg_color,
                                                  int bg_color)
{
  if (setWindowCustomCursorShape(bitmap, mask, sizex, sizey, hotX, hotY, fg_color, bg_color)) {
    m_cursorShape = GHOST_kStandardCursorCustom;
    return GHOST_kSuccess;
  }
  else {
    return GHOST_kFailure;
  }
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
