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

#include "MEM_guardedalloc.h"

#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_context.h"
#include "BKE_tracking.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"	/* SELECT */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "GPU_extensions.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "clip_intern.h"	// own include

/* ******** operactor poll functions ******** */

int ED_space_clip_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc && sc->clip)
		return TRUE;

	return FALSE;
}

int ED_space_clip_view_clip_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc && sc->clip) {
		return sc->view == SC_VIEW_CLIP;
	}

	return FALSE;
}

int ED_space_clip_tracking_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc && sc->clip)
		return ED_space_clip_show_trackedit(sc);

	return FALSE;
}

int ED_space_clip_tracking_size_poll(bContext *C)
{
	if (ED_space_clip_tracking_poll(C)) {
		MovieClip *clip = CTX_data_edit_movieclip(C);

		if (clip) {
			SpaceClip *sc = CTX_wm_space_clip(C);
			int width, height;

			BKE_movieclip_get_size(clip, &sc->user, &width, &height);

			return width > 0 && height > 0;
		}
	}

	return FALSE;
}

int ED_space_clip_tracking_frame_poll(bContext *C)
{
	if (ED_space_clip_tracking_poll(C)) {
		MovieClip *clip = CTX_data_edit_movieclip(C);

		if (clip) {
			SpaceClip *sc = CTX_wm_space_clip(C);

			return BKE_movieclip_has_frame(clip, &sc->user);
		}
	}

	return FALSE;
}

int ED_space_clip_maskediting_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc && sc->clip) {
		return ED_space_clip_show_maskedit(sc);
	}

	return FALSE;
}

int ED_space_clip_maskediting_mask_poll(bContext *C)
{
	if (ED_space_clip_maskediting_poll(C)) {
		MovieClip *clip = CTX_data_edit_movieclip(C);

		if (clip) {
			SpaceClip *sc= CTX_wm_space_clip(C);

			return sc->mask != NULL;
		}
	}

	return FALSE;
}

/* ******** editing functions ******** */

void ED_space_clip_set(bContext *C, bScreen *screen, SpaceClip *sc, MovieClip *clip)
{
	MovieClip *old_clip;

	if (!screen && C)
		screen = CTX_wm_screen(C);

	old_clip = sc->clip;
	sc->clip = clip;

	if (sc->clip && sc->clip->id.us == 0)
		sc->clip->id.us = 1;

	if (screen && sc->view == SC_VIEW_CLIP) {
		ScrArea *area;
		SpaceLink *sl;

		for (area = screen->areabase.first; area; area = area->next) {
			for (sl = area->spacedata.first; sl; sl = sl->next) {
				if (sl->spacetype == SPACE_CLIP) {
					SpaceClip *cur_sc = (SpaceClip *) sl;

					if (cur_sc != sc && cur_sc->view != SC_VIEW_CLIP) {
						if (cur_sc->clip == old_clip || cur_sc->clip == NULL) {
							cur_sc->clip = clip;
						}
					}
				}
			}
		}
	}

	if (C)
		WM_event_add_notifier(C, NC_MOVIECLIP | NA_SELECTED, sc->clip);
}

MovieClip *ED_space_clip(SpaceClip *sc)
{
	return sc->clip;
}

Mask *ED_space_clip_mask(SpaceClip *sc)
{
	return sc->mask;
}

