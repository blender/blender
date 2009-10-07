/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*

 * GHOST_C-Api.cpp
 *
 * C Api for GHOST
 *
 * Version: $Id$
 */

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "intern/GHOST_Debug.h"
#include "GHOST_C-api.h"
#include "GHOST_ISystem.h"
#include "GHOST_IEvent.h"
#include "GHOST_IEventConsumer.h"
#include "intern/GHOST_CallbackEventConsumer.h"

GHOST_SystemHandle GHOST_CreateSystem(void)
{
	GHOST_ISystem::createSystem();
	GHOST_ISystem* system = GHOST_ISystem::getSystem();

	return (GHOST_SystemHandle)system;
}



GHOST_TSuccess GHOST_DisposeSystem(GHOST_SystemHandle systemhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

	return system->disposeSystem();
}


GHOST_EventConsumerHandle GHOST_CreateEventConsumer(GHOST_EventCallbackProcPtr eventCallback, GHOST_TUserDataPtr userdata)
{
	return (GHOST_EventConsumerHandle) new GHOST_CallbackEventConsumer (eventCallback, userdata);
}


GHOST_TSuccess GHOST_DisposeEventConsumer(GHOST_EventConsumerHandle consumerhandle)
{
	delete ((GHOST_CallbackEventConsumer*)consumerhandle);
	return GHOST_kSuccess;
}


GHOST_TUns64 GHOST_GetMilliSeconds(GHOST_SystemHandle systemhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

	return system->getMilliSeconds();
}



GHOST_TimerTaskHandle GHOST_InstallTimer(GHOST_SystemHandle systemhandle,
										 GHOST_TUns64 delay,
										 GHOST_TUns64 interval,
										 GHOST_TimerProcPtr timerproc,
										 GHOST_TUserDataPtr userdata)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

	return (GHOST_TimerTaskHandle) system->installTimer(delay, interval, timerproc, userdata);
}



GHOST_TSuccess GHOST_RemoveTimer(GHOST_SystemHandle systemhandle,
								 GHOST_TimerTaskHandle timertaskhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	GHOST_ITimerTask* timertask = (GHOST_ITimerTask*) timertaskhandle;

	return system->removeTimer(timertask);
}



GHOST_TUns8 GHOST_GetNumDisplays(GHOST_SystemHandle systemhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

	return system->getNumDisplays();
}



void GHOST_GetMainDisplayDimensions(GHOST_SystemHandle systemhandle,
									GHOST_TUns32* width,
									GHOST_TUns32* height)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	
	system->getMainDisplayDimensions(*width, *height);
}



GHOST_WindowHandle GHOST_CreateWindow(GHOST_SystemHandle systemhandle,
									  char* title,
									  GHOST_TInt32 left,
									  GHOST_TInt32 top,
									  GHOST_TUns32 width,
									  GHOST_TUns32 height,
									  GHOST_TWindowState state,
									  GHOST_TDrawingContextType type,
									  const int stereoVisual)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	bool bstereoVisual;

	if(stereoVisual)
		bstereoVisual = true;
	else
		bstereoVisual = false;

	return (GHOST_WindowHandle) system->createWindow(title, left, top, width, height,
		state, type, bstereoVisual);
}

GHOST_TUserDataPtr GHOST_GetWindowUserData(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->getUserData();
}
void GHOST_SetWindowUserData(GHOST_WindowHandle windowhandle, GHOST_TUserDataPtr userdata)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	window->setUserData(userdata);
}

GHOST_TSuccess GHOST_DisposeWindow(GHOST_SystemHandle systemhandle,
								   GHOST_WindowHandle windowhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	return system->disposeWindow(window);
}



int GHOST_ValidWindow(GHOST_SystemHandle systemhandle,
								 GHOST_WindowHandle windowhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	return (int) system->validWindow(window);
}



