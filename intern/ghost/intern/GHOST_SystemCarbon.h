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
 * @file	GHOST_SystemCarbon.h
 * Declaration of GHOST_SystemCarbon class.
 */

#ifndef _GHOST_SYSTEM_CARBON_H_
#define _GHOST_SYSTEM_CARBON_H_

#ifndef __APPLE__
#error Apple OSX only!
#endif // __APPLE__

#define __CARBONSOUND__
#include <Carbon/Carbon.h>

#include "GHOST_System.h"

class GHOST_EventCursor;
class GHOST_EventKey;
class GHOST_EventWindow;

/**
 * OSX/Carbon Implementation of GHOST_System class.
 * @see GHOST_System.
 * @author	Maarten Gribnau
 * @date	May 21, 2001
 */
class GHOST_SystemCarbon : public GHOST_System {
public:
    /**
     * Constructor.
     */
    GHOST_SystemCarbon();
    
    /** 
     * Destructor.
     */
    ~GHOST_SystemCarbon();
    
	/***************************************************************************************
	 ** Time(r) functionality
	 ***************************************************************************************/

	/**
	 * Returns the system time.
	 * Returns the number of milliseconds since the start of the system process.
	 * Based on ANSI clock() routine.
	 * @return The number of milliseconds.
	 */
	virtual GHOST_TUns64 getMilliSeconds() const;

	/***************************************************************************************
	 ** Display/window management functionality
	 ***************************************************************************************/

	/**
	 * Returns the number of displays on this system.
	 * @return The number of displays.
	 */
	virtual	GHOST_TUns8 getNumDisplays() const;

	/**
	 * Returns the dimensions of the main display on this system.
	 * @return The dimension of the main display.
	 */
	virtual void getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const;
	
	/**
	 * Create a new window.
	 * The new window is added to the list of windows managed. 
	 * Never explicitly delete the window, use disposeWindow() instead.
	 * @param	title	The name of the window (displayed in the title bar of the window if the OS supports it).
	 * @param	left	The coordinate of the left edge of the window.
	 * @param	top		The coordinate of the top edge of the window.
	 * @param	width	The width the window.
	 * @param	height	The height the window.
	 * @param	state	The state of the window when opened.
	 * @param	type	The type of drawing context installed in this window.
	 * @param	parentWindow 	Parent (embedder) window
	 * @return	The new window (or 0 if creation failed).
	 */
	virtual GHOST_IWindow* createWindow(
		const STR_String& title,
		GHOST_TInt32 left,
		GHOST_TInt32 top,
		GHOST_TUns32 width,
		GHOST_TUns32 height,
		GHOST_TWindowState state,
		GHOST_TDrawingContextType type,
		const bool stereoVisual,
		const GHOST_TUns16 numOfAASamples = 0,
		const GHOST_TEmbedderWindowID parentWindow = 0 
	);
	
	virtual GHOST_TSuccess beginFullScreen(
		const GHOST_DisplaySetting& setting, 
		GHOST_IWindow** window,
		const bool stereoVisual
	);
	
	virtual GHOST_TSuccess endFullScreen( void );
	
	/***************************************************************************************
	 ** Event management functionality
	 ***************************************************************************************/

	/**
	 * Gets events from the system and stores them in the queue.
	 * @param waitForEvent Flag to wait for an event (or return immediately).
	 * @return Indication of the presence of events.
	 */
	virtual bool processEvents(bool waitForEvent);
	
	/***************************************************************************************
	 ** Cursor management functionality
	 ***************************************************************************************/

	/**
	 * Returns the current location of the cursor (location in screen coordinates)
	 * @param x			The x-coordinate of the cursor.
	 * @param y			The y-coordinate of the cursor.
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const;

	/**
	 * Updates the location of the cursor (location in screen coordinates).
	 * @param x			The x-coordinate of the cursor.
	 * @param y			The y-coordinate of the cursor.
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) const;

	/***************************************************************************************
	 ** Access to mouse button and keyboard states.
	 ***************************************************************************************/

