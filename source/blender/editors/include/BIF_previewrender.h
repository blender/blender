/**
 * $Id: BIF_previewrender.h 8761 2006-11-06 01:08:26Z nicholasbishop $
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

#ifndef BIF_PREVIEWRENDER_H
#define BIF_PREVIEWRENDER_H

#include "DNA_vec_types.h"

struct View3D;
struct SpaceButs;
struct RenderInfo;
struct Image;
struct ScrArea;
struct uiBlock;
struct Render;

#define PREVIEW_RENDERSIZE 140

typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

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

void	BIF_previewrender		(struct ID *id, struct RenderInfo *ri, struct ScrArea *area, int pr_method);
void	BIF_previewrender_buts	(struct SpaceButs *sbuts);
void	BIF_previewdraw			(struct ScrArea *sa, struct uiBlock *block);
void    BIF_preview_changed		(short id_code);

void	BIF_preview_init_dbase	(void);
void	BIF_preview_free_dbase	(void);

void	BIF_view3d_previewrender(struct ScrArea *sa);
void	BIF_view3d_previewdraw	(struct ScrArea *sa, struct uiBlock *block);
void	BIF_view3d_previewrender_free(struct View3D *v3d);
void	BIF_view3d_previewrender_clear(struct ScrArea *sa);
void	BIF_view3d_previewrender_signal(struct ScrArea *sa, short signal);

#endif
