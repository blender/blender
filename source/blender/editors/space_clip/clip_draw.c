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

				glRecti((i-sfra)*framelen, 0, (i-sfra+1)*framelen, 4);
			}
		}
	}

	glDisable(GL_BLEND);

	/* current frame */
	x= (sc->user.framenr-sfra)/(efra-sfra+1)*ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glRecti(x, 0, x+framelen, 8);
}

static void draw_movieclip_buffer(SpaceClip *sc, ARegion *ar, ImBuf *ibuf, float zoomx, float zoomy)
{
	int x, y;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, 0.f, 0.f, &x, &y);

	if(sc->flag&SC_MUTE_FOOTAGE) {
		glColor3f(0.0f, 0.0f, 0.0f);
		glRectf(x, y, x+ibuf->x*sc->zoom, y+ibuf->y*sc->zoom);
	} else {
		if(ibuf->rect_float)
			IMB_rect_from_float(ibuf);

		if(ibuf->rect)
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	}

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

	marker= BKE_tracking_get_marker(track, sc->user.framenr);
	if(marker->framenr!=sc->user.framenr || marker->flag&MARKER_DISABLED)
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
		} else
			break;

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
		} else
			break;

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

	UI_ThemeColor(TH_PATH_BEFORE);

	if(TRACK_SELECTED(track)) {
		glPointSize(3.0f);
		glBegin(GL_POINTS);
			for(i= a; i<b; i++) {
				if(i==count+1)
					UI_ThemeColor(TH_PATH_AFTER);

				if(i!=curindex)
					glVertex2f(path[i][0], path[i][1]);
			}
		glEnd();
	}

	UI_ThemeColor(TH_PATH_BEFORE);

	glBegin(GL_LINE_STRIP);
		for(i= a; i<b; i++) {
			if(i==count+1)
				UI_ThemeColor(TH_PATH_AFTER);

			glVertex2f(path[i][0], path[i][1]);
		}
	glEnd();
	glPointSize(1.0f);
}

