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
/**
 * @file	GHOST_WindowManager.h
 * Declaration of GHOST_WindowManager class.
 */

#ifndef _GHOST_WINDOW_MANAGER_H_
#define _GHOST_WINDOW_MANAGER_H_

#include <vector>

#include "GHOST_Rect.h"
#include "GHOST_IWindow.h"


/**
 * Manages system windows (platform independent implementation).
 * @author	Maarten Gribnau
 * @date	May 11, 2001
 */
class GHOST_WindowManager
{
public:
	/**
	 * Constructor.
	 */
	GHOST_WindowManager();

	/**
	 * Destructor.
	 */
	virtual ~GHOST_WindowManager();

	/**
	 * Add a window to our list.
	 * It is only added if it is not already in the list.
	 * @param	window Pointer to the window to be added.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess addWindow(GHOST_IWindow* window);

	/**
	 * Remove a window from our list.
	 * @param	window Pointer to the window to be removed.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess removeWindow(const GHOST_IWindow* window);

	/**
	 * Returns whether the window is in our list.
	 * @param	window Pointer to the window to query.
	 * @return	A boolean indicator.
	 */
	virtual	bool getWindowFound(const GHOST_IWindow* window) const;

	/**
	 * Returns whether one of the windows is fullscreen.
	 * @return	A boolean indicator.
	 */
	virtual	bool getFullScreen(void) const;

	/**
	 * Returns pointer to the full-screen window.
	 * @return	The fll-screen window (0 if not in full-screen).
	 */
	virtual	GHOST_IWindow* getFullScreenWindow(void) const;

	/**
	 * Activates fullscreen mode for a window.
	 * @param window The window displayed fullscreen.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess beginFullScreen(GHOST_IWindow* window, const bool stereoVisual);

	/**
	 * Closes fullscreen mode down.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess endFullScreen(void);

	/**
	 * Sets new window as active window (the window receiving events).
	 * There can be only one window active which should be in the current window list.
	 * @param window The new active window.
	 */
	virtual GHOST_TSuccess setActiveWindow(GHOST_IWindow* window);
	
	/**
	 * Returns the active window (the window receiving events).
	 * There can be only one window active which should be in the current window list.
	 * @return window The active window (or NULL if there is none).
	 */
	virtual GHOST_IWindow* getActiveWindow(void) const;
	

	/**
	 * Set this window to be inactive (not receiving events).
	 * @param window The window to decativate.
	 */
	virtual void setWindowInactive(const GHOST_IWindow* window);
	

	/**
	 * Return a vector of the windows currently managed by this 
	 * class. 
	 * @warning It is very dangerous to mess with the contents of 
	 * this vector. Please do not destroy or add windows use the 
	 * interface above for this,
	 */
	std::vector<GHOST_IWindow *> & getWindows();

	/**
	 * Return true if any windows has a modified status
	 * @return True if any window has unsaved changes
	 */
	bool getAnyModifiedState();

protected:
	/** The list of windows managed */
	std::vector<GHOST_IWindow*> m_windows;

	/** Window in fullscreen state. There can be only one of this which is not in or window list. */
	GHOST_IWindow* m_fullScreenWindow;

	/** The active window. */
	GHOST_IWindow* m_activeWindow;

	/** Window that was active before entering fullscreen state. */
	GHOST_IWindow* m_activeWindowBeforeFullScreen;
};

#endif // _GHOST_WINDOW_MANAGER_H_

