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
#ifndef __KX_BLENDERCANVAS
#define __KX_BLENDERCANVAS

#ifdef WIN32
#include <windows.h>
#endif 

#include "GL/glew.h"

#include "RAS_ICanvas.h"
#include "RAS_Rect.h"

#include "KX_BlenderGL.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

struct ARegion;
struct wmWindow;

/**
 * 2D Blender device context abstraction. 
 * The connection from 3d rendercontext to 2d Blender surface embedding.
 */

class KX_BlenderCanvas : public RAS_ICanvas
{
private:
	/** Rect that defines the area used for rendering,
	    relative to the context */
	RAS_Rect m_displayarea;

public:
	/* Construct a new canvas.
	 * 
	 * @param area The Blender ARegion to run the game within.
	 */
	KX_BlenderCanvas(struct wmWindow* win, struct ARegion* ar);
	~KX_BlenderCanvas();

		void 
	Init(
	);
	
		void 
	SwapBuffers(
	);
		void 
	Resize(
		int width,
		int height
	);

		void
	BeginFrame(
	);

		void 
	EndFrame(
	);

		void 
	ClearColor(
		float r,
		float g,
		float b,
		float a
	);

		void 
	ClearBuffer(
		int type
	);

		int 
	GetWidth(
	) const ;

		int 
	GetHeight(
	) const ;

	const
		RAS_Rect &
	GetDisplayArea(
	) const {
		return m_displayarea;
	};

		void
	SetDisplayArea(RAS_Rect *rect
	) {
		m_displayarea= *rect;
	};

		RAS_Rect &
	GetWindowArea(
	);

		void
	SetViewPort(
		int x1, int y1,
		int x2, int y2
	);

		void 
	SetMouseState(
		RAS_MouseState mousestate
	);

		void 
	SetMousePosition(
		int x,
		int y
	);

		void 
	MakeScreenShot(
		const char* filename
	);
	
	/**
	 * Nothing needs be done for BlenderCanvas
	 * Begin/End Draw, as the game engine GL context
	 * is always current/active.
	 */

		bool 
	BeginDraw(
	) {
			return true;
	};

		void 
	EndDraw(
	) {
	};

private:
	/** Blender area the game engine is running within */
	struct ARegion* m_ar;
	struct wmWindow* m_win;
	RAS_Rect 	m_area_rect;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_BlenderCanvas"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // __KX_BLENDERCANVAS

