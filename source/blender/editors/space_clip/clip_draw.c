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

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_math_base.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "ED_screen.h"
#include "ED_clip.h"
#include "ED_mask.h"
#include "ED_gpencil.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "BLF_api.h"

#include "clip_intern.h"    // own include

/*********************** main area drawing *************************/

static void draw_keyframe(int frame, int cfra, int sfra, float framelen, int width)
{
	int height = (frame == cfra) ? 22 : 10;
	int x = (frame - sfra) * framelen;

	if (width == 1) {
		glBegin(GL_LINES);
		glVertex2i(x, 0);
		glVertex2i(x, height * UI_DPI_FAC);
		glEnd();
	}
	else {
		glRecti(x, 0, x + width, height * UI_DPI_FAC);
	}
}

static int generic_track_get_markersnr(MovieTrackingTrack *track, MovieTrackingPlaneTrack *plane_track)
{
	if (track) {
		return track->markersnr;
	}
	else if (plane_track) {
		return plane_track->markersnr;
	}

	return 0;
}

static int generic_track_get_marker_framenr(MovieTrackingTrack *track, MovieTrackingPlaneTrack *plane_track,
                                            int marker_index)
{
	if (track) {
		BLI_assert(marker_index < track->markersnr);
		return track->markers[marker_index].framenr;
	}
	else if (plane_track) {
		BLI_assert(marker_index < plane_track->markersnr);
		return plane_track->markers[marker_index].framenr;
	}

	return 0;
}

static bool generic_track_is_marker_enabled(MovieTrackingTrack *track, MovieTrackingPlaneTrack *plane_track,
                                            int marker_index)
{
	if (track) {
		BLI_assert(marker_index < track->markersnr);
		return (track->markers[marker_index].flag & MARKER_DISABLED) == 0;
	}
	else if (plane_track) {
		return true;
	}

	return false;
}

static bool generic_track_is_marker_keyframed(MovieTrackingTrack *track, MovieTrackingPlaneTrack *plane_track,
                                              int marker_index)
{
	if (track) {
		BLI_assert(marker_index < track->markersnr);
		return (track->markers[marker_index].flag & MARKER_TRACKED) == 0;
	}
	else if (plane_track) {
		BLI_assert(marker_index < plane_track->markersnr);
		return (plane_track->markers[marker_index].flag & PLANE_MARKER_TRACKED) == 0;
	}

	return false;
}

static void draw_movieclip_cache(SpaceClip *sc, ARegion *ar, MovieClip *clip, Scene *scene)
{
	float x;
	int *points, totseg, i, a;
	float sfra = SFRA, efra = EFRA, framelen = ar->winx / (efra - sfra + 1);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *act_object = BKE_tracking_object_get_active(tracking);
	MovieTrackingTrack *act_track = BKE_tracking_track_get_active(&clip->tracking);
	MovieTrackingPlaneTrack *act_plane_track = BKE_tracking_plane_track_get_active(&clip->tracking);
	MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(tracking);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* cache background */
	ED_region_cache_draw_background(ar);

	/* cached segments -- could be usefu lto debug caching strategies */
	BKE_movieclip_get_cache_segments(clip, &sc->user, &totseg, &points);
	ED_region_cache_draw_cached_segments(ar, totseg, points, sfra, efra);

	/* track */
	if (act_track || act_plane_track) {
		for (i = sfra - clip->start_frame + 1, a = 0; i <= efra - clip->start_frame + 1; i++) {
			int framenr;
			int markersnr = generic_track_get_markersnr(act_track, act_plane_track);

			while (a < markersnr) {
				int marker_framenr = generic_track_get_marker_framenr(act_track, act_plane_track, a);

				if (marker_framenr >= i)
					break;

				if (a < markersnr - 1 && generic_track_get_marker_framenr(act_track, act_plane_track, a + 1) > i)
					break;

				a++;
			}

			a = min_ii(a, markersnr - 1);

			if (generic_track_is_marker_enabled(act_track, act_plane_track, a)) {
				framenr = generic_track_get_marker_framenr(act_track, act_plane_track, a);

				if (framenr != i)
					glColor4ub(128, 128, 0, 96);
				else if (generic_track_is_marker_keyframed(act_track, act_plane_track, a))
					glColor4ub(255, 255, 0, 196);
				else
					glColor4ub(255, 255, 0, 96);

				glRecti((i - sfra + clip->start_frame - 1) * framelen, 0, (i - sfra + clip->start_frame) * framelen, 4 * UI_DPI_FAC);
			}
		}
	}

	/* failed frames */
	if (reconstruction->flag & TRACKING_RECONSTRUCTED) {
		int n = reconstruction->camnr;
		MovieReconstructedCamera *cameras = reconstruction->cameras;

		glColor4ub(255, 0, 0, 96);

		for (i = sfra, a = 0; i <= efra; i++) {
			bool ok = false;

			while (a < n) {
				if (cameras[a].framenr == i) {
					ok = true;
					break;
				}
				else if (cameras[a].framenr > i) {
					break;
				}

				a++;
			}

			if (!ok)
				glRecti((i - sfra + clip->start_frame - 1) * framelen, 0, (i - sfra + clip->start_frame) * framelen, 8 * UI_DPI_FAC);
		}
	}

	glDisable(GL_BLEND);

	/* current frame */
	x = (sc->user.framenr - sfra) / (efra - sfra + 1) * ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glRecti(x, 0, x + ceilf(framelen), 8 * UI_DPI_FAC);

	ED_region_cache_draw_curfra_label(sc->user.framenr, x, 8.0f * UI_DPI_FAC);

	/* solver keyframes */
	glColor4ub(175, 255, 0, 255);
	draw_keyframe(act_object->keyframe1 + clip->start_frame - 1, CFRA, sfra, framelen, 2);
	draw_keyframe(act_object->keyframe2 + clip->start_frame - 1, CFRA, sfra, framelen, 2);

	/* movie clip animation */
	if ((sc->mode == SC_MODE_MASKEDIT) && sc->mask_info.mask) {
		ED_mask_draw_frames(sc->mask_info.mask, ar, CFRA, sfra, efra);
	}
}

