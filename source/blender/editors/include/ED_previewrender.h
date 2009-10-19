/**
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_PREVIEWRENDER_H
#define ED_PREVIEWRENDER_H

#include "DNA_vec_types.h"

struct View3D;
struct SpaceButs;
struct RenderInfo;
struct Scene;
struct Image;
struct Render;
struct bContext;
struct ID;
struct MTex;

#define PREVIEW_RENDERSIZE 140

/* stores rendered preview  - is also used for icons */
typedef struct RenderInfo {
	int pr_rectx;
	int pr_recty;
	short curtile, tottile, status;
	rcti disprect;			/* storage for view3d preview rect */
	unsigned int* rect;		
	struct Render *re;		/* persistant render */
} RenderInfo;

/* ri->status */
#define PR_DBASE			1
#define PR_DISPRECT			2
#define PR_PROJECTED		4
#define PR_ROTATED			8

/* Render the preview

pr_method:
- PR_DRAW_RENDER: preview is rendered and drawn, as indicated by called context (buttons panel)
- PR_ICON_RENDER: the preview is not drawn and the function is not dynamic,
  so no events are processed. Hopefully fast enough for at least 32x32 
- PR_DO_RENDER: preview is rendered, not drawn, but events are processed for afterqueue,
  in use for node editor now.
*/

#define PR_DRAW_RENDER	0
#define PR_ICON_RENDER	1
#define PR_DO_RENDER	2

void ED_preview_init_dbase(void);
void ED_preview_free_dbase(void);

void ED_preview_shader_job(const struct bContext *C, void *owner, struct ID *id, struct ID *parent, struct MTex *slot, int sizex, int sizey);
void ED_preview_iconrender(struct Scene *scene, struct ID *id, unsigned int *rect, int sizex, int sizey);

void ED_preview_draw(const struct bContext *C, void *idp, void *parentp, void *slot, rcti *rect);

#endif
