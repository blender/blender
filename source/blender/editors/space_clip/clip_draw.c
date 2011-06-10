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
#include "DNA_object_types.h"	/* SELECT */

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BLI_utildefines.h"

#include "ED_screen.h"
#include "ED_clip.h"

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

	glDisable(GL_BLEND);

	/* current frame */
	x= (sc->user.framenr-sfra)/(efra-sfra+1)*ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);

	glBegin(GL_LINE_STRIP);
		glVertex2f(x, 0);
		glVertex2f(x, 5);
	glEnd();
	glLineWidth(1.0);
}

static void draw_movieclip_buffer(SpaceClip *sc, ARegion *ar, ImBuf *ibuf)
{
	int x, y;

	/* set zoom */
	glPixelZoom(sc->zoom, sc->zoom);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x, &y);

	if(ibuf->rect_float)
		IMB_rect_from_float(ibuf);

	if(ibuf->rect)
		glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
}

static void draw_marker_outline(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	UI_ThemeColor(TH_MARKER_OUTLINE);
	glPointSize(4.0f);
	glBegin(GL_POINTS);
		glVertex2f(marker->pos[0], marker->pos[1]);
	glEnd();
	glPointSize(1.0f);

	/* pattern and search outline */
	glPushMatrix();
	glTranslatef(marker->pos[0], marker->pos[1], 0);
	glLineWidth(3.0f);
	if(sc->flag&SC_SHOW_MARKER_PATTERN) {
		glBegin(GL_LINE_LOOP);
			glVertex2f(track->pat_min[0], track->pat_min[1]);
			glVertex2f(track->pat_max[0], track->pat_min[1]);
			glVertex2f(track->pat_max[0], track->pat_max[1]);
			glVertex2f(track->pat_min[0], track->pat_max[1]);
		glEnd();
	}

	if(sc->flag&SC_SHOW_MARKER_SEARCH) {
		glBegin(GL_LINE_LOOP);
			glVertex2f(track->search_min[0], track->search_min[1]);
			glVertex2f(track->search_max[0], track->search_min[1]);
			glVertex2f(track->search_max[0], track->search_max[1]);
			glVertex2f(track->search_min[0], track->search_max[1]);
		glEnd();
	}
	glPopMatrix();
	glLineWidth(1.0f);
}

static void draw_marker_areas(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker, int act, int sel)
{
	int color= act?TH_ACT_MARKER:TH_SEL_MARKER;

	(void)sc;

	/* marker position */
	if((track->flag&SELECT)==sel) {
		if(track->flag&SELECT) UI_ThemeColor(color);
		else UI_ThemeColor(TH_MARKER);

		glPointSize(2.0f);
		glBegin(GL_POINTS);
			glVertex2f(marker->pos[0], marker->pos[1]);
		glEnd();
		glPointSize(1.0f);
	}

	/* pattern */
	glPushMatrix();
	glTranslatef(marker->pos[0], marker->pos[1], 0);

	if((track->pat_flag&SELECT)==sel) {
		if(track->pat_flag&SELECT) UI_ThemeColor(color);
		else UI_ThemeColor(TH_MARKER);

		if(sc->flag&SC_SHOW_MARKER_PATTERN) {
			glBegin(GL_LINE_LOOP);
				glVertex2f(track->pat_min[0], track->pat_min[1]);
				glVertex2f(track->pat_max[0], track->pat_min[1]);
				glVertex2f(track->pat_max[0], track->pat_max[1]);
				glVertex2f(track->pat_min[0], track->pat_max[1]);
			glEnd();
		}
	}

	/* search */
	if((track->search_flag&SELECT)==sel) {
		if(track->search_flag&SELECT) UI_ThemeColor(color);
		else UI_ThemeColor(TH_MARKER);

		if(sc->flag&SC_SHOW_MARKER_SEARCH) {
			glBegin(GL_LINE_LOOP);
				glVertex2f(track->search_min[0], track->search_min[1]);
				glVertex2f(track->search_max[0], track->search_min[1]);
				glVertex2f(track->search_max[0], track->search_max[1]);
				glVertex2f(track->search_min[0], track->search_max[1]);
			glEnd();
		}
	}

	glPopMatrix();

}

static void draw_tracking_tracks(SpaceClip *sc, ARegion *ar, MovieClip *clip)
{
	int x, y;
	MovieTrackingMarker *marker;
	MovieTrackingTrack *track;
	int width, height, sel_type;
	void *sel;

	ED_space_clip_size(sc, &width, &height);

	if(!width || !height) /* no image displayed for frame */
		return;

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x, &y);

	glPushMatrix();
	glTranslatef(x, y, 0);
	glScalef(sc->zoom, sc->zoom, 0);
	glScalef(width, height, 0);

	BKE_movieclip_last_selection(clip, &sel_type, &sel);

	/* markers outline and non-selected areas */
	track= clip->tracking.tracks.first;
	while(track) {
		marker= BKE_tracking_get_marker(track, sc->user.framenr);

		if(marker) {
			draw_marker_outline(sc, track, marker);
			draw_marker_areas(sc, track, marker, 0, 0);
		}

		track= track->next;
	}

	/* selected areas only, so selection wouldn't be overlapped by
	   non-selected areas */
	track= clip->tracking.tracks.first;
	while(track) {
		int act= sel_type==MCLIP_SEL_TRACK && sel==track;

		if(!act) {
			marker= BKE_tracking_get_marker(track, sc->user.framenr);
			if(marker)
				draw_marker_areas(sc, track, marker, 0, 1);
		}

		track= track->next;
	}

	/* active marker would be displayed on top of everything else */
	if(sel_type==MCLIP_SEL_TRACK) {
		marker= BKE_tracking_get_marker(sel, sc->user.framenr);

		if(marker)
			draw_marker_areas(sc, sel, marker, 1, 1);
	}

	glPopMatrix();
}

static void draw_tracking(SpaceClip *sc, ARegion *ar, MovieClip *clip)
{
	draw_tracking_tracks(sc, ar, clip);
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
		draw_movieclip_buffer(sc, ar, ibuf);
		IMB_freeImBuf(ibuf);

		if(sc->mode==SC_MODE_TRACKING)
			draw_tracking(sc, ar, clip);
	}

	if(sc->debug_flag&SC_DBG_SHOW_CACHE)
		draw_movieclip_cache(sc, ar, clip, scene);
}