static void draw_movieclip_notes(SpaceClip *sc, ARegion *ar)
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	char str[256] = {0};
	bool block = false;

	if (tracking->stats) {
		BLI_strncpy(str, tracking->stats->message, sizeof(str));
		block = true;
	}
	else {
		if (sc->flag & SC_LOCK_SELECTION)
			strcpy(str, "Locked");
	}

	if (str[0]) {
		float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.6f};
		ED_region_info_draw(ar, str, block, fill_color);
	}
}

static void draw_movieclip_muted(ARegion *ar, int width, int height, float zoomx, float zoomy)
{
	int x, y;

	/* find window pixel coordinates of origin */
	UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x, &y);

	glColor3f(0.0f, 0.0f, 0.0f);
	glRectf(x, y, x + zoomx * width, y + zoomy * height);
}

static void draw_movieclip_buffer(const bContext *C, SpaceClip *sc, ARegion *ar, ImBuf *ibuf,
                                  int width, int height, float zoomx, float zoomy)
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	int filter = GL_LINEAR;
	int x, y;

	/* find window pixel coordinates of origin */
	UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x, &y);

	/* checkerboard for case alpha */
	if (ibuf->planes == 32) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		fdrawcheckerboard(x, y, x + zoomx * ibuf->x, y + zoomy * ibuf->y);
	}

	/* non-scaled proxy shouldn't use filtering */
	if ((clip->flag & MCLIP_USE_PROXY) == 0 ||
	    ELEM(sc->user.render_size, MCLIP_PROXY_RENDER_SIZE_FULL, MCLIP_PROXY_RENDER_SIZE_100))
	{
		filter = GL_NEAREST;
	}

	/* set zoom */
	glPixelZoom(zoomx * width / ibuf->x, zoomy * height / ibuf->y);

	glaDrawImBuf_glsl_ctx(C, ibuf, x, y, filter);

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);

	if (ibuf->planes == 32)
		glDisable(GL_BLEND);
}

