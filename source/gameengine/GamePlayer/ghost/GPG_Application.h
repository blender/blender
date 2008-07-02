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
 * GHOST Blender Player application declaration file.
 */

#include "GHOST_IEventConsumer.h"
#include "STR_String.h"

#ifdef WIN32
#include <wtypes.h>
#endif

class KX_KetsjiEngine;
class KX_ISceneConverter;
class NG_LoopBackNetworkDeviceInterface;
class SND_IAudioDevice;
class RAS_IRasterizer;
class GHOST_IEvent;
class GHOST_ISystem;
class GHOST_ITimerTask;
class GHOST_IWindow;
class GPC_MouseDevice;
class GPC_RenderTools;
class GPG_Canvas;
class GPG_KeyboardDevice;
class GPG_System;
struct Main;

class GPG_Application : public GHOST_IEventConsumer
{
public:
	GPG_Application(GHOST_ISystem* system, struct Main* maggie, STR_String startSceneName);
	~GPG_Application(void);

			bool SetGameEngineData(struct Main* maggie,STR_String startSceneName);
			bool startWindow(STR_String& title, int windowLeft, int windowTop, int windowWidth, int windowHeight,
			const bool stereoVisual, const int stereoMode);
			bool startFullScreen(int width, int height, int bpp, int frequency, const bool stereoVisual, const int stereoMode);
#ifdef WIN32
			bool startScreenSaverFullScreen(int width, int height, int bpp, int frequency, const bool stereoVisual, const int stereoMode);
			bool startScreenSaverPreview(HWND parentWindow,	const bool stereoVisual, const int stereoMode);
#endif

	virtual	bool processEvent(GHOST_IEvent* event);
			int getExitRequested(void);
			const STR_String& getExitString(void);
			bool StartGameEngine(int stereoMode);
			void StopGameEngine();

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

	/* The game data */
	STR_String				m_startSceneName;
	struct Main*			m_maggie;

	/* Exit state. */
	int						m_exitRequested;
	STR_String				m_exitString;
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
	/** The game engine's platform dependent render tools. */
	GPC_RenderTools* m_rendertools;
	/** the rasterizer */
	RAS_IRasterizer* m_rasterizer;
	/** Converts Blender data files. */
	KX_ISceneConverter* m_sceneconverter;
	/** Network interface. */
	NG_LoopBackNetworkDeviceInterface* m_networkdevice;
	/** Sound device. */
	SND_IAudioDevice* m_audiodevice;

	bool m_blendermat;
	bool m_blenderglslmat;

};