GHOST_WindowHandle GHOST_BeginFullScreen(GHOST_SystemHandle systemhandle,
										 GHOST_DisplaySetting* setting,
										 const int stereoVisual)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	GHOST_IWindow* window = NULL;
	bool bstereoVisual;

	if(stereoVisual)
		bstereoVisual = true;
	else
		bstereoVisual = false;
	
	system->beginFullScreen(*setting, &window, bstereoVisual);

	return (GHOST_WindowHandle)window;
}



GHOST_TSuccess GHOST_EndFullScreen(GHOST_SystemHandle systemhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

	return system->endFullScreen();
}



int GHOST_GetFullScreen(GHOST_SystemHandle systemhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

	return (int) system->getFullScreen();
}



int GHOST_ProcessEvents(GHOST_SystemHandle systemhandle, int waitForEvent)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	
	return (int) system->processEvents(waitForEvent?true:false);
}



int GHOST_DispatchEvents(GHOST_SystemHandle systemhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	
	return (int) system->dispatchEvents();
}


GHOST_TSuccess GHOST_AddEventConsumer(GHOST_SystemHandle systemhandle, GHOST_EventConsumerHandle consumerhandle)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	
	return system->addEventConsumer((GHOST_CallbackEventConsumer*)consumerhandle);
}

int GHOST_OpenNDOF(GHOST_SystemHandle systemhandle, GHOST_WindowHandle windowhandle,
   GHOST_NDOFLibraryInit_fp setNdofLibraryInit, 
    GHOST_NDOFLibraryShutdown_fp setNdofLibraryShutdown,
    GHOST_NDOFDeviceOpen_fp setNdofDeviceOpen)
  //original patch only
  /*  GHOST_NDOFEventHandler_fp setNdofEventHandler)*/
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;

    return system->openNDOF((GHOST_IWindow*) windowhandle,
        setNdofLibraryInit, setNdofLibraryShutdown, setNdofDeviceOpen);
//	original patch
//        setNdofLibraryInit, setNdofLibraryShutdown, setNdofDeviceOpen, setNdofEventHandler);
}



GHOST_TStandardCursor GHOST_GetCursorShape(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->getCursorShape();
}



GHOST_TSuccess GHOST_SetCursorShape(GHOST_WindowHandle windowhandle,
									GHOST_TStandardCursor cursorshape)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setCursorShape(cursorshape);
}

GHOST_TSuccess GHOST_SetCustomCursorShape(GHOST_WindowHandle windowhandle,
										  GHOST_TUns8 bitmap[16][2], 
										  GHOST_TUns8 mask[16][2], 
										  int hotX, 
										  int hotY)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setCustomCursorShape(bitmap, mask, hotX, hotY);
}

GHOST_TSuccess GHOST_SetCustomCursorShapeEx(GHOST_WindowHandle windowhandle,
										  GHOST_TUns8 *bitmap, 
										  GHOST_TUns8 *mask, 
										  int sizex, 
										  int sizey,
										  int hotX, 
										  int hotY, 
										  int fg_color, 
										  int bg_color)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setCustomCursorShape(bitmap, mask, sizex, sizey, 
												hotX, hotY, fg_color, bg_color);
}



int GHOST_GetCursorVisibility(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return (int) window->getCursorVisibility();
}



GHOST_TSuccess GHOST_SetCursorVisibility(GHOST_WindowHandle windowhandle,
										 int visible)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setCursorVisibility(visible?true:false);
}



GHOST_TSuccess GHOST_GetCursorPosition(GHOST_SystemHandle systemhandle,
									   GHOST_TInt32* x,
									   GHOST_TInt32* y)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	
	return system->getCursorPosition(*x, *y);
}



GHOST_TSuccess GHOST_SetCursorPosition(GHOST_SystemHandle systemhandle,
									   GHOST_TInt32 x,
									   GHOST_TInt32 y)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	
	return system->setCursorPosition(x, y);
}


