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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
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
#include "BLI_math.h"

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
	int *points, totseg, sel_type;
	float sfra= SFRA, efra= EFRA;
	void *sel;
	float framelen= ar->winx/(efra-sfra+1);

	BKE_movieclip_last_selection(clip, &sel_type, &sel);

	glEnable(GL_BLEND);

	/* cache background */
	glColor4ub(128, 128, 255, 64);
	glRecti(0, 0, ar->winx, 8);

	/* cached segments -- could be usefu lto debug caching strategies */
	BKE_movieclip_get_cache_segments(clip, &totseg, &points);
	if(totseg) {
		int a;

		glColor4ub(128, 128, 255, 128);

		for(a= 0; a<totseg; a++) {
			float x1, x2;

			x1= (points[a*2]-sfra)/(efra-sfra+1)*ar->winx;
			x2= (points[a*2+1]-sfra+1)/(efra-sfra+1)*ar->winx;

			glRecti(x1, 0, x2, 8);
		}
	}

	/* track */
	if(sel_type==MCLIP_SEL_TRACK) {
		int i, a= 0;
		MovieTrackingTrack *track= (MovieTrackingTrack *)sel;

		for(i= sfra; i <= efra; i++) {
			int framenr;
			MovieTrackingMarker *marker;

			while(a<track->markersnr) {
				if(track->markers[a].framenr>=i)
					break;

				if(a<track->markersnr-1 && track->markers[a+1].framenr>i)
					break;

				a++;
			}

			if(a<track->markersnr) marker= &track->markers[a];
			else marker= &track->markers[track->markersnr-1];

			if((marker->flag&MARKER_DISABLED)==0) {
				framenr= marker->framenr;

				if(framenr!=i) glColor4ub(128, 128, 0, 96);
				else glColor4ub(255, 255, 0, 96);

				glRecti((i-1)*framelen, 0, i*framelen, 4);
			}
		}
	}

	glDisable(GL_BLEND);

	/* current frame */
	x= (sc->user.framenr-sfra)/(efra-sfra+1)*ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glRecti(x, 0, x+framelen, 8);
}

static void draw_movieclip_buffer(ARegion *ar, ImBuf *ibuf, float zoomx, float zoomy)
{
	int x, y;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, 0.f, 0.f, &x, &y);

	if(ibuf->rect_float)
		IMB_rect_from_float(ibuf);

	if(ibuf->rect)
		glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	/* reset zoom */
	glPixelZoom(1.f, 1.f);
}

static void draw_track_path(SpaceClip *sc, MovieClip *clip, MovieTrackingTrack *track)
{
	int count= sc->path_length;
	int i, a, b, sel_type, curindex= -1;
	float path[102][2];
	int tiny= sc->flag&SC_SHOW_TINY_MARKER, framenr;
	MovieTrackingMarker *marker;
	void *sel;

	if(count==0)
		return;

	BKE_movieclip_last_selection(clip, &sel_type, &sel);

	/* non-tracked tracks shouldn't display path */
	if((track->flag&TRACK_PROCESSED)==0)
		return;

	marker= BKE_tracking_get_marker(track, sc->user.framenr);
	if(marker==NULL || marker->flag&MARKER_DISABLED)
		return;

	framenr= marker->framenr;

	a= count;
	i= framenr-1;
	while(i>=framenr-count) {
		marker= BKE_tracking_get_marker(track, i);

		if(!marker || marker->flag&MARKER_DISABLED)
			break;

		if(marker->framenr==i) {
			copy_v2_v2(path[--a], marker->pos);

			if(marker->framenr==sc->user.framenr)
				curindex= a;
		}

		i--;
	}

	b= count;
	i= framenr;
	while(i<=framenr+count) {
		marker= BKE_tracking_get_marker(track, i);

		if(!marker || marker->flag&MARKER_DISABLED)
			break;

		if(marker->framenr==i) {
			if(marker->framenr==sc->user.framenr)
				curindex= b;

			copy_v2_v2(path[b++], marker->pos);
		}

		i++;
	}

	if(!tiny) {
		UI_ThemeColor(TH_MARKER_OUTLINE);

		if(TRACK_SELECTED(track)) {
			glPointSize(5.0f);
			glBegin(GL_POINTS);
				for(i= a; i<b; i++) {
					if(i!=curindex)
						glVertex2f(path[i][0], path[i][1]);
				}
			glEnd();
		}

		glLineWidth(3.0f);
		glBegin(GL_LINE_STRIP);
			for(i= a; i<b; i++)
				glVertex2f(path[i][0], path[i][1]);
		glEnd();
		glLineWidth(1.0f);
	}

	if(sel_type==MCLIP_SEL_TRACK && sel==track) UI_ThemeColor(TH_ACT_MARKER);
	else {
		if (TRACK_SELECTED(track)) UI_ThemeColor(TH_SEL_MARKER);
		else UI_ThemeColor(TH_MARKER);
	}

	if(TRACK_SELECTED(track)) {
		glPointSize(3.0f);
		glBegin(GL_POINTS);
			for(i= a; i<b; i++) {
				if(i!=curindex)
					glVertex2f(path[i][0], path[i][1]);
			}
		glEnd();
	}

	glBegin(GL_LINE_STRIP);
		for(i= a; i<b; i++)
			glVertex2f(path[i][0], path[i][1]);
	glEnd();
	glPointSize(1.0f);
}