	/**
	 * Returns the state of all modifier keys.
	 * @param keys	The state of all modifier keys (true == pressed).
	 * @return		Indication of success.
	 */
	virtual GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys& keys) const;

	/**
	 * Returns the state of the mouse buttons (ouside the message queue).
	 * @param buttons	The state of the buttons.
	 * @return			Indication of success.
	 */
	virtual GHOST_TSuccess getButtons(GHOST_Buttons& buttons) const;

	/**
	 * Returns Clipboard data
	 * @param selection		Indicate which buffer to return
	 * @return				Returns the selected buffer
	 */
	virtual GHOST_TUns8* getClipboard(bool selection) const;
	
	/**
	 * Puts buffer to system clipboard
	 * @param buffer	The buffer to be copied
	 * @param selection	Indicates which buffer to copy too, only used on X11
	 */
	virtual void putClipboard(GHOST_TInt8 *buffer, bool selection) const;

protected:
	/**
	 * Initializes the system.
	 * For now, it justs registers the window class (WNDCLASS).
	 * @return A success value.
	 */
	virtual GHOST_TSuccess init();

	/**
	 * Closes the system down.
	 * @return A success value.
	 */
	virtual GHOST_TSuccess exit();

	
    /**
     * Handles a tablet event.
     * @param event	A Mac event.
     * @return Indication whether the event was handled. 
     */
    OSStatus handleTabletEvent(EventRef event);
    /**
     * Handles a mouse event.
     * @param event	A Mac event.
     * @return Indication whether the event was handled. 
     */
    OSStatus handleMouseEvent(EventRef event);

    /**
     * Handles a key event.
     * @param event	A Mac event.
     * @return Indication whether the event was handled. 
     */
    OSStatus handleKeyEvent(EventRef event);

   /**
     * Handles a window event.
     * @param event	A Mac event.
     * @return Indication whether the event was handled. 
     */
    OSStatus handleWindowEvent(EventRef event);

    /**
     * Handles all basic Mac application stuff for a mouse down event.
     * @param event	A Mac event.
     * @return Indication whether the event was handled. 
     */
    bool handleMouseDown(EventRef event);

    /**
     * Handles a Mac menu command.
     * @param menuResult A Mac menu/item identifier.
     * @return Indication whether the event was handled. 
     */
    bool handleMenuCommand(GHOST_TInt32 menuResult);
    
    /* callback for blender generated events */
//	static OSStatus blendEventHandlerProc(EventHandlerCallRef handler, EventRef event, void* userData);


    /**
     * Callback for Carbon when it has events.
     */
	static OSStatus sEventHandlerProc(EventHandlerCallRef handler, EventRef event, void* userData);
	
	/** Apple Event Handlers */
	static OSErr sAEHandlerLaunch(const AppleEvent *event, AppleEvent *reply, SInt32 refCon);
	static OSErr sAEHandlerOpenDocs(const AppleEvent *event, AppleEvent *reply, SInt32 refCon);
	static OSErr sAEHandlerPrintDocs(const AppleEvent *event, AppleEvent *reply, SInt32 refCon);
	static OSErr sAEHandlerQuit(const AppleEvent *event, AppleEvent *reply, SInt32 refCon);

    /**
     * Callback for Mac Timer tasks that expire.
     * @param tmTask Pointer to the timer task that expired.
     */
    //static void s_timerCallback(TMTaskPtr tmTask);
    
    /** Event handler reference. */
    EventHandlerRef m_handler;
	
	/** Start time at initialization. */
	GHOST_TUns64 m_start_time;
	
    /** State of the modifiers. */
    UInt32 m_modifierMask;

    /** Ignores window size messages (when window is dragged). */
    bool m_ignoreWindowSizedMessages;    
};

#endif // _GHOST_SYSTEM_CARBON_H_

