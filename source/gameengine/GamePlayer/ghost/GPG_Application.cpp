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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * GHOST Blender Player application implementation file.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
	#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
	#include <windows.h>
#endif

#include "GL/glew.h"
#include "GPU_extensions.h"

#include "GPG_Application.h"

#include <iostream>
#include <MT_assert.h>
#include <stdlib.h>

/**********************************
 * Begin Blender include block
 **********************************/
#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus
#include "BLI_blenlib.h"
#include "BLO_readfile.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"
#include "IMB_imbuf.h"
#include "DNA_scene_types.h"
#ifdef __cplusplus
}
#endif // __cplusplus
/**********************************
 * End Blender include block
 **********************************/


#include "SYS_System.h"
#include "KX_KetsjiEngine.h"

// include files needed by "KX_BlenderSceneConverter.h"
#include "GEN_Map.h"
#include "SCA_IActuator.h"
#include "RAS_MeshObject.h"
#include "RAS_OpenGLRasterizer.h"
#include "RAS_VAOpenGLRasterizer.h"
#include "RAS_ListRasterizer.h"
#include "RAS_GLExtensionManager.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"
#include "BL_Material.h" // MAXTEX

#include "KX_BlenderSceneConverter.h"
#include "NG_LoopBackNetworkDeviceInterface.h"

#include "GPC_MouseDevice.h"
#include "GPC_RenderTools.h"
#include "GPG_Canvas.h" 
#include "GPG_KeyboardDevice.h"
#include "GPG_System.h"

#include "STR_String.h"

#include "GHOST_ISystem.h"
#include "GHOST_IEvent.h"
#include "GHOST_IEventConsumer.h"
#include "GHOST_IWindow.h"
#include "GHOST_Rect.h"

static void frameTimerProc(GHOST_ITimerTask* task, GHOST_TUns64 time);

static GHOST_ISystem* fSystem = 0;
static const int kTimerFreq = 10;

GPG_Application::GPG_Application(GHOST_ISystem* system)
	: m_startSceneName(""), 
	  m_startScene(0),
	  m_maggie(0),
	  m_exitRequested(0),
	  m_system(system), 
	  m_mainWindow(0), 
	  m_frameTimer(0), 
	  m_cursor(GHOST_kStandardCursorFirstCursor),
	  m_engineInitialized(0), 
	  m_engineRunning(0), 
	  m_isEmbedded(false),
	  m_ketsjiengine(0),
	  m_kxsystem(0), 
	  m_keyboard(0), 
	  m_mouse(0), 
	  m_canvas(0), 
	  m_rendertools(0), 
	  m_rasterizer(0), 
	  m_sceneconverter(0),
	  m_networkdevice(0),
	  m_blendermat(0),
	  m_blenderglslmat(0),
	  m_pyGlobalDictString(0),
	  m_pyGlobalDictString_Length(0)
{
	fSystem = system;
}



GPG_Application::~GPG_Application(void)
{
    if(m_pyGlobalDictString) {
		delete [] m_pyGlobalDictString;
		m_pyGlobalDictString = 0;
		m_pyGlobalDictString_Length = 0;
	}

	exitEngine();
	fSystem->disposeWindow(m_mainWindow);
}



bool GPG_Application::SetGameEngineData(struct Main* maggie, Scene *scene, int argc, char **argv)
{
	bool result = false;

	if (maggie != NULL && scene != NULL)
	{
// XXX		G.scene = scene;
		m_maggie = maggie;
		m_startSceneName = scene->id.name+2;
		m_startScene = scene;
		result = true;
	}
	
	/* Python needs these */
	m_argc= argc;
	m_argv= argv;

	return result;
}


#ifdef WIN32
#define SCR_SAVE_MOUSE_MOVE_THRESHOLD 15

static HWND found_ghost_window_hwnd;
static GHOST_IWindow* ghost_window_to_find;
static WNDPROC ghost_wnd_proc;
static POINT scr_save_mouse_pos;

