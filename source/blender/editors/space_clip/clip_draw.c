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

/** \file blender/editors/space_clip/clip_draw.c
 *  \ingroup spclip
 */

#include "DNA_gpencil_types.h"
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
#include "BLI_string.h"
#include "BLI_rect.h"
#include "BLI_math_base.h"

#include "ED_screen.h"
#include "ED_clip.h"
#include "ED_gpencil.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "BLF_api.h"

#include "clip_intern.h"	// own include

/*********************** main area drawing *************************/

void clip_draw_curfra_label(SpaceClip *sc, float x, float y)
{
	uiStyle *style = UI_GetStyle();
	int fontid = style->widget.uifont_id;
	char numstr[32];
	float font_dims[2] = {0.0f, 0.0f};

	/* frame number */
	BLF_size(fontid, 11.0f, U.dpi);
	BLI_snprintf(numstr, sizeof(numstr), "%d", sc->user.framenr);

	BLF_width_and_height(fontid, numstr, &font_dims[0], &font_dims[1]);

	glRecti(x, y, x + font_dims[0] + 6.0f, y + font_dims[1] + 4.0f);

	UI_ThemeColor(TH_TEXT);
	BLF_position(fontid, x+2.0f, y+2.0f, 0.0f);
	BLF_draw(fontid, numstr, sizeof(numstr));
}

static void draw_movieclip_cache(SpaceClip *sc, ARegion *ar, MovieClip *clip, Scene *scene)
{
	float x;
	int *points, totseg, i, a;
	float sfra = SFRA, efra = EFRA, framelen = ar->winx / (efra - sfra + 1);
	MovieTrackingTrack *act_track = BKE_tracking_active_track(&clip->tracking);
	MovieTrackingReconstruction *reconstruction = BKE_tracking_get_reconstruction(&clip->tracking);

	glEnable(GL_BLEND);

	/* cache background */
	glColor4ub(128, 128, 255, 64);
	glRecti(0, 0, ar->winx, 8);

	/* cached segments -- could be usefu lto debug caching strategies */
	BKE_movieclip_get_cache_segments(clip, &sc->user, &totseg, &points);
	if (totseg) {
		glColor4ub(128, 128, 255, 128);

		for (a = 0; a < totseg; a++) {
			float x1, x2;

			x1 = (points[a * 2] - sfra) / (efra - sfra + 1) * ar->winx;
			x2 = (points[a * 2 + 1] - sfra + 1) / (efra - sfra + 1) * ar->winx;

			glRecti(x1, 0, x2, 8);
		}
	}

	/* track */
	if (act_track) {
		MovieTrackingTrack *track = act_track;

		for (i = sfra, a = 0; i <= efra; i++) {
			int framenr;
			MovieTrackingMarker *marker;

			while (a<track->markersnr) {
				if (track->markers[a].framenr >= i)
					break;

				if (a < track->markersnr - 1 && track->markers[a + 1].framenr > i)
					break;

				a++;
			}

			if (a < track->markersnr)
				marker = &track->markers[a];
			else
				marker = &track->markers[track->markersnr - 1];

			if ((marker->flag & MARKER_DISABLED) == 0) {
				framenr = marker->framenr;

				if (framenr != i)
					glColor4ub(128, 128, 0, 96);
				else if ((marker->flag & MARKER_TRACKED) == 0)
					glColor4ub(255, 255, 0, 196);
				else
					glColor4ub(255, 255, 0, 96);

				glRecti((i - sfra) * framelen, 0, (i - sfra + 1)*framelen, 4);
			}
		}
	}

	/* failed frames */
	if (reconstruction->flag & TRACKING_RECONSTRUCTED) {
		int n = reconstruction->camnr;
		MovieReconstructedCamera *cameras = reconstruction->cameras;

		glColor4ub(255, 0, 0, 96);

		for (i = sfra, a = 0; i <= efra; i++) {
			int ok = FALSE;

			while (a < n) {
				if (cameras[a].framenr == i) {
					ok = TRUE;
					break;
				}
				else if (cameras[a].framenr > i) {
					break;
				}

				a++;
			}

			if (!ok)
				glRecti((i - sfra) * framelen, 0, (i - sfra + 1) * framelen, 8);
		}
	}

	glDisable(GL_BLEND);

	/* current frame */
	x = (sc->user.framenr - sfra) / (efra - sfra + 1) * ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glRecti(x, 0, x+framelen, 8);

	clip_draw_curfra_label(sc, x, 8.0f);
}

static void draw_movieclip_notes(SpaceClip *sc, ARegion *ar)
{
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	char str[256] = {0};
	int block = FALSE;

	if (tracking->stats) {
		BLI_strncpy(str, tracking->stats->message, sizeof(str));
		block = TRUE;
	}
	else {
		if (sc->flag & SC_LOCK_SELECTION)
			strcpy(str, "Locked");
	}

	if (str[0])
		ED_region_info_draw(ar, str, block, 0.6f);
}

static void verify_buffer_float(ImBuf *ibuf)
{
	if (ibuf->rect_float && (ibuf->rect == NULL || (ibuf->userflags & IB_RECT_INVALID))) {
		IMB_rect_from_float(ibuf);
	}
}

