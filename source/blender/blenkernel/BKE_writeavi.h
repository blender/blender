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

#ifndef BKE_WRITEAVI_H
#define BKE_WRITEAVI_H

#ifdef __cplusplus
extern "C" {
#endif

/* generic blender movie support, could move to own module */

struct RenderData;	
void start_avi(struct RenderData *rd, int rectx, int recty);
void end_avi(void);
void append_avi(int frame, int *pixels, int rectx, int recty);
void makeavistring (struct RenderData *rd, char *string);

typedef struct bMovieHandle {
	void (*start_movie)(struct RenderData *rd, int rectx, int recty);
	void (*append_movie)(int frame, int *pixels, int rectx, int recty);
	void (*end_movie)(void);
	int (*get_next_frame)(void); /* can be null */
} bMovieHandle;

bMovieHandle *BKE_get_movie_handle(int imtype);

#ifdef __cplusplus
}
#endif

#endif

