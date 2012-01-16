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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
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
#include "BKE_tracking.h"
#include "DNA_object_types.h"	/* SELECT */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "clip_intern.h"	// own include

int ED_space_clip_poll(bContext *C)
{
	SpaceClip *sc= CTX_wm_space_clip(C);

	if(sc && sc->clip)
		return 1;

	return 0;
}

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

ImBuf *ED_space_clip_get_buffer(SpaceClip *sc)
{
	if(sc->clip) {
		ImBuf *ibuf;

		ibuf= BKE_movieclip_get_postprocessed_ibuf(sc->clip, &sc->user, sc->postproc_flag);

		if(ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;

		if(ibuf)
			IMB_freeImBuf(ibuf);
	}

	return NULL;
}

ImBuf *ED_space_clip_get_stable_buffer(SpaceClip *sc, float loc[2], float *scale, float *angle)
{
	if(sc->clip) {
		ImBuf *ibuf;

		ibuf= BKE_movieclip_get_stable_ibuf(sc->clip, &sc->user, loc, scale, angle, sc->postproc_flag);

		if(ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;

		if(ibuf)
			IMB_freeImBuf(ibuf);
	}

	return NULL;
}

void ED_space_clip_size(SpaceClip *sc, int *width, int *height)
{
	if(!sc->clip) {
		*width= 0;
		*height= 0;
	} else
		BKE_movieclip_get_size(sc->clip, &sc->user, width, height);
}

void ED_space_clip_zoom(SpaceClip *sc, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	ED_space_clip_size(sc, &width, &height);

	*zoomx= (float)(ar->winrct.xmax - ar->winrct.xmin + 1)/(float)((ar->v2d.cur.xmax - ar->v2d.cur.xmin)*width);
	*zoomy= (float)(ar->winrct.ymax - ar->winrct.ymin + 1)/(float)((ar->v2d.cur.ymax - ar->v2d.cur.ymin)*height);
}

void ED_space_clip_aspect(SpaceClip *sc, float *aspx, float *aspy)
{
	MovieClip *clip= ED_space_clip(sc);

	if(clip)
		BKE_movieclip_aspect(clip, aspx, aspy);
	else
		*aspx= *aspy= 1.0f;
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

					sc->scopes.ok= 0;

					BKE_movieclip_user_set_frame(&sc->user, cfra);
				}
			}
		}
	}
}

static int selected_boundbox(SpaceClip *sc, float min[2], float max[2])
{
	MovieClip *clip= ED_space_clip(sc);
	MovieTrackingTrack *track;
	int width, height, ok= 0;
	ListBase *tracksbase= BKE_tracking_get_tracks(&clip->tracking);

	INIT_MINMAX2(min, max);

	ED_space_clip_size(sc, &width, &height);

	track= tracksbase->first;
	while(track) {
		if(TRACK_VIEW_SELECTED(sc, track)) {
			MovieTrackingMarker *marker= BKE_tracking_get_marker(track, sc->user.framenr);

			if(marker) {
				float pos[3];

				pos[0]= marker->pos[0]+track->offset[0];
				pos[1]= marker->pos[1]+track->offset[1];
				pos[2]= 0.0f;

				/* undistortion happens for normalized coords */
				if(sc->user.render_flag&MCLIP_PROXY_RENDER_UNDISTORT)
					/* undistortion happens for normalized coords */
					ED_clip_point_undistorted_pos(sc, pos, pos);

				pos[0]*= width;
				pos[1]*= height;

				mul_v3_m4v3(pos, sc->stabmat, pos);

				DO_MINMAX2(pos, min, max);

				ok= 1;
			}
		}

		track= track->next;
	}

	return ok;
}

int ED_clip_view_selection(SpaceClip *sc, ARegion *ar, int fit)
{
	int w, h, frame_width, frame_height;
	float min[2], max[2];

	ED_space_clip_size(sc, &frame_width, &frame_height);

	if(frame_width==0 || frame_height==0) return 0;

	if(!selected_boundbox(sc, min, max))
		return 0;

	/* center view */
	clip_view_center_to_point(sc, (max[0]+min[0])/(2*frame_width), (max[1]+min[1])/(2*frame_height));

	w= max[0]-min[0];
	h= max[1]-min[1];

	/* set zoom to see all selection */
	if(w>0 && h>0) {
		int width, height;
		float zoomx, zoomy, newzoom, aspx, aspy;

		ED_space_clip_aspect(sc, &aspx, &aspy);

		width= ar->winrct.xmax - ar->winrct.xmin + 1;
		height= ar->winrct.ymax - ar->winrct.ymin + 1;

		zoomx= (float)width/w/aspx;
		zoomy= (float)height/h/aspy;

		newzoom= 1.0f/power_of_2(1/MIN2(zoomx, zoomy));

		if(fit || sc->zoom>newzoom)
			sc->zoom= newzoom;
	}

	return 1;
}

void ED_clip_point_undistorted_pos(SpaceClip *sc, float co[2], float nco[2])
{
	copy_v2_v2(nco, co);

	if(sc->user.render_flag&MCLIP_PROXY_RENDER_UNDISTORT) {
		MovieClip *clip= ED_space_clip(sc);
		float aspy= 1.0f/clip->tracking.camera.pixel_aspect;
		int width, height;

		ED_space_clip_size(sc, &width, &height);

		nco[0]*= width;
		nco[1]*= height*aspy;

		BKE_tracking_invert_intrinsics(&clip->tracking, nco, nco);
		nco[0]/= width;
		nco[1]/= height*aspy;
	}
}

void ED_clip_point_stable_pos(bContext *C, float x, float y, float *xr, float *yr)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceClip *sc= CTX_wm_space_clip(C);
	int sx, sy, width, height;
	float zoomx, zoomy, pos[3]={0.0f, 0.0f, 0.0f}, imat[4][4];

	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);
	ED_space_clip_size(sc, &width, &height);

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);

	pos[0]= (x-sx)/zoomx;
	pos[1]= (y-sy)/zoomy;

	invert_m4_m4(imat, sc->stabmat);
	mul_v3_m4v3(pos, imat, pos);

	*xr= pos[0]/width;
	*yr= pos[1]/height;

	if(sc->user.render_flag&MCLIP_PROXY_RENDER_UNDISTORT) {
		MovieClip *clip= ED_space_clip(sc);
		MovieTracking *tracking= &clip->tracking;
		float aspy= 1.0f/tracking->camera.pixel_aspect;
		float tmp[2]= {*xr*width, *yr*height*aspy};

		BKE_tracking_apply_intrinsics(tracking, tmp, tmp);

		*xr= tmp[0]/width;
		*yr= tmp[1]/(height*aspy);
	}
}

void ED_clip_mouse_pos(bContext *C, wmEvent *event, float co[2])
{
	ED_clip_point_stable_pos(C, event->mval[0], event->mval[1], &co[0], &co[1]);
}