static void draw_movieclip_buffer(SpaceClip *sc, ARegion *ar, ImBuf *ibuf,
                                  int width, int height, float zoomx, float zoomy)
{
	int x, y;
	MovieClip *clip = ED_space_clip(sc);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x, &y);

	if (sc->flag & SC_MUTE_FOOTAGE) {
		glColor3f(0.0f, 0.0f, 0.0f);
		glRectf(x, y, x + zoomx * width, y + zoomy * height);
	}
	else {
		verify_buffer_float(ibuf);

		if (ibuf->rect) {
			int need_fallback = 1;

			if (ED_space_clip_texture_buffer_supported(sc)) {
				if (ED_space_clip_load_movieclip_buffer(sc, ibuf)) {
					glPushMatrix();
					glTranslatef(x, y, 0.0f);
					glScalef(zoomx, zoomy, 1.0f);

					glBegin(GL_QUADS);
						glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f,  0.0f);
						glTexCoord2f(1.0f, 0.0f); glVertex2f(width, 0.0f);
						glTexCoord2f(1.0f, 1.0f); glVertex2f(width, height);
						glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f,  height);
					glEnd();

					glPopMatrix();

					ED_space_clip_unload_movieclip_buffer(sc);

					need_fallback = 0;
				}
			}

			/* if texture buffers aren't efifciently supported or texture is too large to
			 * be binder fallback to simple draw pixels solution */
			if (need_fallback) {
				/* set zoom */
				glPixelZoom(zoomx*width/ibuf->x, zoomy*height/ibuf->y);

				glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

				/* reset zoom */
				glPixelZoom(1.0f, 1.0f);
			}
		}
	}

	/* draw boundary border for frame if stabilization is enabled */
	if (sc->flag & SC_SHOW_STABLE && clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
		glColor3f(0.0f, 0.0f, 0.0f);
		glLineStipple(3, 0xaaaa);
		glEnable(GL_LINE_STIPPLE);
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(GL_NOR);

		glPushMatrix();
		glTranslatef(x, y, 0.0f);

		glScalef(zoomx, zoomy, 1.0f);
		glMultMatrixf(sc->stabmat);

		glBegin(GL_LINE_LOOP);
			glVertex2f(0.0f, 0.0f);
			glVertex2f(width, 0.0f);
			glVertex2f(width, height);
			glVertex2f(0.0f, height);
		glEnd();

		glPopMatrix();

		glDisable(GL_COLOR_LOGIC_OP);
		glDisable(GL_LINE_STIPPLE);
	}
}

static void draw_track_path(SpaceClip *sc, MovieClip *UNUSED(clip), MovieTrackingTrack *track)
{
	int count = sc->path_length;
	int i, a, b, curindex = -1;
	float path[102][2];
	int tiny = sc->flag&SC_SHOW_TINY_MARKER, framenr;
	MovieTrackingMarker *marker;

	if (count == 0)
		return;

	marker = BKE_tracking_get_marker(track, sc->user.framenr);
	if (marker->framenr != sc->user.framenr || marker->flag & MARKER_DISABLED)
		return;

	framenr = marker->framenr;

	a = count;
	i = framenr - 1;
	while (i >= framenr - count) {
		marker = BKE_tracking_get_marker(track, i);

		if (!marker || marker->flag & MARKER_DISABLED)
			break;

		if (marker->framenr == i) {
			add_v2_v2v2(path[--a], marker->pos, track->offset);
			ED_clip_point_undistorted_pos(sc, path[a], path[a]);

			if (marker->framenr == sc->user.framenr)
				curindex = a;
		}
		else {
			break;
		}

		i--;
	}

	b = count;
	i = framenr;
	while (i <= framenr+count) {
		marker = BKE_tracking_get_marker(track, i);

		if (!marker || marker->flag & MARKER_DISABLED)
			break;

		if (marker->framenr == i) {
			if (marker->framenr == sc->user.framenr)
				curindex= b;

			add_v2_v2v2(path[b++], marker->pos, track->offset);
			ED_clip_point_undistorted_pos(sc, path[b-1], path[b-1]);
		}
		else
			break;

		i++;
	}

	if (!tiny) {
		UI_ThemeColor(TH_MARKER_OUTLINE);

		if (TRACK_VIEW_SELECTED(sc, track)) {
			glPointSize(5.0f);
			glBegin(GL_POINTS);
				for (i = a; i < b; i++) {
					if (i != curindex)
						glVertex2f(path[i][0], path[i][1]);
				}
			glEnd();
		}

		glLineWidth(3.0f);
		glBegin(GL_LINE_STRIP);
			for (i = a; i < b; i++)
				glVertex2f(path[i][0], path[i][1]);
		glEnd();
		glLineWidth(1.0f);
	}

	UI_ThemeColor(TH_PATH_BEFORE);

	if (TRACK_VIEW_SELECTED(sc, track)) {
		glPointSize(3.0f);
		glBegin(GL_POINTS);
			for (i = a; i < b; i++) {
				if (i == count + 1)
					UI_ThemeColor(TH_PATH_AFTER);

				if (i != curindex)
					glVertex2f(path[i][0], path[i][1]);
			}
		glEnd();
	}

	UI_ThemeColor(TH_PATH_BEFORE);

	glBegin(GL_LINE_STRIP);
		for (i = a; i < b; i++) {
			if (i == count + 1)
				UI_ThemeColor(TH_PATH_AFTER);

			glVertex2f(path[i][0], path[i][1]);
		}
	glEnd();
	glPointSize(1.0f);
}