GHOST_TSuccess GHOST_SetCursorGrab(GHOST_WindowHandle windowhandle,
								   int grab, int warp)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	return window->setCursorGrab(grab?true:false, warp?true:false);
}


GHOST_TSuccess GHOST_GetModifierKeyState(GHOST_SystemHandle systemhandle,
										 GHOST_TModifierKeyMask mask,
										 int* isDown)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	GHOST_TSuccess result;
	bool isdown;
	
	result = system->getModifierKeyState(mask, isdown);
	*isDown = (int) isdown;

	return result;
}



GHOST_TSuccess GHOST_GetButtonState(GHOST_SystemHandle systemhandle,
									GHOST_TButtonMask mask,
									int* isDown)
{
	GHOST_ISystem* system = (GHOST_ISystem*) systemhandle;
	GHOST_TSuccess result;
	bool isdown;
	
	result = system->getButtonState(mask, isdown);
	*isDown = (int) isdown;

	return result;
}



GHOST_TEventType GHOST_GetEventType(GHOST_EventHandle eventhandle)
{
	GHOST_IEvent* event = (GHOST_IEvent*) eventhandle;
	
	return event->getType();
}



GHOST_TUns64 GHOST_GetEventTime(GHOST_EventHandle eventhandle)
{
	GHOST_IEvent* event = (GHOST_IEvent*) eventhandle;

	return event->getTime();
}


GHOST_WindowHandle GHOST_GetEventWindow(GHOST_EventHandle eventhandle)
{
	GHOST_IEvent* event = (GHOST_IEvent*) eventhandle;

	return (GHOST_WindowHandle) event->getWindow();
}


GHOST_TEventDataPtr GHOST_GetEventData(GHOST_EventHandle eventhandle)
{
	GHOST_IEvent* event = (GHOST_IEvent*) eventhandle;
	
	return event->getData();
}



GHOST_TimerProcPtr GHOST_GetTimerProc(GHOST_TimerTaskHandle timertaskhandle)
{
	GHOST_ITimerTask* timertask = (GHOST_ITimerTask*) timertaskhandle;
	
	return timertask->getTimerProc();
}



void GHOST_SetTimerProc(GHOST_TimerTaskHandle timertaskhandle,
						GHOST_TimerProcPtr timerproc)
{
	GHOST_ITimerTask* timertask = (GHOST_ITimerTask*) timertaskhandle;
	
	timertask->setTimerProc(timerproc);
}



GHOST_TUserDataPtr GHOST_GetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle)
{
	GHOST_ITimerTask* timertask = (GHOST_ITimerTask*) timertaskhandle;

	return timertask->getUserData();
}

	

void GHOST_SetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle,
								GHOST_TUserDataPtr userdata)
{
	GHOST_ITimerTask* timertask = (GHOST_ITimerTask*) timertaskhandle;
	
	timertask->setUserData(userdata);
}



int GHOST_GetValid(GHOST_WindowHandle windowhandle) 
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return (int) window->getValid();
}



GHOST_TDrawingContextType GHOST_GetDrawingContextType(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->getDrawingContextType();
}



GHOST_TSuccess GHOST_SetDrawingContextType(GHOST_WindowHandle windowhandle,
										   GHOST_TDrawingContextType type)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setDrawingContextType(type);
}



void GHOST_SetTitle(GHOST_WindowHandle windowhandle,
					char* title)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	window->setTitle(title);
}


char* GHOST_GetTitle(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	STR_String title;

	window->getTitle(title);

	char *ctitle = (char*) malloc(title.Length() + 1);

	if (ctitle == NULL) return NULL;
	strcpy(ctitle, title.Ptr());
		
	return ctitle;
}



GHOST_RectangleHandle GHOST_GetWindowBounds(GHOST_WindowHandle windowhandle) 
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	GHOST_Rect* rectangle = NULL;

	rectangle = new GHOST_Rect();
	window->getWindowBounds(*rectangle);

	return (GHOST_RectangleHandle)rectangle;
}



