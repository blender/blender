/**
 * $Id$
 *
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
   
#include <assert.h>
#include <unistd.h>
#include "GPU_Engine.h"
#include "GPC_MouseDevice.h"
#include "GPU_Canvas.h"
#include "GPU_KeyboardDevice.h"
#include "GPU_System.h"

#include "BLI_blenlib.h"
#include "BLO_readfile.h"

#include "SND_DeviceManager.h"

#include "NG_NetworkScene.h"
#include "NG_LoopBackNetworkDeviceInterface.h"
#include "SND_DeviceManager.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_KetsjiEngine.h"

#include "GPC_RenderTools.h"
#include "GPC_RawImage.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void Redraw(GPU_Engine *engine);  // -the- redraw function

// callback functions
/*
void RedrawCallback(Widget, XtPointer closure, XEvent *, Boolean *continue_to_dispatch);
 
void KeyDownCallback(Widget w, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch);
void KeyUpCallback(Widget w, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch);
 
void ButtonPressReleaseCallback(Widget w, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch);
void PointerMotionCallback(Widget w, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch);
 
void TimeOutCallback(XtPointer closure, XtIntervalId *id);
*/

GPU_Engine::GPU_Engine(char *customLoadingAnimationURL,
		int foregroundColor, int backgroundColor, int frameRate) :
		GPC_Engine(customLoadingAnimationURL, foregroundColor, backgroundColor,
		frameRate), m_timerTimeOutMsecs(10)
{
}


GPU_Engine::~GPU_Engine()
{
}

/* 
bool GPU_Engine::Initialize(Display *display, Window window, int width, int height)
{
	SND_DeviceManager::Subscribe();
	m_audiodevice = SND_DeviceManager::Instance(); 

	m_keyboarddev = new GPU_KeyboardDevice();
	m_mousedev = new GPC_MouseDevice();
		
	// constructor only initializes data
	//	m_canvas = new GPU_Canvas(display, window, width, height);
	//m_canvas->Init();  // create the actual visual and rendering context
	//cout << "GPU_Canvas created and initialized, m_canvas " << m_canvas << endl;
	//AddEventHandlers();  // done here (in GPU_Engine) since the event handlers need access to 'this', ie the engine

	// put the Blender logo in the topleft corner
	if(m_BlenderLogo != 0)
		// adding a banner automatically enables them
		m_BlenderLogoId = m_canvas->AddBanner(m_BlenderLogo->Width(), m_BlenderLogo->Height(),
				m_BlenderLogo->Width(), m_BlenderLogo->Height(),
				m_BlenderLogo->Data(), GPC_Canvas::alignTopLeft);

	// put the Blender3D logo in the bottom right corner
	if(m_Blender3DLogo != 0)
		// adding a banner automatically enables them
		m_Blender3DLogoId = m_canvas->AddBanner(m_Blender3DLogo->Width(), m_Blender3DLogo->Height(),
				m_Blender3DLogo->Width(), m_Blender3DLogo->Height(),
				m_Blender3DLogo->Data(), GPC_Canvas::alignTopLeft);

#if 0
	// put the NaN logo in the bottom right corner
	if(m_NaNLogo != 0)
		// adding a banner automatically enables them
		m_NaNLogoId = m_canvas->AddBanner(m_NaNLogo->Width(), m_NaNLogo->Height(),
				m_NaNLogo->Width(), m_NaNLogo->Height(),
				m_NaNLogo->Data(), GPC_Canvas::alignBottomRight);
#endif
	// enable the display of all banners
	m_canvas->SetBannerDisplayEnabled(true);

	m_rendertools = new GPC_RenderTools();

	m_networkdev = new NG_LoopBackNetworkDeviceInterface();
	assert(m_networkdev);

	// creation of system needs 'current rendering context', this is taken care
	// of by the GPU_Canvas::Init()
	m_system = new GPU_System();

	m_system->SetKeyboardDevice((GPU_KeyboardDevice *)m_keyboarddev);
	m_system->SetMouseDevice(m_mousedev);
	m_system->SetNetworkDevice(m_networkdev);

	m_initialized = true;

	return m_initialized;
}
*/

