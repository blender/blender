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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_render.h
 *  \ingroup editors
 */

#ifndef __ED_RENDER_H__
#define __ED_RENDER_H__

#include "DNA_vec_types.h"

struct bContext;
struct ID;
struct Main;
struct MTex;
struct Render;
struct RenderInfo;
struct Scene;

/* render_ops.c */

void ED_operatortypes_render(void);

/* render_shading.c */

void ED_render_id_flush_update(struct Main *bmain, struct ID *id);
void ED_render_engine_changed(struct Main *bmain);
void ED_render_scene_update(struct Main *bmain, struct Scene *scene, int updated);

/* render_preview.c */

/* stores rendered preview  - is also used for icons */
typedef struct RenderInfo {
	int pr_rectx;
	int pr_recty;
	short curtile, tottile, status;
	rcti disprect;			/* storage for view3d preview rect */
	unsigned int* rect;		
	struct Render *re;		/* persistent render */
} RenderInfo;

/* ri->status */
#define PR_DBASE			1
#define PR_DISPRECT			2
#define PR_PROJECTED		4
#define PR_ROTATED			8

/* Render the preview

pr_method:
- PR_BUTS_RENDER: preview is rendered for buttons window
- PR_ICON_RENDER: preview is rendered for icons. hopefully fast enough for at least 32x32 
- PR_NODE_RENDER: preview is rendered for node editor.
*/

#define PR_BUTS_RENDER	0
#define PR_ICON_RENDER	1
#define PR_NODE_RENDER	2

void ED_preview_init_dbase(void);
void ED_preview_free_dbase(void);

void ED_preview_shader_job(const struct bContext *C, void *owner, struct ID *id, struct ID *parent, struct MTex *slot, int sizex, int sizey, int method);
void ED_preview_icon_job(const struct bContext *C, void *owner, struct ID *id, unsigned int *rect, int sizex, int sizey);
void ED_preview_kill_jobs(const struct bContext *C);

void ED_preview_draw(const struct bContext *C, void *idp, void *parentp, void *slot, rcti *rect);

void ED_render_clear_mtex_copybuf(void);

#endif
