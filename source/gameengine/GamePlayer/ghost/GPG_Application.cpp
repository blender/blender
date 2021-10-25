/*
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

/** \file gameengine/GamePlayer/ghost/GPG_Application.cpp
 *  \ingroup player
 */


#ifdef WIN32
#  pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#  include <windows.h>
#endif

#include "GPU_glew.h"
#include "GPU_extensions.h"
#include "GPU_init_exit.h"

#include "GPG_Application.h"
#include "BL_BlenderDataConversion.h"

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


#include "BL_System.h"
#include "KX_KetsjiEngine.h"

// include files needed by "KX_BlenderSceneConverter.h"
#include "CTR_Map.h"
#include "SCA_IActuator.h"
#include "RAS_MeshObject.h"
#include "RAS_OpenGLRasterizer.h"
#include "RAS_ListRasterizer.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"
#include "BL_Material.h" // MAXTEX

#include "KX_BlenderSceneConverter.h"
#include "NG_LoopBackNetworkDeviceInterface.h"

#include "GPC_MouseDevice.h"
#include "GPG_Canvas.h" 
#include "GPG_KeyboardDevice.h"
#include "GPG_System.h"

#include "STR_String.h"

#include "GHOST_ISystem.h"
#include "GHOST_IEvent.h"
#include "GHOST_IEventConsumer.h"
#include "GHOST_IWindow.h"
#include "GHOST_Rect.h"

#ifdef WITH_AUDASPACE
#  include AUD_DEVICE_H
#endif

static void frameTimerProc(GHOST_ITimerTask* task, GHOST_TUns64 time);

static GHOST_ISystem* fSystem = 0;
static const int kTimerFreq = 10;

GPG_Application::GPG_Application(GHOST_ISystem* system)
	: m_startSceneName(""), 
	  m_startScene(0),
	  m_maggie(0),
	  m_kxStartScene(NULL),
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
	if (m_pyGlobalDictString) {
		delete [] m_pyGlobalDictString;
		m_pyGlobalDictString = 0;
		m_pyGlobalDictString_Length = 0;
	}

	exitEngine();
	fSystem->disposeWindow(m_mainWindow);
}



bool GPG_Application::SetGameEngineData(struct Main* maggie, Scene *scene, GlobalSettings *gs, int argc, char **argv)
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

	/* Global Settings */
	m_globalSettings= gs;

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
	BOOL close = false;
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
				close = true;
			}
			scr_save_mouse_pos = pt;
			break;
		}
		case WM_LBUTTONDOWN: 
		case WM_MBUTTONDOWN: 
		case WM_RBUTTONDOWN: 
		case WM_KEYDOWN:
			close = true;
	}
	if (close)
		PostMessage(hwnd,WM_CLOSE,0,0);
	return CallWindowProc(ghost_wnd_proc, hwnd, uMsg, wParam, lParam);
}