ImBuf *ED_space_clip_get_buffer(SpaceClip *sc)
{
	if (sc->clip) {
		ImBuf *ibuf;

		ibuf = BKE_movieclip_get_postprocessed_ibuf(sc->clip, &sc->user, sc->postproc_flag);

		if (ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;

		if (ibuf)
			IMB_freeImBuf(ibuf);
	}

	return NULL;
}

ImBuf *ED_space_clip_get_stable_buffer(SpaceClip *sc, float loc[2], float *scale, float *angle)
{
	if (sc->clip) {
		ImBuf *ibuf;

		ibuf = BKE_movieclip_get_stable_ibuf(sc->clip, &sc->user, loc, scale, angle, sc->postproc_flag);

		if (ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;

		if (ibuf)
			IMB_freeImBuf(ibuf);
	}

	return NULL;
}

void ED_space_clip_size(SpaceClip *sc, int *width, int *height)
{
	if (!sc->clip) {
		*width = *height = 0;
	}
	else {
		BKE_movieclip_get_size(sc->clip, &sc->user, width, height);
	}
}

void ED_space_clip_mask_size(SpaceClip *sc, int *width, int *height)
{
	/* quite the same as ED_space_clip_size, but it also runs aspect correction on output resolution
	 * this is needed because mask should be rasterized with exactly the same resolution as
	 * currently displaying frame and it doesn't have access to aspect correction currently
	 * used for display. (sergey)
	 */

	if (!sc->mask) {
		*width = 0;
		*height = 0;
	} else {
		float aspx, aspy;

		ED_space_clip_size(sc, width, height);
		ED_space_clip_aspect(sc, &aspx, &aspy);

		*width *= aspx;
		*height *= aspy;
	}
}

void ED_space_clip_mask_aspect(SpaceClip *sc, float *aspx, float *aspy)
{
	int w, h;

	ED_space_clip_aspect(sc, aspx, aspy);
	ED_space_clip_size(sc, &w, &h);

	/* now this is not accounted for! */
#if 0
	*aspx *= (float)w;
	*aspy *= (float)h;
#endif

	if(*aspx < *aspy) {
		*aspy= *aspy / *aspx;
		*aspx= 1.0f;
	}
	else {
		*aspx= *aspx / *aspy;
		*aspy= 1.0f;
	}
}

void ED_space_clip_zoom(SpaceClip *sc, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	ED_space_clip_size(sc, &width, &height);

	*zoomx = (float)(ar->winrct.xmax - ar->winrct.xmin + 1) / (float)((ar->v2d.cur.xmax - ar->v2d.cur.xmin) * width);
	*zoomy = (float)(ar->winrct.ymax - ar->winrct.ymin + 1) / (float)((ar->v2d.cur.ymax - ar->v2d.cur.ymin) * height);
}

void ED_space_clip_aspect(SpaceClip *sc, float *aspx, float *aspy)
{
	MovieClip *clip = ED_space_clip(sc);

	if (clip)
		BKE_movieclip_aspect(clip, aspx, aspy);
	else
		*aspx = *aspy = 1.0f;
}

void ED_space_clip_aspect_dimension_aware(SpaceClip *sc, float *aspx, float *aspy)
{
	int w, h;

	/* most of tools does not require aspect to be returned with dimensions correction
	 * due to they're invariant to this stuff, but some transformation tools like rotation
	 * should be aware of aspect correction caused by different resolution in different
	 * directions.
	 * mainly this is sued for transformation stuff
	 */

	ED_space_clip_aspect(sc, aspx, aspy);
	ED_space_clip_size(sc, &w, &h);

	*aspx *= (float)w;
	*aspy *= (float)h;

	if(*aspx < *aspy) {
		*aspy= *aspy / *aspx;
		*aspx= 1.0f;
	}
	else {
		*aspx= *aspx / *aspy;
		*aspy= 1.0f;
	}
}

void ED_clip_update_frame(const Main *mainp, int cfra)
{
	wmWindowManager *wm;
	wmWindow *win;

	/* image window, compo node users */
	for (wm = mainp->wm.first; wm; wm = wm->id.next) { /* only 1 wm */
		for (win = wm->windows.first; win; win = win->next) {
			ScrArea *sa;

			for (sa = win->screen->areabase.first; sa; sa = sa->next) {
				if (sa->spacetype == SPACE_CLIP) {
					SpaceClip *sc = sa->spacedata.first;

					sc->scopes.ok = FALSE;

					BKE_movieclip_user_set_frame(&sc->user, cfra);
				}
			}
		}
	}
}

static int selected_boundbox(SpaceClip *sc, float min[2], float max[2])
{
	MovieClip *clip = ED_space_clip(sc);
	MovieTrackingTrack *track;
	int width, height, ok = FALSE;
	ListBase *tracksbase = BKE_tracking_get_tracks(&clip->tracking);

	INIT_MINMAX2(min, max);

	ED_space_clip_size(sc, &width, &height);

	track = tracksbase->first;
	while (track) {
		if (TRACK_VIEW_SELECTED(sc, track)) {
			MovieTrackingMarker *marker = BKE_tracking_get_marker(track, sc->user.framenr);

			if (marker) {
				float pos[3];

				pos[0] = marker->pos[0] + track->offset[0];
				pos[1] = marker->pos[1] + track->offset[1];
				pos[2] = 0.0f;

				/* undistortion happens for normalized coords */
				if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
					/* undistortion happens for normalized coords */
					ED_clip_point_undistorted_pos(sc, pos, pos);
				}

				pos[0] *= width;
				pos[1] *= height;

				mul_v3_m4v3(pos, sc->stabmat, pos);

				DO_MINMAX2(pos, min, max);

				ok = TRUE;
			}
		}

		track = track->next;
	}

	return ok;
}

int ED_clip_view_selection(SpaceClip *sc, ARegion *ar, int fit)
{
	int w, h, frame_width, frame_height;
	float min[2], max[2];

	ED_space_clip_size(sc, &frame_width, &frame_height);

	if (frame_width == 0 || frame_height == 0)
		return FALSE;

	if (!selected_boundbox(sc, min, max))
		return FALSE;

	/* center view */
	clip_view_center_to_point(sc, (max[0] + min[0]) / (2 * frame_width),
	                          (max[1] + min[1]) / (2 * frame_height));

	w = max[0] - min[0];
	h = max[1] - min[1];

	/* set zoom to see all selection */
	if (w > 0 && h > 0) {
		int width, height;
		float zoomx, zoomy, newzoom, aspx, aspy;

		ED_space_clip_aspect(sc, &aspx, &aspy);

		width = ar->winrct.xmax - ar->winrct.xmin + 1;
		height = ar->winrct.ymax - ar->winrct.ymin + 1;

		zoomx = (float)width / w / aspx;
		zoomy = (float)height / h / aspy;

		newzoom = 1.0f / power_of_2(1.0f / MIN2(zoomx, zoomy));

		if (fit || sc->zoom > newzoom)
			sc->zoom = newzoom;
	}

	return TRUE;
}

void ED_clip_point_undistorted_pos(SpaceClip *sc, float co[2], float nco[2])
{
	copy_v2_v2(nco, co);

	if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
		MovieClip *clip = ED_space_clip(sc);
		float aspy = 1.0f / clip->tracking.camera.pixel_aspect;
		int width, height;

		ED_space_clip_size(sc, &width, &height);

		nco[0] *= width;
		nco[1] *= height * aspy;

		BKE_tracking_invert_intrinsics(&clip->tracking, nco, nco);

		nco[0] /= width;
		nco[1] /= height * aspy;
	}
}

