/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_draw.c
 *  \ingroup spclip
 */

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_movieclip.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "clip_intern.h"	// own include

/*********************** main area drawing *************************/

static void draw_movieclip_cache(SpaceClip *sc, ARegion *ar, MovieClip *clip, Scene *scene)
{
	float x;
	int *points, totseg;
	float sfra= scene->r.sfra, efra= scene->r.efra;

	glEnable(GL_BLEND);

	/* cache background */
	glColor4ub(128, 128, 255, 64);
	glRecti(0, 0, ar->winx, 5);

	/* cached segments -- could be usefu lto debug caching strategies */
	BKE_movieclip_get_cache_segments(clip, &totseg, &points);
	if(totseg) {
		int a;

		glColor4ub(128, 128, 255, 128);

		for(a= 0; a<totseg; a++) {
			float x1, x2;

			x1= (points[a*2]-sfra)/(efra-sfra+1)*ar->winx;
			x2= (points[a*2+1]-sfra+1)/(efra-sfra+1)*ar->winx;

			glRecti(x1, 0, x2, 5);
		}
	}

	/* current frame */
	x= (sc->user.framenr-sfra)/(efra-sfra+1)*ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);

	glBegin(GL_LINE_STRIP);
		glVertex2f(x, 0);
		glVertex2f(x, 5);
	glEnd();
	glLineWidth(1.0);

	glDisable(GL_BLEND);
}

static void draw_movieclip_buffer(ARegion *ar, ImBuf *ibuf, float zoom)
{
	int x, y;

	/* set zoom */
	glPixelZoom(zoom, zoom);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x, &y);

	if(ibuf->rect_float)
		IMB_rect_from_float(ibuf);

	if(ibuf->rect)
		glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
}

void draw_clip_main(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	MovieClip *clip= ED_space_clip(sc);
	ImBuf *ibuf;

	/* if no clip, nothing to do */
	if(!clip)
		return;

	ibuf= ED_space_clip_acquire_buffer(sc);

	if(ibuf) {
		draw_movieclip_buffer(ar, ibuf, sc->zoom);
		IMB_freeImBuf(ibuf);
	}

	if(sc->debug_flags&SC_DBG_SHOW_CACHE)
		draw_movieclip_cache(sc, ar, clip, scene);
}
