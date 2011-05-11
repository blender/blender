/**
 * $Id$
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

typedef struct _ScrollBar ScrollBar;


	/***/
	
ScrollBar*	scrollbar_new				(int inset, int minthumb);

int			scrollbar_is_scrolling		(ScrollBar *sb);
int			scrollbar_contains_pt		(ScrollBar *sb, int pt[2]);

void		scrollbar_start_scrolling	(ScrollBar *sb, int yco);
void		scrollbar_keep_scrolling	(ScrollBar *sb, int yco);
void		scrollbar_stop_scrolling	(ScrollBar *sb);

void		scrollbar_set_thumbpct		(ScrollBar *sb, float pct);
void		scrollbar_set_thumbpos		(ScrollBar *sb, float pos);
void		scrollbar_set_rect			(ScrollBar *sb, int rect[2][2]);

float		scrollbar_get_thumbpct		(ScrollBar *sb);
float		scrollbar_get_thumbpos		(ScrollBar *sb);
void		scrollbar_get_rect			(ScrollBar *sb, int rect_r[2][2]);

void		scrollbar_get_thumb			(ScrollBar *sb, int thumb_r[2][2]);

void		scrollbar_free				(ScrollBar *sb);

