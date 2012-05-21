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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemSDL.h
 *  \ingroup GHOST
 * Declaration of GHOST_SystemSDL class.
 */

#ifndef __GHOST_SYSTEMSDL_H__
#define __GHOST_SYSTEMSDL_H__

#include "GHOST_System.h"
#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerSDL.h"
#include "GHOST_TimerManager.h"
#include "GHOST_WindowSDL.h"
#include "GHOST_Event.h"

extern "C" {
	#include "SDL.h"
}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#  error "SDL 2.0 or newer is needed to build with Ghost"
#endif


class GHOST_WindowSDL;


class GHOST_SystemSDL : public GHOST_System {
public:

	void addDirtyWindow(GHOST_WindowSDL *bad_wind);

	GHOST_SystemSDL();
	~GHOST_SystemSDL();

	bool
	processEvents(bool waitForEvent);

	int
	toggleConsole(int action) { return 0; }

	GHOST_TSuccess
	getModifierKeys(GHOST_ModifierKeys& keys) const;

	GHOST_TSuccess
	getButtons(GHOST_Buttons& buttons) const;

	GHOST_TUns8 *
	getClipboard(bool selection) const;

	void
	putClipboard(GHOST_TInt8 *buffer, bool selection) const;

	GHOST_TUns64
	getMilliSeconds();

	GHOST_TUns8
	getNumDisplays() const;

	GHOST_TSuccess
	getCursorPosition(GHOST_TInt32& x,
	                  GHOST_TInt32& y) const;

	GHOST_TSuccess
	setCursorPosition(GHOST_TInt32 x,
	                  GHOST_TInt32 y);

	void
	getMainDisplayDimensions(GHOST_TUns32& width,
	                         GHOST_TUns32& height) const;

private:

	GHOST_TSuccess
	init();

	GHOST_IWindow *
	createWindow(const STR_String& title,
	             GHOST_TInt32 left,
	             GHOST_TInt32 top,
	             GHOST_TUns32 width,
	             GHOST_TUns32 height,
	             GHOST_TWindowState state,
	             GHOST_TDrawingContextType type,
	             bool stereoVisual,
	             const GHOST_TUns16 numOfAASamples,
	             const GHOST_TEmbedderWindowID parentWindow
	             );

	/* SDL specific */
	GHOST_WindowSDL *findGhostWindow(SDL_Window *sdl_win);

	bool
	generateWindowExposeEvents();

	void
	processEvent(SDL_Event *sdl_event);

	/// The vector of windows that need to be updated.
	std::vector<GHOST_WindowSDL *> m_dirty_windows;
};

#endif
