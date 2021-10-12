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
 *
 * C Api for GHOST
 */

#include <stdlib.h>
#include <string.h>

#include "GHOST_C-api.h"
#include "GHOST_IEvent.h"
#include "GHOST_IEventConsumer.h"
#include "GHOST_ISystem.h"
#include "intern/GHOST_Debug.h"
#ifdef WITH_XR_OPENXR
#  include "GHOST_IXrContext.h"
#  include "intern/GHOST_XrSession.h"
#endif
#include "intern/GHOST_CallbackEventConsumer.h"
#include "intern/GHOST_XrException.h"

GHOST_SystemHandle GHOST_CreateSystem(void)
{
  GHOST_ISystem::createSystem();
  GHOST_ISystem *system = GHOST_ISystem::getSystem();

  return (GHOST_SystemHandle)system;
}

void GHOST_SystemInitDebug(GHOST_SystemHandle systemhandle, int is_debug_enabled)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  system->initDebug(is_debug_enabled);
}

GHOST_TSuccess GHOST_DisposeSystem(GHOST_SystemHandle systemhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->disposeSystem();
}

void GHOST_ShowMessageBox(GHOST_SystemHandle systemhandle,
                          const char *title,
                          const char *message,
                          const char *help_label,
                          const char *continue_label,
                          const char *link,
                          GHOST_DialogOptions dialog_options)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  system->showMessageBox(title, message, help_label, continue_label, link, dialog_options);
}

GHOST_EventConsumerHandle GHOST_CreateEventConsumer(GHOST_EventCallbackProcPtr eventCallback,
                                                    GHOST_TUserDataPtr userdata)
{
  return (GHOST_EventConsumerHandle) new GHOST_CallbackEventConsumer(eventCallback, userdata);
}

GHOST_TSuccess GHOST_DisposeEventConsumer(GHOST_EventConsumerHandle consumerhandle)
{
  delete ((GHOST_CallbackEventConsumer *)consumerhandle);
  return GHOST_kSuccess;
}

uint64_t GHOST_GetMilliSeconds(GHOST_SystemHandle systemhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->getMilliSeconds();
}

GHOST_TimerTaskHandle GHOST_InstallTimer(GHOST_SystemHandle systemhandle,
                                         uint64_t delay,
                                         uint64_t interval,
                                         GHOST_TimerProcPtr timerproc,
                                         GHOST_TUserDataPtr userdata)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return (GHOST_TimerTaskHandle)system->installTimer(delay, interval, timerproc, userdata);
}

GHOST_TSuccess GHOST_RemoveTimer(GHOST_SystemHandle systemhandle,
                                 GHOST_TimerTaskHandle timertaskhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_ITimerTask *timertask = (GHOST_ITimerTask *)timertaskhandle;

  return system->removeTimer(timertask);
}

uint8_t GHOST_GetNumDisplays(GHOST_SystemHandle systemhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->getNumDisplays();
}

void GHOST_GetMainDisplayDimensions(GHOST_SystemHandle systemhandle,
                                    uint32_t *width,
                                    uint32_t *height)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  system->getMainDisplayDimensions(*width, *height);
}

void GHOST_GetAllDisplayDimensions(GHOST_SystemHandle systemhandle,
                                   uint32_t *width,
                                   uint32_t *height)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  system->getAllDisplayDimensions(*width, *height);
}

GHOST_ContextHandle GHOST_CreateOpenGLContext(GHOST_SystemHandle systemhandle,
                                              GHOST_GLSettings glSettings)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return (GHOST_ContextHandle)system->createOffscreenContext(glSettings);
}

GHOST_TSuccess GHOST_DisposeOpenGLContext(GHOST_SystemHandle systemhandle,
                                          GHOST_ContextHandle contexthandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_IContext *context = (GHOST_IContext *)contexthandle;

  return system->disposeContext(context);
}