static LRESULT CALLBACK screenSaverWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL close = FALSE;
	switch (uMsg)
	{
		case WM_MOUSEMOVE:
		{ 
			POINT pt; 
			GetCursorPos(&pt);
			LONG dx = scr_save_mouse_pos.x - pt.x;
			LONG dy = scr_save_mouse_pos.y - pt.y;
			if (abs(dx) > SCR_SAVE_MOUSE_MOVE_THRESHOLD
			    || abs(dy) > SCR_SAVE_MOUSE_MOVE_THRESHOLD)
			{
				close = TRUE;
			}
			scr_save_mouse_pos = pt;
			break;
		}
		case WM_LBUTTONDOWN: 
		case WM_MBUTTONDOWN: 
		case WM_RBUTTONDOWN: 
		case WM_KEYDOWN:
			close = TRUE;
	}
	if (close)
		PostMessage(hwnd,WM_CLOSE,0,0);
	return CallWindowProc(ghost_wnd_proc, hwnd, uMsg, wParam, lParam);
}

BOOL CALLBACK findGhostWindowHWNDProc(HWND hwnd, LPARAM lParam)
{
	GHOST_IWindow *p = (GHOST_IWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	BOOL ret = TRUE;
	if (p == ghost_window_to_find)
	{
		found_ghost_window_hwnd = hwnd;
		ret = FALSE;
	}
	return ret;
}

static HWND findGhostWindowHWND(GHOST_IWindow* window)
{
	found_ghost_window_hwnd = NULL;
	ghost_window_to_find = window;
	EnumWindows(findGhostWindowHWNDProc, NULL);
	return found_ghost_window_hwnd;
}

bool GPG_Application::startScreenSaverPreview(
	HWND parentWindow,
	const bool stereoVisual,
	const int stereoMode)
{
	bool success = false;

	RECT rc;
	if (GetWindowRect(parentWindow, &rc))
	{
		int windowWidth = rc.right - rc.left;
		int windowHeight = rc.bottom - rc.top;
		STR_String title = "";
							
		m_mainWindow = fSystem->createWindow(title, 0, 0, windowWidth, windowHeight, GHOST_kWindowStateMinimized,
			GHOST_kDrawingContextTypeOpenGL, stereoVisual);
		if (!m_mainWindow) {
			printf("error: could not create main window\n");
			exit(-1);
		}

		HWND ghost_hwnd = findGhostWindowHWND(m_mainWindow);
		if (!ghost_hwnd) {
			printf("error: could find main window\n");
			exit(-1);
		}

		SetParent(ghost_hwnd, parentWindow);
		LONG style = GetWindowLong(ghost_hwnd, GWL_STYLE);
		LONG exstyle = GetWindowLong(ghost_hwnd, GWL_EXSTYLE);

		RECT adjrc = { 0, 0, windowWidth, windowHeight };
		AdjustWindowRectEx(&adjrc, style, FALSE, exstyle);

		style = (style & (~(WS_POPUP|WS_OVERLAPPEDWINDOW|WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_TILEDWINDOW ))) | WS_CHILD;
		SetWindowLong(ghost_hwnd, GWL_STYLE, style);
		SetWindowPos(ghost_hwnd, NULL, adjrc.left, adjrc.top, 0, 0, SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);

		/* Check the size of the client rectangle of the window and resize the window
		 * so that the client rectangle has the size requested.
		 */
		m_mainWindow->setClientSize(windowWidth, windowHeight);

		success = initEngine(m_mainWindow, stereoMode);
		if (success) {
			success = startEngine();
		}

	}
	return success;
}

bool GPG_Application::startScreenSaverFullScreen(
		int width,
		int height,
		int bpp,int frequency,
		const bool stereoVisual,
		const int stereoMode)
{
	bool ret = startFullScreen(width, height, bpp, frequency, stereoVisual, stereoMode);
	if (ret)
	{
		HWND ghost_hwnd = findGhostWindowHWND(m_mainWindow);
		if (ghost_hwnd != NULL)
		{
			GetCursorPos(&scr_save_mouse_pos);
			ghost_wnd_proc = (WNDPROC) GetWindowLongPtr(ghost_hwnd, GWLP_WNDPROC);
			SetWindowLongPtr(ghost_hwnd,GWLP_WNDPROC, (uintptr_t) screenSaverWindowProc);
		}
	}
	return ret;
}

#endif

bool GPG_Application::startWindow(STR_String& title,
	int windowLeft,
	int windowTop,
	int windowWidth,
	int windowHeight,
	const bool stereoVisual,
	const int stereoMode)
{
	bool success;
	// Create the main window
	//STR_String title ("Blender Player - GHOST");
	m_mainWindow = fSystem->createWindow(title, windowLeft, windowTop, windowWidth, windowHeight, GHOST_kWindowStateNormal,
		GHOST_kDrawingContextTypeOpenGL, stereoVisual);
	if (!m_mainWindow) {
		printf("error: could not create main window\n");
		exit(-1);
	}

	/* Check the size of the client rectangle of the window and resize the window
	 * so that the client rectangle has the size requested.
	 */
	m_mainWindow->setClientSize(windowWidth, windowHeight);
	m_mainWindow->setCursorVisibility(false);

	success = initEngine(m_mainWindow, stereoMode);
	if (success) {
		success = startEngine();
	}
	return success;
}

bool GPG_Application::startEmbeddedWindow(STR_String& title,
	const GHOST_TEmbedderWindowID parentWindow, 
	const bool stereoVisual, 
	const int stereoMode) {

	m_mainWindow = fSystem->createWindow(title, 0, 0, 0, 0, GHOST_kWindowStateNormal,
		GHOST_kDrawingContextTypeOpenGL, stereoVisual, parentWindow);

	if (!m_mainWindow) {
		printf("error: could not create main window\n");
		exit(-1);
	}
	m_isEmbedded = true;

	bool success = initEngine(m_mainWindow, stereoMode);
	if (success) {
		success = startEngine();
	}
	return success;
}


bool GPG_Application::startFullScreen(
		int width,
		int height,
		int bpp,int frequency,
		const bool stereoVisual,
		const int stereoMode)
{
	bool success;
	// Create the main window
	GHOST_DisplaySetting setting;
	setting.xPixels = width;
	setting.yPixels = height;
	setting.bpp = bpp;
	setting.frequency = frequency;

	fSystem->beginFullScreen(setting, &m_mainWindow, stereoVisual);
	m_mainWindow->setCursorVisibility(false);

	success = initEngine(m_mainWindow, stereoMode);
	if (success) {
		success = startEngine();
	}
	return success;
}




bool GPG_Application::StartGameEngine(int stereoMode)
{
	bool success = initEngine(m_mainWindow, stereoMode);
	
	if (success)
		success = startEngine();

	return success;
}



void GPG_Application::StopGameEngine()
{
	exitEngine();
}



bool GPG_Application::processEvent(GHOST_IEvent* event)
{
	bool handled = true;

	switch (event->getType())
	{
		case GHOST_kEventUnknown:
			break;

		case GHOST_kEventButtonDown:
			handled = handleButton(event, true);
			break;

		case GHOST_kEventButtonUp:
			handled = handleButton(event, false);
			break;
			
		case GHOST_kEventWheel:
			handled = handleWheel(event);
			break;

		case GHOST_kEventCursorMove:
			handled = handleCursorMove(event);
			break;

		case GHOST_kEventKeyDown:
			handleKey(event, true);
			break;

		case GHOST_kEventKeyUp:
			handleKey(event, false);
			break;


		case GHOST_kEventWindowClose:
			m_exitRequested = KX_EXIT_REQUEST_OUTSIDE;
			break;

		case GHOST_kEventWindowActivate:
			handled = false;
			break;
		case GHOST_kEventWindowDeactivate:
			handled = false;
			break;

		case GHOST_kEventWindowUpdate:
			{
				GHOST_IWindow* window = event->getWindow();
				if (!m_system->validWindow(window)) break;
				// Update the state of the game engine
				if (m_kxsystem && !m_exitRequested)
				{
					// Proceed to next frame
					window->activateDrawingContext();

					// first check if we want to exit
					m_exitRequested = m_ketsjiengine->GetExitCode();
					
					// kick the engine
					bool renderFrame = m_ketsjiengine->NextFrame();
					if (renderFrame)
					{
						// render the frame
						m_ketsjiengine->Render();
					}
				}
				m_exitString = m_ketsjiengine->GetExitString();
			}
			break;
		
		case GHOST_kEventWindowSize:
			{
			GHOST_IWindow* window = event->getWindow();
			if (!m_system->validWindow(window)) break;
			if (m_canvas) {
				GHOST_Rect bnds;
				window->getClientBounds(bnds);
				m_canvas->Resize(bnds.getWidth(), bnds.getHeight());
			}
			}
			break;
		
		default:
			handled = false;
			break;
	}
	return handled;
}



int GPG_Application::getExitRequested(void)
{
	return m_exitRequested;
}



const STR_String& GPG_Application::getExitString(void)
{
	return m_exitString;
}



bool GPG_Application::initEngine(GHOST_IWindow* window, const int stereoMode)
{
	if (!m_engineInitialized)
	{
		GPU_extensions_init();
		bgl::InitExtensions(true);

		// get and set the preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		if (!syshandle)
			return false;
		
		// SYS_WriteCommandLineInt(syshandle, "fixedtime", 0);
		// SYS_WriteCommandLineInt(syshandle, "vertexarrays",1);		
		GameData *gm= &m_startScene->gm;
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);
		bool fixedFr = (gm->flag & GAME_ENABLE_ALL_FRAMES);

		bool showPhysics = (gm->flag & GAME_SHOW_PHYSICS);
		SYS_WriteCommandLineInt(syshandle, "show_physics", showPhysics);

		bool fixed_framerate= (SYS_GetCommandLineInt(syshandle, "fixed_framerate", fixedFr) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		bool useLists = (SYS_GetCommandLineInt(syshandle, "displaylists", gm->flag & GAME_DISPLAY_LISTS) != 0);
		bool nodepwarnings = (SYS_GetCommandLineInt(syshandle, "ignore_deprecation_warnings", 1) != 0);

		if(GLEW_ARB_multitexture && GLEW_VERSION_1_1)
			m_blendermat = (SYS_GetCommandLineInt(syshandle, "blender_material", 1) != 0);

		if(GPU_glsl_support())
			m_blenderglslmat = (SYS_GetCommandLineInt(syshandle, "blender_glsl_material", 1) != 0);
		else if(gm->matmode == GAME_MAT_GLSL)
			m_blendermat = false;

		// create the canvas, rasterizer and rendertools
		m_canvas = new GPG_Canvas(window);
		if (!m_canvas)
			return false;
				
		m_canvas->Init();				
		m_rendertools = new GPC_RenderTools();
		if (!m_rendertools)
			goto initFailed;
		
		if(useLists) {
			if(GLEW_VERSION_1_1)
				m_rasterizer = new RAS_ListRasterizer(m_canvas, true);
			else
				m_rasterizer = new RAS_ListRasterizer(m_canvas);
		}
		else if (GLEW_VERSION_1_1)
			m_rasterizer = new RAS_VAOpenGLRasterizer(m_canvas);
		else
			m_rasterizer = new RAS_OpenGLRasterizer(m_canvas);

		/* Stereo parameters - Eye Separation from the UI - stereomode from the command-line/UI */
		m_rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) stereoMode);
		m_rasterizer->SetEyeSeparation(m_startScene->gm.eyeseparation);
		
		if (!m_rasterizer)
			goto initFailed;
						
		// create the inputdevices
		m_keyboard = new GPG_KeyboardDevice();
		if (!m_keyboard)
			goto initFailed;
			
		m_mouse = new GPC_MouseDevice();
		if (!m_mouse)
			goto initFailed;
			
		// create a networkdevice
		m_networkdevice = new NG_LoopBackNetworkDeviceInterface();
		if (!m_networkdevice)
			goto initFailed;
			
		sound_init(m_maggie);

		// create a ketsjisystem (only needed for timing and stuff)
		m_kxsystem = new GPG_System (m_system);
		if (!m_kxsystem)
			goto initFailed;
		
		// create the ketsjiengine
		m_ketsjiengine = new KX_KetsjiEngine(m_kxsystem);
		
		// set the devices
		m_ketsjiengine->SetKeyboardDevice(m_keyboard);
		m_ketsjiengine->SetMouseDevice(m_mouse);
		m_ketsjiengine->SetNetworkDevice(m_networkdevice);
		m_ketsjiengine->SetCanvas(m_canvas);
		m_ketsjiengine->SetRenderTools(m_rendertools);
		m_ketsjiengine->SetRasterizer(m_rasterizer);
		m_ketsjiengine->SetNetworkDevice(m_networkdevice);

		m_ketsjiengine->SetTimingDisplay(frameRate, false, false);

		CValue::SetDeprecationWarnings(nodepwarnings);

		m_ketsjiengine->SetUseFixedTime(fixed_framerate);
		m_ketsjiengine->SetTimingDisplay(frameRate, profile, properties);

		m_engineInitialized = true;
	}

	return m_engineInitialized;