void ED_clip_point_stable_pos(bContext *C, float x, float y, float *xr, float *yr)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	int sx, sy, width, height;
	float zoomx, zoomy, pos[3] = {0.0f, 0.0f, 0.0f}, imat[4][4];

	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);
	ED_space_clip_size(sc, &width, &height);

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);

	pos[0] = (x - sx) / zoomx;
	pos[1] = (y - sy) / zoomy;

	invert_m4_m4(imat, sc->stabmat);
	mul_v3_m4v3(pos, imat, pos);

	*xr = pos[0] / width;
	*yr = pos[1] / height;

	if (sc->user.render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
		MovieClip *clip = ED_space_clip(sc);
		MovieTracking *tracking = &clip->tracking;
		float aspy = 1.0f / tracking->camera.pixel_aspect;
		float tmp[2] = {*xr * width, *yr * height * aspy};

		BKE_tracking_apply_intrinsics(tracking, tmp, tmp);

		*xr = tmp[0] / width;
		*yr = tmp[1] / (height * aspy);
	}
}

/**
 * \brief the reverse of ED_clip_point_stable_pos(), gets the marker region coords.
 * better name here? view_to_track / track_to_view or so?
 */
void ED_clip_point_stable_pos__reverse(SpaceClip *sc, ARegion *ar, float co[2], float nco[2])
{
	float zoomx, zoomy;
	float pos[3];
	int width, height;
	int sx, sy;

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);
	ED_space_clip_size(sc, &width, &height);
	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);

	ED_clip_point_undistorted_pos(sc, co, pos);

	/* untested */
	mul_v3_m4v3(pos, sc->stabmat, pos);

	nco[0] = (pos[0] * width  * zoomx) + (float)sx;
	nco[1] = (pos[1] * height * zoomy) + (float)sy;
}

void ED_clip_mouse_pos(bContext *C, wmEvent *event, float co[2])
{
	ED_clip_point_stable_pos(C, event->mval[0], event->mval[1], &co[0], &co[1]);
}

/* OpenGL draw context */

typedef struct SpaceClipDrawContext {
	int support_checked, buffers_supported;

	GLuint texture;			/* OGL texture ID */
	short texture_allocated;	/* flag if texture was allocated by glGenTextures */
	struct ImBuf *texture_ibuf;	/* image buffer for which texture was created */
	int image_width, image_height;	/* image width and height for which texture was created */
	unsigned last_texture;		/* ID of previously used texture, so it'll be restored after clip drawing */
	int framenr;
} SpaceClipDrawContext;

