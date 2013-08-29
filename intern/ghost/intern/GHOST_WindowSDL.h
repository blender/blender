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

/** \file ghost/intern/GHOST_WindowSDL.h
 *  \ingroup GHOST
 * Declaration of GHOST_WindowSDL class.
 */

#ifndef __GHOST_WINDOWSDL_H__
#define __GHOST_WINDOWSDL_H__

#include "GHOST_Window.h"
#include "GHOST_SystemSDL.h"
#include <map>

extern "C" {
	#include "SDL.h"
}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#  error "SDL 2.0 or newer is needed to build with Ghost"
#endif

class STR_String;
class GHOST_SystemSDL;

class GHOST_WindowSDL : public GHOST_Window
{
private:
	GHOST_SystemSDL  *m_system;
	bool m_invalid_window;

	SDL_Window       *m_sdl_win;
	SDL_GLContext     m_sdl_glcontext;
	SDL_Cursor       *m_sdl_custom_cursor;

public:

	const GHOST_TabletData *GetTabletData() {
		return NULL;
	}

	GHOST_WindowSDL(GHOST_SystemSDL *system,
	                const STR_String& title,
	                GHOST_TInt32 left, GHOST_TInt32 top,
	                GHOST_TUns32 width, GHOST_TUns32 height,
	                GHOST_TWindowState state,
	                const GHOST_TEmbedderWindowID parentWindow,
	                GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
	                const bool stereoVisual = false,
	                const bool exclusive = false,
	                const GHOST_TUns16 numOfAASamples = 0
	                );

	~GHOST_WindowSDL();

	/* SDL specific */
	SDL_Window *
	getSDLWindow()
	{
		return m_sdl_win;
	}


	GHOST_TSuccess invalidate(void);

	/**
	 * called by the X11 system implementation when expose events
	 * for the window have been pushed onto the GHOST queue
	 */

	void validate()
	{
		m_invalid_window = false;
	}

	bool getValid() const
	{
		return (m_sdl_win != NULL);
	}

	void getWindowBounds(GHOST_Rect& bounds) const;
	void getClientBounds(GHOST_Rect& bounds) const;

protected:
	GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType type);
	GHOST_TSuccess removeDrawingContext();

	GHOST_TSuccess
	setWindowCursorGrab(GHOST_TGrabCursorMode mode);

	GHOST_TSuccess
	setWindowCursorShape(GHOST_TStandardCursor shape);

	GHOST_TSuccess
	setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
	                           GHOST_TUns8 mask[16][2],
	                           int hotX, int hotY);

	GHOST_TSuccess
	setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
	                           GHOST_TUns8 *mask,
	                           int sizex, int sizey,
	                           int hotX, int hotY,
	                           int fg_color, int bg_color);

	GHOST_TSuccess
	setWindowCursorVisibility(bool visible);

	void
	setTitle(const STR_String& title);

	void
	getTitle(STR_String& title) const;

	GHOST_TSuccess
	setClientWidth(GHOST_TUns32 width);

	GHOST_TSuccess
	setClientHeight(GHOST_TUns32 height);

	GHOST_TSuccess
	setClientSize(GHOST_TUns32 width,
	              GHOST_TUns32 height);

	void
	screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY,
	               GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	void
	clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY,
	               GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	GHOST_TSuccess
	swapBuffers();

	GHOST_TSuccess
	activateDrawingContext();

	GHOST_TSuccess
	setState(GHOST_TWindowState state);

	GHOST_TWindowState
	getState() const;

	GHOST_TSuccess setOrder(GHOST_TWindowOrder order)
	{
		// TODO
		return GHOST_kSuccess;
	}

	// TODO
	GHOST_TSuccess beginFullScreen() const { return GHOST_kFailure; }

	GHOST_TSuccess endFullScreen() const { return GHOST_kFailure; }

	GHOST_TSuccess setSwapInterval(int interval);
	int getSwapInterval();
};


#endif // __GHOST_WINDOWSDL_H__