/* 
void GPU_Engine::HandleNewWindow(Window window)
{
	// everything only if it's really a new window
	if(window != ((GPU_Canvas *)m_canvas)->GetWindow())
	{
		cout << "GPU_Engine::HandleNewWindow(), new window so calling SetNewWindowMakeNewWidgetAndMakeCurrent()" << endl;
		// We don't have to remove the event handlers ourselves, they are destroyed by X11
		
		// make canvas aware of new window, and make it current
		((GPU_Canvas *)m_canvas)->SetNewWindowMakeNewWidgetAndMakeCurrent(window);
		
		// and add event handlers to new widget
		AddEventHandlers();
		cout << "GPU_Engine::HandleNewWindow(), event handlers added" << endl;
	}
}
*/
/*
void GPU_Engine::AddEventHandlers(void)
{
	Widget widget = ((GPU_Canvas *)m_canvas)->GetWidget();

	// redraw
	// MUST be the *Raw* event handler, the normal one doesn't work!
	XtAddRawEventHandler(widget, ExposureMask, FALSE, RedrawCallback, this);
#if 0
	// key down
	XtAddRawEventHandler(widget, KeyPressMask, FALSE, KeyDownCallback, this);
	// key up
	XtAddRawEventHandler(widget, KeyReleaseMask, FALSE, KeyUpCallback, this);
 
	// mouse button press
	XtAddRawEventHandler(widget, ButtonPressMask, FALSE, ButtonPressReleaseCallback, this);
	// mouse button release
	XtAddRawEventHandler(widget, ButtonReleaseMask, FALSE, ButtonPressReleaseCallback, this);
	// mouse motion
	XtAddRawEventHandler(widget, PointerMotionMask, FALSE, PointerMotionCallback, this);
#endif
#if 0
	// time out, not a real timer. New time out will be set in callback
	m_timerId = XtAppAddTimeOut(XtWidgetToApplicationContext(widget),
					m_timerTimeOutMsecs, TimeOutCallback, this);
#endif
}
*/

void Redraw(GPU_Engine *engine)
{
/*	if(engine->Running())
	{
		if(engine->Loading())
		{
			engine->UpdateLoadingAnimation();
		}
 
		engine->m_system->DoMainLoopCallback();
	}*/
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * Callback functions
 +++++++++++++++++++++++++++++++++++++++++++++++++*/
void RedrawCallback(Widget, XtPointer closure, XEvent *, Boolean *continue_to_dispatch)
{
	GPU_Engine *engine = (GPU_Engine *)closure;

	Redraw(engine);

	*continue_to_dispatch = True;
}


void KeyDownCallback(Widget, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch)
{
/*	GPU_Engine *engine = (GPU_Engine *)closure;
	XKeyEvent *keyEvent = (XKeyEvent *)event;
 
	if(engine->Running())
		engine->m_system->AddKey(int(keyEvent->keycode), 1);

	*continue_to_dispatch = True;*/
}
 
 
void KeyUpCallback(Widget, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch)
{
/*	GPU_Engine *engine = (GPU_Engine *)closure;
	XKeyEvent *keyEvent = (XKeyEvent *)event;
 
	if(engine->Running())
		engine->m_system->AddKey(int(keyEvent->keycode), 0);

	*continue_to_dispatch = True;*/
}
 
 
void ButtonPressReleaseCallback(Widget, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch)
{
	GPU_Engine *engine = (GPU_Engine *)closure;
	XButtonEvent *buttonEvent = (XButtonEvent *)event;
	bool isDown;
	GPC_MouseDevice::TButtonId button;
 
	if(engine->Running())
	{
		// determine type of event, press or release
		isDown = false;
		if(buttonEvent->type == ButtonPress)
		isDown = true;
		// determine which button exactly generated this event
		switch(buttonEvent->button)
		{
			case 1:
				button = GPC_MouseDevice::buttonLeft;
				break;
			case 2:
				button = GPC_MouseDevice::buttonMiddle;
				break;
			case 3:
				button = GPC_MouseDevice::buttonRight;
				break;
		}
		engine->m_mousedev->ConvertButtonEvent(button,
				isDown, buttonEvent->x, buttonEvent->y);
	}

	*continue_to_dispatch = True;
}
 
 
void PointerMotionCallback(Widget w, XtPointer closure, XEvent *event, Boolean *continue_to_dispatch)
{
	GPU_Engine *engine = (GPU_Engine *)closure;
	XButtonEvent *buttonEvent = (XButtonEvent *)event;
 
	if(engine->Running())
	{
		engine->m_mousedev->ConvertMoveEvent(buttonEvent->x, buttonEvent->y);
	}

	*continue_to_dispatch = True;
}

/*
void TimeOutCallback(XtPointer closure, XtIntervalId *id)
{
	GPU_Engine *engine = (GPU_Engine *)closure;

	Redraw(engine);
	// add a new time out since there is no real timer for X (not a simple one like under windows)
	// TODO Have to get faster timer !

	if(engine->Running())
		engine->m_timerId = XtAppAddTimeOut(XtWidgetToApplicationContext(
			((GPU_Canvas *)engine->m_canvas)->GetWidget()),
			engine->m_timerTimeOutMsecs, TimeOutCallback,
			closure);
}

*/
