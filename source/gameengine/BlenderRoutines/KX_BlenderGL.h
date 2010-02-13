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
#ifndef __BLENDERGL
#define __BLENDERGL

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

struct wmWindow;
struct ARegion;

// special swapbuffers, that takes care of which area (viewport) needs to be swapped
void	BL_SwapBuffers(struct wmWindow *win);

void	BL_warp_pointer(struct wmWindow *win,int x,int y);

void	BL_MakeScreenShot(struct ARegion *ar, const char* filename);

void	BL_HideMouse(struct wmWindow *win);
void	BL_NormalMouse(struct wmWindow *win);
void	BL_WaitMouse(struct wmWindow *win);

void BL_print_gamedebug_line(const char* text, int xco, int yco, int width, int height);
void BL_print_gamedebug_line_padded(const char* text, int xco, int yco, int width, int height);



#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__BLENDERGL

