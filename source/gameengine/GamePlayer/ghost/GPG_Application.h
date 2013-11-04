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
 */

/** \file GPG_Application.h
 *  \ingroup player
 *  \brief GHOST Blender Player application declaration file.
 */

#include "GHOST_IEventConsumer.h"
#include "STR_String.h"

#ifdef WIN32
#include <wtypes.h>
#endif

#include "KX_KetsjiEngine.h"

class KX_KetsjiEngine;
class KX_ISceneConverter;
class NG_LoopBackNetworkDeviceInterface;
class RAS_IRasterizer;
class GHOST_IEvent;
class GHOST_ISystem;
class GHOST_ITimerTask;
class GHOST_IWindow;
class GPC_MouseDevice;
class GPG_Canvas;
class GPG_KeyboardDevice;
class GPG_System;
struct Main;
struct Scene;

class GPG_Application : public GHOST_IEventConsumer
{
public:
	GPG_Application(GHOST_ISystem* system);
	~GPG_Application(void);

	bool SetGameEngineData(struct Main* maggie, struct Scene* scene, GlobalSettings* gs, int argc, char** argv);
	bool startWindow(STR_String& title,
	                 int windowLeft, int windowTop,
	                 int windowWidth, int windowHeight,
	                 const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
	bool startFullScreen(int width, int height,
	                     int bpp, int frequency,
	                     const bool stereoVisual, const int stereoMode,
	                     const GHOST_TUns16 samples=0, bool useDesktop=false);
	bool startEmbeddedWindow(STR_String& title, const GHOST_TEmbedderWindowID parent_window,
	                         const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
#ifdef WIN32
	bool startScreenSaverFullScreen(int width, int height,
	                                int bpp, int frequency,
	                                const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
	bool startScreenSaverPreview(HWND parentWindow,
	                             const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
#endif

	virtual	bool processEvent(GHOST_IEvent* event);
	int getExitRequested(void);
	const STR_String& getExitString(void);
	GlobalSettings* getGlobalSettings(void);
	bool StartGameEngine(int stereoMode);
	void StopGameEngine();
	void EngineNextFrame();

protected:
	bool	handleWheel(GHOST_IEvent* event);
	bool	handleButton(GHOST_IEvent* event, bool isDown);
	bool	handleCursorMove(GHOST_IEvent* event);
	bool	handleKey(GHOST_IEvent* event, bool isDown);

	/**
	 * Initializes the game engine.
	 */
	bool initEngine(GHOST_IWindow* window, int stereoMode);

	/**
	 * Starts the game engine.
	 */
	bool startEngine(void);

	/**
	 * Stop the game engine.
	 */
	void stopEngine(void);

	/**
	 * Shuts the game engine down.
	 */
	void exitEngine(void);
	short					m_exitkey;

	/* The game data */
	STR_String				m_startSceneName;
	struct Scene*			m_startScene;
	struct Main*			m_maggie;

	/* Exit state. */
	int						m_exitRequested;
	STR_String				m_exitString;
	GlobalSettings*	m_globalSettings;

	/* GHOST system abstraction. */
	GHOST_ISystem*			m_system;
	/* Main window. */
	GHOST_IWindow*			m_mainWindow;
	/* Timer to advance frames. */
	GHOST_ITimerTask*		m_frameTimer;
	/* The cursor shape displayed. */
	GHOST_TStandardCursor	m_cursor;

	/** Engine construction state. */
	bool m_engineInitialized;
	/** Engine state. */
	bool m_engineRunning;
	/** Running on embedded window */
	bool m_isEmbedded;

	/** the gameengine itself */
	KX_KetsjiEngine* m_ketsjiengine;
	/** The game engine's system abstraction. */
	GPG_System* m_kxsystem;
	/** The game engine's keyboard abstraction. */
	GPG_KeyboardDevice* m_keyboard;
	/** The game engine's mouse abstraction. */
	GPC_MouseDevice* m_mouse;
	/** The game engine's canvas abstraction. */
	GPG_Canvas* m_canvas;
	/** the rasterizer */
	RAS_IRasterizer* m_rasterizer;
	/** Converts Blender data files. */
	KX_ISceneConverter* m_sceneconverter;
	/** Network interface. */
	NG_LoopBackNetworkDeviceInterface* m_networkdevice;

	bool m_blendermat;
	bool m_blenderglslmat;
	
	/*
	 * GameLogic.globalDict as a string so that loading new blend files can use the same dict.
	 * Do this because python starts/stops when loading blend files.
	 */
	char* m_pyGlobalDictString;
	int m_pyGlobalDictString_Length;
	
	/* argc and argv need to be passed on to python */
	int		m_argc;
	char**	m_argv;
};
