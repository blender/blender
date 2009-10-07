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
 * @file	GHOST_Window.h
 * Declaration of GHOST_Window class.
 */

#ifndef _GHOST_WINDOW_H_
#define _GHOST_WINDOW_H_

#include "GHOST_IWindow.h"

class STR_String;

/**
 * Platform independent implementation of GHOST_IWindow.
 * Dimensions are given in screen coordinates that are relative to the 
 * upper-left corner of the screen.
 * Implements part of the GHOST_IWindow interface and adds some methods to
 * be implemented by childs of this class.
 * @author	Maarten Gribnau
 * @date	May 7, 2001
 */
class GHOST_Window : public GHOST_IWindow
{
public:
	/**
	 * @section Interface inherited from GHOST_IWindow left for derived class
	 * implementation.
	 * virtual	bool getValid() const = 0;
	 * virtual void setTitle(const STR_String& title) = 0;
	 * virtual void getTitle(STR_String& title) const = 0;
	 * virtual	void getWindowBounds(GHOST_Rect& bounds) const = 0;
	 * virtual	void getClientBounds(GHOST_Rect& bounds) const = 0;
	 * virtual	GHOST_TSuccess setClientWidth(GHOST_TUns32 width) = 0;
	 * virtual	GHOST_TSuccess setClientHeight(GHOST_TUns32 height) = 0;
	 * virtual	GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height) = 0;
	 * virtual	void screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const = 0;
	 * virtual	void clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const = 0;
	 * virtual GHOST_TWindowState getState() const = 0;
	 * virtual GHOST_TSuccess setState(GHOST_TWindowState state) = 0;
	 * virtual GHOST_TWindowOrder getOrder(void) = 0;
	 * virtual GHOST_TSuccess setOrder(GHOST_TWindowOrder order) = 0;
	 * virtual GHOST_TSuccess swapBuffers() = 0;
	 * virtual GHOST_TSuccess activateDrawingContext() = 0;
	 * virtual GHOST_TSuccess invalidate() = 0;
	 */

	/**
	 * Constructor.
	 * Creates a new window and opens it.
	 * To check if the window was created properly, use the getValid() method.
	 * @param title		The text shown in the title bar of the window.
	 * @param left		The coordinate of the left edge of the window.
	 * @param top		The coordinate of the top edge of the window.
	 * @param width		The width the window.
	 * @param heigh		The height the window.
	 * @param state		The state the window is initially opened with.
	 * @param type		The type of drawing context installed in this window.
	 * @param stereoVisual	Stereo visual for quad buffered stereo.
	 */
	GHOST_Window(
		const STR_String& title, 
		GHOST_TInt32 left,
		GHOST_TInt32 top,
		GHOST_TUns32 width,
		GHOST_TUns32 height,
		GHOST_TWindowState state,
		GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
		const bool stereoVisual = false);

	/**
	 * @section Interface inherited from GHOST_IWindow left for derived class
	 * implementation.
	 * virtual	bool getValid() const = 0;
	 * virtual void setTitle(const STR_String& title) = 0;
	 * virtual void getTitle(STR_String& title) const = 0;
	 * virtual	void getWindowBounds(GHOST_Rect& bounds) const = 0;
	 * virtual	void getClientBounds(GHOST_Rect& bounds) const = 0;
	 * virtual	GHOST_TSuccess setClientWidth(GHOST_TUns32 width) = 0;
	 * virtual	GHOST_TSuccess setClientHeight(GHOST_TUns32 height) = 0;
	 * virtual	GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height) = 0;
	 * virtual	void screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const = 0;
	 * virtual	void clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const = 0;
	 * virtual GHOST_TWindowState getState() const = 0;
	 * virtual GHOST_TSuccess setState(GHOST_TWindowState state) = 0;
	 * virtual GHOST_TSuccess setOrder(GHOST_TWindowOrder order) = 0;
	 * virtual GHOST_TSuccess swapBuffers() = 0;
	 * virtual GHOST_TSuccess activateDrawingContext() = 0;
	 * virtual GHOST_TSuccess invalidate() = 0;
	 */
	 
	/**
	 * Destructor.
	 * Closes the window and disposes resources allocated.
	 */
	virtual ~GHOST_Window();

	/**
	 * Returns the current cursor shape.
	 * @return	The current cursor shape.
	 */
	inline virtual GHOST_TStandardCursor getCursorShape() const;

	/**
	 * Set the shape of the cursor.
	 * @param	cursor	The new cursor shape type id.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursorShape);

	/**
	 * Set the shape of the cursor to a custom cursor.
	 * @param	bitmap	The bitmap data for the cursor.
	 * @param	mask	The mask data for the cursor.
	 * @param	hotX	The X coordinate of the cursor hotspot.
	 * @param	hotY	The Y coordinate of the cursor hotspot.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess setCustomCursorShape(GHOST_TUns8 bitmap[16][2], 
												GHOST_TUns8 mask[16][2], 
												int hotX, 
												int hotY);
												
	virtual GHOST_TSuccess setCustomCursorShape(GHOST_TUns8 *bitmap, 
												GHOST_TUns8 *mask, 
												int sizex, int sizey,
												int hotX,  int hotY,
												int fg_color, int bg_color);
	
	/**
	 * Returns the visibility state of the cursor.
	 * @return	The visibility state of the cursor.
	 */
	inline virtual bool getCursorVisibility() const;
	inline virtual bool getCursorWarp() const;
	inline virtual bool getCursorWarpPos(GHOST_TInt32 &x, GHOST_TInt32 &y) const;
	inline virtual bool getCursorWarpAccum(GHOST_TInt32 &x, GHOST_TInt32 &y) const;
	inline virtual bool setCursorWarpAccum(GHOST_TInt32 x, GHOST_TInt32 y);