GHOST_WindowHandle GHOST_CreateWindow(GHOST_SystemHandle systemhandle,
                                      GHOST_WindowHandle parent_windowhandle,
                                      const char *title,
                                      int32_t left,
                                      int32_t top,
                                      uint32_t width,
                                      uint32_t height,
                                      GHOST_TWindowState state,
                                      bool is_dialog,
                                      GHOST_TDrawingContextType type,
                                      GHOST_GLSettings glSettings)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return (GHOST_WindowHandle)system->createWindow(title,
                                                  left,
                                                  top,
                                                  width,
                                                  height,
                                                  state,
                                                  type,
                                                  glSettings,
                                                  false,
                                                  is_dialog,
                                                  (GHOST_IWindow *)parent_windowhandle);
}

GHOST_TUserDataPtr GHOST_GetWindowUserData(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->getUserData();
}
void GHOST_SetWindowUserData(GHOST_WindowHandle windowhandle, GHOST_TUserDataPtr userdata)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  window->setUserData(userdata);
}

int GHOST_IsDialogWindow(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return (int)window->isDialog();
}

GHOST_TSuccess GHOST_DisposeWindow(GHOST_SystemHandle systemhandle,
                                   GHOST_WindowHandle windowhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return system->disposeWindow(window);
}

int GHOST_ValidWindow(GHOST_SystemHandle systemhandle, GHOST_WindowHandle windowhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return (int)system->validWindow(window);
}

GHOST_WindowHandle GHOST_BeginFullScreen(GHOST_SystemHandle systemhandle,
                                         GHOST_DisplaySetting *setting,
                                         const int stereoVisual)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_IWindow *window = NULL;
  bool bstereoVisual;

  if (stereoVisual)
    bstereoVisual = true;
  else
    bstereoVisual = false;

  system->beginFullScreen(*setting, &window, bstereoVisual);

  return (GHOST_WindowHandle)window;
}

GHOST_TSuccess GHOST_EndFullScreen(GHOST_SystemHandle systemhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->endFullScreen();
}

int GHOST_GetFullScreen(GHOST_SystemHandle systemhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return (int)system->getFullScreen();
}

bool GHOST_ProcessEvents(GHOST_SystemHandle systemhandle, bool waitForEvent)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->processEvents(waitForEvent);
}

void GHOST_DispatchEvents(GHOST_SystemHandle systemhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  system->dispatchEvents();
}

GHOST_TSuccess GHOST_AddEventConsumer(GHOST_SystemHandle systemhandle,
                                      GHOST_EventConsumerHandle consumerhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->addEventConsumer((GHOST_CallbackEventConsumer *)consumerhandle);
}

GHOST_TSuccess GHOST_RemoveEventConsumer(GHOST_SystemHandle systemhandle,
                                         GHOST_EventConsumerHandle consumerhandle)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->removeEventConsumer((GHOST_CallbackEventConsumer *)consumerhandle);
}

GHOST_TSuccess GHOST_SetProgressBar(GHOST_WindowHandle windowhandle, float progress)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setProgressBar(progress);
}

GHOST_TSuccess GHOST_EndProgressBar(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->endProgressBar();
}

GHOST_TStandardCursor GHOST_GetCursorShape(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->getCursorShape();
}

GHOST_TSuccess GHOST_SetCursorShape(GHOST_WindowHandle windowhandle,
                                    GHOST_TStandardCursor cursorshape)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setCursorShape(cursorshape);
}

GHOST_TSuccess GHOST_HasCursorShape(GHOST_WindowHandle windowhandle,
                                    GHOST_TStandardCursor cursorshape)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->hasCursorShape(cursorshape);
}

GHOST_TSuccess GHOST_SetCustomCursorShape(GHOST_WindowHandle windowhandle,
                                          uint8_t *bitmap,
                                          uint8_t *mask,
                                          int sizex,
                                          int sizey,
                                          int hotX,
                                          int hotY,
                                          bool canInvertColor)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setCustomCursorShape(bitmap, mask, sizex, sizey, hotX, hotY, canInvertColor);
}

int GHOST_GetCursorVisibility(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return (int)window->getCursorVisibility();
}

GHOST_TSuccess GHOST_SetCursorVisibility(GHOST_WindowHandle windowhandle, int visible)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setCursorVisibility(visible ? true : false);
}

GHOST_TSuccess GHOST_GetCursorPosition(GHOST_SystemHandle systemhandle, int32_t *x, int32_t *y)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->getCursorPosition(*x, *y);
}

