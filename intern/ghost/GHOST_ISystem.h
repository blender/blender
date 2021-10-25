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

/** \file ghost/GHOST_ISystem.h
 *  \ingroup GHOST
 * %Main interface file for C++ Api with declaration of GHOST_ISystem interface
 * class.
 * Contains the doxygen documentation main page.
 */

#ifndef __GHOST_ISYSTEM_H__
#define __GHOST_ISYSTEM_H__

#include "GHOST_Types.h"
#include "GHOST_ITimerTask.h"
#include "GHOST_IWindow.h"

class GHOST_IEventConsumer;

/**
 * \page GHOSTPage GHOST
 *
 * \section intro Introduction
 *
 * GHOST is yet another acronym. It stands for "Generic Handy Operating System
 * Toolkit". It has been created to replace the OpenGL utility tool kit
 * <a href="http://www.opengl.org/resources/libraries/glut/">GLUT</a>.
 * GLUT was used in <a href="http://www.blender3d.com">Blender</a> until the
 * point that Blender needed to be ported to Apple's Mac OSX. Blender needed a
 * number of modifications in GLUT to work but the GLUT sources for OSX were
 * unavailable at the time. The decision was made to build our own replacement
 * for GLUT. In those days, NaN Technologies BV was the company that developed
 * Blender.
 * <br><br>
 * Enough history. What does GHOST have to offer?<br>
 * In short: everything that Blender needed from GLUT to run on all it's supported
 * operating systems and some extra's.
 * This includes :
 *
 * - Time(r) management.
 * - Display/window management (windows are only created on the main display).
 * - Event management.
 * - Cursor shape management (no custom cursors for now).
 * - Access to the state of the mouse buttons and the keyboard.
 * - Menus for windows with events generated when they are accessed (this is
 *   work in progress).
 * - Video mode switching.
 * - Copy/Paste buffers.
 * - System paths.
 *
 * Font management has been moved to a separate library.
 *
 * \section platforms Platforms
 *
 * GHOST supports the following platforms:
 *
 * - OSX Cocoa.
 * - Windows.
 * - X11.
 * - SDL2 (experimental).
 * - NULL (headless mode).
 *
 * \section Building GHOST
 *
 * GHOST is not build standalone however there are tests in intern/ghost/test
 *
 * \section interface Interface
 * GHOST has two programming interfaces:
 *
 * - The C-API. For programs written in C.
 * - The C++-API. For programs written in C++.
 *
 * GHOST itself is written in C++ and the C-API is a wrapper around the C++
 * API.
 *
 * \subsection cplusplus_api The C++ API consists of the following files:
 *
 * - GHOST_IEvent.h
 * - GHOST_IEventConsumer.h
 * - GHOST_ISystem.h
 * - GHOST_ITimerTask.h
 * - GHOST_IWindow.h
 * - GHOST_Rect.h
 * - GHOST_Types.h
 *
 * For an example of using the C++-API, have a look at the GHOST_C-Test.cpp
 * program in the ?/ghost/test/gears/ directory.
 *
 * \subsection c_api The C-API
 * To use GHOST in programs written in C, include the file GHOST_C-API.h in
 * your program. This file includes the GHOST_Types.h file for all GHOST types
 * and defines functions that give you access to the same functionality present
 * in the C++ API.<br>
 * For an example of using the C-API, have a look at the GHOST_C-Test.c program
 * in the ?/ghost/test/gears/ directory.
 *
 * \section work Work in progress
 * \todo write WIP section
 */

/** \interface GHOST_ISystem
 * Interface for classes that provide access to the operating system.
 * There should be only one system class in an application.
 * Therefore, the routines to create and dispose the system are static.
 * Provides:
 *  -# Time(r) management.
 *  -# Display/window management (windows are only created on the main display).
 *  -# Event management.
 *  -# Cursor shape management (no custom cursors for now).
 *  -# Access to the state of the mouse buttons and the keyboard.
 *  -# Menus for windows with events generated when they are accessed (this is
 *     work in progress).
 * \author  Maarten Gribnau
 * \date    May 30, 2001
 */
class GHOST_ISystem
{
public:
	/**
	 * Creates the one and only system.
	 * \return An indication of success.
	 */
	static GHOST_TSuccess createSystem();

	/**
	 * Disposes the one and only system.
	 * \return An indication of success.
	 */
	static GHOST_TSuccess disposeSystem();

