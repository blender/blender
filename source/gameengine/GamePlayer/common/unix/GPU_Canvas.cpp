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
   
//#include <iostream>
#include "GPU_Canvas.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GPU_Canvas::GPU_Canvas(KXH_plugin_handle p, int width, int height)
	: GPC_Canvas(width, height), m_plugin(p)
{
	/* intentionally empty */
}


GPU_Canvas::~GPU_Canvas(void)
{
	/* intentionally empty */
}

void GPU_Canvas::Init(void)
{
	/* intentionally empty */
}

void GPU_Canvas::SwapBuffers(void)
{
	if (m_plugin) KXH_swap_buffers(m_plugin);
}

bool 
GPU_Canvas::BeginDraw(void) 
{
	if (m_plugin) {
		return KXH_begin_draw(m_plugin);
	} else {
		return false;
	}
}

void GPU_Canvas::EndDraw(void) 
{
	if (m_plugin) KXH_end_draw(m_plugin);
}
