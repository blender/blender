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

/** \file ghost/intern/GHOST_Window.h
 *  \ingroup GHOST
 * Declaration of GHOST_Window class.
 */

#ifndef __GHOST_WINDOW_H__
#define __GHOST_WINDOW_H__

#include "GHOST_IWindow.h"

class STR_String;

/**
 * Platform independent implementation of GHOST_IWindow.
 * Dimensions are given in screen coordinates that are relative to the 
 * upper-left corner of the screen.
 * Implements part of the GHOST_IWindow interface and adds some methods to
 * be implemented by childs of this class.
 * \author	Maarten Gribnau
 * \date	May 7, 2001
 */
class GHOST_Window : public GHOST_IWindow
{
public:
	/**
	 * \section Interface inherited from GHOST_IWindow left for derived class
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
	 * virtual GHOST_TSuccess setSwapInterval() = 0;
	 * virtual int getSwapInterval() = 0;
	 * virtual GHOST_TSuccess activateDrawingContext() = 0;
	 * virtual GHOST_TSuccess invalidate() = 0;
	 */

	/**
	 * Constructor.
	 * Creates a new window and opens it.
	 * To check if the window was created properly, use the getValid() method.
	 * \param width				The width the window.
	 * \param heigh				The height the window.
	 * \param state				The state the window is initially opened with.
	 * \param type				The type of drawing context installed in this window.
	 * \param stereoVisual		Stereo visual for quad buffered stereo.
	 * \param exclusive			Use to show the window ontop and ignore others
	 *							(used fullscreen).
	 * \param numOfAASamples	Number of samples used for AA (zero if no AA)
	 */
	GHOST_Window(
	    GHOST_TUns32 width,
	    GHOST_TUns32 height,
	    GHOST_TWindowState state,
	    GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
	    const bool stereoVisual = false,
	    const bool exclusive = false,
	    const GHOST_TUns16 numOfAASamples = 0);

	/**
	 * \section Interface inherited from GHOST_IWindow left for derived class
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
	 * virtual GHOST_TSuccess setSwapInterval() = 0;
	 * virtual int getSwapInterval() = 0;
	 * virtual GHOST_TSuccess activateDrawingContext() = 0;
	 * virtual GHOST_TSuccess invalidate() = 0;
	 */
	 
	/**
	 * Destructor.
	 * Closes the window and disposes resources allocated.
	 */
	virtual ~GHOST_Window();

	/**
	 * Returns the associated OS object/handle
	 * \return The associated OS object/handle
	 */
	virtual void *getOSWindow() const;
	
	/**
	 * Returns the current cursor shape.
	 * \return	The current cursor shape.
	 */
	inline virtual GHOST_TStandardCursor getCursorShape() const;

	/**
	 * Set the shape of the cursor.
	 * \param	cursor	The new cursor shape type id.
	 * \return	Indication of success.
	 */
	virtual GHOST_TSuccess setCursorShape(GHOST_TStandardCursor cursorShape);

	/**
	 * Set the shape of the cursor to a custom cursor.
	 * \param	bitmap	The bitmap data for the cursor.
	 * \param	mask	The mask data for the cursor.
	 * \param	hotX	The X coordinate of the cursor hotspot.
	 * \param	hotY	The Y coordinate of the cursor hotspot.
	 * \return	Indication of success.
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
	 * \return	The visibility state of the cursor.
	 */
	inline virtual bool getCursorVisibility() const;
	inline virtual GHOST_TGrabCursorMode getCursorGrabMode() const;
	inline virtual bool getCursorGrabModeIsWarp() const;
	inline virtual void getCursorGrabInitPos(GHOST_TInt32 &x, GHOST_TInt32 &y) const;
	inline virtual void getCursorGrabAccum(GHOST_TInt32 &x, GHOST_TInt32 &y) const;
	inline virtual void setCursorGrabAccum(GHOST_TInt32 x, GHOST_TInt32 y);

	/**
	 * Shows or hides the cursor.
	 * \param	visible The new visibility state of the cursor.
	 * \return	Indication of success.
	 */
	virtual GHOST_TSuccess setCursorVisibility(bool visible);

	/**
	 * Sets the cursor grab.
	 * \param	mode The new grab state of the cursor.
	 * \return	Indication of success.
	 */
	virtual GHOST_TSuccess setCursorGrab(GHOST_TGrabCursorMode mode, GHOST_Rect *bounds, GHOST_TInt32 mouse_ungrab_xy[2]);

	/**
	 * Gets the cursor grab region, if unset the window is used.
	 * reset when grab is disabled.
	 */
	virtual GHOST_TSuccess getCursorGrabBounds(GHOST_Rect& bounds);

	/**
	 * Sets the progress bar value displayed in the window/application icon
	 * \param progress The progress % (0.0 to 1.0)
	 */
	virtual GHOST_TSuccess setProgressBar(float progress) {
		return GHOST_kFailure;
	}
	
	/**
	 * Hides the progress bar in the icon
	 */
	virtual GHOST_TSuccess endProgressBar() {
		return GHOST_kFailure;
	}
	
	/**
	 * Sets the swap interval for swapBuffers.
	 * \param interval The swap interval to use.
	 * \return A boolean success indicator.
	 */
	virtual GHOST_TSuccess setSwapInterval(int interval) {
		return GHOST_kFailure;
	}
	