static void draw_marker_outline(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker, float marker_pos[2], int width, int height)
{
	int tiny = sc->flag&SC_SHOW_TINY_MARKER;
	int show_search = FALSE;
	float px[2];

	UI_ThemeColor(TH_MARKER_OUTLINE);

	px[0] = 1.0f / width / sc->zoom;
	px[1] = 1.0f / height / sc->zoom;

	if ((marker->flag & MARKER_DISABLED) == 0) {
		float pos[2];
		rctf r;

		BLI_init_rctf(&r, track->pat_min[0], track->pat_max[0], track->pat_min[1], track->pat_max[1]);
		add_v2_v2v2(pos, marker->pos, track->offset);

		ED_clip_point_undistorted_pos(sc, pos, pos);

		if (BLI_in_rctf(&r, pos[0]-marker_pos[0], pos[1]-marker_pos[1])) {
			if (tiny) glPointSize(3.0f);
			else glPointSize(4.0f);
			glBegin(GL_POINTS);
				glVertex2f(pos[0], pos[1]);
			glEnd();
			glPointSize(1.0f);
		}
		else {
			if (!tiny) glLineWidth(3.0f);
			glBegin(GL_LINES);
				glVertex2f(pos[0] + px[0]*2, pos[1]);
				glVertex2f(pos[0] + px[0]*8, pos[1]);

				glVertex2f(pos[0] - px[0]*2, pos[1]);
				glVertex2f(pos[0] - px[0]*8, pos[1]);

				glVertex2f(pos[0], pos[1] - px[1]*2);
				glVertex2f(pos[0], pos[1] - px[1]*8);

				glVertex2f(pos[0], pos[1] + px[1]*2);
				glVertex2f(pos[0], pos[1] + px[1]*8);
			glEnd();
			if (!tiny) glLineWidth(1.0f);
		}
	}

	/* pattern and search outline */
	glPushMatrix();
	glTranslatef(marker_pos[0], marker_pos[1], 0);

	if (!tiny)
		glLineWidth(3.0f);

	if (sc->flag & SC_SHOW_MARKER_PATTERN) {
		glBegin(GL_LINE_LOOP);
			glVertex2f(track->pat_min[0], track->pat_min[1]);
			glVertex2f(track->pat_max[0], track->pat_min[1]);
			glVertex2f(track->pat_max[0], track->pat_max[1]);
			glVertex2f(track->pat_min[0], track->pat_max[1]);
		glEnd();
	}

	show_search = TRACK_VIEW_SELECTED(sc, track) &&
	              ((marker->flag & MARKER_DISABLED) == 0 || (sc->flag & SC_SHOW_MARKER_PATTERN) == 0);
	if (sc->flag & SC_SHOW_MARKER_SEARCH && show_search) {
		glBegin(GL_LINE_LOOP);
			glVertex2f(track->search_min[0], track->search_min[1]);
			glVertex2f(track->search_max[0], track->search_min[1]);
			glVertex2f(track->search_max[0], track->search_max[1]);
			glVertex2f(track->search_min[0], track->search_max[1]);
		glEnd();
	}
	glPopMatrix();

	if (!tiny)
		glLineWidth(1.0f);
}

static void track_colors(MovieTrackingTrack *track, int act, float col[3], float scol[3])
{
	if (track->flag & TRACK_CUSTOMCOLOR) {
		if (act)
			UI_GetThemeColor3fv(TH_ACT_MARKER, scol);
		else
			copy_v3_v3(scol, track->color);

		mul_v3_v3fl(col, track->color, 0.5f);
	}
	else {
		UI_GetThemeColor3fv(TH_MARKER, col);

		if (act)
			UI_GetThemeColor3fv(TH_ACT_MARKER, scol);
		else
			UI_GetThemeColor3fv(TH_SEL_MARKER, scol);
	}
}

