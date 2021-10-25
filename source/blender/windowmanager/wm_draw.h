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

/** \file blender/windowmanager/wm_draw.h
 *  \ingroup wm
 */


#ifndef __WM_DRAW_H__
#define __WM_DRAW_H__

#include "GPU_glew.h"

typedef struct wmDrawTriple {
	GLuint bind;
	int x, y;
	GLenum target;
} wmDrawTriple;

typedef struct wmDrawData {
	struct wmDrawData *next, *prev;
	wmDrawTriple *triple;
} wmDrawData;

struct bContext;
struct wmWindow;
struct ARegion;

/* wm_draw.c */
void		wm_draw_update			(struct bContext *C);
void		wm_draw_window_clear	(struct wmWindow *win);
void		wm_draw_region_clear	(struct wmWindow *win, struct ARegion *ar);

void		wm_tag_redraw_overlay	(struct wmWindow *win, struct ARegion *ar);

void		wm_triple_draw_textures	(struct wmWindow *win, struct wmDrawTriple *triple, float alpha, bool is_interlace);

void		wm_draw_data_free		(struct wmWindow *win);

#endif /* __WM_DRAW_H__ */