	/**
	 * Returns a pointer to the one and only system (nil if it hasn't been created).
	 * \return A pointer to the system.
	 */
	static GHOST_ISystem *getSystem();

protected:
	/**
	 * Constructor.
	 * Protected default constructor to force use of static createSystem member.
	 */
	GHOST_ISystem() {
	}

	/**
	 * Destructor.
	 * Protected default constructor to force use of static dispose member.
	 */
	virtual ~GHOST_ISystem() {
	}

public:
	/***************************************************************************************
	 * Time(r) functionality
	 ***************************************************************************************/

	/**
	 * Returns the system time.
	 * Returns the number of milliseconds since the start of the system process.
	 * Based on ANSI clock() routine.
	 * \return The number of milliseconds.
	 */
	virtual GHOST_TUns64 getMilliSeconds() const = 0;

	/**
	 * Installs a timer.
	 * Note that, on most operating systems, messages need to be processed in order
	 * for the timer callbacks to be invoked.
	 * \param delay     The time to wait for the first call to the timerProc (in milliseconds)
	 * \param interval  The interval between calls to the timerProc (in milliseconds)
	 * \param timerProc The callback invoked when the interval expires,
	 * \param userData  Placeholder for user data.
	 * \return A timer task (0 if timer task installation failed).
	 */
	virtual GHOST_ITimerTask *installTimer(GHOST_TUns64 delay,
	                                       GHOST_TUns64 interval,
	                                       GHOST_TimerProcPtr timerProc,
	                                       GHOST_TUserDataPtr userData = NULL) = 0;

	/**
	 * Removes a timer.
	 * \param timerTask Timer task to be removed.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess removeTimer(GHOST_ITimerTask *timerTask) = 0;

	/***************************************************************************************
	 * Display/window management functionality
	 ***************************************************************************************/

	/**
	 * Returns the number of displays on this system.
	 * \return The number of displays.
	 */
	virtual GHOST_TUns8 getNumDisplays() const = 0;

	/**
	 * Returns the dimensions of the main display on this system.
	 * \return The dimension of the main display.
	 */
	virtual void getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const = 0;

	/**
	 * Returns the combine dimensions of all monitors.
	 * \return The dimension of the workspace.
	 */
	virtual void getAllDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const = 0;

	/**
	 * Create a new window.
	 * The new window is added to the list of windows managed.
	 * Never explicitly delete the window, use disposeWindow() instead.
	 * \param   title           The name of the window (displayed in the title bar of the window if the OS supports it).
	 * \param   left            The coordinate of the left edge of the window.
	 * \param   top             The coordinate of the top edge of the window.
	 * \param   width           The width the window.
	 * \param   height          The height the window.
	 * \param   state           The state of the window when opened.
	 * \param   type            The type of drawing context installed in this window.
	 * \param glSettings: Misc OpenGL settings.
	 * \param exclusive: Use to show the window ontop and ignore others (used fullscreen).
	 * \param   parentWindow    Parent (embedder) window
	 * \return  The new window (or 0 if creation failed).
	 */
	virtual GHOST_IWindow *createWindow(
	        const STR_String& title,
	        GHOST_TInt32 left, GHOST_TInt32 top, GHOST_TUns32 width, GHOST_TUns32 height,
	        GHOST_TWindowState state, GHOST_TDrawingContextType type,
	        GHOST_GLSettings glSettings,
	        const bool exclusive = false,
	        const GHOST_TEmbedderWindowID parentWindow = 0) = 0;

	/**
	 * Dispose a window.
	 * \param   window Pointer to the window to be disposed.
	 * \return  Indication of success.
	 */
	virtual GHOST_TSuccess disposeWindow(GHOST_IWindow *window) = 0;

	/**
	 * Returns whether a window is valid.
	 * \param   window Pointer to the window to be checked.
	 * \return  Indication of validity.
	 */
	virtual bool validWindow(GHOST_IWindow *window) = 0;

	/**
	 * Begins full screen mode.
	 * \param setting   The new setting of the display.
	 * \param window    Window displayed in full screen.
	 *                  This window is invalid after full screen has been ended.
	 * \return  Indication of success.
	 */
	virtual GHOST_TSuccess beginFullScreen(
	        const GHOST_DisplaySetting& setting, GHOST_IWindow **window,
	        const bool stereoVisual, const bool alphaBackground = 0, const GHOST_TUns16 numOfAASamples = 0) = 0;