GHOST_TSuccess GHOST_SetCursorPosition(GHOST_SystemHandle systemhandle, int32_t x, int32_t y)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;

  return system->setCursorPosition(x, y);
}

GHOST_TSuccess GHOST_SetCursorGrab(GHOST_WindowHandle windowhandle,
                                   GHOST_TGrabCursorMode mode,
                                   GHOST_TAxisFlag wrap_axis,
                                   int bounds[4],
                                   const int mouse_ungrab_xy[2])
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  GHOST_Rect bounds_rect;
  int32_t mouse_xy[2];

  if (bounds) {
    bounds_rect = GHOST_Rect(bounds[0], bounds[1], bounds[2], bounds[3]);
  }
  if (mouse_ungrab_xy) {
    mouse_xy[0] = mouse_ungrab_xy[0];
    mouse_xy[1] = mouse_ungrab_xy[1];
  }

  return window->setCursorGrab(
      mode, wrap_axis, bounds ? &bounds_rect : NULL, mouse_ungrab_xy ? mouse_xy : NULL);
}

GHOST_TSuccess GHOST_GetModifierKeyState(GHOST_SystemHandle systemhandle,
                                         GHOST_TModifierKeyMask mask,
                                         int *isDown)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_TSuccess result;
  bool isdown = false;

  result = system->getModifierKeyState(mask, isdown);
  *isDown = (int)isdown;

  return result;
}

GHOST_TSuccess GHOST_GetButtonState(GHOST_SystemHandle systemhandle,
                                    GHOST_TButtonMask mask,
                                    int *isDown)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  GHOST_TSuccess result;
  bool isdown = false;

  result = system->getButtonState(mask, isdown);
  *isDown = (int)isdown;

  return result;
}

#ifdef WITH_INPUT_NDOF
void GHOST_setNDOFDeadZone(float deadzone)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  system->setNDOFDeadZone(deadzone);
}
#endif

void GHOST_setAcceptDragOperation(GHOST_WindowHandle windowhandle, bool canAccept)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  window->setAcceptDragOperation(canAccept);
}

GHOST_TEventType GHOST_GetEventType(GHOST_EventHandle eventhandle)
{
  GHOST_IEvent *event = (GHOST_IEvent *)eventhandle;

  return event->getType();
}

uint64_t GHOST_GetEventTime(GHOST_EventHandle eventhandle)
{
  GHOST_IEvent *event = (GHOST_IEvent *)eventhandle;

  return event->getTime();
}

GHOST_WindowHandle GHOST_GetEventWindow(GHOST_EventHandle eventhandle)
{
  GHOST_IEvent *event = (GHOST_IEvent *)eventhandle;

  return (GHOST_WindowHandle)event->getWindow();
}

GHOST_TEventDataPtr GHOST_GetEventData(GHOST_EventHandle eventhandle)
{
  GHOST_IEvent *event = (GHOST_IEvent *)eventhandle;

  return event->getData();
}

GHOST_TimerProcPtr GHOST_GetTimerProc(GHOST_TimerTaskHandle timertaskhandle)
{
  GHOST_ITimerTask *timertask = (GHOST_ITimerTask *)timertaskhandle;

  return timertask->getTimerProc();
}

void GHOST_SetTimerProc(GHOST_TimerTaskHandle timertaskhandle, GHOST_TimerProcPtr timerproc)
{
  GHOST_ITimerTask *timertask = (GHOST_ITimerTask *)timertaskhandle;

  timertask->setTimerProc(timerproc);
}

GHOST_TUserDataPtr GHOST_GetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle)
{
  GHOST_ITimerTask *timertask = (GHOST_ITimerTask *)timertaskhandle;

  return timertask->getUserData();
}

void GHOST_SetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle, GHOST_TUserDataPtr userdata)
{
  GHOST_ITimerTask *timertask = (GHOST_ITimerTask *)timertaskhandle;

  timertask->setUserData(userdata);
}

int GHOST_GetValid(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return (int)window->getValid();
}

GHOST_TDrawingContextType GHOST_GetDrawingContextType(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->getDrawingContextType();
}