static void draw_marker_areas(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker, float marker_pos[2], int width, int height, int act, int sel)
{
	int tiny= sc->flag&SC_SHOW_TINY_MARKER;
	int show_search= 0;
	float col[3], scol[3], px[2];

	track_colors(track, act, col, scol);

	px[0]= 1.0f / width / sc->zoom;
	px[1]= 1.0f / height / sc->zoom;

	/* marker position and offset position */
	if ((track->flag&SELECT) == sel && (marker->flag & MARKER_DISABLED) == 0) {
		float pos[2];
		rctf r;

		if (track->flag & TRACK_LOCKED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->flag & SELECT)
				UI_ThemeColorShade(TH_LOCK_MARKER, 64);
			else
				UI_ThemeColor(TH_LOCK_MARKER);
		}
		else {
			if (track->flag & SELECT)
				glColor3fv(scol);
			else
				glColor3fv(col);
		}

		BLI_init_rctf(&r, track->pat_min[0], track->pat_max[0], track->pat_min[1], track->pat_max[1]);
		add_v2_v2v2(pos, marker->pos, track->offset);
		ED_clip_point_undistorted_pos(sc, pos, pos);

		if (BLI_in_rctf(&r, pos[0]-marker_pos[0], pos[1]-marker_pos[1])) {
			if (!tiny)
				glPointSize(2.0f);

			glBegin(GL_POINTS);
				glVertex2f(pos[0], pos[1]);
			glEnd();

			if (!tiny)
				glPointSize(1.0f);
		}
		else {
			glBegin(GL_LINES);
				glVertex2f(pos[0] + px[0]*3, pos[1]);
				glVertex2f(pos[0] + px[0]*7, pos[1]);

				glVertex2f(pos[0] - px[0]*3, pos[1]);
				glVertex2f(pos[0] - px[0]*7, pos[1]);

				glVertex2f(pos[0], pos[1] - px[1]*3);
				glVertex2f(pos[0], pos[1] - px[1]*7);

				glVertex2f(pos[0], pos[1] + px[1]*3);
				glVertex2f(pos[0], pos[1] + px[1]*7);
			glEnd();

			glColor3f(0.0f, 0.0f, 0.0f);
			glLineStipple(3, 0xaaaa);
			glEnable(GL_LINE_STIPPLE);
			glEnable(GL_COLOR_LOGIC_OP);
			glLogicOp(GL_NOR);

			glBegin(GL_LINES);
				glVertex2fv(pos);
				glVertex2fv(marker_pos);
			glEnd();

			glDisable(GL_COLOR_LOGIC_OP);
			glDisable(GL_LINE_STIPPLE);
		}
	}

	/* pattern */
	glPushMatrix();
	glTranslatef(marker_pos[0], marker_pos[1], 0);

	if (tiny) {
		glLineStipple(3, 0xaaaa);
		glEnable(GL_LINE_STIPPLE);
	}

	if ((track->pat_flag & SELECT) == sel && (sc->flag & SC_SHOW_MARKER_PATTERN)) {
		if (track->flag & TRACK_LOCKED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->pat_flag & SELECT)
				UI_ThemeColorShade(TH_LOCK_MARKER, 64);
			else UI_ThemeColor(TH_LOCK_MARKER);
		}
		else if (marker->flag & MARKER_DISABLED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->pat_flag & SELECT)
				UI_ThemeColorShade(TH_DIS_MARKER, 128);
			else UI_ThemeColor(TH_DIS_MARKER);
		}
		else {
			if (track->pat_flag & SELECT)
				glColor3fv(scol);
			else glColor3fv(col);
		}

		glBegin(GL_LINE_LOOP);
			glVertex2f(track->pat_min[0], track->pat_min[1]);
			glVertex2f(track->pat_max[0], track->pat_min[1]);
			glVertex2f(track->pat_max[0], track->pat_max[1]);
			glVertex2f(track->pat_min[0], track->pat_max[1]);
		glEnd();
	}

	/* search */
	show_search = TRACK_VIEW_SELECTED(sc, track) &&
	             ((marker->flag & MARKER_DISABLED) == 0 || (sc->flag & SC_SHOW_MARKER_PATTERN) == 0);
	if ((track->search_flag & SELECT) == sel && (sc->flag & SC_SHOW_MARKER_SEARCH) && show_search) {
		if (track->flag & TRACK_LOCKED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->search_flag & SELECT)
				UI_ThemeColorShade(TH_LOCK_MARKER, 64);
			else UI_ThemeColor(TH_LOCK_MARKER);
		}
		else if (marker->flag & MARKER_DISABLED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->search_flag & SELECT)
				UI_ThemeColorShade(TH_DIS_MARKER, 128);
			else UI_ThemeColor(TH_DIS_MARKER);
		}
		else {
			if (track->search_flag & SELECT)
				glColor3fv(scol);
			else
				glColor3fv(col);
		}

		glBegin(GL_LINE_LOOP);
			glVertex2f(track->search_min[0], track->search_min[1]);
			glVertex2f(track->search_max[0], track->search_min[1]);
			glVertex2f(track->search_max[0], track->search_max[1]);
			glVertex2f(track->search_min[0], track->search_max[1]);
		glEnd();
	}

	/* pyramid */
	if (sel && TRACK_VIEW_SELECTED(sc, track) &&
	    (track->tracker == TRACKER_KLT) &&
		(marker->flag & MARKER_DISABLED) == 0)
	{
		if (track->flag & TRACK_LOCKED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->pat_flag & SELECT)
				UI_ThemeColorShade(TH_LOCK_MARKER, 64);
			else UI_ThemeColor(TH_LOCK_MARKER);
		}
		else if (marker->flag & MARKER_DISABLED) {
			if (act)
				UI_ThemeColor(TH_ACT_MARKER);
			else if (track->pat_flag & SELECT)
				UI_ThemeColorShade(TH_DIS_MARKER, 128);
			else UI_ThemeColor(TH_DIS_MARKER);
		}
		else {
			if (track->pat_flag & SELECT)
				glColor3fv(scol);
			else
				glColor3fv(col);
		}

		{
			int i = 0;
			glPushMatrix();
			glEnable(GL_LINE_STIPPLE);
			for (i = 1; i < track->pyramid_levels; ++i) {
				glScalef(2.0f, 2.0f, 1.0);
			}
			/* only draw a pattern for the coarsest level */
			glBegin(GL_LINE_LOOP);
				glVertex2f(track->pat_min[0], track->pat_min[1]);
				glVertex2f(track->pat_max[0], track->pat_min[1]);
				glVertex2f(track->pat_max[0], track->pat_max[1]);
				glVertex2f(track->pat_min[0], track->pat_max[1]);
			glEnd();
			glDisable(GL_LINE_STIPPLE);
			glPopMatrix();
		}
	}

	if (tiny)
		glDisable(GL_LINE_STIPPLE);

	glPopMatrix();
}

