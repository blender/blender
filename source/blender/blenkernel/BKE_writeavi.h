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
struct ReportList;
struct Scene;

typedef struct bMovieHandle {
	int (*start_movie)(struct Scene *scene, struct RenderData *rd, int rectx, int recty, struct ReportList *reports);
	int (*append_movie)(struct RenderData *rd, int frame, int *pixels, int rectx, int recty, struct ReportList *reports);
	void (*end_movie)(void);
	int (*get_next_frame)(struct RenderData *rd, struct ReportList *reports); /* optional */
	void (*get_movie_path)(char *string, struct RenderData *rd); /* optional */
} bMovieHandle;

bMovieHandle *BKE_get_movie_handle(int imtype);
void BKE_makeanimstring(char *string, struct RenderData *rd);

#ifdef __cplusplus
}
#endif

#endif

