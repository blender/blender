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

#ifndef __GPU_ENGINE_H
#define __GPU_ENGINE_H

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#define Object DNA_Object  // tricky stuff !!! but without it it doesn't compile...

#include "GPC_Engine.h"


class GPU_Engine : public GPC_Engine
{
public:
	XtIntervalId m_timerId;
	unsigned long m_timerTimeOutMsecs;

public:
	GPU_Engine(char *customLoadingAnimation,
		int foregroundColor, int backgroundColor, int frameRate);
	virtual ~GPU_Engine();
	bool Initialize(Display *display, Window window, int width, int height);

	void HandleNewWindow(Window window);

private:
	void AddEventHandlers();
};

#endif  // __GPU_ENGINE_H

