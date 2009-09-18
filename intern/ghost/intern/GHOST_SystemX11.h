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
 * @file	GHOST_SystemX11.h
 * Declaration of GHOST_SystemX11 class.
 */

#ifndef _GHOST_SYSTEM_X11_H_
#define _GHOST_SYSTEM_X11_H_

#include <X11/Xlib.h>
#include <GL/glx.h>

#include "GHOST_System.h"
#include "../GHOST_Types.h"

class GHOST_WindowX11;

/**
 * X11 Implementation of GHOST_System class.
 * @see GHOST_System.
 * @author	Laurence Bourn
 * @date	October 26, 2001
 */

class GHOST_SystemX11 : public GHOST_System {
public:

	/**
	 * Constructor
	 * this class should only be instanciated by GHOST_ISystem.
	 */

	GHOST_SystemX11(
	);
	
	/**
	 * Destructor.
	 */
	virtual ~GHOST_SystemX11();


		GHOST_TSuccess 
	init(
	);


	/**
	 * @section Interface Inherited from GHOST_ISystem 
	 */

	/**
	 * Returns the system time.
	 * Returns the number of milliseconds since the start of the system process.
	 * @return The number of milliseconds.
	 */
		GHOST_TUns64 
	getMilliSeconds(
	) const;
	

	/**
	 * Returns the number of displays on this system.
	 * @return The number of displays.
	 */
		GHOST_TUns8 
	getNumDisplays(
	) const;

	/**
	 * Returns the dimensions of the main display on this system.
	 * @return The dimension of the main display.
	 */
		void 
	getMainDisplayDimensions(
		GHOST_TUns32& width,
		GHOST_TUns32& height
	) const;

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
	 * @param       stereoVisual    Create a stereo visual for quad buffered stereo.
	 * @param	parentWindow 	Parent (embedder) window
	 * @return	The new window (or 0 if creation failed).
	 */
		GHOST_IWindow* 
	createWindow(
		const STR_String& title,
		GHOST_TInt32 left,
		GHOST_TInt32 top,
		GHOST_TUns32 width,
		GHOST_TUns32 height,
		GHOST_TWindowState state,
		GHOST_TDrawingContextType type,
		const bool stereoVisual,
		const GHOST_TEmbedderWindowID parentWindow = 0 
	);

	/**
	 * @section Interface Inherited from GHOST_ISystem 
	 */

	/**
	 * Retrieves events from the system and stores them in the queue.
	 * @param waitForEvent Flag to wait for an event (or return immediately).
	 * @return Indication of the presence of events.
	 */
		bool 
	processEvents(
		bool waitForEvent
	);

	/**
	 * @section Interface Inherited from GHOST_System 
	 */
		GHOST_TSuccess 
	getCursorPosition(
		GHOST_TInt32& x,
		GHOST_TInt32& y
	) const;
	
		GHOST_TSuccess 
	setCursorPosition(
		GHOST_TInt32 x,
		GHOST_TInt32 y
	) const;

	/**
	 * Returns the state of all modifier keys.
	 * @param keys	The state of all modifier keys (true == pressed).
	 * @return		Indication of success.
	 */
		GHOST_TSuccess 
	getModifierKeys(
		GHOST_ModifierKeys& keys
	) const ;

	/**
	 * Returns the state of the mouse buttons (ouside the message queue).
	 * @param buttons	The state of the buttons.
	 * @return			Indication of success.
	 */
		GHOST_TSuccess 
	getButtons(
		GHOST_Buttons& buttons
	) const;

	/**
	 * @section
	 * Flag a window as dirty. This will
	 * generate a GHOST window update event on a call to processEvents() 
	 */

		void
	addDirtyWindow(
		GHOST_WindowX11 * bad_wind
	);
  
 
	/**
	 * return a pointer to the X11 display structure
	 */

		Display *
	getXDisplay(
	) {
		return m_display;
	}	

		void *
	prepareNdofInfo(
		volatile GHOST_TEventNDOFData *current_values
	);

	/* Helped function for get data from the clipboard. */
	void getClipboard_xcout(XEvent evt, Atom sel, Atom target,
			 unsigned char **txt, unsigned long *len,
			 unsigned int *context) const;

	/**
	 * Returns unsinged char from CUT_BUFFER0
	 * @param selection		Get selection, X11 only feature
	 * @return				Returns the Clipboard indicated by Flag
	 */
	GHOST_TUns8 *getClipboard(bool selection) const;
	
	/**
	 * Puts buffer to system clipboard
	 * @param buffer	The buffer to copy to the clipboard	
	 * @param selection	Set the selection into the clipboard, X11 only feature
	 */
	void putClipboard(GHOST_TInt8 *buffer, bool selection) const;

	/**
	 * Atom used for ICCCM, WM-spec and Motif.
	 * We only need get this atom at the start, it's relative
	 * to the display not the window and are public for every
	 * window that need it.
	 */
	Atom m_wm_state;
	Atom m_wm_change_state;
	Atom m_net_state;
	Atom m_net_max_horz;
	Atom m_net_max_vert;
	Atom m_net_fullscreen;
	Atom m_motif;
	Atom m_wm_take_focus;
	Atom m_wm_protocols;
	Atom m_delete_window_atom;

	/* Atoms for Selection, copy & paste. */
	Atom m_targets;
	Atom m_string;
	Atom m_compound_text;
	Atom m_text;
	Atom m_clipboard;
	Atom m_primary;
	Atom m_xclip_out;
	Atom m_incr;
	Atom m_utf8_string;

private :

	Display * m_display;

	/// The vector of windows that need to be updated.
	std::vector<GHOST_WindowX11 *> m_dirty_windows;

	/// Start time at initialization.
	GHOST_TUns64 m_start_time;

	/// A vector of keyboard key masks
	char m_keyboard_vector[32];

	/**
	 * Return the ghost window associated with the
	 * X11 window xwind
	 */

		GHOST_WindowX11 * 
	findGhostWindow(
		Window xwind
	) const ;

		void
	processEvent(
		XEvent *xe
 	);

		bool
	generateWindowExposeEvents(
 	);
 
		GHOST_TKey
	convertXKey(
		KeySym key
	);

};

#endif

