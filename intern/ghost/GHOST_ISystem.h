/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	May 30, 2001
 */

#ifndef _GHOST_ISYSTEM_H_
#define _GHOST_ISYSTEM_H_

#include "GHOST_Types.h"
#include "GHOST_ITimerTask.h"
#include "GHOST_IWindow.h"

class GHOST_IEventConsumer;

/**
 * Interface for classes that provide access to the operating system.
 * There should be only one system class in an application.
 * Therefore, the routines to create and dispose the system are static.
 * Provides:
 * 1. Time(r) management.
 * 2. Display/window management (windows are only created on the main display for now).
 * 3. Event management.
 * 4. Access to the state of the mouse buttons and the keyboard.
 * @author	Maarten Gribnau
 * @date	May 30, 2001
 */

class GHOST_ISystem
{
public:
	/**
	 * Creates the one and only system.
	 * @return An indication of success.
	 */
	static GHOST_TSuccess createSystem();

	/**
	 * Disposes the one and only system.
	 * @return An indication of success.
	 */
	static GHOST_TSuccess disposeSystem();

	/**
	 * Returns a pointer to the one and only system (nil if it hasn't been created).
	 * @return A pointer to the system.
	 */
	static GHOST_ISystem* getSystem();

protected:
	/**
	 * Constructor.
	 * Protected default constructor to force use of static createSystem member.
	 */
	GHOST_ISystem() {}

	/**
	 * Destructor.
	 * Protected default constructor to force use of static dispose member.
	 */
	virtual ~GHOST_ISystem() {}

public:
	/***************************************************************************************
	 ** Time(r) functionality
	 ***************************************************************************************/

	/**
	 * Returns the system time.
	 * Returns the number of milliseconds since the start of the system process.
	 * Based on ANSI clock() routine.
	 * @return The number of milliseconds.
	 */
	virtual GHOST_TUns64 getMilliSeconds() const = 0;

	/**
	 * Installs a timer.
	 * Note that, on most operating systems, messages need to be processed in order 
	 * for the timer callbacks to be invoked.
	 * @param delay		The time to wait for the first call to the timerProc (in milliseconds)
	 * @param interval	The interval between calls to the timerProc (in milliseconds)
	 * @param timerProc	The callback invoked when the interval expires,
	 * @param userData	Placeholder for user data.
	 * @return A timer task (0 if timer task installation failed).
	 */
	virtual GHOST_ITimerTask* installTimer(GHOST_TUns64 delay, GHOST_TUns64 interval, GHOST_TimerProcPtr timerProc, GHOST_TUserDataPtr userData = 0) = 0;

	/**
	 * Removes a timer.
	 * @param timerTask Timer task to be removed.
	 * @return Indication of success.
	 */
	virtual GHOST_TSuccess removeTimer(GHOST_ITimerTask* timerTask) = 0;

	/***************************************************************************************
	 ** Display/window management functionality
	 ***************************************************************************************/

	/**
	 * Returns the number of displays on this system.
	 * @return The number of displays.
	 */
	virtual	GHOST_TUns8 getNumDisplays() const = 0;

	/**
	 * Returns the dimensions of the main display on this system.
	 * @return The dimension of the main display.
	 */
	virtual void getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const = 0;
	
	/**
	 * Create a new window.
	 * The new window is added to the list of windows managed. 
	 * Never explicitly delete the window, use disposeWindow() instead.
	 * @param	title	The name of the window (displayed in the title bar of the window if the OS supports it).
	 * @param	left		The coordinate of the left edge of the window.
	 * @param	top		The coordinate of the top edge of the window.
	 * @param	width		The width the window.
	 * @param	height		The height the window.
	 * @param	state		The state of the window when opened.
	 * @param	type		The type of drawing context installed in this window.
	 * @param	stereoVisual	Create a stereo visual for quad buffered stereo.
	 * @return	The new window (or 0 if creation failed).
	 */
	virtual GHOST_IWindow* createWindow(
		const STR_String& title,
		GHOST_TInt32 left, GHOST_TInt32 top, GHOST_TUns32 width, GHOST_TUns32 height,
		GHOST_TWindowState state, GHOST_TDrawingContextType type,
		const bool stereoVisual) = 0;

	/**
	 * Dispose a window.
	 * @param	window Pointer to the window to be disposed.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess disposeWindow(GHOST_IWindow* window) = 0;

	/**
	 * Returns whether a window is valid.
	 * @param	window Pointer to the window to be checked.
	 * @return	Indication of validity.
	 */
	virtual bool validWindow(GHOST_IWindow* window) = 0;

	/**
	 * Begins full screen mode.
	 * @param setting	The new setting of the display.
	 * @param window	Window displayed in full screen.
	 *					This window is invalid after full screen has been ended.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess beginFullScreen(const GHOST_DisplaySetting& setting, GHOST_IWindow** window,
		const bool stereoVisual) = 0;

	/**
	 * Ends full screen mode.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess endFullScreen(void) = 0;

	/**
	 * Returns current full screen mode status.
	 * @return The current status.
	 */
	virtual bool getFullScreen(void) = 0;

	/***************************************************************************************
	 ** Event management functionality
	 ***************************************************************************************/

	/**
	 * Retrieves events from the system and stores them in the queue.
	 * @param waitForEvent Flag to wait for an event (or return immediately).
	 * @return Indication of the presence of events.
	 */
	virtual bool processEvents(bool waitForEvent) = 0;
	
	/**
	 * Retrieves events from the queue and send them to the event consumers.
	 * @return Indication of the presence of events.
	 */
	virtual bool dispatchEvents() = 0;

	/**
	 * Adds the given event consumer to our list.
	 * @param consumer The event consumer to add.
	 * @return Indication of success.
	 */
	virtual GHOST_TSuccess addEventConsumer(GHOST_IEventConsumer* consumer) = 0;
	
	/***************************************************************************************
	 ** Cursor management functionality
	 ***************************************************************************************/

	/**
	 * Returns the current location of the cursor (location in screen coordinates)
	 * @param x			The x-coordinate of the cursor.
	 * @param y			The y-coordinate of the cursor.
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const = 0;

	/**
	 * Updates the location of the cursor (location in screen coordinates).
	 * Not all operating systems allow the cursor to be moved (without the input device being moved).
	 * @param x			The x-coordinate of the cursor.
	 * @param y			The y-coordinate of the cursor.
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) const = 0;

	/***************************************************************************************
	 ** Access to mouse button and keyboard states.
	 ***************************************************************************************/

	/**
	 * Returns the state of a modifier key (ouside the message queue).
	 * @param mask		The modifier key state to retrieve.
	 * @param isDown	The state of a modifier key (true == pressed).
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess getModifierKeyState(GHOST_TModifierKeyMask mask, bool& isDown) const = 0;

	/**
	 * Returns the state of a mouse button (ouside the message queue).
	 * @param mask		The button state to retrieve.
	 * @param isDown	Button state.
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess getButtonState(GHOST_TButtonMask mask, bool& isDown) const = 0;

protected:
	/**
	 * Initialize the system.
	 * @return Indication of success.
	 */
	virtual GHOST_TSuccess init() = 0;

	/**
	 * Shut the system down.
	 * @return Indication of success.
	 */
	virtual GHOST_TSuccess exit() = 0;

	/** The one and only system */
	static GHOST_ISystem* m_system;
};

#endif // _GHOST_ISYSTEM_H_