	/**
	 * Updates the resolution while in fullscreen mode.
	 * \param setting   The new setting of the display.
	 * \param window    Window displayed in full screen.
	 *
	 * \return  Indication of success.
	 */
	virtual GHOST_TSuccess updateFullScreen(
	        const GHOST_DisplaySetting& setting, GHOST_IWindow **window) = 0;

	/**
	 * Ends full screen mode.
	 * \return  Indication of success.
	 */
	virtual GHOST_TSuccess endFullScreen(void) = 0;

	/**
	 * Returns current full screen mode status.
	 * \return The current status.
	 */
	virtual bool getFullScreen(void) = 0;
	
	/**
	 * Native pixel size support (MacBook 'retina').
	 */
	virtual bool useNativePixel(void) = 0;

	/***************************************************************************************
	 * Event management functionality
	 ***************************************************************************************/

	/**
	 * Retrieves events from the system and stores them in the queue.
	 * \param waitForEvent Flag to wait for an event (or return immediately).
	 * \return Indication of the presence of events.
	 */
	virtual bool processEvents(bool waitForEvent) = 0;

	/**
	 * Retrieves events from the queue and send them to the event consumers.
	 */
	virtual void dispatchEvents() = 0;

	/**
	 * Adds the given event consumer to our list.
	 * \param consumer The event consumer to add.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess addEventConsumer(GHOST_IEventConsumer *consumer) = 0;

	/**
	 * Removes the given event consumer to our list.
	 * \param consumer The event consumer to remove.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess removeEventConsumer(GHOST_IEventConsumer *consumer) = 0;

	/***************************************************************************************
	 * Cursor management functionality
	 ***************************************************************************************/

	/**
	 * Returns the current location of the cursor (location in screen coordinates)
	 * \param x         The x-coordinate of the cursor.
	 * \param y         The y-coordinate of the cursor.
	 * \return          Indication of success.
	 */
	virtual GHOST_TSuccess getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const = 0;

	/**
	 * Updates the location of the cursor (location in screen coordinates).
	 * Not all operating systems allow the cursor to be moved (without the input device being moved).
	 * \param x         The x-coordinate of the cursor.
	 * \param y         The y-coordinate of the cursor.
	 * \return          Indication of success.
	 */
	virtual GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) = 0;

	/***************************************************************************************
	 * Access to mouse button and keyboard states.
	 ***************************************************************************************/

	/**
	 * Returns the state of a modifier key (ouside the message queue).
	 * \param mask      The modifier key state to retrieve.
	 * \param isDown    The state of a modifier key (true == pressed).
	 * \return          Indication of success.
	 */
	virtual GHOST_TSuccess getModifierKeyState(GHOST_TModifierKeyMask mask, bool& isDown) const = 0;

	/**
	 * Returns the state of a mouse button (ouside the message queue).
	 * \param mask      The button state to retrieve.
	 * \param isDown    Button state.
	 * \return          Indication of success.
	 */
	virtual GHOST_TSuccess getButtonState(GHOST_TButtonMask mask, bool& isDown) const = 0;

#ifdef WITH_INPUT_NDOF
	/**
	 * Sets 3D mouse deadzone
	 * \param deadzone: Deadzone of the 3D mouse (both for rotation and pan) relative to full range
	 */
	virtual void setNDOFDeadZone(float deadzone) = 0;
#endif

	/**
	 * Toggles console
	 * \param action
	 * - 0: Hides
	 * - 1: Shows
	 * - 2: Toggles
	 * - 3: Hides if it runs not from  command line
	 * - *: Does nothing
	 * \return current status (1 -visible, 0 - hidden)
	 */
	virtual int toggleConsole(int action) = 0;

	/***************************************************************************************
	 * Access to clipboard.
	 ***************************************************************************************/

	/**
	 * Returns the selection buffer
	 * \return "unsigned char" from X11 XA_CUT_BUFFER0 buffer
	 *
	 */
	virtual GHOST_TUns8 *getClipboard(bool selection) const = 0;

	/**
	 * Put data to the Clipboard
	 */
	virtual void putClipboard(GHOST_TInt8 *buffer, bool selection) const = 0;

	/**
	 * Confirms quitting he program when there is just one window left open
	 * in the application
	 */
	virtual int confirmQuit(GHOST_IWindow *window) const = 0;
protected:
	/**
	 * Initialize the system.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess init() = 0;

	/**
	 * Shut the system down.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess exit() = 0;

	/** The one and only system */
	static GHOST_ISystem *m_system;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_ISystem")
#endif
};

#endif // __GHOST_ISYSTEM_H__

