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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SizerWin32.h
 *  \ingroup GHOST
 */
#ifndef __GHOST_SIZERWIN32_H__
#define __GHOST_SIZERWIN32_H__

#include <windows.h>
class GHOST_SizerWin32
{
	private:
		HWND hwnd;
		int startpos[2];
		int minsize[2];
		RECT initrect;
		RECT goodrect;
		unsigned char hortransf, vertransf;


	public:
		GHOST_SizerWin32(void);
		~GHOST_SizerWin32(void);

		bool isWinChanges(void);
		void startSizing(unsigned short type);
		void updateWindowSize(void);

		void setHWND(HWND hWnd);
		void setMinSize(int minx, int miny);

		void cancel(void);
		void accept(void);

	
};

#endif /*#ifndef __GHOST_SIZERWIN32_H__*/