GHOST_TSuccess GHOST_SetDrawingContextType(GHOST_WindowHandle windowhandle,
                                           GHOST_TDrawingContextType type)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setDrawingContextType(type);
}

void GHOST_SetTitle(GHOST_WindowHandle windowhandle, const char *title)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  window->setTitle(title);
}

char *GHOST_GetTitle(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  std::string title = window->getTitle();

  char *ctitle = (char *)malloc(title.size() + 1);

  if (ctitle == NULL) {
    return NULL;
  }

  strcpy(ctitle, title.c_str());

  return ctitle;
}

GHOST_RectangleHandle GHOST_GetWindowBounds(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  GHOST_Rect *rectangle = NULL;

  rectangle = new GHOST_Rect();
  window->getWindowBounds(*rectangle);

  return (GHOST_RectangleHandle)rectangle;
}

GHOST_RectangleHandle GHOST_GetClientBounds(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  GHOST_Rect *rectangle = NULL;

  rectangle = new GHOST_Rect();
  window->getClientBounds(*rectangle);

  return (GHOST_RectangleHandle)rectangle;
}

void GHOST_DisposeRectangle(GHOST_RectangleHandle rectanglehandle)
{
  delete (GHOST_Rect *)rectanglehandle;
}

GHOST_TSuccess GHOST_SetClientWidth(GHOST_WindowHandle windowhandle, uint32_t width)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setClientWidth(width);
}

GHOST_TSuccess GHOST_SetClientHeight(GHOST_WindowHandle windowhandle, uint32_t height)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setClientHeight(height);
}

GHOST_TSuccess GHOST_SetClientSize(GHOST_WindowHandle windowhandle,
                                   uint32_t width,
                                   uint32_t height)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setClientSize(width, height);
}

void GHOST_ScreenToClient(
    GHOST_WindowHandle windowhandle, int32_t inX, int32_t inY, int32_t *outX, int32_t *outY)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  window->screenToClient(inX, inY, *outX, *outY);
}

void GHOST_ClientToScreen(
    GHOST_WindowHandle windowhandle, int32_t inX, int32_t inY, int32_t *outX, int32_t *outY)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  window->clientToScreen(inX, inY, *outX, *outY);
}

GHOST_TWindowState GHOST_GetWindowState(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->getState();
}

GHOST_TSuccess GHOST_SetWindowState(GHOST_WindowHandle windowhandle, GHOST_TWindowState state)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setState(state);
}

GHOST_TSuccess GHOST_SetWindowModifiedState(GHOST_WindowHandle windowhandle, bool isUnsavedChanges)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setModifiedState(isUnsavedChanges);
}

GHOST_TSuccess GHOST_SetWindowOrder(GHOST_WindowHandle windowhandle, GHOST_TWindowOrder order)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setOrder(order);
}

GHOST_TSuccess GHOST_SwapWindowBuffers(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->swapBuffers();
}

GHOST_TSuccess GHOST_SetSwapInterval(GHOST_WindowHandle windowhandle, int interval)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->setSwapInterval(interval);
}

GHOST_TSuccess GHOST_GetSwapInterval(GHOST_WindowHandle windowhandle, int *intervalOut)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->getSwapInterval(*intervalOut);
}

GHOST_TSuccess GHOST_ActivateWindowDrawingContext(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->activateDrawingContext();
}

GHOST_TSuccess GHOST_ActivateOpenGLContext(GHOST_ContextHandle contexthandle)
{
  GHOST_IContext *context = (GHOST_IContext *)contexthandle;
  if (context) {
    return context->activateDrawingContext();
  }
  else {
    GHOST_PRINTF("%s: Context not valid\n", __func__);
    return GHOST_kFailure;
  }
}

GHOST_TSuccess GHOST_ReleaseOpenGLContext(GHOST_ContextHandle contexthandle)
{
  GHOST_IContext *context = (GHOST_IContext *)contexthandle;

  return context->releaseDrawingContext();
}

unsigned int GHOST_GetContextDefaultOpenGLFramebuffer(GHOST_ContextHandle contexthandle)
{
  GHOST_IContext *context = (GHOST_IContext *)contexthandle;

  return context->getDefaultFramebuffer();
}