GHOST_RectangleHandle GHOST_GetClientBounds(GHOST_WindowHandle windowhandle) 
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	GHOST_Rect* rectangle = NULL;

	rectangle = new GHOST_Rect();
	window->getClientBounds(*rectangle);

	return (GHOST_RectangleHandle)rectangle;
}



void GHOST_DisposeRectangle(GHOST_RectangleHandle rectanglehandle)
{
	delete (GHOST_Rect*) rectanglehandle;
}



GHOST_TSuccess GHOST_SetClientWidth(GHOST_WindowHandle windowhandle,
									GHOST_TUns32 width)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setClientWidth(width);
}



GHOST_TSuccess GHOST_SetClientHeight(GHOST_WindowHandle windowhandle,
									 GHOST_TUns32 height)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setClientHeight(height);
}



GHOST_TSuccess GHOST_SetClientSize(GHOST_WindowHandle windowhandle,
								   GHOST_TUns32 width,
								   GHOST_TUns32 height)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setClientSize(width, height);
}



void GHOST_ScreenToClient(GHOST_WindowHandle windowhandle,
						  GHOST_TInt32 inX,
						  GHOST_TInt32 inY,
						  GHOST_TInt32* outX,
						  GHOST_TInt32* outY) 
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	window->screenToClient(inX, inY, *outX, *outY);
}



void GHOST_ClientToScreen(GHOST_WindowHandle windowhandle,
						  GHOST_TInt32 inX,
						  GHOST_TInt32 inY,
						  GHOST_TInt32* outX,
						  GHOST_TInt32* outY)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	window->clientToScreen(inX, inY, *outX, *outY);
}



GHOST_TWindowState GHOST_GetWindowState(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->getState();
}



GHOST_TSuccess GHOST_SetWindowState(GHOST_WindowHandle windowhandle,
									GHOST_TWindowState state)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->setState(state);
}


GHOST_TSuccess GHOST_SetWindowModifiedState(GHOST_WindowHandle windowhandle, GHOST_TUns8 isUnsavedChanges)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	return window->setModifiedState(isUnsavedChanges);
}	


GHOST_TSuccess GHOST_SetWindowOrder(GHOST_WindowHandle windowhandle,
									GHOST_TWindowOrder order)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	return window->setOrder(order);
}



GHOST_TSuccess GHOST_SwapWindowBuffers(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->swapBuffers();
}



GHOST_TSuccess GHOST_ActivateWindowDrawingContext(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;
	
	return window->activateDrawingContext();
}



GHOST_TSuccess GHOST_InvalidateWindow(GHOST_WindowHandle windowhandle)
{
	GHOST_IWindow* window = (GHOST_IWindow*) windowhandle;

	return window->invalidate();
}


extern const GHOST_TabletData* GHOST_GetTabletData(GHOST_WindowHandle windowhandle)
{
	return ((GHOST_IWindow*)windowhandle)->GetTabletData();
}


GHOST_TInt32 GHOST_GetWidthRectangle(GHOST_RectangleHandle rectanglehandle)
{
	return ((GHOST_Rect*)rectanglehandle)->getWidth();
}



GHOST_TInt32 GHOST_GetHeightRectangle(GHOST_RectangleHandle rectanglehandle)
{
	return ((GHOST_Rect*)rectanglehandle)->getHeight();
}



void GHOST_GetRectangle(GHOST_RectangleHandle rectanglehandle,
							   GHOST_TInt32* l,
							   GHOST_TInt32* t,
							   GHOST_TInt32* r,
							   GHOST_TInt32* b)
{
	GHOST_Rect *rect= (GHOST_Rect*) rectanglehandle;
	
	*l= rect->m_l;
	*t= rect->m_t;
	*r= rect->m_r;
	*b= rect->m_b;
}


