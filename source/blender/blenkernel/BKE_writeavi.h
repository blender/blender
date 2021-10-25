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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_WRITEAVI_H__
#define __BKE_WRITEAVI_H__

/** \file BKE_writeavi.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

/* generic blender movie support, could move to own module */

struct RenderData;	
struct ReportList;
struct Scene;

typedef struct bMovieHandle {
	int (*start_movie)(void *context_v, struct Scene *scene, struct RenderData *rd, int rectx, int recty,
	                   struct ReportList *reports, bool preview, const char *suffix);
	int (*append_movie)(void *context_v, struct RenderData *rd, int start_frame, int frame, int *pixels,
	                    int rectx, int recty, const char *suffix, struct ReportList *reports);
	void (*end_movie)(void *context_v);
	int (*get_next_frame)(void *context_v, struct RenderData *rd, struct ReportList *reports); /* optional */
	void (*get_movie_path)(char *string, struct RenderData *rd, bool preview, const char *suffix); /* optional */
	void *(*context_create)(void);
	void (*context_free)(void *context_v);
} bMovieHandle;

bMovieHandle *BKE_movie_handle_get(const char imtype);
void BKE_movie_filepath_get(char *string, struct RenderData *rd, bool preview, const char *suffix);
void BKE_context_create(bMovieHandle *mh);

#ifdef __cplusplus
}
#endif

#endif