unsigned int GHOST_GetDefaultOpenGLFramebuffer(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->getDefaultFramebuffer();
}

GHOST_TSuccess GHOST_InvalidateWindow(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;

  return window->invalidate();
}

void GHOST_SetTabletAPI(GHOST_SystemHandle systemhandle, GHOST_TTabletAPI api)
{
  GHOST_ISystem *system = (GHOST_ISystem *)systemhandle;
  system->setTabletAPI(api);
}

int32_t GHOST_GetWidthRectangle(GHOST_RectangleHandle rectanglehandle)
{
  return ((GHOST_Rect *)rectanglehandle)->getWidth();
}

int32_t GHOST_GetHeightRectangle(GHOST_RectangleHandle rectanglehandle)
{
  return ((GHOST_Rect *)rectanglehandle)->getHeight();
}

void GHOST_GetRectangle(
    GHOST_RectangleHandle rectanglehandle, int32_t *l, int32_t *t, int32_t *r, int32_t *b)
{
  GHOST_Rect *rect = (GHOST_Rect *)rectanglehandle;

  *l = rect->m_l;
  *t = rect->m_t;
  *r = rect->m_r;
  *b = rect->m_b;
}

void GHOST_SetRectangle(
    GHOST_RectangleHandle rectanglehandle, int32_t l, int32_t t, int32_t r, int32_t b)
{
  ((GHOST_Rect *)rectanglehandle)->set(l, t, r, b);
}

GHOST_TSuccess GHOST_IsEmptyRectangle(GHOST_RectangleHandle rectanglehandle)
{
  GHOST_TSuccess result = GHOST_kFailure;

  if (((GHOST_Rect *)rectanglehandle)->isEmpty())
    result = GHOST_kSuccess;

  return result;
}

GHOST_TSuccess GHOST_IsValidRectangle(GHOST_RectangleHandle rectanglehandle)
{
  GHOST_TSuccess result = GHOST_kFailure;

  if (((GHOST_Rect *)rectanglehandle)->isValid())
    result = GHOST_kSuccess;

  return result;
}

void GHOST_InsetRectangle(GHOST_RectangleHandle rectanglehandle, int32_t i)
{
  ((GHOST_Rect *)rectanglehandle)->inset(i);
}

void GHOST_UnionRectangle(GHOST_RectangleHandle rectanglehandle,
                          GHOST_RectangleHandle anotherrectanglehandle)
{
  ((GHOST_Rect *)rectanglehandle)->unionRect(*(GHOST_Rect *)anotherrectanglehandle);
}

void GHOST_UnionPointRectangle(GHOST_RectangleHandle rectanglehandle, int32_t x, int32_t y)
{
  ((GHOST_Rect *)rectanglehandle)->unionPoint(x, y);
}

GHOST_TSuccess GHOST_IsInsideRectangle(GHOST_RectangleHandle rectanglehandle, int32_t x, int32_t y)
{
  GHOST_TSuccess result = GHOST_kFailure;

  if (((GHOST_Rect *)rectanglehandle)->isInside(x, y))
    result = GHOST_kSuccess;

  return result;
}

GHOST_TVisibility GHOST_GetRectangleVisibility(GHOST_RectangleHandle rectanglehandle,
                                               GHOST_RectangleHandle anotherrectanglehandle)
{
  GHOST_TVisibility visible = GHOST_kNotVisible;

  visible = ((GHOST_Rect *)rectanglehandle)->getVisibility(*(GHOST_Rect *)anotherrectanglehandle);

  return visible;
}

void GHOST_SetCenterRectangle(GHOST_RectangleHandle rectanglehandle, int32_t cx, int32_t cy)
{
  ((GHOST_Rect *)rectanglehandle)->setCenter(cx, cy);
}

void GHOST_SetRectangleCenter(
    GHOST_RectangleHandle rectanglehandle, int32_t cx, int32_t cy, int32_t w, int32_t h)
{
  ((GHOST_Rect *)rectanglehandle)->setCenter(cx, cy, w, h);
}