initFailed:
	sound_exit();
	delete m_kxsystem;
	delete m_networkdevice;
	delete m_mouse;
	delete m_keyboard;
	delete m_rasterizer;
	delete m_rendertools;
	delete m_canvas;
	m_canvas = NULL;
	m_rendertools = NULL;
	m_rasterizer = NULL;
	m_keyboard = NULL;
	m_mouse = NULL;
	m_networkdevice = NULL;
	m_kxsystem = NULL;
	return false;
}



bool GPG_Application::startEngine(void)
{
	if (m_engineRunning) {
		return false;
	}
	
	// Temporary hack to disable banner display for NaN approved content.
	/*
	m_canvas->SetBannerDisplayEnabled(true);	
	Camera* cam;
	cam = (Camera*)scene->camera->data;
	if (cam) {
	if (((cam->flag) & 48)==48) {
	m_canvas->SetBannerDisplayEnabled(false);
	}
	}
	else {
	showError(CString("Camera data invalid."));
	return false;
	}
	*/
	
	// create a scene converter, create and convert the stratingscene
	m_sceneconverter = new KX_BlenderSceneConverter(m_maggie, m_ketsjiengine);
	if (m_sceneconverter)
	{
		STR_String startscenename = m_startSceneName.Ptr();
		m_ketsjiengine->SetSceneConverter(m_sceneconverter);

		//	if (always_use_expand_framing)
		//		sceneconverter->SetAlwaysUseExpandFraming(true);
		if(m_blendermat && (m_startScene->gm.matmode != GAME_MAT_TEXFACE))
			m_sceneconverter->SetMaterials(true);
		if(m_blenderglslmat && (m_startScene->gm.matmode == GAME_MAT_GLSL))
			m_sceneconverter->SetGLSLMaterials(true);

		KX_Scene* startscene = new KX_Scene(m_keyboard,
			m_mouse,
			m_networkdevice,
			startscenename,
			m_startScene);
		
#ifndef DISABLE_PYTHON
			// some python things
			PyObject *gameLogic, *gameLogic_keys;
			setupGamePython(m_ketsjiengine, startscene, m_maggie, NULL, &gameLogic, &gameLogic_keys, m_argc, m_argv);
#endif // DISABLE_PYTHON

		//initialize Dome Settings
		if(m_startScene->gm.stereoflag == STEREO_DOME)
			m_ketsjiengine->InitDome(m_startScene->gm.dome.res, m_startScene->gm.dome.mode, m_startScene->gm.dome.angle, m_startScene->gm.dome.resbuf, m_startScene->gm.dome.tilt, m_startScene->gm.dome.warptext);

		// Set the GameLogic.globalDict from marshal'd data, so we can
		// load new blend files and keep data in GameLogic.globalDict
		loadGamePythonConfig(m_pyGlobalDictString, m_pyGlobalDictString_Length);
		
		m_sceneconverter->ConvertScene(
			startscene,
			m_rendertools,
			m_canvas);
		m_ketsjiengine->AddScene(startscene);
		
		// Create a timer that is used to kick the engine
		if (!m_frameTimer) {
			m_frameTimer = m_system->installTimer(0, kTimerFreq, frameTimerProc, m_mainWindow);
		}
		m_rasterizer->Init();
		m_ketsjiengine->StartEngine(true);
		m_engineRunning = true;
		
		// Set the animation playback rate for ipo's and actions
		// the framerate below should patch with FPS macro defined in blendef.h
		// Could be in StartEngine set the framerate, we need the scene to do this
// XXX		m_ketsjiengine->SetAnimFrameRate( (((double) scene->r.frs_sec) / scene->r.frs_sec_base) );
		
	}
	
	if (!m_engineRunning)
	{
		stopEngine();
	}
	
	return m_engineRunning;
}