static void draw_stabilization_border(SpaceClip *sc, ARegion *ar, int width, int height, float zoomx, float zoomy)
{
	int x, y;
	MovieClip *clip = ED_space_clip_get_clip(sc);

	/* find window pixel coordinates of origin */
	UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x, &y);

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
	int tiny = sc->flag & SC_SHOW_TINY_MARKER, framenr, start_frame;
	MovieTrackingMarker *marker;

	if (count == 0)
		return;

	start_frame = framenr = ED_space_clip_get_clip_frame_number(sc);

	marker = BKE_tracking_marker_get(track, framenr);
	if (marker->framenr != framenr || marker->flag & MARKER_DISABLED)
		return;

	a = count;
	i = framenr - 1;
	while (i >= framenr - count) {
		marker = BKE_tracking_marker_get(track, i);

		if (!marker || marker->flag & MARKER_DISABLED)
			break;

		if (marker->framenr == i) {
			add_v2_v2v2(path[--a], marker->pos, track->offset);
			ED_clip_point_undistorted_pos(sc, path[a], path[a]);

			if (marker->framenr == start_frame)
				curindex = a;
		}
		else {
			break;
		}

		i--;
	}

	b = count;
	i = framenr;
	while (i <= framenr + count) {
		marker = BKE_tracking_marker_get(track, i);

		if (!marker || marker->flag & MARKER_DISABLED)
			break;

		if (marker->framenr == i) {
			if (marker->framenr == start_frame)
				curindex = b;

			add_v2_v2v2(path[b++], marker->pos, track->offset);
			ED_clip_point_undistorted_pos(sc, path[b - 1], path[b - 1]);
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

static void draw_marker_outline(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                const float marker_pos[2], int width, int height)
{
	int tiny = sc->flag & SC_SHOW_TINY_MARKER;
	bool show_search = false;
	float px[2];

	UI_ThemeColor(TH_MARKER_OUTLINE);

	px[0] = 1.0f / width / sc->zoom;
	px[1] = 1.0f / height / sc->zoom;

	if ((marker->flag & MARKER_DISABLED) == 0) {
		float pos[2];
		float p[2];

		add_v2_v2v2(pos, marker->pos, track->offset);

		ED_clip_point_undistorted_pos(sc, pos, pos);

		sub_v2_v2v2(p, pos, marker_pos);

		if (isect_point_quad_v2(p, marker->pattern_corners[0], marker->pattern_corners[1],
		                        marker->pattern_corners[2], marker->pattern_corners[3]))
		{
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
			glVertex2f(pos[0] + px[0] * 2, pos[1]);
			glVertex2f(pos[0] + px[0] * 8, pos[1]);

			glVertex2f(pos[0] - px[0] * 2, pos[1]);
			glVertex2f(pos[0] - px[0] * 8, pos[1]);

			glVertex2f(pos[0], pos[1] - px[1] * 2);
			glVertex2f(pos[0], pos[1] - px[1] * 8);

			glVertex2f(pos[0], pos[1] + px[1] * 2);
			glVertex2f(pos[0], pos[1] + px[1] * 8);
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
		glVertex2fv(marker->pattern_corners[0]);
		glVertex2fv(marker->pattern_corners[1]);
		glVertex2fv(marker->pattern_corners[2]);
		glVertex2fv(marker->pattern_corners[3]);
		glEnd();
	}

	show_search = (TRACK_VIEW_SELECTED(sc, track) &&
	               ((marker->flag & MARKER_DISABLED) == 0 || (sc->flag & SC_SHOW_MARKER_PATTERN) == 0)) != 0;
	if (sc->flag & SC_SHOW_MARKER_SEARCH && show_search) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(marker->search_min[0], marker->search_min[1]);
		glVertex2f(marker->search_max[0], marker->search_min[1]);
		glVertex2f(marker->search_max[0], marker->search_max[1]);
		glVertex2f(marker->search_min[0], marker->search_max[1]);
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

static void draw_marker_areas(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                              const float marker_pos[2], int width, int height, int act, int sel)
{
	int tiny = sc->flag & SC_SHOW_TINY_MARKER;
	bool show_search = false;
	float col[3], scol[3], px[2];

	track_colors(track, act, col, scol);

	px[0] = 1.0f / width / sc->zoom;
	px[1] = 1.0f / height / sc->zoom;

	/* marker position and offset position */
	if ((track->flag & SELECT) == sel && (marker->flag & MARKER_DISABLED) == 0) {
		float pos[2], p[2];

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

		add_v2_v2v2(pos, marker->pos, track->offset);
		ED_clip_point_undistorted_pos(sc, pos, pos);

		sub_v2_v2v2(p, pos, marker_pos);

		if (isect_point_quad_v2(p, marker->pattern_corners[0], marker->pattern_corners[1],
		                        marker->pattern_corners[2], marker->pattern_corners[3]))
		{
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
			glVertex2f(pos[0] + px[0] * 3, pos[1]);
			glVertex2f(pos[0] + px[0] * 7, pos[1]);

			glVertex2f(pos[0] - px[0] * 3, pos[1]);
			glVertex2f(pos[0] - px[0] * 7, pos[1]);

			glVertex2f(pos[0], pos[1] - px[1] * 3);
			glVertex2f(pos[0], pos[1] - px[1] * 7);

			glVertex2f(pos[0], pos[1] + px[1] * 3);
			glVertex2f(pos[0], pos[1] + px[1] * 7);
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
		glVertex2fv(marker->pattern_corners[0]);
		glVertex2fv(marker->pattern_corners[1]);
		glVertex2fv(marker->pattern_corners[2]);
		glVertex2fv(marker->pattern_corners[3]);
		glEnd();
	}

	/* search */
	show_search = (TRACK_VIEW_SELECTED(sc, track) &&
	               ((marker->flag & MARKER_DISABLED) == 0 || (sc->flag & SC_SHOW_MARKER_PATTERN) == 0)) != 0;
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
		glVertex2f(marker->search_min[0], marker->search_min[1]);
		glVertex2f(marker->search_max[0], marker->search_min[1]);
		glVertex2f(marker->search_max[0], marker->search_max[1]);
		glVertex2f(marker->search_min[0], marker->search_max[1]);
		glEnd();
	}

	if (tiny)
		glDisable(GL_LINE_STIPPLE);

	glPopMatrix();
}

static float get_shortest_pattern_side(MovieTrackingMarker *marker)
{
	int i, next;
	float len_sq = FLT_MAX;

	for (i = 0; i < 4; i++) {
		float cur_len;

		next = (i + 1) % 4;

		cur_len = len_squared_v2v2(marker->pattern_corners[i], marker->pattern_corners[next]);

		len_sq = min_ff(cur_len, len_sq);
	}

	return sqrtf(len_sq);
}

static void draw_marker_slide_square(float x, float y, float dx, float dy, int outline, float px[2])
{
	float tdx, tdy;

	tdx = dx;
	tdy = dy;

	if (outline) {
		tdx += px[0];
		tdy += px[1];
	}

	glBegin(GL_QUADS);
	glVertex3f(x - tdx, y + tdy, 0.0f);
	glVertex3f(x + tdx, y + tdy, 0.0f);
	glVertex3f(x + tdx, y - tdy, 0.0f);
	glVertex3f(x - tdx, y - tdy, 0.0f);
	glEnd();
}

static void draw_marker_slide_triangle(float x, float y, float dx, float dy, int outline, float px[2])
{
	float tdx, tdy;

	tdx = dx * 2.0f;
	tdy = dy * 2.0f;

	if (outline) {
		tdx += px[0];
		tdy += px[1];
	}

	glBegin(GL_TRIANGLES);
	glVertex3f(x,       y,       0.0f);
	glVertex3f(x - tdx, y,       0.0f);
	glVertex3f(x,       y + tdy, 0.0f);
	glEnd();
}

static void draw_marker_slide_zones(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                                    const float marker_pos[2], int outline, int sel, int act, int width, int height)
{
	float dx, dy, patdx, patdy, searchdx, searchdy;
	int tiny = sc->flag & SC_SHOW_TINY_MARKER;
	float col[3], scol[3], px[2], side;

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

	side = get_shortest_pattern_side(marker);
	patdx = min_ff(dx * 2.0f / 3.0f, side / 6.0f) * UI_DPI_FAC;
	patdy = min_ff(dy * 2.0f / 3.0f, side * width / height / 6.0f) * UI_DPI_FAC;

	searchdx = min_ff(dx, (marker->search_max[0] - marker->search_min[0]) / 6.0f) * UI_DPI_FAC;
	searchdy = min_ff(dy, (marker->search_max[1] - marker->search_min[1]) / 6.0f) * UI_DPI_FAC;

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
		draw_marker_slide_square(marker->search_min[0], marker->search_max[1], searchdx, searchdy, outline, px);

		/* search re-sizing triangle */
		draw_marker_slide_triangle(marker->search_max[0], marker->search_min[1], searchdx, searchdy, outline, px);
	}

	if ((sc->flag & SC_SHOW_MARKER_PATTERN) && ((track->pat_flag & SELECT) == sel || outline)) {
		int i;
		float pat_min[2], pat_max[2];
/*		float dx = 12.0f / width, dy = 12.0f / height;*/ /* XXX UNUSED */
		float tilt_ctrl[2];

		if (!outline) {
			if (track->pat_flag & SELECT)
				glColor3fv(scol);
			else
				glColor3fv(col);
		}

		/* pattern's corners sliding squares */
		for (i = 0; i < 4; i++) {
			draw_marker_slide_square(marker->pattern_corners[i][0], marker->pattern_corners[i][1],
			                         patdx / 1.5f, patdy / 1.5f, outline, px);
		}

		/* ** sliders to control overall pattern  ** */
		add_v2_v2v2(tilt_ctrl, marker->pattern_corners[1], marker->pattern_corners[2]);

		BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

		glEnable(GL_LINE_STIPPLE);
		glLineStipple(3, 0xaaaa);

#if 0
		/* TODO: disable for now, needs better approach visualizing this */

		glBegin(GL_LINE_LOOP);
		glVertex2f(pat_min[0] - dx, pat_min[1] - dy);
		glVertex2f(pat_max[0] + dx, pat_min[1] - dy);
		glVertex2f(pat_max[0] + dx, pat_max[1] + dy);
		glVertex2f(pat_min[0] - dx, pat_max[1] + dy);
		glEnd();

		/* marker's offset slider */
		draw_marker_slide_square(pat_min[0] - dx, pat_max[1] + dy, patdx, patdy, outline, px);

		/* pattern re-sizing triangle */
		draw_marker_slide_triangle(pat_max[0] + dx, pat_min[1] - dy, patdx, patdy, outline, px);
#endif

		glBegin(GL_LINES);
		glVertex2f(0.0f, 0.0f);
		glVertex2fv(tilt_ctrl);
		glEnd();

		glDisable(GL_LINE_STIPPLE);


		/* slider to control pattern tilt */
		draw_marker_slide_square(tilt_ctrl[0], tilt_ctrl[1], patdx, patdy, outline, px);
	}

	glPopMatrix();

	if (outline)
		glLineWidth(1.0f);
}

static void draw_marker_texts(SpaceClip *sc, MovieTrackingTrack *track, MovieTrackingMarker *marker,
                              const float marker_pos[2], int act, int width, int height, float zoomx, float zoomy)
{
	char str[128] = {0}, state[64] = {0};
	float dx = 0.0f, dy = 0.0f, fontsize, pos[3];
	uiStyle *style = U.uistyles.first;
	int fontid = style->widget.uifont_id;

	if (!TRACK_VIEW_SELECTED(sc, track))
		return;

	BLF_size(fontid, 11.0f * U.pixelsize, U.dpi);
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
		dx = marker->search_min[0];
		dy = marker->search_min[1];
	}
	else if (sc->flag & SC_SHOW_MARKER_PATTERN) {
		float pat_min[2], pat_max[2];

		BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);
		dx = pat_min[0];
		dy = pat_min[1];
	}

	pos[0] = (marker_pos[0] + dx) * width;
	pos[1] = (marker_pos[1] + dy) * height;
	pos[2] = 0.0f;

	mul_m4_v3(sc->stabmat, pos);

	pos[0] = pos[0] * zoomx;
	pos[1] = pos[1] * zoomy - fontsize;

	if (marker->flag & MARKER_DISABLED)
		strcpy(state, "disabled");
	else if (marker->framenr != ED_space_clip_get_clip_frame_number(sc))
		strcpy(state, "estimated");
	else if (marker->flag & MARKER_TRACKED)
		strcpy(state, "tracked");
	else
		strcpy(state, "keyframed");

	if (state[0])
		BLI_snprintf(str, sizeof(str), "%s: %s", track->name, state);
	else
		BLI_strncpy(str, track->name, sizeof(str));

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

static void plane_track_colors(bool is_active, float color[3], float selected_color[3])
{
	UI_GetThemeColor3fv(TH_MARKER, color);

	if (is_active)
		UI_GetThemeColor3fv(TH_ACT_MARKER, selected_color);
	else
		UI_GetThemeColor3fv(TH_SEL_MARKER, selected_color);
}

static void getArrowEndPoint(const int width, const int height, const float zoom,
                             const float start_corner[2], const float end_corner[2],
                             float end_point[2])
{
	float direction[2];
	float max_length;

	sub_v2_v2v2(direction, end_corner, start_corner);

	direction[0] *= width;
	direction[1] *= height;
	max_length = normalize_v2(direction);
	mul_v2_fl(direction, min_ff(32.0f / zoom, max_length));
	direction[0] /= width;
	direction[1] /= height;

	add_v2_v2v2(end_point, start_corner, direction);
}

static void homogeneous_2d_to_gl_matrix(/*const*/ float matrix[3][3],
                                        float gl_matrix[4][4])
{
	gl_matrix[0][0] = matrix[0][0];
	gl_matrix[0][1] = matrix[0][1];
	gl_matrix[0][2] = 0.0f;
	gl_matrix[0][3] = matrix[0][2];

	gl_matrix[1][0] = matrix[1][0];
	gl_matrix[1][1] = matrix[1][1];
	gl_matrix[1][2] = 0.0f;
	gl_matrix[1][3] = matrix[1][2];

	gl_matrix[2][0] = 0.0f;
	gl_matrix[2][1] = 0.0f;
	gl_matrix[2][2] = 1.0f;
	gl_matrix[2][3] = 0.0f;

	gl_matrix[3][0] = matrix[2][0];
	gl_matrix[3][1] = matrix[2][1];
	gl_matrix[3][2] = 0.0f;
	gl_matrix[3][3] = matrix[2][2];
}

static void draw_plane_marker_image(Scene *scene,
                                    MovieTrackingPlaneTrack *plane_track,
                                    MovieTrackingPlaneMarker *plane_marker)
{
	Image *image = plane_track->image;
	ImBuf *ibuf;
	void *lock;

	if (image == NULL) {
		return;
	}

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

	if (ibuf) {
		unsigned char *display_buffer;
		void *cache_handle;

		if (image->flag & IMA_VIEW_AS_RENDER) {
			display_buffer = IMB_display_buffer_acquire(ibuf,
			                                            &scene->view_settings,
			                                            &scene->display_settings,
			                                            &cache_handle);
		}
		else {
			display_buffer = IMB_display_buffer_acquire(ibuf, NULL,
			                                            &scene->display_settings,
			                                            &cache_handle);
		}

		if (display_buffer) {
			GLuint texid, last_texid;
			float frame_corners[4][2] = {{0.0f, 0.0f},
			                             {1.0f, 0.0f},
			                             {1.0f, 1.0f},
			                             {0.0f, 1.0f}};
			float perspective_matrix[3][3];
			float gl_matrix[4][4];
			bool transparent = false;
			BKE_tracking_homography_between_two_quads(frame_corners,
			                                          plane_marker->corners,
			                                          perspective_matrix);

			homogeneous_2d_to_gl_matrix(perspective_matrix, gl_matrix);

			if (plane_track->image_opacity != 1.0f || ibuf->planes == 32) {
				transparent = true;
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);
			}

			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1.0, 1.0, 1.0, plane_track->image_opacity);

			last_texid = glaGetOneInteger(GL_TEXTURE_2D);
			glEnable(GL_TEXTURE_2D);
			glGenTextures(1, (GLuint *)&texid);

			glBindTexture(GL_TEXTURE_2D, texid);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ibuf->x, ibuf->y, 0, GL_RGBA,
			             GL_UNSIGNED_BYTE, display_buffer);

			glPushMatrix();
			glMultMatrixf(gl_matrix);

			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 0.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1.0f);
			glEnd();

			glPopMatrix();

			glBindTexture(GL_TEXTURE_2D, last_texid);
			glDisable(GL_TEXTURE_2D);

			if (transparent) {
				glDisable(GL_BLEND);
			}
		}

		IMB_display_buffer_release(cache_handle);
	}

	BKE_image_release_ibuf(image, ibuf, lock);
}