static void draw_marker_slide_zones(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                    float marker_pos[2], int outline, int sel, int act, int width, int height)
{
	float x, y, dx, dy, patdx, patdy, searchdx, searchdy, tdx, tdy;
	int tiny = sc->flag&SC_SHOW_TINY_MARKER;
	float col[3], scol[3], px[2];

	if ((tiny && outline) || (marker->flag & MARKER_DISABLED))
		return;

	if (!TRACK_VIEW_SELECTED(sc, track) || track->flag & TRACK_LOCKED)
		return;

	track_colors(track, act, col, scol);

	if (outline) {
		glLineWidth(3.0f);
		UI_ThemeColor(TH_MARKER_OUTLINE);
	}

	glPushMatrix();
	glTranslatef(marker_pos[0], marker_pos[1], 0);

	dx = 6.0f / width / sc->zoom;
	dy = 6.0f / height / sc->zoom;

	patdx = MIN2(dx * 2.0f / 3.0f, (track->pat_max[0] - track->pat_min[0]) / 6.0f);
	patdy = MIN2(dy * 2.0f / 3.0f, (track->pat_max[1] - track->pat_min[1]) / 6.0f);

	searchdx = MIN2(dx, (track->search_max[0] - track->search_min[0]) / 6.0f);
	searchdy = MIN2(dy, (track->search_max[1] - track->search_min[1]) / 6.0f);

	px[0] = 1.0f / sc->zoom / width / sc->scale;
	px[1] = 1.0f / sc->zoom / height / sc->scale;

	if ((sc->flag & SC_SHOW_MARKER_SEARCH) && ((track->search_flag & SELECT) == sel || outline)) {
		if (!outline) {
			if (track->search_flag & SELECT)
				glColor3fv(scol);
			else
				glColor3fv(col);
		}

		/* search offset square */
		x = track->search_min[0];
		y = track->search_max[1];

		tdx = searchdx;
		tdy = searchdy;

		if (outline) {
			tdx += px[0];
			tdy += px[1];
		}

		glBegin(GL_QUADS);
			glVertex3f(x-tdx, y+tdy, 0);
			glVertex3f(x+tdx, y+tdy, 0);
			glVertex3f(x+tdx, y-tdy, 0);
			glVertex3f(x-tdx, y-tdy, 0);
		glEnd();

		/* search re-sizing triangle */
		x = track->search_max[0];
		y = track->search_min[1];

		tdx = searchdx*2.0f;
		tdy = searchdy*2.0f;

		if (outline) {
			tdx += px[0];
			tdy += px[1];
		}

		glBegin(GL_TRIANGLES);
			glVertex3f(x, y, 0);
			glVertex3f(x-tdx, y, 0);
			glVertex3f(x, y+tdy, 0);
		glEnd();
	}

	if ((sc->flag & SC_SHOW_MARKER_PATTERN) && ((track->pat_flag & SELECT)==sel || outline)) {
		if (!outline) {
			if (track->pat_flag & SELECT)
				glColor3fv(scol);
			else
				glColor3fv(col);
		}

		/* pattern offset square */
		x = track->pat_min[0];
		y = track->pat_max[1];

		tdx = patdx;
		tdy = patdy;

		if (outline) {
			tdx += px[0];
			tdy += px[1];
		}

		glBegin(GL_QUADS);
			glVertex3f(x-tdx, y+tdy, 0);
			glVertex3f(x+tdx, y+tdy, 0);
			glVertex3f(x+tdx, y-tdy, 0);
			glVertex3f(x-tdx, y-tdy, 0);
		glEnd();

		/* pattern re-sizing triangle */
		x = track->pat_max[0];
		y = track->pat_min[1];

		tdx = patdx*2.0f;
		tdy = patdy*2.0f;

		if (outline) {
			tdx += px[0];
			tdy += px[1];
		}

		glBegin(GL_TRIANGLES);
			glVertex3f(x, y, 0);
			glVertex3f(x-tdx, y, 0);
			glVertex3f(x, y+tdy, 0);
		glEnd();
	}

	glPopMatrix();

	if (outline)
		glLineWidth(1.0f);
}

static void draw_marker_texts(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker, float marker_pos[2], int act,
                              int width, int height, float zoomx, float zoomy)
{
	char str[128] = {0}, state[64] = {0};
	float dx= 0.0f, dy = 0.0f, fontsize, pos[3];
	uiStyle *style = U.uistyles.first;
	int fontid = style->widget.uifont_id;

	if (!TRACK_VIEW_SELECTED(sc, track))
		return;

	BLF_size(fontid, 11.0f, U.dpi);
	fontsize = BLF_height_max(fontid);

	if (marker->flag & MARKER_DISABLED) {
		if (act)
			UI_ThemeColor(TH_ACT_MARKER);
		else
			UI_ThemeColorShade(TH_DIS_MARKER, 128);
	}
	else {
		if (act)
			UI_ThemeColor(TH_ACT_MARKER);
		else
			UI_ThemeColor(TH_SEL_MARKER);
	}

	if ((sc->flag & SC_SHOW_MARKER_SEARCH) &&
	   ((marker->flag & MARKER_DISABLED) == 0 || (sc->flag & SC_SHOW_MARKER_PATTERN) == 0))
	{
		dx = track->search_min[0];
		dy = track->search_min[1];
	}
	else if (sc->flag & SC_SHOW_MARKER_PATTERN) {
		dx = track->pat_min[0];
		dy = track->pat_min[1];
	}

	pos[0] = (marker_pos[0] + dx) * width;
	pos[1] = (marker_pos[1] + dy) * height;
	pos[2] = 0.0f;

	mul_m4_v3(sc->stabmat, pos);

	pos[0] = pos[0]*zoomx;
	pos[1] = pos[1]*zoomy - fontsize;

	if (marker->flag & MARKER_DISABLED)
		strcpy(state, "disabled");
	else if (marker->framenr != sc->user.framenr)
		strcpy(state, "estimated");
	else if (marker->flag & MARKER_TRACKED)
		strcpy(state, "tracked");
	else
		strcpy(state, "keyframed");

	if (state[0])
		BLI_snprintf(str, sizeof(str), "%s: %s", track->name, state);
	else
		BLI_snprintf(str, sizeof(str), "%s", track->name);

	BLF_position(fontid, pos[0], pos[1], 0.0f);
	BLF_draw(fontid, str, sizeof(str));
	pos[1] -= fontsize;

	if (track->flag & TRACK_HAS_BUNDLE) {
		BLI_snprintf(str, sizeof(str), "Average error: %.3f", track->error);
		BLF_position(fontid, pos[0], pos[1], 0.0f);
		BLF_draw(fontid, str, sizeof(str));
		pos[1] -= fontsize;
	}

	if (track->flag & TRACK_LOCKED) {
		BLF_position(fontid, pos[0], pos[1], 0.0f);
		BLF_draw(fontid, "locked", 6);
	}
}

