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

#ifndef _GPG_CANVAS_H_
#define _GPG_CANVAS_H_

#ifdef WIN32
#pragma warning (disable : 4786)
#endif // WIN32

#include "GPC_Canvas.h"

#include "GHOST_IWindow.h"


class GPG_Canvas : public GPC_Canvas
{
protected:
	/** GHOST window. */
	GHOST_IWindow* m_window;

public:
	GPG_Canvas(GHOST_IWindow* window);
	virtual ~GPG_Canvas(void);

	virtual void Init(void);
	virtual void SetMousePosition(int x, int y);
	virtual void SetMouseState(RAS_MouseState mousestate);
	virtual void SwapBuffers();

	bool BeginDraw() { return true;};
	void EndDraw() {};
};

#endif // _GPG_CANVAS_H_