static void draw_plane_marker_ex(SpaceClip *sc, Scene *scene, MovieTrackingPlaneTrack *plane_track,
                                 MovieTrackingPlaneMarker *plane_marker, bool is_active_track,
                                 bool draw_outline, int width, int height)
{
	bool tiny = (sc->flag & SC_SHOW_TINY_MARKER) != 0;
	bool is_selected_track = plane_track->flag & SELECT;
	bool draw_plane_quad = plane_track->image == NULL || plane_track->image_opacity == 0.0f;
	float px[2];

	if (draw_outline) {
		UI_ThemeColor(TH_MARKER_OUTLINE);
	}
	else {
		float color[3], selected_color[3];
		plane_track_colors(is_active_track, color, selected_color);
		if (is_selected_track) {
			glColor3fv(selected_color);
		}
		else {
			glColor3fv(color);
		}
	}

	px[0] = 1.0f / width / sc->zoom;
	px[1] = 1.0f / height / sc->zoom;

	/* Draw image */
	if (draw_outline == false) {
		draw_plane_marker_image(scene, plane_track, plane_marker);
	}

	if (draw_outline) {
		if (!tiny) {
			glLineWidth(3.0f);
		}
	}
	else if (tiny) {
		glLineStipple(3, 0xaaaa);
		glEnable(GL_LINE_STIPPLE);
	}

	if (draw_plane_quad) {
		/* Draw rectangle itself. */
		glBegin(GL_LINE_LOOP);
		glVertex2fv(plane_marker->corners[0]);
		glVertex2fv(plane_marker->corners[1]);
		glVertex2fv(plane_marker->corners[2]);
		glVertex2fv(plane_marker->corners[3]);
		glEnd();

		/* Draw axis. */
		if (!draw_outline) {
			float end_point[2];
			glPushAttrib(GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);

			getArrowEndPoint(width, height, sc->zoom, plane_marker->corners[0], plane_marker->corners[1], end_point);
			glColor3f(1.0, 0.0, 0.0f);
			glBegin(GL_LINES);
			glVertex2fv(plane_marker->corners[0]);
			glVertex2fv(end_point);
			glEnd();

			getArrowEndPoint(width, height, sc->zoom, plane_marker->corners[0], plane_marker->corners[3], end_point);
			glColor3f(0.0, 1.0, 0.0f);
			glBegin(GL_LINES);
			glVertex2fv(plane_marker->corners[0]);
			glVertex2fv(end_point);
			glEnd();

			glPopAttrib();
		}
	}

	/* Draw sliders. */
	if (is_selected_track) {
		int i;
		for (i = 0; i < 4; i++) {
			draw_marker_slide_square(plane_marker->corners[i][0], plane_marker->corners[i][1],
			                         3.0f * px[0], 3.0f * px[1], draw_outline, px);
		}
	}

	if (draw_outline) {
		if (!tiny) {
			glLineWidth(1.0f);
		}
	}
	else if (tiny) {
		glDisable(GL_LINE_STIPPLE);
	}
}