static void view2d_to_region_float(View2D *v2d, float x, float y, float *regionx, float *regiony)
{
	/* express given coordinates as proportional values */
	x = -v2d->cur.xmin / (v2d->cur.xmax - v2d->cur.xmin);
	y = -v2d->cur.ymin / (v2d->cur.ymax - v2d->cur.ymin);

	/* convert proportional distances to screen coordinates */
	*regionx = v2d->mask.xmin + x*(v2d->mask.xmax - v2d->mask.xmin);
	*regiony = v2d->mask.ymin + y*(v2d->mask.ymax - v2d->mask.ymin);
}

static void draw_tracking_tracks(SpaceClip *sc, ARegion *ar, MovieClip *clip,
                                 int width, int height, float zoomx, float zoomy)
{
	float x, y;
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_tracks(tracking);
	MovieTrackingTrack *track, *act_track;
	MovieTrackingMarker *marker;
	int framenr = sc->user.framenr;
	int undistort = sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
	float *marker_pos = NULL, *fp, *active_pos = NULL, cur_pos[2];

	/* ** find window pixel coordinates of origin ** */

	/* UI_view2d_to_region_no_clip return integer values, this could
	 * lead to 1px flickering when view is locked to selection during playbeck.
	 * to avoid this flickering, calculate base point in the same way as it happens
	 * in UI_view2d_to_region_no_clip, but do it in floats here */

	view2d_to_region_float(&ar->v2d, 0.0f, 0.0f, &x, &y);

	glPushMatrix();
	glTranslatef(x, y, 0);

	glPushMatrix();
	glScalef(zoomx, zoomy, 0);
	glMultMatrixf(sc->stabmat);
	glScalef(width, height, 0);

	act_track = BKE_tracking_active_track(tracking);

	if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
		int count = 0;

		/* count */
		track = tracksbase->first;
		while (track) {
			if ((track->flag & TRACK_HIDDEN)==0) {
				marker = BKE_tracking_get_marker(track, framenr);

				if (MARKER_VISIBLE(sc, marker))
					count++;
			}

			track = track->next;
		}

		/* undistort */
		if (count) {
			marker_pos = MEM_callocN(2*sizeof(float)*count, "draw_tracking_tracks marker_pos");

			track = tracksbase->first;
			fp = marker_pos;
			while (track) {
				if ((track->flag & TRACK_HIDDEN)==0) {
					marker = BKE_tracking_get_marker(track, framenr);

					if (MARKER_VISIBLE(sc, marker)) {
						ED_clip_point_undistorted_pos(sc, marker->pos, fp);

						if (track == act_track)
							active_pos = fp;

						fp += 2;
					}
				}

				track = track->next;
			}
		}
	}

	if (sc->flag & SC_SHOW_TRACK_PATH) {
		track = tracksbase->first;
		while (track) {
			if ((track->flag & TRACK_HIDDEN)==0)
				draw_track_path(sc, clip, track);

			track = track->next;
		}
	}

	/* markers outline and non-selected areas */
	track = tracksbase->first;
	fp = marker_pos;
	while (track) {
		if ((track->flag & TRACK_HIDDEN)==0) {
			marker = BKE_tracking_get_marker(track, framenr);

			if (MARKER_VISIBLE(sc, marker)) {
				copy_v2_v2(cur_pos, fp ? fp : marker->pos);

				draw_marker_outline(sc, track, marker, cur_pos, width, height);
				draw_marker_areas(sc, track, marker, cur_pos, width, height, 0, 0);
				draw_marker_slide_zones(sc, track, marker, cur_pos, 1, 0, 0, width, height);
				draw_marker_slide_zones(sc, track, marker, cur_pos, 0, 0, 0, width, height);

				if (fp)
					fp += 2;
			}
		}

		track = track->next;
	}

	/* selected areas only, so selection wouldn't be overlapped by
	 * non-selected areas */
	track = tracksbase->first;
	fp = marker_pos;
	while (track) {
		if ((track->flag & TRACK_HIDDEN)==0) {
			int act = track == act_track;
			marker = BKE_tracking_get_marker(track, framenr);

			if (MARKER_VISIBLE(sc, marker)) {
				if (!act) {
					copy_v2_v2(cur_pos, fp ? fp : marker->pos);

					draw_marker_areas(sc, track, marker, cur_pos, width, height, 0, 1);
					draw_marker_slide_zones(sc, track, marker, cur_pos, 0, 1, 0, width, height);
				}

				if (fp)
					fp += 2;
			}
		}

		track = track->next;
	}

	/* active marker would be displayed on top of everything else */
	if (act_track) {
		if ((act_track->flag & TRACK_HIDDEN)==0) {
			marker = BKE_tracking_get_marker(act_track, framenr);

			if (MARKER_VISIBLE(sc, marker)) {
				copy_v2_v2(cur_pos, active_pos ? active_pos : marker->pos);

				draw_marker_areas(sc, act_track, marker, cur_pos, width, height, 1, 1);
				draw_marker_slide_zones(sc, act_track, marker, cur_pos, 0, 1, 1, width, height);
			}
		}
	}

	if (sc->flag & SC_SHOW_BUNDLES) {
		MovieTrackingObject *object = BKE_tracking_active_object(tracking);
		float pos[4], vec[4], mat[4][4], aspy;

		glEnable(GL_POINT_SMOOTH);
		glPointSize(3.0f);

		aspy = 1.0f / clip->tracking.camera.pixel_aspect;
		BKE_tracking_projection_matrix(tracking, object, framenr, width, height, mat);

		track = tracksbase->first;
		while (track) {
			if ((track->flag & TRACK_HIDDEN) == 0 && track->flag & TRACK_HAS_BUNDLE) {
				marker= BKE_tracking_get_marker(track, framenr);

				if (MARKER_VISIBLE(sc, marker)) {
					float npos[2];
					copy_v4_v4(vec, track->bundle_pos);
					vec[3]=1;

					mul_v4_m4v4(pos, mat, vec);

					pos[0] = (pos[0] / (pos[3] * 2.0f) + 0.5f) * width;
					pos[1] = (pos[1] / (pos[3] * 2.0f) + 0.5f) * height * aspy;

					BKE_tracking_apply_intrinsics(tracking, pos, npos);

					if (npos[0] >= 0.0f && npos[1] >= 0.0f && npos[0] <= width && npos[1] <= height * aspy) {
						vec[0] = (marker->pos[0] + track->offset[0]) * width;
						vec[1] = (marker->pos[1] + track->offset[1]) * height * aspy;

						sub_v2_v2(vec, npos);

						if (len_v2(vec)< 3.0f)
							glColor3f(0.0f, 1.0f, 0.0f);
						else
							glColor3f(1.0f, 0.0f, 0.0f);

						glBegin(GL_POINTS);
							if (undistort)
								glVertex3f(pos[0] / width, pos[1] / (height * aspy), 0);
							else
								glVertex3f(npos[0] / width, npos[1] / (height * aspy), 0);
						glEnd();
					}
				}
			}

			track = track->next;
		}

		glPointSize(1.0f);
		glDisable(GL_POINT_SMOOTH);
	}

	glPopMatrix();

	if (sc->flag & SC_SHOW_NAMES) {
		/* scaling should be cleared before drawing texts, otherwise font would also be scaled */
		track = tracksbase->first;
		fp = marker_pos;
		while (track) {
			if ((track->flag & TRACK_HIDDEN) == 0) {
				marker = BKE_tracking_get_marker(track, framenr);

				if (MARKER_VISIBLE(sc, marker)) {
					int act = track == act_track;

					copy_v2_v2(cur_pos, fp ? fp : marker->pos);

					draw_marker_texts(sc, track, marker, cur_pos, act, width, height, zoomx, zoomy);

					if (fp)
						fp += 2;
				}
			}

			track = track->next;
		}
	}

	glPopMatrix();

	if (marker_pos)
		MEM_freeN(marker_pos);
}

