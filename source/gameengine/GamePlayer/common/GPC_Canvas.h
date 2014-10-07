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

/** \file GPC_Canvas.h
 *  \ingroup player
 */

#ifndef __GPC_CANVAS_H__
#define __GPC_CANVAS_H__

#include "RAS_ICanvas.h"
#include "RAS_Rect.h"

#ifdef WIN32
#  pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#  include <windows.h>
#endif  /* WIN32 */

#include "glew-mx.h"

#include <map>


class GPC_Canvas : public RAS_ICanvas
{
protected:

	/** Width of the context. */
	int m_width;
	/** Height of the context. */
	int m_height;
	/** Rect that defines the area used for rendering,
	 * relative to the context */
	RAS_Rect m_displayarea;

	int m_viewport[4];

public:

	GPC_Canvas(int width, int height);

	virtual ~GPC_Canvas();

	void Resize(int width, int height);

	virtual void ResizeWindow(int width, int height) {}

	/**
	 * \section Methods inherited from abstract base class RAS_ICanvas.
	 */
	
		int 
	GetWidth(
	) const {
		return m_width;
	}
	
		int 
	GetHeight(
	) const {
		return m_height;
	}

	const 
		RAS_Rect &
	GetDisplayArea(
	) const {
		return m_displayarea;
	};

		void
	SetDisplayArea(
		RAS_Rect *rect
	) {
		m_displayarea= *rect;
	};
	
		RAS_Rect &
	GetWindowArea(
	) {
		return m_displayarea;
	}

		void 
	BeginFrame(
	) {};

	/**
	 * Draws overlay banners and progress bars.
	 */
		void 
	EndFrame(
	) {};
	
	void SetViewPort(int x1, int y1, int x2, int y2);
	void UpdateViewPort(int x1, int y1, int x2, int y2);
	const int *GetViewPort();

	void ClearColor(float r, float g, float b, float a);

	/**
	 * \section Methods inherited from abstract base class RAS_ICanvas.
	 * Semantics are not yet honored.
	 */
	
	void SetMouseState(RAS_MouseState mousestate)
	{
		// not yet
	}

	void SetMousePosition(int x, int y)
	{
		// not yet
	}

	virtual void MakeScreenShot(const char* filename);

	void ClearBuffer(int type);
};

#endif  /* __GPC_CANVAS_H__ */