void GHOST_SetRectangle(GHOST_RectangleHandle rectanglehandle,
						GHOST_TInt32 l,
						GHOST_TInt32 t,
						GHOST_TInt32 r,
						GHOST_TInt32 b)
{
	((GHOST_Rect*)rectanglehandle)->set(l, t, r, b);
}



GHOST_TSuccess GHOST_IsEmptyRectangle(GHOST_RectangleHandle rectanglehandle)
{
	GHOST_TSuccess result = GHOST_kFailure;

	if (((GHOST_Rect*)rectanglehandle)->isEmpty())
		result = GHOST_kSuccess;

	return result;
}



GHOST_TSuccess GHOST_IsValidRectangle(GHOST_RectangleHandle rectanglehandle)
{
	GHOST_TSuccess result = GHOST_kFailure;

	if(((GHOST_Rect*)rectanglehandle)->isValid())
		result = GHOST_kSuccess;

	return result;
}



void GHOST_InsetRectangle(GHOST_RectangleHandle rectanglehandle,
						  GHOST_TInt32 i)
{
	((GHOST_Rect*)rectanglehandle)->inset(i);
}



void GHOST_UnionRectangle(GHOST_RectangleHandle rectanglehandle,
						  GHOST_RectangleHandle anotherrectanglehandle)
{
	((GHOST_Rect*)rectanglehandle)->unionRect(*(GHOST_Rect*)anotherrectanglehandle);
}



void GHOST_UnionPointRectangle(GHOST_RectangleHandle rectanglehandle,
							   GHOST_TInt32 x,
							   GHOST_TInt32 y)
{
	((GHOST_Rect*)rectanglehandle)->unionPoint(x, y);
}



GHOST_TSuccess GHOST_IsInsideRectangle(GHOST_RectangleHandle rectanglehandle,
									   GHOST_TInt32 x,
									   GHOST_TInt32 y)
{
	GHOST_TSuccess result = GHOST_kFailure;

	if (((GHOST_Rect*)rectanglehandle)->isInside(x, y))
		result = GHOST_kSuccess;

	return result;
}



GHOST_TVisibility GHOST_GetRectangleVisibility(GHOST_RectangleHandle rectanglehandle,
											   GHOST_RectangleHandle anotherrectanglehandle)
{
	GHOST_TVisibility visible = GHOST_kNotVisible;

	visible = ((GHOST_Rect*)rectanglehandle)->getVisibility(*(GHOST_Rect*)anotherrectanglehandle);

	return visible;
}



void GHOST_SetCenterRectangle(GHOST_RectangleHandle rectanglehandle,
							  GHOST_TInt32 cx,
							  GHOST_TInt32 cy)
{
	((GHOST_Rect*)rectanglehandle)->setCenter(cx, cy);
}



void GHOST_SetRectangleCenter(GHOST_RectangleHandle rectanglehandle,
							  GHOST_TInt32 cx,
							  GHOST_TInt32 cy,
							  GHOST_TInt32 w,
							  GHOST_TInt32 h)
{
	((GHOST_Rect*)rectanglehandle)->setCenter(cx, cy, w, h);
}



GHOST_TSuccess GHOST_ClipRectangle(GHOST_RectangleHandle rectanglehandle,
								   GHOST_RectangleHandle anotherrectanglehandle)
{
	GHOST_TSuccess result = GHOST_kFailure;

	if (((GHOST_Rect*)rectanglehandle)->clip(*(GHOST_Rect*)anotherrectanglehandle))
		result = GHOST_kSuccess;

	return result;
}

GHOST_TUns8* GHOST_getClipboard(int selection)
{
	GHOST_ISystem* system = GHOST_ISystem::getSystem();
	return system->getClipboard(selection);
}

void GHOST_putClipboard(GHOST_TInt8 *buffer, int selection)
{
	GHOST_ISystem* system = GHOST_ISystem::getSystem();
	system->putClipboard(buffer, selection);
}

