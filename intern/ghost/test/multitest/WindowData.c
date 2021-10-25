/**
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

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "GHOST_C-api.h"

#include "WindowData.h"

struct _WindowData {
	void             *data;
	WindowDataHandler handler;
};

WindowData *windowdata_new(void *data, WindowDataHandler handler)
{
	WindowData *wb = MEM_mallocN(sizeof(*wb), "windowdata_new");
	wb->data = data;
	wb->handler = handler;
	
	return wb;
}

void windowdata_handle(WindowData *wb, GHOST_EventHandle evt)
{
	wb->handler(wb->data, evt);
}

void windowdata_free(WindowData *wb)
{
	MEM_freeN(wb);
}