void GPG_Application::stopEngine()
{
	// GameLogic.globalDict gets converted into a buffer, and sorted in
	// m_pyGlobalDictString so we can restore after python has stopped
	// and started between .blend file loads.
	if(m_pyGlobalDictString) {
		delete [] m_pyGlobalDictString;
		m_pyGlobalDictString = 0;
	}

	m_pyGlobalDictString_Length = saveGamePythonConfig(&m_pyGlobalDictString);
	
	// when exiting the mainloop
	exitGamePythonScripting();
	m_ketsjiengine->StopEngine();
	m_networkdevice->Disconnect();

	if (m_sceneconverter) {
		delete m_sceneconverter;
		m_sceneconverter = 0;
	}
	if (m_system && m_frameTimer) {
		m_system->removeTimer(m_frameTimer);
		m_frameTimer = 0;
	}
	m_engineRunning = false;
}


void GPG_Application::exitEngine()
{
	sound_exit();
	if (m_ketsjiengine)
	{
		stopEngine();
		delete m_ketsjiengine;
		m_ketsjiengine = 0;
	}
	if (m_kxsystem)
	{
		delete m_kxsystem;
		m_kxsystem = 0;
	}
	if (m_networkdevice)
	{
		delete m_networkdevice;
		m_networkdevice = 0;
	}
	if (m_mouse)
	{
		delete m_mouse;
		m_mouse = 0;
	}
	if (m_keyboard)
	{
		delete m_keyboard;
		m_keyboard = 0;
	}
	if (m_rasterizer)
	{
		delete m_rasterizer;
		m_rasterizer = 0;
	}
	if (m_rendertools)
	{
		delete m_rendertools;
		m_rendertools = 0;
	}
	if (m_canvas)
	{
		delete m_canvas;
		m_canvas = 0;
	}

	libtiff_exit();
#ifdef WITH_QUICKTIME
	quicktime_exit();
#endif
	GPU_extensions_exit();

	m_exitRequested = 0;
	m_engineInitialized = false;
}