BOOL CALLBACK findGhostWindowHWNDProc(HWND hwnd, LPARAM lParam)
{
	GHOST_IWindow *p = (GHOST_IWindow*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	BOOL ret = true;
	if (p == ghost_window_to_find)
	{
		found_ghost_window_hwnd = hwnd;
		ret = false;
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
	const int stereoMode,
	const GHOST_TUns16 samples)
{
	bool success = false;

	RECT rc;
	if (GetWindowRect(parentWindow, &rc))
	{
		int windowWidth = rc.right - rc.left;
		int windowHeight = rc.bottom - rc.top;
		STR_String title = "";
		GHOST_GLSettings glSettings = {0};

		if (stereoVisual) {
			glSettings.flags |= GHOST_glStereoVisual;
		}
		glSettings.numOfAASamples = samples;

		m_mainWindow = fSystem->createWindow(title, 0, 0, windowWidth, windowHeight, GHOST_kWindowStateMinimized,
		                                     GHOST_kDrawingContextTypeOpenGL, glSettings);
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
		LONG_PTR style = GetWindowLongPtr(ghost_hwnd, GWL_STYLE);
		LONG_PTR exstyle = GetWindowLongPtr(ghost_hwnd, GWL_EXSTYLE);

		RECT adjrc = { 0, 0, windowWidth, windowHeight };
		AdjustWindowRectEx(&adjrc, style, false, exstyle);

		style = (style & (~(WS_POPUP|WS_OVERLAPPEDWINDOW|WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_TILEDWINDOW ))) | WS_CHILD;
		SetWindowLongPtr(ghost_hwnd, GWL_STYLE, style);
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
		const int stereoMode,
		const GHOST_TUns16 samples)
{
	bool ret = startFullScreen(width, height, bpp, frequency, stereoVisual, stereoMode, 0, samples);
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

bool GPG_Application::startWindow(
        STR_String& title,
        int windowLeft,
        int windowTop,
        int windowWidth,
        int windowHeight,
        const bool stereoVisual,
        const int stereoMode,
		const int alphaBackground,
        const GHOST_TUns16 samples)
{
	GHOST_GLSettings glSettings = {0};
	bool success;
	// Create the main window
	//STR_String title ("Blender Player - GHOST");
	if (stereoVisual)
		glSettings.flags |= GHOST_glStereoVisual;
	if (alphaBackground)
		glSettings.flags |= GHOST_glAlphaBackground;
	glSettings.numOfAASamples = samples;

	m_mainWindow = fSystem->createWindow(title, windowLeft, windowTop, windowWidth, windowHeight, GHOST_kWindowStateNormal,
	                                     GHOST_kDrawingContextTypeOpenGL, glSettings);
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

bool GPG_Application::startEmbeddedWindow(
        STR_String& title,
        const GHOST_TEmbedderWindowID parentWindow,
        const bool stereoVisual,
        const int stereoMode,
		const int alphaBackground,
        const GHOST_TUns16 samples)
{
	GHOST_TWindowState state = GHOST_kWindowStateNormal;
	GHOST_GLSettings glSettings = {0};

	if (stereoVisual)
		glSettings.flags |= GHOST_glStereoVisual;
	if (alphaBackground)
		glSettings.flags |= GHOST_glAlphaBackground;
	glSettings.numOfAASamples = samples;

	if (parentWindow != 0)
		state = GHOST_kWindowStateEmbedded;
	m_mainWindow = fSystem->createWindow(title, 0, 0, 0, 0, state,
	                                     GHOST_kDrawingContextTypeOpenGL, glSettings, parentWindow);

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
        const int stereoMode,
        const int alphaBackground,
        const GHOST_TUns16 samples,
        bool useDesktop)
{
	bool success;
	GHOST_TUns32 sysWidth=0, sysHeight=0;
	fSystem->getMainDisplayDimensions(sysWidth, sysHeight);
	// Create the main window
	GHOST_DisplaySetting setting;
	setting.xPixels = (useDesktop) ? sysWidth : width;
	setting.yPixels = (useDesktop) ? sysHeight : height;
	setting.bpp = bpp;
	setting.frequency = frequency;

	fSystem->beginFullScreen(setting, &m_mainWindow, stereoVisual, alphaBackground, samples);
	m_mainWindow->setCursorVisibility(false);
	/* note that X11 ignores this (it uses a window internally for fullscreen) */
	m_mainWindow->setState(GHOST_kWindowStateFullScreen);

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
		case GHOST_kEventQuit:
			m_exitRequested = KX_EXIT_REQUEST_OUTSIDE;
			break;

		case GHOST_kEventWindowActivate:
			handled = false;
			break;
		case GHOST_kEventWindowDeactivate:
			handled = false;
			break;

		// The player now runs as often as it can (repsecting vsync and fixedtime).
		// This allows the player to break 100fps, but this code is being left here
		// as reference. (see EngineNextFrame)
		//case GHOST_kEventWindowUpdate:
		//	{
		//		GHOST_IWindow* window = event->getWindow();
		//		if (!m_system->validWindow(window)) break;
		//		// Update the state of the game engine
		//		if (m_kxsystem && !m_exitRequested)
		//		{
		//			// Proceed to next frame
		//			window->activateDrawingContext();

		//			// first check if we want to exit
		//			m_exitRequested = m_ketsjiengine->GetExitCode();
		//
		//			// kick the engine
		//			bool renderFrame = m_ketsjiengine->NextFrame();
		//			if (renderFrame)
		//			{
		//				// render the frame
		//				m_ketsjiengine->Render();
		//			}
		//		}
		//		m_exitString = m_ketsjiengine->GetExitString();
		//	}
		//	break;
		//
		case GHOST_kEventWindowSize:
			{
			GHOST_IWindow* window = event->getWindow();
			if (!m_system->validWindow(window)) break;
			if (m_canvas) {
				GHOST_Rect bnds;
				window->getClientBounds(bnds);
				m_canvas->Resize(bnds.getWidth(), bnds.getHeight());
				m_ketsjiengine->Resize();
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


GlobalSettings* GPG_Application::getGlobalSettings(void)
{
	return m_ketsjiengine->GetGlobalSettings();
}



const STR_String& GPG_Application::getExitString(void)
{
	return m_exitString;
}



bool GPG_Application::initEngine(GHOST_IWindow* window, const int stereoMode)
{
	if (!m_engineInitialized)
	{
		GPU_init();

		// get and set the preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		if (!syshandle)
			return false;
		
		// SYS_WriteCommandLineInt(syshandle, "fixedtime", 0);
		// SYS_WriteCommandLineInt(syshandle, "vertexarrays",1);
		GameData *gm= &m_startScene->gm;
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);

		bool showPhysics = (gm->flag & GAME_SHOW_PHYSICS);
		SYS_WriteCommandLineInt(syshandle, "show_physics", showPhysics);

		bool fixed_framerate= (SYS_GetCommandLineInt(syshandle, "fixedtime", (gm->flag & GAME_ENABLE_ALL_FRAMES)) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		bool useLists = (SYS_GetCommandLineInt(syshandle, "displaylists", gm->flag & GAME_DISPLAY_LISTS) != 0) && GPU_display_list_support();
		bool nodepwarnings = (SYS_GetCommandLineInt(syshandle, "ignore_deprecation_warnings", 1) != 0);
		bool restrictAnimFPS = (gm->flag & GAME_RESTRICT_ANIM_UPDATES) != 0;

		m_blendermat = (SYS_GetCommandLineInt(syshandle, "blender_material", 1) != 0);
		m_blenderglslmat = (SYS_GetCommandLineInt(syshandle, "blender_glsl_material", 1) != 0);

		// create the canvas, rasterizer and rendertools
		m_canvas = new GPG_Canvas(window);
		if (!m_canvas)
			return false;

		if (gm->vsync == VSYNC_ADAPTIVE)
			m_canvas->SetSwapInterval(-1);
		else
			m_canvas->SetSwapInterval((gm->vsync == VSYNC_ON) ? 1 : 0);

		m_canvas->Init();
		if (gm->flag & GAME_SHOW_MOUSE)
			m_canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		
		RAS_STORAGE_TYPE raster_storage = RAS_AUTO_STORAGE;

		if (gm->raster_storage == RAS_STORE_VBO) {
			raster_storage = RAS_VBO;
		}
		else if (gm->raster_storage == RAS_STORE_VA) {
			raster_storage = RAS_VA;
		}
		//Don't use displaylists with VBOs
		//If auto starts using VBOs, make sure to check for that here
		if (useLists && raster_storage != RAS_VBO)
			m_rasterizer = new RAS_ListRasterizer(m_canvas, true, raster_storage);
		else
			m_rasterizer = new RAS_OpenGLRasterizer(m_canvas, raster_storage);

		/* Stereo parameters - Eye Separation from the UI - stereomode from the command-line/UI */
		m_rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) stereoMode);
		m_rasterizer->SetEyeSeparation(m_startScene->gm.eyeseparation);
		
		if (!m_rasterizer)
			goto initFailed;

		m_rasterizer->PrintHardwareInfo();

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
			
		BKE_sound_init(m_maggie);

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
		m_ketsjiengine->SetRasterizer(m_rasterizer);

		KX_KetsjiEngine::SetExitKey(ConvertKeyCode(gm->exitkey));
#ifdef WITH_PYTHON
		CValue::SetDeprecationWarnings(nodepwarnings);
#else
		(void)nodepwarnings;
#endif

		m_ketsjiengine->SetUseFixedTime(fixed_framerate);
		m_ketsjiengine->SetTimingDisplay(frameRate, profile, properties);
		m_ketsjiengine->SetRestrictAnimationFPS(restrictAnimFPS);

		//set the global settings (carried over if restart/load new files)
		m_ketsjiengine->SetGlobalSettings(m_globalSettings);
		m_ketsjiengine->SetRender(true);

		m_engineInitialized = true;
	}

	return m_engineInitialized;
initFailed:
	BKE_sound_exit();
	delete m_kxsystem;
	delete m_networkdevice;
	delete m_mouse;
	delete m_keyboard;
	delete m_rasterizer;
	delete m_canvas;
	m_canvas = NULL;
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
		STR_String m_kxStartScenename = m_startSceneName.Ptr();
		m_ketsjiengine->SetSceneConverter(m_sceneconverter);

		//	if (always_use_expand_framing)
		//		sceneconverter->SetAlwaysUseExpandFraming(true);
		if (m_blendermat)
			m_sceneconverter->SetMaterials(true);
		if (m_blenderglslmat && (m_globalSettings->matmode == GAME_MAT_GLSL))
			m_sceneconverter->SetGLSLMaterials(true);
		if (m_startScene->gm.flag & GAME_NO_MATERIAL_CACHING)
			m_sceneconverter->SetCacheMaterials(false);

		m_kxStartScene = new KX_Scene(m_keyboard,
			m_mouse,
			m_networkdevice,
			m_kxStartScenename,
			m_startScene,
			m_canvas);
		
#ifdef WITH_PYTHON
			// some python things
			PyObject *gameLogic, *gameLogic_keys;
			setupGamePython(m_ketsjiengine, m_kxStartScene, m_maggie, NULL, &gameLogic, &gameLogic_keys, m_argc, m_argv);
#endif // WITH_PYTHON

		//initialize Dome Settings
		if (m_startScene->gm.stereoflag == STEREO_DOME)
			m_ketsjiengine->InitDome(m_startScene->gm.dome.res, m_startScene->gm.dome.mode, m_startScene->gm.dome.angle, m_startScene->gm.dome.resbuf, m_startScene->gm.dome.tilt, m_startScene->gm.dome.warptext);

		// initialize 3D Audio Settings
		AUD_Device* device = BKE_sound_get_device();
		AUD_Device_setSpeedOfSound(device, m_startScene->audio.speed_of_sound);
		AUD_Device_setDopplerFactor(device, m_startScene->audio.doppler_factor);
		AUD_Device_setDistanceModel(device, AUD_DistanceModel(m_startScene->audio.distance_model));

#ifdef WITH_PYTHON
		// Set the GameLogic.globalDict from marshal'd data, so we can
		// load new blend files and keep data in GameLogic.globalDict
		loadGamePythonConfig(m_pyGlobalDictString, m_pyGlobalDictString_Length);
#endif
		m_sceneconverter->ConvertScene(
			m_kxStartScene,
			m_rasterizer,
			m_canvas);
		m_ketsjiengine->AddScene(m_kxStartScene);
		
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
		Scene *scene= m_kxStartScene->GetBlenderScene(); // needed for macro
		m_ketsjiengine->SetAnimFrameRate(FPS);
	}
	
	if (!m_engineRunning)
	{
		stopEngine();
	}
	
	return m_engineRunning;
}


void GPG_Application::stopEngine()
{
#ifdef WITH_PYTHON
	// GameLogic.globalDict gets converted into a buffer, and sorted in
	// m_pyGlobalDictString so we can restore after python has stopped
	// and started between .blend file loads.
	if (m_pyGlobalDictString) {
		delete [] m_pyGlobalDictString;
		m_pyGlobalDictString = 0;
	}

	m_pyGlobalDictString_Length = saveGamePythonConfig(&m_pyGlobalDictString);
#endif
	
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

void GPG_Application::EngineNextFrame()
{
	// Update the state of the game engine
	if (m_kxsystem && !m_exitRequested)
	{
		// Proceed to next frame
		if (m_mainWindow)
			m_mainWindow->activateDrawingContext();

		// first check if we want to exit
		m_exitRequested = m_ketsjiengine->GetExitCode();
		
		// kick the engine
		bool renderFrame = m_ketsjiengine->NextFrame();
		if (renderFrame && m_mainWindow)
		{
			// render the frame
			m_ketsjiengine->Render();
		}
	}
	m_exitString = m_ketsjiengine->GetExitString();
}

void GPG_Application::exitEngine()
{
	// We only want to kill the engine if it has been initialized
	if (!m_engineInitialized)
		return;

	BKE_sound_exit();
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
	if (m_canvas)
	{
		delete m_canvas;
		m_canvas = 0;
	}

	GPU_exit();

#ifdef WITH_PYTHON
	// Call this after we're sure nothing needs Python anymore (e.g., destructors)
	exitGamePlayerPythonScripting();
#endif

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
		unsigned int unicode = keyData->utf8_buf[0] ? BLI_str_utf8_as_unicode(keyData->utf8_buf) : keyData->ascii;

		if (m_keyboard->ToNative(keyData->key) == KX_KetsjiEngine::GetExitKey() && !m_keyboard->m_hookesc && !m_isEmbedded) {
			m_exitRequested = KX_EXIT_REQUEST_OUTSIDE;
		}
		m_keyboard->ConvertEvent(keyData->key, isDown, unicode);
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