	/**
	 * Gets the current swap interval for swapBuffers.
	 * \return An integer.
	 */
	virtual int getSwapInterval() {
		return 0;
	}
	
	/**
	 * Tells if the ongoing drag'n'drop object can be accepted upon mouse drop
	 */
	virtual void setAcceptDragOperation(bool canAccept);
	
	/**
	 * Returns acceptance of the dropped object
	 * Usually called by the "object dropped" event handling function
	 */
	virtual bool canAcceptDragOperation() const;
	
	/**
	 * Sets the window "modified" status, indicating unsaved changes
	 * \param isUnsavedChanges Unsaved changes or not
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess setModifiedState(bool isUnsavedChanges);
	
	/**
	 * Gets the window "modified" status, indicating unsaved changes
	 * \return True if there are unsaved changes
	 */
	virtual bool getModifiedState();
	
	/**
	 * Returns the type of drawing context used in this window.
	 * \return The current type of drawing context.
	 */
	inline virtual GHOST_TDrawingContextType getDrawingContextType();

	/**
	 * Tries to install a rendering context in this window.
	 * Child classes do not need to overload this method.
	 * They should overload the installDrawingContext and removeDrawingContext instead.
	 * \param type	The type of rendering context installed.
	 * \return Indication as to whether installation has succeeded.
	 */
	virtual GHOST_TSuccess setDrawingContextType(GHOST_TDrawingContextType type);

	/**
	 * Returns the window user data.
	 * \return The window user data.
	 */
	inline virtual GHOST_TUserDataPtr getUserData() const
	{
		return m_userData;
	}
	
	/**
	 * Changes the window user data.
	 * \param data The window user data.
	 */
	virtual void setUserData(const GHOST_TUserDataPtr userData)
	{
		m_userData = userData;
	}
	
	virtual float getNativePixelSize(void)
	{
		if (m_nativePixelSize > 0.0f)
			return m_nativePixelSize;
		return 1.0f;
	}

protected:
	/**
	 * Tries to install a rendering context in this window.
	 * \param type	The type of rendering context installed.
	 * \return Indication as to whether installation has succeeded.
	 */
	virtual GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType type) = 0;

	/**
	 * Removes the current drawing context.
	 * \return Indication as to whether removal has succeeded.
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
	virtual GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode) {
		return GHOST_kSuccess;
	}
	
	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape) = 0;

	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
	                                                  GHOST_TUns8 mask[16][2],
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
	GHOST_TGrabCursorMode m_cursorGrab;

	/** Initial grab location. */
	GHOST_TInt32 m_cursorGrabInitPos[2];

	/** Accumulated offset from m_cursorGrabInitPos. */
	GHOST_TInt32 m_cursorGrabAccumPos[2];

	/** Wrap the cursor within this region. */
	GHOST_Rect m_cursorGrabBounds;

	/** The current shape of the cursor */
	GHOST_TStandardCursor m_cursorShape;
    
	/** The presence of progress indicator with the application icon */
	bool m_progressBarVisible;
	
	/** The acceptance of the "drop candidate" of the current drag'n'drop operation */
	bool m_canAcceptDragOperation;
	
	/** Modified state : are there unsaved changes */
	bool m_isUnsavedChanges;
	
	/** Stores whether this is a full screen window. */
	bool m_fullScreen;

	/** Stereo visual created. Only necessary for 'real' stereo support,
	 *  ie quad buffered stereo. This is not always possible, depends on
	 *  the graphics h/w
	 */
	bool m_stereoVisual;
	
	/** Number of samples used in anti-aliasing, set to 0 if no AA **/
	GHOST_TUns16 m_numOfAASamples;

	/** Full-screen width */
	GHOST_TUns32 m_fullScreenWidth;
	/** Full-screen height */
	GHOST_TUns32 m_fullScreenHeight;
	
	/* OSX only, retina screens */
	float m_nativePixelSize;
};


inline GHOST_TDrawingContextType GHOST_Window::getDrawingContextType()
{
	return m_drawingContextType;
}

inline bool GHOST_Window::getCursorVisibility() const
{
	return m_cursorVisible;
}

inline GHOST_TGrabCursorMode GHOST_Window::getCursorGrabMode() const
{
	return m_cursorGrab;
}

inline bool GHOST_Window::getCursorGrabModeIsWarp() const
{
	return (m_cursorGrab == GHOST_kGrabWrap) ||
	       (m_cursorGrab == GHOST_kGrabHide);
}

inline void GHOST_Window::getCursorGrabInitPos(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
	x = m_cursorGrabInitPos[0];
	y = m_cursorGrabInitPos[1];
}

inline void GHOST_Window::getCursorGrabAccum(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
	x = m_cursorGrabAccumPos[0];
	y = m_cursorGrabAccumPos[1];
}

inline void GHOST_Window::setCursorGrabAccum(GHOST_TInt32 x, GHOST_TInt32 y)
{
	m_cursorGrabAccumPos[0] = x;
	m_cursorGrabAccumPos[1] = y;
}

inline GHOST_TStandardCursor GHOST_Window::getCursorShape() const
{
	return m_cursorShape;
}

#endif // _GHOST_WINDOW_H