bool GPG_Application::handleWheel(GHOST_IEvent* event)
{
	bool handled = false;
	MT_assert(event);
	if (m_mouse) 
	{
		GHOST_TEventDataPtr eventData = ((GHOST_IEvent*)event)->getData();
		GHOST_TEventWheelData* wheelData = static_cast<GHOST_TEventWheelData*>(eventData);
		GPC_MouseDevice::TButtonId button;
		if (wheelData->z > 0)
			button = GPC_MouseDevice::buttonWheelUp;
		else
			button = GPC_MouseDevice::buttonWheelDown;
		m_mouse->ConvertButtonEvent(button, true);
		handled = true;
	}
	return handled;
}

bool GPG_Application::handleButton(GHOST_IEvent* event, bool isDown)
{
	bool handled = false;
	MT_assert(event);
	if (m_mouse) 
	{
		GHOST_TEventDataPtr eventData = ((GHOST_IEvent*)event)->getData();
		GHOST_TEventButtonData* buttonData = static_cast<GHOST_TEventButtonData*>(eventData);
		GPC_MouseDevice::TButtonId button;
		switch (buttonData->button)
		{
		case GHOST_kButtonMaskMiddle:
			button = GPC_MouseDevice::buttonMiddle;
			break;
		case GHOST_kButtonMaskRight:
			button = GPC_MouseDevice::buttonRight;
			break;
		case GHOST_kButtonMaskLeft:
		default:
			button = GPC_MouseDevice::buttonLeft;
			break;
		}
		m_mouse->ConvertButtonEvent(button, isDown);
		handled = true;
	}
	return handled;
}


