/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* Abstract window operations */

struct BCursor;

typedef struct _Window Window;
typedef void	(*WindowHandlerFP)	(Window *win, void *user_data, short evt, short val, char ascii);

Window*	window_open			(char *title, int x, int y, int width, int height, int start_maximized);
void	window_set_handler	(Window *win, WindowHandlerFP handler, void *user_data);
void	window_destroy		(Window *win);

void	window_set_timer	(Window *win, int delay_ms, int event);

void	window_make_active	(Window *win);
void	window_swap_buffers	(Window *win);

#if 0
//#ifdef _WIN32	// FULLSCREEN
void 	window_toggle_fullscreen(Window *win, int fullscreen);
#endif

void	window_raise		(Window *win);
void	window_lower		(Window *win);

short	window_get_qual		(Window *win);
short	window_get_mbut		(Window *win);
void	window_get_mouse	(Window *win, short *mval);
void	window_get_ndof		(Window* win, float* sbval);

float window_get_pressure(Window *win);
void window_get_tilt(Window *win, float *xtilt, float *ytilt);
short window_get_activedevice(Window *win);

void	window_get_position	(Window *win, int *posx_r, int *poxy_r);

void	window_get_size		(Window *win, int *width_r, int *height_r);
void	window_set_size		(Window *win, int width, int height);

char*	window_get_title	(Window *win);
void	window_set_title	(Window *win, char *title);

void	window_set_cursor	(Window *win, int cursor);
void	window_set_custom_cursor	(Window *win, unsigned char mask[16][2], 
				unsigned char bitmap[16][2], int hotx, int hoty );
void	window_set_custom_cursor_ex	(Window *win, struct BCursor *cursor, int useBig);

void	window_warp_pointer	(Window *win, int x, int y);

void	window_queue_redraw	(Window *win);

void    window_open_ndof(Window* win);

	/* Global windowing operations */

Window*	winlay_get_active_window(void);
	
void	winlay_process_events	(int wait_for_event);

void	winlay_get_screensize	(int *width_r, int *height_r);
