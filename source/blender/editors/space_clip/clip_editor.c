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

/** \file blender/editors/space_clip/clip_editor.c
 *  \ingroup spclip
 */

#include <stddef.h>

#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_context.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

void ED_space_clip_set(bContext *C, SpaceClip *sc, MovieClip *clip)
{
	sc->clip= clip;

	if(sc->clip && sc->clip->id.us==0)
		sc->clip->id.us= 1;

	if(C)
		WM_event_add_notifier(C, NC_MOVIECLIP|NA_SELECTED, sc->clip);
}

MovieClip *ED_space_clip(SpaceClip *sc)
{
	return sc->clip;
}

ImBuf *ED_space_clip_acquire_buffer(SpaceClip *sc)
{
	if(sc->clip) {
		ImBuf *ibuf;

		ibuf= BKE_movieclip_acquire_ibuf(sc->clip, &sc->user);

		if(ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;
	}

	return NULL;
}

void ED_space_clip_size(SpaceClip *sc, int *width, int *height)
{
	if(!sc->clip) {
		*width= 0;
		*height= 0;
	} else
		BKE_movieclip_acquire_size(sc->clip, &sc->user, width, height);
}

void ED_space_clip_zoom(SpaceClip *sc, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	ED_space_clip_size(sc, &width, &height);

	*zoomx= (float)(ar->winrct.xmax - ar->winrct.xmin + 1)/(float)((ar->v2d.cur.xmax - ar->v2d.cur.xmin)*width);
	*zoomy= (float)(ar->winrct.ymax - ar->winrct.ymin + 1)/(float)((ar->v2d.cur.ymax - ar->v2d.cur.ymin)*height);
}

void ED_clip_update_frame(const Main *mainp, int cfra)
{
	wmWindowManager *wm;
	wmWindow *win;

	/* image window, compo node users */
	for(wm=mainp->wm.first; wm; wm= wm->id.next) { /* only 1 wm */
		for(win= wm->windows.first; win; win= win->next) {
			ScrArea *sa;
			for(sa= win->screen->areabase.first; sa; sa= sa->next) {
				if(sa->spacetype==SPACE_CLIP) {
					SpaceClip *sc= sa->spacedata.first;
					BKE_movieclip_user_set_frame(&sc->user, cfra);
				}
			}
		}
	}
}