	/**
	 * Shows or hides the cursor.
	 * @param	visible The new visibility state of the cursor.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess setCursorVisibility(bool visible);

	/**
	 * Sets the cursor grab.
	 * @param	grab The new grab state of the cursor.
	 * @return	Indication of success.
	 */
	virtual GHOST_TSuccess setCursorGrab(bool grab, bool warp, bool restore);

	/**
	 * Sets the window "modified" status, indicating unsaved changes
	 * @param isUnsavedChanges Unsaved changes or not
	 * @return Indication of success.
	 */
	virtual GHOST_TSuccess setModifiedState(bool isUnsavedChanges);
	
	/**
	 * Gets the window "modified" status, indicating unsaved changes
	 * @return True if there are unsaved changes
	 */
	virtual bool getModifiedState();
	
	/**
	 * Returns the type of drawing context used in this window.
	 * @return The current type of drawing context.
	 */
	inline virtual GHOST_TDrawingContextType getDrawingContextType();

	/**
	 * Tries to install a rendering context in this window.
	 * Child classes do not need to overload this method.
	 * They should overload the installDrawingContext and removeDrawingContext instead.
	 * @param type	The type of rendering context installed.
	 * @return Indication as to whether installation has succeeded.
	 */
	virtual GHOST_TSuccess setDrawingContextType(GHOST_TDrawingContextType type);

	/**
	 * Returns the window user data.
	 * @return The window user data.
	 */
	inline virtual GHOST_TUserDataPtr getUserData() const
	{
		return m_userData;
	}
	
	/**
	 * Changes the window user data.
	 * @param data The window user data.
	 */
	virtual void setUserData(const GHOST_TUserDataPtr userData)
	{
		m_userData = userData;
	}

protected:
	/**
	 * Tries to install a rendering context in this window.
	 * @param type	The type of rendering context installed.
	 * @return Indication as to whether installation has succeeded.
	 */
	virtual GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType type) = 0;

	/**
	 * Removes the current drawing context.
	 * @return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess removeDrawingContext() = 0;

	/**
	 * Sets the cursor visibility on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorVisibility(bool visible) = 0;

	/**
	 * Sets the cursor grab on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorGrab(bool grab, bool warp, bool restore) { return GHOST_kSuccess; };
	
	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) = 0;

	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2], GHOST_TUns8 mask[16][2],
							 int hotX, int hotY) = 0;
	
	virtual GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 *bitmap, GHOST_TUns8 *mask, 
						int szx, int szy, int hotX, int hotY, int fg, int bg) = 0;
	/** The the of drawing context installed in this window. */
	GHOST_TDrawingContextType m_drawingContextType;
	
	/** The window user data */
	GHOST_TUserDataPtr m_userData;

	/** The current visibility of the cursor */
	bool m_cursorVisible;

	/** The current grabbed state of the cursor */
	bool m_cursorGrabbed;
	
	/** The current warped state of the cursor */
	bool m_cursorWarp;

	/** Initial grab location. */
	GHOST_TInt32 m_cursorWarpInitPos[2];

	/** Accumulated offset from m_cursorWarpInitPos. */
	GHOST_TInt32 m_cursorWarpAccumPos[2];

	/** The current shape of the cursor */
	GHOST_TStandardCursor m_cursorShape;
    
	/** Modified state : are there unsaved changes */
	bool m_isUnsavedChanges;
	
	/** Stores wether this is a full screen window. */
	bool m_fullScreen;

	/** Stereo visual created. Only necessary for 'real' stereo support,
	 *  ie quad buffered stereo. This is not always possible, depends on
	 *  the graphics h/w
	 */
	bool m_stereoVisual;
    
    /** Full-screen width */
    GHOST_TUns32 m_fullScreenWidth;
    /** Full-screen height */
    GHOST_TUns32 m_fullScreenHeight;
};


inline GHOST_TDrawingContextType GHOST_Window::getDrawingContextType()
{
	return m_drawingContextType;
}

inline bool GHOST_Window::getCursorVisibility() const
{
	return m_cursorVisible;
}

inline bool GHOST_Window::getCursorWarp() const
{
	return m_cursorWarp;
}

inline bool GHOST_Window::getCursorWarpPos(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
	if(m_cursorWarp==false)
		return GHOST_kFailure;

	x= m_cursorWarpInitPos[0];
	y= m_cursorWarpInitPos[1];
	return GHOST_kSuccess;
}

inline bool GHOST_Window::getCursorWarpAccum(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
	if(m_cursorWarp==false)
		return GHOST_kFailure;

	x= m_cursorWarpAccumPos[0];
	y= m_cursorWarpAccumPos[1];
	return GHOST_kSuccess;
}

inline bool GHOST_Window::setCursorWarpAccum(GHOST_TInt32 x, GHOST_TInt32 y)
{
	if(m_cursorWarp==false)
		return GHOST_kFailure;

	m_cursorWarpAccumPos[0]= x;
	m_cursorWarpAccumPos[1]= y;

	return GHOST_kSuccess;
}

inline GHOST_TStandardCursor GHOST_Window::getCursorShape() const
{
	return m_cursorShape;
}

#endif // _GHOST_WINDOW_H