int ED_space_clip_texture_buffer_supported(SpaceClip *sc)
{
	SpaceClipDrawContext *context = sc->draw_context;

	if (!context) {
		context = MEM_callocN(sizeof(SpaceClipDrawContext), "SpaceClipDrawContext");
		sc->draw_context = context;
	}

	if (!context->support_checked) {
		context->support_checked = TRUE;
		if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
			context->buffers_supported = FALSE;
		}
		else {
			context->buffers_supported = GPU_non_power_of_two_support();
		}
	}

	return context->buffers_supported;
}

int ED_space_clip_load_movieclip_buffer(SpaceClip *sc, ImBuf *ibuf)
{
	SpaceClipDrawContext *context = sc->draw_context;
	MovieClip *clip = ED_space_clip(sc);
	int need_rebind = 0;

	context->last_texture = glaGetOneInteger(GL_TEXTURE_2D);

	/* image texture need to be rebinded if displaying another image buffer
	 * assuming displaying happens of footage frames only on which painting doesn't heppen.
	 * so not changed image buffer pointer means unchanged image content */
	need_rebind |= context->texture_ibuf != ibuf;
	need_rebind |= context->framenr != sc->user.framenr;

	if (need_rebind) {
		int width = ibuf->x, height = ibuf->y;
		int need_recreate = 0;

		if (width > GL_MAX_TEXTURE_SIZE || height > GL_MAX_TEXTURE_SIZE)
			return 0;

		/* if image resolution changed (e.g. switched to proxy display) texture need to be recreated */
		need_recreate = context->image_width != ibuf->x || context->image_height != ibuf->y;

		if (context->texture_ibuf && need_recreate) {
			glDeleteTextures(1, &context->texture);
			context->texture_allocated = 0;
		}

		if (need_recreate || !context->texture_allocated) {
			/* texture doesn't exist yet or need to be re-allocated because of changed dimensions */
			int filter = GL_LINEAR;

			/* non-scaled proxy shouldn;t use diltering */
			if ((clip->flag & MCLIP_USE_PROXY) == 0 ||
			    ELEM(sc->user.render_size, MCLIP_PROXY_RENDER_SIZE_FULL, MCLIP_PROXY_RENDER_SIZE_100))
			{
				filter = GL_NEAREST;
			}

			glGenTextures(1, &context->texture);
			glBindTexture(GL_TEXTURE_2D, context->texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}
		else {
			/* if texture doesn't need to be reallocated itself, just bind it so
			 * loading of image will happen to a proper texture */
			glBindTexture(GL_TEXTURE_2D, context->texture);
		}

		if (ibuf->rect_float) {
			if (ibuf->rect == NULL)
				IMB_rect_from_float(ibuf);
		}

		if (ibuf->rect)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

		/* store settings */
		context->texture_allocated = 1;
		context->texture_ibuf = ibuf;
		context->image_width = ibuf->x;
		context->image_height = ibuf->y;
		context->framenr = sc->user.framenr;
	}
	else {
		/* displaying exactly the same image which was loaded t oa texture,
		 * just bint texture in this case */
		glBindTexture(GL_TEXTURE_2D, context->texture);
	}

	glEnable(GL_TEXTURE_2D);

	return TRUE;
}

void ED_space_clip_unload_movieclip_buffer(SpaceClip *sc)
{
	SpaceClipDrawContext *context = sc->draw_context;

	glBindTexture(GL_TEXTURE_2D, context->last_texture);
	glDisable(GL_TEXTURE_2D);
}

void ED_space_clip_free_texture_buffer(SpaceClip *sc)
{
	SpaceClipDrawContext *context = sc->draw_context;

	if (context) {
		glDeleteTextures(1, &context->texture);

		MEM_freeN(context);
	}
}

/* ******** masking editing related functions ******** */

int ED_space_clip_show_trackedit(SpaceClip *sc)
{
	if (sc) {
		return ELEM3(sc->mode, SC_MODE_TRACKING, SC_MODE_RECONSTRUCTION, SC_MODE_DISTORTION);
	}

	return FALSE;
}

int ED_space_clip_show_maskedit(SpaceClip *sc)
{
	if (sc) {
		return sc->mode == SC_MODE_MASKEDITING;
	}

	return FALSE;
}

void ED_space_clip_set_mask(bContext *C, SpaceClip *sc, Mask *mask)
{
	sc->mask = mask;

	if(sc->mask && sc->mask->id.us==0)
		sc->clip->id.us = 1;

	if(C)
		WM_event_add_notifier(C, NC_MASK|NA_SELECTED, mask);
}
