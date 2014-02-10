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

/** \file ghost/intern/GHOST_WindowCocoa.h
 *  \ingroup GHOST
 * Declaration of GHOST_WindowCocoa class.
 */

#ifndef __GHOST_WINDOWCOCOA_H__
#define __GHOST_WINDOWCOCOA_H__

#ifndef __APPLE__
#error Apple OSX only!
#endif // __APPLE__

#include "GHOST_Window.h"
#include "STR_String.h"

@class CocoaWindow;
@class CocoaOpenGLView;

class GHOST_SystemCocoa;

/**
 * Window on Mac OSX/Cocoa.
 * Carbon windows have a size widget in the lower right corner of the window.
 * To force it to be visible, the height of the client rectangle is reduced so
 * that applications do not draw in that area. GHOST will manage that area
 * which is called the gutter.
 * When OpenGL contexts are active, GHOST will use AGL_BUFFER_RECT to prevent
 * OpenGL drawing outside the reduced client rectangle.
 * \author	Maarten Gribnau
 * \date	May 23, 2001
 */
class GHOST_WindowCocoa : public GHOST_Window {
public:
	/**
	 * Constructor.
	 * Creates a new window and opens it.
	 * To check if the window was created properly, use the getValid() method.
	 * \param systemCocoa		The associated system class to forward events to
	 * \param title				The text shown in the title bar of the window.
	 * \param left				The coordinate of the left edge of the window.
	 * \param top				The coordinate of the top edge of the window.
	 * \param width				The width the window.
	 * \param height			The height the window.
	 * \param state				The state the window is initially opened with.
	 * \param type				The type of drawing context installed in this window.
	 * \param stereoVisual		Stereo visual for quad buffered stereo.
	 * \param numOfAASamples	Number of samples used for AA (zero if no AA)
	 */
	GHOST_WindowCocoa(
	    GHOST_SystemCocoa *systemCocoa,
	    const STR_String& title,
	    GHOST_TInt32 left,
	    GHOST_TInt32 bottom,
	    GHOST_TUns32 width,
	    GHOST_TUns32 height,
	    GHOST_TWindowState state,
	    GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
	    const bool stereoVisual = false,
	    const GHOST_TUns16 numOfAASamples = 0
	    );

	/**
	 * Destructor.
	 * Closes the window and disposes resources allocated.
	 */
	virtual ~GHOST_WindowCocoa();

	/**
	 * Returns indication as to whether the window is valid.
	 * \return The validity of the window.
	 */
	virtual bool getValid() const;
	
	/**
	 * Returns the associated NSWindow object
	 * \return The associated NSWindow object
	 */
	virtual void *getOSWindow() const;

	/**
	 * Sets the title displayed in the title bar.
	 * \param title	The title to display in the title bar.
	 */
	virtual void setTitle(const STR_String& title);

	/**
	 * Returns the title displayed in the title bar.
	 * \param title	The title displayed in the title bar.
	 */
	virtual void getTitle(STR_String& title) const;

	/**
	 * Returns the window rectangle dimensions.
	 * The dimensions are given in screen coordinates that are relative to the upper-left corner of the screen. 
	 * \param bounds The bounding rectangle of the window.
	 */
	virtual void getWindowBounds(GHOST_Rect& bounds) const;
	
	/**
	 * Returns the client rectangle dimensions.
	 * The left and top members of the rectangle are always zero.
	 * \param bounds The bounding rectangle of the cleient area of the window.
	 */
	virtual void getClientBounds(GHOST_Rect& bounds) const;

	/**
	 * Resizes client rectangle width.
	 * \param width The new width of the client area of the window.
	 */
	virtual GHOST_TSuccess setClientWidth(GHOST_TUns32 width);

	/**
	 * Resizes client rectangle height.
	 * \param height The new height of the client area of the window.
	 */
	virtual GHOST_TSuccess setClientHeight(GHOST_TUns32 height);