static void draw_marker_outline(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	int tiny= sc->flag&SC_SHOW_TINY_MARKER;
	int show_pat= 0;

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

	show_pat= ((marker->flag&MARKER_DISABLED)==0 || (sc->flag&SC_SHOW_MARKER_SEARCH)==0);
	if(sc->flag&SC_SHOW_MARKER_PATTERN && show_pat) {
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
	int show_pat= 0;

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

	/* pattern */
	glPushMatrix();
	glTranslatef(marker->pos[0], marker->pos[1], 0);

	if(tiny) {
		glLineStipple(3, 0xaaaa);
		glEnable(GL_LINE_STIPPLE);
	}

	show_pat= ((marker->flag&MARKER_DISABLED)==0 || (sc->flag&SC_SHOW_MARKER_SEARCH)==0);
	if((track->pat_flag&SELECT)==sel && show_pat) {
		if(marker->flag&MARKER_DISABLED) {
			if(act) UI_ThemeColor(TH_ACT_MARKER);
			else if(track->search_flag&SELECT) UI_ThemeColorShade(TH_DIS_MARKER, 128);
			else UI_ThemeColor(TH_DIS_MARKER);
		} else {
			if(track->pat_flag&SELECT) UI_ThemeColor(color);
			else UI_ThemeColor(TH_MARKER);
		}

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
		if(marker->flag&MARKER_DISABLED) {
			if(act) UI_ThemeColor(TH_ACT_MARKER);
			else if(track->search_flag&SELECT) UI_ThemeColorShade(TH_DIS_MARKER, 128);
			else UI_ThemeColor(TH_DIS_MARKER);
		} else {
			if(track->search_flag&SELECT) UI_ThemeColor(color);
			else UI_ThemeColor(TH_MARKER);
		}

		if(sc->flag&SC_SHOW_MARKER_SEARCH) {
			glBegin(GL_LINE_LOOP);
				glVertex2f(track->search_min[0], track->search_min[1]);
				glVertex2f(track->search_max[0], track->search_min[1]);
				glVertex2f(track->search_max[0], track->search_max[1]);
				glVertex2f(track->search_min[0], track->search_max[1]);
			glEnd();
		}
	}

	if(tiny)
		glDisable(GL_LINE_STIPPLE);

	glPopMatrix();
}

static void draw_marker_slide_zones(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker, int outline, int act, int width, int height)
{
	int color= act?TH_ACT_MARKER:TH_SEL_MARKER;
	float x, y, dx, dy, tdx, tdy;
	int tiny= sc->flag&SC_SHOW_TINY_MARKER;

	if(!TRACK_SELECTED(track) || (tiny && outline) || (marker->flag&MARKER_DISABLED))
		return;

	if(outline) {
		glLineWidth(3.0f);
		UI_ThemeColor(TH_MARKER_OUTLINE);
	} else {
		if(track->search_flag&SELECT) UI_ThemeColor(color);
		else UI_ThemeColor(TH_MARKER);
	}

	glPushMatrix();
	glTranslatef(marker->pos[0], marker->pos[1], 0);

	x= track->search_min[0];
	y= track->search_max[1];

	dx= 12.0f/width/sc->zoom;
	dy= 12.0f/height/sc->zoom;

	if(sc->flag&SC_SHOW_MARKER_SEARCH) {
		tdx=MIN2(dx, (track->search_max[0]-track->search_min[0])/5);
		tdy=MIN2(dy, (track->search_max[1]-track->search_min[1])/5);

		if(outline) {
			tdx+= 1.0f/sc->zoom/width;
			tdy+= 1.0f/sc->zoom/height;
		}

		glBegin(GL_QUADS);
			glVertex3f(x, y, 0);
			glVertex3f(x+tdx, y, 0);
			glVertex3f(x+tdx, y-tdy, 0);
			glVertex3f(x, y-tdy, 0);
		glEnd();

		x= track->search_max[0];
		y= track->search_min[1];

		if(outline) {
			tdx+= 1.0f/sc->zoom/width;
			tdy+= 1.0f/sc->zoom/height;
		}

		glBegin(GL_TRIANGLES);
			glVertex3f(x, y, 0);
			glVertex3f(x-tdx, y, 0);
			glVertex3f(x, y+tdy, 0);
		glEnd();
	}

	if(sc->flag&SC_SHOW_MARKER_PATTERN) {
		/* use smaller slider for pattern area */
		dx= 10.0f/width/sc->zoom;
		dy= 10.0f/height/sc->zoom;

		if(!outline) {
			if(track->pat_flag&SELECT) UI_ThemeColor(color);
			else UI_ThemeColor(TH_MARKER);
		}

		x= track->pat_max[0];
		y= track->pat_min[1];

		tdx=MIN2(dx, track->pat_max[0]-track->pat_min[0]);
		tdy=MIN2(dy, track->pat_max[1]-track->pat_min[1]);

		if(outline) {
			tdx+= 2.0f/sc->zoom/width;
			tdy+= 2.0f/sc->zoom/height;
		}

		glBegin(GL_TRIANGLES);
			glVertex3f(x, y, 0);
			glVertex3f(x-tdx, y, 0);
			glVertex3f(x, y+tdy, 0);
		glEnd();
	}

	glPopMatrix();

	if(outline)
		glLineWidth(1.0f);
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

static void draw_tracking_tracks(SpaceClip *sc, ARegion *ar, MovieClip *clip,
			int width, int height, float zoomx, float zoomy)
{
	float x, y;
	MovieTracking* tracking= &clip->tracking;
	MovieTrackingMarker *marker;
	MovieTrackingTrack *track;
	int sel_type, framenr= sc->user.framenr;
	void *sel;

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

	if(sc->flag&SC_SHOW_TRACK_PATH) {
		track= tracking->tracks.first;
		while(track) {
			if(TRACK_VISIBLE(track))
				draw_track_path(sc, clip, track);

			track= track->next;
		}
	}

	/* markers outline and non-selected areas */
	track= tracking->tracks.first;
	while(track) {
		if(TRACK_VISIBLE(track)) {
			marker= BKE_tracking_get_marker(track, framenr);

			if(MARKER_VISIBLE(sc, marker)) {
				draw_marker_outline(sc, track, marker);
				draw_marker_slide_zones(sc, track, marker, 1, 0, width, height);
				draw_marker_areas(sc, track, marker, 0, 0);
			}
		}

		track= track->next;
	}

	/* selected areas only, so selection wouldn't be overlapped by
	   non-selected areas */
	track= tracking->tracks.first;
	while(track) {
		if(TRACK_VISIBLE(track)) {
			int act= sel_type==MCLIP_SEL_TRACK && sel==track;

			if(!act) {
				marker= BKE_tracking_get_marker(track, framenr);

				if(MARKER_VISIBLE(sc, marker)) {
					draw_marker_areas(sc, track, marker, 0, 1);
					draw_marker_slide_zones(sc, track, marker, 0, 0, width, height);
				}
			}
		}

		track= track->next;
	}

	/* active marker would be displayed on top of everything else */
	if(sel_type==MCLIP_SEL_TRACK) {
		if(TRACK_VISIBLE((MovieTrackingTrack *)sel)) {
			marker= BKE_tracking_get_marker(sel, framenr);

			if(MARKER_VISIBLE(sc, marker)) {
				draw_marker_areas(sc, sel, marker, 1, 1);
				draw_marker_slide_zones(sc, sel, marker, 0, 1, width, height);
			}
		}
	}

	if(sc->flag&SC_SHOW_BUNDLES) {
		float pos[4], vec[4], mat[4][4];

		glEnable(GL_POINT_SMOOTH);
		glPointSize(3.0f);

		BKE_tracking_projection_matrix(tracking, framenr, width, height, mat);

		track= tracking->tracks.first;
		while(track) {
			if(TRACK_VISIBLE(track) && track->flag&TRACK_HAS_BUNDLE) {
				marker= BKE_tracking_get_marker(track, framenr);

				if(MARKER_VISIBLE(sc, marker)) {
					copy_v4_v4(vec, track->bundle_pos);
					vec[3]=1;

					mul_v4_m4v4(pos, mat, vec);

					pos[0]= (pos[0]/(pos[3]*2.0f)+0.5f)*width;
					pos[1]= (pos[1]/(pos[3]*2.0f)+0.5f)*height;

					BKE_tracking_apply_intrinsics(tracking, pos, width, height, pos);

					vec[0]= marker->pos[0]*width;
					vec[1]= marker->pos[1]*height;
					sub_v2_v2(vec, pos);

					if(len_v2(vec)<3) glColor3f(0.0f, 1.0f, 0.0f);
					else glColor3f(1.0f, 0.0f, 0.0f);

					glBegin(GL_POINTS);
						glVertex3f(pos[0]/width, pos[1]/height, 0);
					glEnd();
				}
			}

			track= track->next;
		}

		glPointSize(1.0f);
		glDisable(GL_POINT_SMOOTH);
	}

	glPopMatrix();
}

static void draw_tracking(SpaceClip *sc, ARegion *ar, MovieClip *clip,
			int width, int height, float zoomx, float zoomy)
{
	draw_tracking_tracks(sc, ar, clip, width, height, zoomx, zoomy);
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
		draw_movieclip_buffer(sc, ar, ibuf, zoomx, zoomy);
		IMB_freeImBuf(ibuf);

		draw_tracking(sc, ar, clip, ibuf->x, ibuf->y, zoomx, zoomy);
	}

	draw_movieclip_cache(sc, ar, clip, scene);
}

static ImBuf *scale_ibuf(ImBuf *ibuf, float zoomx, float zoomy)
{
	ImBuf *scaleibuf;
	int x, y, w= ibuf->x*zoomx, h= ibuf->y*zoomy;
	scaleibuf= IMB_allocImBuf(w, h, 32, IB_rect);

	for(y= 0; y<scaleibuf->y; y++) {
		for (x= 0; x<scaleibuf->x; x++) {
			int pixel= scaleibuf->x*y + x;
			int orig_pixel= ibuf->x*(int)(((float)y)/zoomy) + (int)(((float)x)/zoomx);
			char *rrgb= (char*)scaleibuf->rect + pixel*4;
			char *orig_rrgb= (char*)ibuf->rect + orig_pixel*4;
			rrgb[0]= orig_rrgb[0];
			rrgb[1]= orig_rrgb[1];
			rrgb[2]= orig_rrgb[2];
			rrgb[3]= orig_rrgb[3];
		}
	}

	return scaleibuf;
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

		if(marker->flag&MARKER_DISABLED) {
			glColor4f(0.7f, 0.3f, 0.3f, 0.3f);
			uiSetRoundBox(15);
			uiDrawBox(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 3.0f);

			ok= 1;
		}
		else {
			ImBuf* ibuf= BKE_movieclip_acquire_ibuf(clip, user);

			if(ibuf && ibuf->rect) {
				float pos[2];
				ImBuf *tmpibuf;

				tmpibuf= BKE_tracking_acquire_pattern_imbuf(ibuf, track, marker, 1, pos, NULL);

				if(tmpibuf->rect_float)
					IMB_rect_from_float(tmpibuf);

				if(tmpibuf->rect) {
					int a;
					float zoomx, zoomy, off_x, off_y;
					GLint scissor[4];
					ImBuf *drawibuf;

					zoomx= ((float)rect->xmax-rect->xmin) / (tmpibuf->x-2);
					zoomy= ((float)rect->ymax-rect->ymin) / (tmpibuf->y-2);

					off_x= ((int)pos[0]-pos[0]-0.5)*zoomx;
					off_y= ((int)pos[1]-pos[1]-0.5)*zoomy;

					glPushMatrix();

					glGetIntegerv(GL_VIEWPORT, scissor);

					/* draw content of pattern area */
					glScissor(ar->winrct.xmin+rect->xmin, ar->winrct.ymin+rect->ymin, scissor[2], scissor[3]);

					drawibuf= scale_ibuf(tmpibuf, zoomx, zoomy);
					glaDrawPixelsSafe(off_x+rect->xmin, off_y+rect->ymin, rect->xmax-rect->xmin-off_x, rect->ymax-rect->ymin-off_y, drawibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, drawibuf->rect);

					/* draw cross for pizel position */
					glTranslatef(off_x+rect->xmin+pos[0]*zoomx, off_y+rect->ymin+pos[1]*zoomy, 0.f);
					glScissor(ar->winrct.xmin + (rect->xmin-1), ar->winrct.ymin+(rect->ymin-1), (rect->xmax+1)-(rect->xmin-1), (rect->ymax+1)-(rect->ymin-1));

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
							glVertex2f(-10.0f, 0.0f);
							glVertex2f(10.0f, 0.0f);
							glVertex2f(0.0f, -10.0f);
							glVertex2f(0.0f, 10.0f);
						glEnd();
					}

					glDisable(GL_LINE_STIPPLE);

					glPopMatrix();

					glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

					IMB_freeImBuf(drawibuf);
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