static void draw_distortion(SpaceClip *sc, ARegion *ar, MovieClip *clip, int width, int height, float zoomx, float zoomy)
{
	float x, y;
	const int n = 10;
	int i, j, a;
	float pos[2], tpos[2], grid[11][11][2];
	MovieTracking *tracking = &clip->tracking;
	float aspy = 1.0f / tracking->camera.pixel_aspect;
	float dx = (float)width / n, dy = (float)height / n * aspy;

	if (sc->mode != SC_MODE_DISTORTION)
		return;

	if (!tracking->camera.focal)
		return;

	if ((sc->flag & SC_SHOW_GRID) == 0 && (sc->flag & SC_MANUAL_CALIBRATION) == 0)
		return;

	view2d_to_region_float(&ar->v2d, 0.0f, 0.0f, &x, &y);

	glPushMatrix();
	glTranslatef(x, y, 0);
	glScalef(zoomx, zoomy, 0);
	glMultMatrixf(sc->stabmat);
	glScalef(width, height, 0);

	/* grid */
	if (sc->flag & SC_SHOW_GRID) {
		float val[4][2], idx[4][2];
		float min[2], max[2];

		for (a = 0; a < 4; a++) {
			if (a < 2)
				val[a][a%2] = FLT_MAX;
			else
				val[a][a%2] = -FLT_MAX;
		}

		zero_v2(pos);
		for (i = 0; i <= n; i++) {
			for (j = 0; j <= n; j++) {
				if (i == 0 || j == 0 || i == n || j == n) {
					BKE_tracking_apply_intrinsics(tracking, pos, tpos);

					for (a = 0; a < 4; a++) {
						int ok;

						if (a<2)
							ok = tpos[a%2] < val[a][a%2];
						else
							ok= tpos[a%2] > val[a][a%2];

						if (ok) {
							copy_v2_v2(val[a], tpos);
							idx[a][0] = j;
							idx[a][1] = i;
						}
					}
				}

				pos[0] += dx;
			}

			pos[0] = 0.0f;
			pos[1] += dy;
		}

		INIT_MINMAX2(min, max);

		for (a = 0; a < 4; a++) {
			pos[0] = idx[a][0] * dx;
			pos[1] = idx[a][1] * dy;

			BKE_tracking_invert_intrinsics(tracking, pos, tpos);

			DO_MINMAX2(tpos, min, max);
		}

		copy_v2_v2(pos, min);
		dx = (max[0] - min[0]) / n;
		dy = (max[1] - min[1]) / n;

		for (i = 0; i <= n; i++) {
			for (j = 0; j <= n; j++) {
				BKE_tracking_apply_intrinsics(tracking, pos, grid[i][j]);

				grid[i][j][0] /= width;
				grid[i][j][1] /= height*aspy;

				pos[0] += dx;
			}

			pos[0] = min[0];
			pos[1] += dy;
		}

		glColor3f(1.0f, 0.0f, 0.0f);

		for (i = 0; i <= n; i++) {
			glBegin(GL_LINE_STRIP);
				for (j = 0; j <= n; j++) {
					glVertex2fv(grid[i][j]);
				}
			glEnd();
		}

		for (j = 0; j <= n; j++) {
			glBegin(GL_LINE_STRIP);
				for (i = 0; i <= n; i++) {
					glVertex2fv(grid[i][j]);
				}
			glEnd();
		}
	}

	if (sc->flag & SC_MANUAL_CALIBRATION && clip->gpd) {
		bGPDlayer *layer= clip->gpd->layers.first;

		while (layer) {
			bGPDframe *frame = layer->frames.first;

			if (layer->flag & GP_LAYER_HIDE) {
				layer = layer->next;
				continue;
			}

			glColor4fv(layer->color);
			glLineWidth(layer->thickness);
			glPointSize((float)(layer->thickness + 2));

			while (frame) {
				bGPDstroke *stroke = frame->strokes.first;

				while (stroke) {
					if (stroke->flag & GP_STROKE_2DSPACE) {
						if (stroke->totpoints > 1) {
							glBegin(GL_LINE_STRIP);
								for (i = 0; i < stroke->totpoints - 1; i++) {
									float npos[2], dpos[2], len;
									int steps;

									pos[0] = stroke->points[i].x * width;
									pos[1] = stroke->points[i].y * height * aspy;

									npos[0] = stroke->points[i+1].x * width;
									npos[1] = stroke->points[i+1].y * height * aspy;

									len = len_v2v2(pos, npos);
									steps= ceil(len/5.0f);

									/* we want to distort only long straight lines */
									if (stroke->totpoints == 2) {
										BKE_tracking_invert_intrinsics(tracking, pos, pos);
										BKE_tracking_invert_intrinsics(tracking, npos, npos);
									}

									sub_v2_v2v2(dpos, npos, pos);
									mul_v2_fl(dpos, 1.0f/steps);

									for (j = 0; j <= steps; j++) {
										BKE_tracking_apply_intrinsics(tracking, pos, tpos);
										glVertex2f(tpos[0]/width, tpos[1]/(height*aspy));

										add_v2_v2(pos, dpos);
									}
								}
							glEnd();
						}
						else if (stroke->totpoints == 1) {
							glBegin(GL_POINTS);
								glVertex2f(stroke->points[0].x, stroke->points[0].y);
							glEnd();
						}
					}

					stroke = stroke->next;
				}

				frame = frame->next;
			}

			layer = layer->next;
		}

		glLineWidth(1.0f);
		glPointSize(1.0f);
	}

	glPopMatrix();
}

