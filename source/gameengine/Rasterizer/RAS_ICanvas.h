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

/** \file RAS_ICanvas.h
 *  \ingroup bgerast
 */

#ifndef __RAS_ICANVAS_H__
#define __RAS_ICANVAS_H__

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_Rect;
struct TaskScheduler;
struct TaskPool;
struct ImageFormatData;

/**
 * 2D rendering device context. The connection from 3d rendercontext to 2d surface.
 */
class RAS_ICanvas
{
public:
	enum BufferType {
		COLOR_BUFFER=1,
		DEPTH_BUFFER=2
	};

	enum RAS_MouseState
	{
		MOUSE_INVISIBLE=1,
		MOUSE_WAIT,
		MOUSE_NORMAL
	};

	RAS_ICanvas();
	virtual ~RAS_ICanvas();

	virtual 
		void 
	Init(
	) = 0;

	virtual 
		void 
	BeginFrame(
	)=0;

	virtual 
		void 
	EndFrame(
	)=0;

	/**
	 * Initializes the canvas for drawing.  Drawing to the canvas is
	 * only allowed between BeginDraw() and EndDraw().
	 *
	 * \retval false Acquiring the canvas failed.
	 * \retval true Acquiring the canvas succeeded.
	 *
	 */

	virtual 
		bool 
	BeginDraw(
	)=0;

	/**
	 * Unitializes the canvas for drawing.
	 */

	virtual 
		void 
	EndDraw(
	)=0;


	/// probably needs some arguments for PS2 in future
	virtual 
		void 
	SwapBuffers(
	)=0;
	
	virtual
		void
	SetSwapInterval(
		int interval
	)=0;

	virtual
		bool
	GetSwapInterval(
		int& intervalOut
	)=0;
 
	virtual 
		void 
	ClearBuffer(
		int type
	)=0;

	virtual 
		void 
	ClearColor(
		float r,
		float g,
		float b,
		float a
	)=0;

	virtual 
		int	 
	GetWidth(
	) const = 0;

	virtual 
		int	 
	GetHeight(
	) const = 0;

	virtual
		int
	GetMouseX(int x
	)=0;

	virtual
		int
	GetMouseY(int y
	)= 0;

	virtual
		float
	GetMouseNormalizedX(int x
	)=0;

	virtual
		float
	GetMouseNormalizedY(int y
	)= 0;

	virtual
		const RAS_Rect &
	GetDisplayArea(
	) const = 0;

	virtual
		void
	SetDisplayArea(RAS_Rect *rect
	) = 0;

	/**
	 * Used to get canvas area within blender.
	 */
	virtual
		RAS_Rect &
	GetWindowArea(
	) = 0;

	/**
	 * Set the visible view-port 
	 */

	virtual
		void
	SetViewPort(
		int x1, int y1,
		int x2, int y2
	) = 0;

	/**
	 * Update the Canvas' viewport (used when the viewport changes without using SetViewPort()
	 * eg: Shadow buffers and FBOs
	 */

	virtual
		void
	UpdateViewPort(
		int x1, int y1,
		int x2, int y2
	) = 0;

	/**
	 * Get the visible viewport
	 */
	virtual
		const int*
	GetViewPort() = 0;

	virtual 
		void 
	SetMouseState(
		RAS_MouseState mousestate
	)=0;

	virtual 
		void 
	SetMousePosition(
		int x,
		int y
	)=0;

	virtual
		RAS_MouseState
	GetMouseState()
	{
		return m_mousestate;
	}

	virtual 
		void 
	MakeScreenShot(
		const char* filename
	)=0;

	virtual void GetDisplayDimensions(int &width, int &height) = 0;

	virtual
		void 
	ResizeWindow(
		int width,
		int height
	)=0;

	virtual
		void
	SetFullScreen(
		bool enable
	)=0;

	virtual
		bool
	GetFullScreen()=0;

		
	
protected:
	RAS_MouseState m_mousestate;
	int m_frame;  /// frame number for screenshots.
	TaskScheduler *m_taskscheduler;
	TaskPool *m_taskpool;

	/**
	 * Saves screenshot data to a file. The actual compression and disk I/O is performed in
	 * a separate thread.
	 *
	 * @param filename name of the file, can contain "###" for sequential numbering. A copy of the string
	 *                 is made, so the pointer can be freed by the caller.
	 * @param dumpsx width in pixels.
	 * @param dumpsy height in pixels.
	 * @param dumprect pixel data; ownership is passed to this function, which also frees the data.
	 * @param im_format image format for the file; ownership is passed to this function, which also frees the data.
	 */
	void save_screenshot(const char *filename, int dumpsx, int dumpsy, unsigned int *dumprect,
	                     ImageFormatData * im_format);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_ICanvas")
#endif
};

#endif  /* __RAS_ICANVAS_H__ */