static void draw_plane_marker_outline(SpaceClip *sc, Scene *scene, MovieTrackingPlaneTrack *plane_track,
                                      MovieTrackingPlaneMarker *plane_marker, int width, int height)
{
	draw_plane_marker_ex(sc, scene, plane_track, plane_marker, false, true, width, height);
}

static void draw_plane_marker(SpaceClip *sc, Scene *scene, MovieTrackingPlaneTrack *plane_track,
                              MovieTrackingPlaneMarker *plane_marker, bool is_active_track,
                              int width, int height)
{
	draw_plane_marker_ex(sc, scene, plane_track, plane_marker, is_active_track, false, width, height);
}

static void draw_plane_track(SpaceClip *sc, Scene *scene, MovieTrackingPlaneTrack *plane_track,
                             int framenr, bool is_active_track, int width, int height)
{
	MovieTrackingPlaneMarker *plane_marker;

	plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

	draw_plane_marker_outline(sc, scene, plane_track, plane_marker, width, height);
	draw_plane_marker(sc, scene, plane_track, plane_marker, is_active_track, width, height);
}

/* Draw all kind of tracks. */
static void draw_tracking_tracks(SpaceClip *sc, Scene *scene, ARegion *ar, MovieClip *clip,
                                 int width, int height, float zoomx, float zoomy)
{
	float x, y;
	MovieTracking *tracking = &clip->tracking;
	ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
	ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(tracking);
	MovieTrackingTrack *track, *act_track;
	MovieTrackingPlaneTrack *plane_track, *active_plane_track;
	MovieTrackingMarker *marker;
	int framenr = ED_space_clip_get_clip_frame_number(sc);
	int undistort = sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
	float *marker_pos = NULL, *fp, *active_pos = NULL, cur_pos[2];

	/* ** find window pixel coordinates of origin ** */

	/* UI_view2d_view_to_region_no_clip return integer values, this could
	 * lead to 1px flickering when view is locked to selection during playbeck.
	 * to avoid this flickering, calculate base point in the same way as it happens
	 * in UI_view2d_view_to_region_no_clip, but do it in floats here */

	UI_view2d_view_to_region_fl(&ar->v2d, 0.0f, 0.0f, &x, &y);

	glPushMatrix();
	glTranslatef(x, y, 0);

	glPushMatrix();
	glScalef(zoomx, zoomy, 0);
	glMultMatrixf(sc->stabmat);
	glScalef(width, height, 0);

	act_track = BKE_tracking_track_get_active(tracking);

	/* Draw plane tracks */
	active_plane_track = BKE_tracking_plane_track_get_active(tracking);
	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		if ((plane_track->flag & PLANE_TRACK_HIDDEN) == 0) {
			draw_plane_track(sc, scene, plane_track, framenr, plane_track == active_plane_track, width, height);
		}
	}

	if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
		int count = 0;

		/* count */
		track = tracksbase->first;
		while (track) {
			if ((track->flag & TRACK_HIDDEN) == 0) {
				marker = BKE_tracking_marker_get(track, framenr);

				if (MARKER_VISIBLE(sc, track, marker))
					count++;
			}

			track = track->next;
		}

		/* undistort */
		if (count) {
			marker_pos = MEM_callocN(2 * sizeof(float) * count, "draw_tracking_tracks marker_pos");

			track = tracksbase->first;
			fp = marker_pos;
			while (track) {
				if ((track->flag & TRACK_HIDDEN) == 0) {
					marker = BKE_tracking_marker_get(track, framenr);

					if (MARKER_VISIBLE(sc, track, marker)) {
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
			if ((track->flag & TRACK_HIDDEN) == 0)
				draw_track_path(sc, clip, track);

			track = track->next;
		}
	}

	/* markers outline and non-selected areas */
	track = tracksbase->first;
	fp = marker_pos;
	while (track) {
		if ((track->flag & TRACK_HIDDEN) == 0) {
			marker = BKE_tracking_marker_get(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
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
		if ((track->flag & TRACK_HIDDEN) == 0) {
			int act = track == act_track;
			marker = BKE_tracking_marker_get(track, framenr);

			if (MARKER_VISIBLE(sc, track, marker)) {
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
		if ((act_track->flag & TRACK_HIDDEN) == 0) {
			marker = BKE_tracking_marker_get(act_track, framenr);

			if (MARKER_VISIBLE(sc, act_track, marker)) {
				copy_v2_v2(cur_pos, active_pos ? active_pos : marker->pos);

				draw_marker_areas(sc, act_track, marker, cur_pos, width, height, 1, 1);
				draw_marker_slide_zones(sc, act_track, marker, cur_pos, 0, 1, 1, width, height);
			}
		}
	}

	if (sc->flag & SC_SHOW_BUNDLES) {
		MovieTrackingObject *object = BKE_tracking_object_get_active(tracking);
		float pos[4], vec[4], mat[4][4], aspy;

		glEnable(GL_POINT_SMOOTH);
		glPointSize(3.0f);

		aspy = 1.0f / clip->tracking.camera.pixel_aspect;
		BKE_tracking_get_projection_matrix(tracking, object, framenr, width, height, mat);

		track = tracksbase->first;
		while (track) {
			if ((track->flag & TRACK_HIDDEN) == 0 && track->flag & TRACK_HAS_BUNDLE) {
				marker = BKE_tracking_marker_get(track, framenr);

				if (MARKER_VISIBLE(sc, track, marker)) {
					float npos[2];
					copy_v3_v3(vec, track->bundle_pos);
					vec[3] = 1;

					mul_v4_m4v4(pos, mat, vec);

					pos[0] = (pos[0] / (pos[3] * 2.0f) + 0.5f) * width;
					pos[1] = (pos[1] / (pos[3] * 2.0f) + 0.5f) * height * aspy;

					BKE_tracking_distort_v2(tracking, pos, npos);

					if (npos[0] >= 0.0f && npos[1] >= 0.0f && npos[0] <= width && npos[1] <= height * aspy) {
						vec[0] = (marker->pos[0] + track->offset[0]) * width;
						vec[1] = (marker->pos[1] + track->offset[1]) * height * aspy;

						sub_v2_v2(vec, npos);

						if (len_squared_v2(vec) < (3.0f * 3.0f))
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
				marker = BKE_tracking_marker_get(track, framenr);

				if (MARKER_VISIBLE(sc, track, marker)) {
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

static void draw_distortion(SpaceClip *sc, ARegion *ar, MovieClip *clip,
                            int width, int height, float zoomx, float zoomy)
{
	float x, y;
	const int n = 10;
	int i, j, a;
	float pos[2], tpos[2], grid[11][11][2];
	MovieTracking *tracking = &clip->tracking;
	bGPdata *gpd = NULL;
	float aspy = 1.0f / tracking->camera.pixel_aspect;
	float dx = (float)width / n, dy = (float)height / n * aspy;
	float offsx = 0.0f, offsy = 0.0f;

	if (!tracking->camera.focal)
		return;

	if ((sc->flag & SC_SHOW_GRID) == 0 && (sc->flag & SC_MANUAL_CALIBRATION) == 0)
		return;

	UI_view2d_view_to_region_fl(&ar->v2d, 0.0f, 0.0f, &x, &y);

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
				val[a][a % 2] = FLT_MAX;
			else
				val[a][a % 2] = -FLT_MAX;
		}

		zero_v2(pos);
		for (i = 0; i <= n; i++) {
			for (j = 0; j <= n; j++) {
				if (i == 0 || j == 0 || i == n || j == n) {
					BKE_tracking_distort_v2(tracking, pos, tpos);

					for (a = 0; a < 4; a++) {
						int ok;

						if (a < 2)
							ok = tpos[a % 2] < val[a][a % 2];
						else
							ok = tpos[a % 2] > val[a][a % 2];

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

			BKE_tracking_undistort_v2(tracking, pos, tpos);

			minmax_v2v2_v2(min, max, tpos);
		}

		copy_v2_v2(pos, min);
		dx = (max[0] - min[0]) / n;
		dy = (max[1] - min[1]) / n;

		for (i = 0; i <= n; i++) {
			for (j = 0; j <= n; j++) {
				BKE_tracking_distort_v2(tracking, pos, grid[i][j]);

				grid[i][j][0] /= width;
				grid[i][j][1] /= height * aspy;

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

	if (sc->gpencil_src != SC_GPENCIL_SRC_TRACK) {
		gpd = clip->gpd;
	}

	if (sc->flag & SC_MANUAL_CALIBRATION && gpd) {
		bGPDlayer *layer = gpd->layers.first;

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

								pos[0] = (stroke->points[i].x + offsx) * width;
								pos[1] = (stroke->points[i].y + offsy) * height * aspy;

								npos[0] = (stroke->points[i + 1].x + offsx) * width;
								npos[1] = (stroke->points[i + 1].y + offsy) * height * aspy;

								len = len_v2v2(pos, npos);
								steps = ceil(len / 5.0f);

								/* we want to distort only long straight lines */
								if (stroke->totpoints == 2) {
									BKE_tracking_undistort_v2(tracking, pos, pos);
									BKE_tracking_undistort_v2(tracking, npos, npos);
								}

								sub_v2_v2v2(dpos, npos, pos);
								mul_v2_fl(dpos, 1.0f / steps);

								for (j = 0; j <= steps; j++) {
									BKE_tracking_distort_v2(tracking, pos, tpos);
									glVertex2f(tpos[0] / width, tpos[1] / (height * aspy));

									add_v2_v2(pos, dpos);
								}
							}
							glEnd();
						}
						else if (stroke->totpoints == 1) {
							glBegin(GL_POINTS);
							glVertex2f(stroke->points[0].x + offsx, stroke->points[0].y + offsy);
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

void clip_draw_main(const bContext *C, SpaceClip *sc, ARegion *ar)
{
	MovieClip *clip = ED_space_clip_get_clip(sc);
	Scene *scene = CTX_data_scene(C);
	ImBuf *ibuf = NULL;
	int width, height;
	float zoomx, zoomy;

	ED_space_clip_get_size(sc, &width, &height);
	ED_space_clip_get_zoom(sc, ar, &zoomx, &zoomy);

	/* if no clip, nothing to do */
	if (!clip) {
		ED_region_grid_draw(ar, zoomx, zoomy);
		return;
	}

	if (sc->flag & SC_SHOW_STABLE) {
		float smat[4][4], ismat[4][4];

		ibuf = ED_space_clip_get_stable_buffer(sc, sc->loc, &sc->scale, &sc->angle);

		if (ibuf) {
			float translation[2];
			float aspect = clip->tracking.camera.pixel_aspect;

			if (width != ibuf->x)
				mul_v2_v2fl(translation, sc->loc, (float)width / ibuf->x);
			else
				copy_v2_v2(translation, sc->loc);

			BKE_tracking_stabilization_data_to_mat4(width, height, aspect,
			                                        translation, sc->scale, sc->angle, sc->stabmat);

			unit_m4(smat);
			smat[0][0] = 1.0f / width;
			smat[1][1] = 1.0f / height;
			invert_m4_m4(ismat, smat);

			mul_m4_series(sc->unistabmat, smat, sc->stabmat, ismat);
		}
	}
	else if ((sc->flag & SC_MUTE_FOOTAGE) == 0) {
		ibuf = ED_space_clip_get_buffer(sc);

		zero_v2(sc->loc);
		sc->scale = 1.0f;
		unit_m4(sc->stabmat);
		unit_m4(sc->unistabmat);
	}

	if (ibuf) {
		draw_movieclip_buffer(C, sc, ar, ibuf, width, height, zoomx, zoomy);
		IMB_freeImBuf(ibuf);
	}
	else if (sc->flag & SC_MUTE_FOOTAGE) {
		draw_movieclip_muted(ar, width, height, zoomx, zoomy);
	}
	else {
		ED_region_grid_draw(ar, zoomx, zoomy);
	}

	if (width && height) {
		draw_stabilization_border(sc, ar, width, height, zoomx, zoomy);
		draw_tracking_tracks(sc, scene, ar, clip, width, height, zoomx, zoomy);
		draw_distortion(sc, ar, clip, width, height, zoomx, zoomy);
	}
}

void clip_draw_cache_and_notes(const bContext *C, SpaceClip *sc, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);
	if (clip) {
		draw_movieclip_cache(sc, ar, clip, scene);
		draw_movieclip_notes(sc, ar);
	}
}

/* draw grease pencil */
void clip_draw_grease_pencil(bContext *C, int onlyv2d)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip_get_clip(sc);

	if (!clip)
		return;

	if (onlyv2d) {
		bool is_track_source = sc->gpencil_src == SC_GPENCIL_SRC_TRACK;
		/* if manual calibration is used then grease pencil data
		 * associated with the clip is already drawn in draw_distortion
		 */
		if ((sc->flag & SC_MANUAL_CALIBRATION) == 0 || is_track_source) {
			glPushMatrix();
			glMultMatrixf(sc->unistabmat);

			if (is_track_source) {
				MovieTrackingTrack *track = BKE_tracking_track_get_active(&sc->clip->tracking);

				if (track) {
					int framenr = ED_space_clip_get_clip_frame_number(sc);
					MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

					glTranslatef(marker->pos[0], marker->pos[1], 0.0f);
				}
			}

			ED_gpencil_draw_2dimage(C);

			glPopMatrix();
		}
	}
	else {
		ED_gpencil_draw_view2d(C, 0);
	}
}