void clip_draw_main(SpaceClip *sc, ARegion *ar, Scene *scene)
{
	MovieClip *clip= ED_space_clip(sc);
	ImBuf *ibuf;
	int width, height;
	float zoomx, zoomy;

	/* if no clip, nothing to do */
	if (!clip)
		return;

	ED_space_clip_size(sc, &width, &height);
	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);

	if (sc->flag & SC_SHOW_STABLE) {
		float smat[4][4], ismat[4][4];

		ibuf = ED_space_clip_get_stable_buffer(sc, sc->loc, &sc->scale, &sc->angle);

		if (ibuf) {
			float loc[2];
			float aspect = clip->tracking.camera.pixel_aspect;

			if (width != ibuf->x)
				mul_v2_v2fl(loc, sc->loc, (float)width / ibuf->x);
			else
				copy_v2_v2(loc, sc->loc);

			BKE_tracking_stabdata_to_mat4(width, height, aspect, loc, sc->scale, sc->angle, sc->stabmat);

			unit_m4(smat);
			smat[0][0] = 1.0f / width;
			smat[1][1] = 1.0f / height;
			invert_m4_m4(ismat, smat);

			mul_serie_m4(sc->unistabmat, smat, sc->stabmat, ismat, NULL, NULL, NULL, NULL, NULL);
		}
	}
	else {
		ibuf = ED_space_clip_get_buffer(sc);

		zero_v2(sc->loc);
		sc->scale = 1.0f;
		unit_m4(sc->stabmat);
		unit_m4(sc->unistabmat);
	}

	if (ibuf) {
		draw_movieclip_buffer(sc, ar, ibuf, width, height, zoomx, zoomy);
		IMB_freeImBuf(ibuf);
	}
	else {
		ED_region_grid_draw(ar, zoomx, zoomy);
	}

	if (width && height) {
		draw_tracking_tracks(sc, ar, clip, width, height, zoomx, zoomy);
		draw_distortion(sc, ar, clip, width, height, zoomx, zoomy);
	}

	draw_movieclip_cache(sc, ar, clip, scene);
	draw_movieclip_notes(sc, ar);
}

/* draw grease pencil */
void clip_draw_grease_pencil(bContext *C, int onlyv2d)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	ImBuf *ibuf;

	if (!clip)
		return;

	if (onlyv2d) {
		/* if manual calibration is used then grease pencil data is already
		 * drawed in draw_distortion */
		if ((sc->flag & SC_MANUAL_CALIBRATION)==0 || sc->mode != SC_MODE_DISTORTION) {
			ibuf = ED_space_clip_get_buffer(sc);

			if (ibuf) {
				glPushMatrix();
				glMultMatrixf(sc->unistabmat);
				draw_gpencil_2dimage(C, ibuf);

				IMB_freeImBuf(ibuf);
				glPopMatrix();
			}
		}
	}
	else {
		draw_gpencil_view2d(C, 0);
	}
}
