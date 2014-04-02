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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/wm_subwindow.h
 *  \ingroup wm
 */


#ifndef __WM_SUBWINDOW_H__
#define __WM_SUBWINDOW_H__


/* *************** internal api ************** */
void	wm_subwindows_free(wmWindow *win);

int		wm_subwindow_open(wmWindow *win, const rcti *winrct);
void	wm_subwindow_close(wmWindow *win, int swinid);
int		wm_subwindow_get_id(wmWindow *win);				/* returns id */

void	wm_subwindow_position(wmWindow *win, int swinid, const rcti *winrct);

void	wm_subwindow_size_get(wmWindow *win, int swinid, int *x, int *y);
void	wm_subwindow_origin_get(wmWindow *win, int swinid, int *x, int *y);
void	wm_subwindow_matrix_get(wmWindow *win, int swinid, float mat[4][4]);
void	wm_subwindow_rect_get(wmWindow *win, int swinid, struct rcti *r_rect);
void    wm_subwindow_rect_set(wmWindow *win, int swinid, const rcti *rect);

unsigned int index_to_framebuffer(int index);

#endif /* __WM_SUBWINDOW_H__ */