static void draw_marker_outline(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	int tiny= sc->flag&SC_SHOW_TINY_MARKER;

	UI_ThemeColor(TH_MARKER_OUTLINE);

	if((marker->flag&MARKER_DISABLED)==0) {
		if(tiny) glPointSize(3.0f);
		else glPointSize(4.0f);
		glBegin(GL_POINTS);
			glVertex2f(marker->pos[0], marker->pos[1]);
		glEnd();
		glPointSize(1.0f);
	}

	/* pattern and search outline */
	glPushMatrix();
	glTranslatef(marker->pos[0], marker->pos[1], 0);

	if(!tiny) glLineWidth(3.0f);

	if(sc->flag&SC_SHOW_MARKER_PATTERN && (marker->flag&MARKER_DISABLED)==0) {
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

	if(!tiny) glLineWidth(1.0f);
}

static void draw_marker_areas(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker, int act, int sel)
{
	int color= act?TH_ACT_MARKER:TH_SEL_MARKER;
	int tiny= sc->flag&SC_SHOW_TINY_MARKER;

	/* marker position */
	if((track->flag&SELECT)==sel && (marker->flag&MARKER_DISABLED)==0) {
		if(track->flag&SELECT) UI_ThemeColor(color);
		else UI_ThemeColor(TH_MARKER);

		if(!tiny) glPointSize(2.0f);
		glBegin(GL_POINTS);
			glVertex2f(marker->pos[0], marker->pos[1]);
		glEnd();
		if(!tiny) glPointSize(1.0f);
	}

	if(tiny) {
		glLineStipple(3, 0xaaaa);
		glEnable(GL_LINE_STIPPLE);
	}

	/* pattern */
	glPushMatrix();
	glTranslatef(marker->pos[0], marker->pos[1], 0);

	if((track->pat_flag&SELECT)==sel && (marker->flag&MARKER_DISABLED)==0) {
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

	if(tiny)
		glDisable(GL_LINE_STIPPLE);
}

static void view2d_to_region_float(View2D *v2d, float x, float y, float *regionx, float *regiony)
{
	/* express given coordinates as proportional values */
	x= -v2d->cur.xmin / (v2d->cur.xmax-v2d->cur.xmin);
	y= -v2d->cur.ymin / (v2d->cur.ymax-v2d->cur.ymin);

	/* convert proportional distances to screen coordinates */
	*regionx= v2d->mask.xmin + x*(v2d->mask.xmax-v2d->mask.xmin);
	*regiony= v2d->mask.ymin + y*(v2d->mask.ymax-v2d->mask.ymin);
}

static void draw_tracking_tracks(SpaceClip *sc, ARegion *ar, MovieClip *clip, float zoomx, float zoomy)
{
	float x, y;
	MovieTrackingMarker *marker;
	MovieTrackingTrack *track;
	int width, height, sel_type;
	void *sel;

	ED_space_clip_size(sc, &width, &height);

	if(!width || !height) /* no image displayed for frame */
		return;

	/* ** find window pixel coordinates of origin ** */

	/* UI_view2d_to_region_no_clip return integer values, this could
	   lead to 1px flickering when view is locked to selection during playbeck.
	   to avoid this flickering, calclate base point in the same way as it happens
	   in UI_view2d_to_region_no_clip, but do it in floats here */

	view2d_to_region_float(&ar->v2d, 0.0f, 0.0f, &x, &y);

	glPushMatrix();
	glTranslatef(x, y, 0);
	glScalef(zoomx, zoomy, 0);
	glScalef(width, height, 0);

	BKE_movieclip_last_selection(clip, &sel_type, &sel);

	if(sc->flag&SC_SHOW_MARKER_PATH) {
		track= clip->tracking.tracks.first;
		while(track) {
			draw_track_path(sc, clip, track);
			track= track->next;
		}
	}

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

static void draw_tracking(SpaceClip *sc, ARegion *ar, MovieClip *clip, float zoomx, float zoomy)
{
	draw_tracking_tracks(sc, ar, clip, zoomx, zoomy);
}

void draw_clip_main(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	MovieClip *clip= ED_space_clip(sc);
	ImBuf *ibuf;
	float zoomx, zoomy;

	/* if no clip, nothing to do */
	if(!clip)
		return;

	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);

	ibuf= ED_space_clip_acquire_buffer(sc);

	if(ibuf) {
		draw_movieclip_buffer(ar, ibuf, zoomx, zoomy);
		IMB_freeImBuf(ibuf);

		draw_tracking(sc, ar, clip, zoomx, zoomy);
	}

	if(sc->debug_flag&SC_DBG_SHOW_CACHE)
		draw_movieclip_cache(sc, ar, clip, scene);
}

void draw_clip_track_widget(const bContext *C, void *trackp, void *userp, void *clipp, rcti *rect)
{
	ARegion *ar= CTX_wm_region(C);
	MovieClipUser *user= (MovieClipUser *)userp;
	MovieClip *clip= (MovieClip *)clipp;
	MovieTrackingTrack *track= (MovieTrackingTrack *)trackp;
	int ok= 0;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	if(track) {
		MovieTrackingMarker *marker= BKE_tracking_get_marker(track, user->framenr);

		if(marker && marker->flag&MARKER_DISABLED) {
			glColor4f(0.7f, 0.3f, 0.3f, 0.3f);
			uiSetRoundBox(15);
			uiDrawBox(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 3.0f);

			ok= 1;
		}
		else if(marker) {
			ImBuf* ibuf= BKE_movieclip_acquire_ibuf(clip, user);

			if(ibuf && ibuf->rect) {
				int pos[2];
				ImBuf* tmpibuf;

				tmpibuf= BKE_tracking_acquire_pattern_imbuf(ibuf, track, marker, pos);

				if(tmpibuf->rect_float)
					IMB_rect_from_float(tmpibuf);

				if(tmpibuf->rect) {
					int a, w, h;
					float zoomx, zoomy;
					GLint scissor[4];

					w= rect->xmax-rect->xmin;
					h= rect->ymax-rect->ymin;

					zoomx= ((float)w) / tmpibuf->x;
					zoomy= ((float)h) / tmpibuf->y;

					glPushMatrix();

					glPixelZoom(zoomx, zoomy);
					glaDrawPixelsSafe(rect->xmin, rect->ymin, tmpibuf->x, tmpibuf->y, tmpibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, tmpibuf->rect);
					glPixelZoom(1.f, 1.f);

					glGetIntegerv(GL_VIEWPORT, scissor);
					glScissor(ar->winrct.xmin + (rect->xmin-1), ar->winrct.ymin+(rect->ymin-1), (rect->xmax+1)-(rect->xmin-1), (rect->ymax+1)-(rect->ymin-1));

					glTranslatef(rect->xmin+(pos[0]+0.5f)*zoomx, rect->ymin+(pos[1]+0.5f)*zoomy, 0.f);

					for(a= 0; a< 2; a++) {
						if(a==1) {
							glLineStipple(3, 0xaaaa);
							glEnable(GL_LINE_STIPPLE);
							UI_ThemeColor(TH_SEL_MARKER);
						}
						else {
							UI_ThemeColor(TH_MARKER_OUTLINE);
						}

						glBegin(GL_LINES);
							glVertex2f(-10, 0);
							glVertex2f(10, 0);
							glVertex2f(0, -10);
							glVertex2f(0, 10);
						glEnd();
					}

					glDisable(GL_LINE_STIPPLE);

					glPopMatrix();

					glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
				}

				IMB_freeImBuf(tmpibuf);
				IMB_freeImBuf(ibuf);

				ok= 1;
			}
		}
	}

	if(!ok) {
		glColor4f(0.f, 0.f, 0.f, 0.3f);
		uiSetRoundBox(15);
		uiDrawBox(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 3.0f);
	}
}