GHOST_TSuccess GHOST_ClipRectangle(GHOST_RectangleHandle rectanglehandle,
                                   GHOST_RectangleHandle anotherrectanglehandle)
{
  GHOST_TSuccess result = GHOST_kFailure;

  if (((GHOST_Rect *)rectanglehandle)->clip(*(GHOST_Rect *)anotherrectanglehandle))
    result = GHOST_kSuccess;

  return result;
}

char *GHOST_getClipboard(bool selection)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  return system->getClipboard(selection);
}

void GHOST_putClipboard(const char *buffer, bool selection)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  system->putClipboard(buffer, selection);
}

int GHOST_toggleConsole(int action)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  return system->toggleConsole(action);
}

int GHOST_UseNativePixels(void)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  return system->useNativePixel();
}

void GHOST_UseWindowFocus(int use_focus)
{
  GHOST_ISystem *system = GHOST_ISystem::getSystem();
  return system->useWindowFocus(use_focus);
}

float GHOST_GetNativePixelSize(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  if (window)
    return window->getNativePixelSize();
  return 1.0f;
}

uint16_t GHOST_GetDPIHint(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  return window->getDPIHint();
}

#ifdef WITH_INPUT_IME

void GHOST_BeginIME(
    GHOST_WindowHandle windowhandle, int32_t x, int32_t y, int32_t w, int32_t h, bool complete)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  window->beginIME(x, y, w, h, complete);
}

void GHOST_EndIME(GHOST_WindowHandle windowhandle)
{
  GHOST_IWindow *window = (GHOST_IWindow *)windowhandle;
  window->endIME();
}

#endif /* WITH_INPUT_IME */

#ifdef WITH_XR_OPENXR

#  define GHOST_XR_CAPI_CALL(call, ctx) \
    try { \
      call; \
    } \
    catch (GHOST_XrException & e) { \
      (ctx)->dispatchErrorMessage(&e); \
    }

#  define GHOST_XR_CAPI_CALL_RET(call, ctx) \
    try { \
      return call; \
    } \
    catch (GHOST_XrException & e) { \
      (ctx)->dispatchErrorMessage(&e); \
    }

void GHOST_XrSessionStart(GHOST_XrContextHandle xr_contexthandle,
                          const GHOST_XrSessionBeginInfo *begin_info)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XR_CAPI_CALL(xr_context->startSession(begin_info), xr_context);
}

void GHOST_XrSessionEnd(GHOST_XrContextHandle xr_contexthandle)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XR_CAPI_CALL(xr_context->endSession(), xr_context);
}

void GHOST_XrSessionDrawViews(GHOST_XrContextHandle xr_contexthandle, void *draw_customdata)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XR_CAPI_CALL(xr_context->drawSessionViews(draw_customdata), xr_context);
}

int GHOST_XrSessionIsRunning(const GHOST_XrContextHandle xr_contexthandle)
{
  const GHOST_IXrContext *xr_context = (const GHOST_IXrContext *)xr_contexthandle;
  GHOST_XR_CAPI_CALL_RET(xr_context->isSessionRunning(), xr_context);
  return 0; /* Only reached if exception is thrown. */
}

void GHOST_XrGraphicsContextBindFuncs(GHOST_XrContextHandle xr_contexthandle,
                                      GHOST_XrGraphicsContextBindFn bind_fn,
                                      GHOST_XrGraphicsContextUnbindFn unbind_fn)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XR_CAPI_CALL(xr_context->setGraphicsContextBindFuncs(bind_fn, unbind_fn), xr_context);
}

void GHOST_XrDrawViewFunc(GHOST_XrContextHandle xr_contexthandle, GHOST_XrDrawViewFn draw_view_fn)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XR_CAPI_CALL(xr_context->setDrawViewFunc(draw_view_fn), xr_context);
}

int GHOST_XrSessionNeedsUpsideDownDrawing(const GHOST_XrContextHandle xr_contexthandle)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;

  GHOST_XR_CAPI_CALL_RET(xr_context->needsUpsideDownDrawing(), xr_context);
  return 0; /* Only reached if exception is thrown. */
}

int GHOST_XrCreateActionSet(GHOST_XrContextHandle xr_contexthandle,
                            const GHOST_XrActionSetInfo *info)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->createActionSet(*info), xr_context);
  return 0;
}