bool GPG_Application::handleCursorMove(GHOST_IEvent* event)
{
	bool handled = false;
	MT_assert(event);
	if (m_mouse && m_mainWindow)
	{
		GHOST_TEventDataPtr eventData = ((GHOST_IEvent*)event)->getData();
		GHOST_TEventCursorData* cursorData = static_cast<GHOST_TEventCursorData*>(eventData);
		GHOST_TInt32 x, y;
		m_mainWindow->screenToClient(cursorData->x, cursorData->y, x, y);
		m_mouse->ConvertMoveEvent(x, y);
		handled = true;
	}
	return handled;
}


bool GPG_Application::handleKey(GHOST_IEvent* event, bool isDown)
{
	bool handled = false;
	MT_assert(event);
	if (m_keyboard)
	{
		GHOST_TEventDataPtr eventData = ((GHOST_IEvent*)event)->getData();
		GHOST_TEventKeyData* keyData = static_cast<GHOST_TEventKeyData*>(eventData);
		//no need for this test
		//if (fSystem->getFullScreen()) {
			if (keyData->key == GHOST_kKeyEsc && !m_keyboard->m_hookesc && !m_isEmbedded) {
				m_exitRequested = KX_EXIT_REQUEST_OUTSIDE;
			}
		//}
		m_keyboard->ConvertEvent(keyData->key, isDown);
		handled = true;
	}
	return handled;
}



static void frameTimerProc(GHOST_ITimerTask* task, GHOST_TUns64 time)
{
	GHOST_IWindow* window = (GHOST_IWindow*)task->getUserData();
	if (fSystem->validWindow(window)) {
		window->invalidate();
	}
}
