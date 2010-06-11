/**
 * $Id$
 *
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
#ifndef __RAS_ICANVAS
#define __RAS_ICANVAS

/**
 * 2D rendering device context. The connection from 3d rendercontext to 2d surface.
 */

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_Rect;


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

	virtual 
	~RAS_ICanvas(
	) {
	};

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
	 * @retval false Acquiring the canvas failed.
	 * @retval true Acquiring the canvas succeeded.
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
	GetMouseX( int x
	)=0;

	virtual
		int
	GetMouseY( int y
	)= 0;

	virtual
		float
	GetMouseNormalizedX( int x
	)=0;

	virtual
		float
	GetMouseNormalizedY( int y
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
	 * Set the visible vieport 
	 */

	virtual
		void
	SetViewPort(
		int x1, int y1,
		int x2, int y2
	) = 0; 

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
	
protected:
	RAS_MouseState m_mousestate;
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_ICanvas"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__RAS_ICANVAS