void GHOST_XrDestroyActionSet(GHOST_XrContextHandle xr_contexthandle, const char *action_set_name)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->destroyActionSet(action_set_name), xr_context);
}

int GHOST_XrCreateActions(GHOST_XrContextHandle xr_contexthandle,
                          const char *action_set_name,
                          uint32_t count,
                          const GHOST_XrActionInfo *infos)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->createActions(action_set_name, count, infos), xr_context);
  return 0;
}

void GHOST_XrDestroyActions(GHOST_XrContextHandle xr_contexthandle,
                            const char *action_set_name,
                            uint32_t count,
                            const char *const *action_names)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->destroyActions(action_set_name, count, action_names), xr_context);
}

int GHOST_XrCreateActionBindings(GHOST_XrContextHandle xr_contexthandle,
                                 const char *action_set_name,
                                 uint32_t count,
                                 const GHOST_XrActionProfileInfo *infos)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->createActionBindings(action_set_name, count, infos),
                         xr_context);
  return 0;
}

void GHOST_XrDestroyActionBindings(GHOST_XrContextHandle xr_contexthandle,
                                   const char *action_set_name,
                                   uint32_t count,
                                   const char *const *action_names,
                                   const char *const *profile_paths)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(
      xr_session->destroyActionBindings(action_set_name, count, action_names, profile_paths),
      xr_context);
}

int GHOST_XrAttachActionSets(GHOST_XrContextHandle xr_contexthandle)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->attachActionSets(), xr_context);
  return 0;
}

int GHOST_XrSyncActions(GHOST_XrContextHandle xr_contexthandle, const char *action_set_name)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->syncActions(action_set_name), xr_context);
  return 0;
}

int GHOST_XrApplyHapticAction(GHOST_XrContextHandle xr_contexthandle,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path,
                              const int64_t *duration,
                              const float *frequency,
                              const float *amplitude)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(
      xr_session->applyHapticAction(
          action_set_name, action_name, subaction_path, *duration, *frequency, *amplitude),
      xr_context);
  return 0;
}

void GHOST_XrStopHapticAction(GHOST_XrContextHandle xr_contexthandle,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->stopHapticAction(action_set_name, action_name, subaction_path),
                     xr_context);
}

void *GHOST_XrGetActionSetCustomdata(GHOST_XrContextHandle xr_contexthandle,
                                     const char *action_set_name)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getActionSetCustomdata(action_set_name), xr_context);
  return 0;
}

void *GHOST_XrGetActionCustomdata(GHOST_XrContextHandle xr_contexthandle,
                                  const char *action_set_name,
                                  const char *action_name)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getActionCustomdata(action_set_name, action_name),
                         xr_context);
  return 0;
}

unsigned int GHOST_XrGetActionCount(GHOST_XrContextHandle xr_contexthandle,
                                    const char *action_set_name)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getActionCount(action_set_name), xr_context);
  return 0;
}

void GHOST_XrGetActionCustomdataArray(GHOST_XrContextHandle xr_contexthandle,
                                      const char *action_set_name,
                                      void **r_customdata_array)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->getActionCustomdataArray(action_set_name, r_customdata_array),
                     xr_context);
}

int GHOST_XrLoadControllerModel(GHOST_XrContextHandle xr_contexthandle, const char *subaction_path)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->loadControllerModel(subaction_path), xr_context);
  return 0;
}

void GHOST_XrUnloadControllerModel(GHOST_XrContextHandle xr_contexthandle,
                                   const char *subaction_path)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->unloadControllerModel(subaction_path), xr_context);
}

int GHOST_XrUpdateControllerModelComponents(GHOST_XrContextHandle xr_contexthandle,
                                            const char *subaction_path)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->updateControllerModelComponents(subaction_path), xr_context);
  return 0;
}

int GHOST_XrGetControllerModelData(GHOST_XrContextHandle xr_contexthandle,
                                   const char *subaction_path,
                                   GHOST_XrControllerModelData *r_data)
{
  GHOST_IXrContext *xr_context = (GHOST_IXrContext *)xr_contexthandle;
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getControllerModelData(subaction_path, *r_data), xr_context);
  return 0;
}

#endif /* WITH_XR_OPENXR */