	/**
	 * Resizes client rectangle.
	 * \param width		The new width of the client area of the window.
	 * \param height	The new height of the client area of the window.
	 */
	virtual GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height);

	/**
	 * Returns the state of the window (normal, minimized, maximized).
	 * \return The state of the window.
	 */
	virtual GHOST_TWindowState getState() const;

	/**
	 * Sets the window "modified" status, indicating unsaved changes
	 * \param isUnsavedChanges Unsaved changes or not
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess setModifiedState(bool isUnsavedChanges);
	
	/**
	 * Converts a point in screen coordinates to client rectangle coordinates
	 * \param inX	The x-coordinate on the screen.
	 * \param inY	The y-coordinate on the screen.
	 * \param outX	The x-coordinate in the client rectangle.
	 * \param outY	The y-coordinate in the client rectangle.
	 */
	virtual void screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	/**
	 * Converts a point in screen coordinates to client rectangle coordinates
	 * \param inX	The x-coordinate in the client rectangle.
	 * \param inY	The y-coordinate in the client rectangle.
	 * \param outX	The x-coordinate on the screen.
	 * \param outY	The y-coordinate on the screen.
	 */
	virtual void clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	/**
	 * Converts a point in screen coordinates to client rectangle coordinates
	 * but without the y coordinate conversion needed for ghost compatibility.
	 * \param inX	The x-coordinate in the client rectangle.
	 * \param inY	The y-coordinate in the client rectangle.
	 * \param outX	The x-coordinate on the screen.
	 * \param outY	The y-coordinate on the screen.
	 */
	void clientToScreenIntern(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	/**
	 * Converts a point in screen coordinates to client rectangle coordinates,
	 * but without the y coordinate conversion needed for ghost compatibility.
	 * \param inX	The x-coordinate in the client rectangle.
	 * \param inY	The y-coordinate in the client rectangle.
	 * \param outX	The x-coordinate on the screen.
	 * \param outY	The y-coordinate on the screen.
	 */
	void screenToClientIntern(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	/**
	 * Gets the screen the window is displayed in
	 * \return The NSScreen object
	 */
	NSScreen *getScreen();
	
	/**
	 * Sets the state of the window (normal, minimized, maximized).
	 * \param state The state of the window.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess setState(GHOST_TWindowState state);

	/**
	 * Sets the order of the window (bottom, top).
	 * \param order The order of the window.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess setOrder(GHOST_TWindowOrder order);

	/**
	 * Swaps front and back buffers of a window.
	 * \return	A boolean success indicator.
	 */
	virtual GHOST_TSuccess swapBuffers();

	/**
	 * Updates the drawing context of this window. Needed
	 * whenever the window is changed.
	 * \return Indication of success.
	 */
	GHOST_TSuccess updateDrawingContext();

	/**
	 * Activates the drawing context of this window.
	 * \return	A boolean success indicator.
	 */
	virtual GHOST_TSuccess activateDrawingContext();

	virtual void loadCursor(bool visible, GHOST_TStandardCursor cursor) const;
    

	const GHOST_TabletData *GetTabletData()
	{
		return &m_tablet;
	}

	GHOST_TabletData& GetCocoaTabletData()
	{
		return m_tablet;
	}
	
	/**
	 * Sets the progress bar value displayed in the window/application icon
	 * \param progress The progress % (0.0 to 1.0)
	 */
	virtual GHOST_TSuccess setProgressBar(float progress);
	
	/**
	 * Hides the progress bar icon
	 */
	virtual GHOST_TSuccess endProgressBar();
	
	
	virtual void setNativePixelSize(void);

	GHOST_TSuccess beginFullScreen() const {return GHOST_kFailure;}

	GHOST_TSuccess endFullScreen() const {return GHOST_kFailure;}
	
	/** public function to get the window containing the OpenGL view */
	CocoaWindow *getCocoaWindow() const {return m_window;};

	/* Internal value to ensure proper redraws during animations */
	void setImmediateDraw(bool value) { m_immediateDraw = value; }
	bool getImmediateDraw(void) const { return m_immediateDraw; }
	
protected:
	/**
	 * Tries to install a rendering context in this window.
	 * \param type	The type of rendering context installed.
	 * \return Indication as to whether installation has succeeded.
	 */
	virtual GHOST_TSuccess installDrawingContext(GHOST_TDrawingContextType type);

	/**
	 * Removes the current drawing context.
	 * \return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess removeDrawingContext();
    
	/**
	 * Invalidates the contents of this window.
	 * \return Indication of success.
	 */
	virtual GHOST_TSuccess invalidate();

	/**
	 * Sets the cursor visibility on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorVisibility(bool visible);
	
	/**
	 * Sets the cursor grab on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode);
		
	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape);

	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	virtual GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 *bitmap, GHOST_TUns8 *mask,
	                                                  int sizex, int sizey, int hotX, int hotY, int fg_color, int bg_color);
					
	virtual GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2], GHOST_TUns8 mask[16][2], int hotX, int hotY);

	/** The window containing the OpenGL view */
	CocoaWindow *m_window;
	
	/** The openGL view */
	CocoaOpenGLView *m_openGLView; 

	/** The opgnGL drawing context */
	NSOpenGLContext *m_openGLContext;
	
	/** The mother SystemCocoa class to send events */
	GHOST_SystemCocoa *m_systemCocoa;
			
	/** The first created OpenGL context (for sharing display lists) */
	static NSOpenGLContext *s_firstOpenGLcontext;
	
	NSCursor *m_customCursor;

	GHOST_TabletData m_tablet;

	bool m_lionStyleFullScreen;

	bool m_immediateDraw;
};

#endif // __GHOST_WINDOWCOCOA_H__

