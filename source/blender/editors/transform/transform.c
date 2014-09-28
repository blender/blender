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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform.c
 *  \ingroup edtransform
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mask_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"  /* PET modes */

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_ghash.h"

#include "BKE_nla.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_particle.h"
#include "BKE_unit.h"
#include "BKE_mask.h"
#include "BKE_report.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_markers.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_clip.h"
#include "ED_node.h"

#include "WM_types.h"
#include "WM_api.h"

#include "UI_view2d.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "transform.h"

/* Disabling, since when you type you know what you are doing, and being able to set it to zero is handy. */
// #define USE_NUM_NO_ZERO

#define MAX_INFO_LEN 256

static void drawTransformApply(const struct bContext *C, ARegion *ar, void *arg);
static int doEdgeSlide(TransInfo *t, float perc);
static int doVertSlide(TransInfo *t, float perc);

static void drawEdgeSlide(const struct bContext *C, TransInfo *t);
static void drawVertSlide(const struct bContext *C, TransInfo *t);
static void len_v3_ensure(float v[3], const float length);
static void postInputRotation(TransInfo *t, float values[3]);


/* Transform Callbacks */
static void initBend(TransInfo *t);
static eRedrawFlag handleEventBend(TransInfo *t, const struct wmEvent *event);
static void Bend(TransInfo *t, const int mval[2]);

static void initShear(TransInfo *t);
static eRedrawFlag handleEventShear(TransInfo *t, const struct wmEvent *event);
static void applyShear(TransInfo *t, const int mval[2]);

static void initResize(TransInfo *t);
static void applyResize(TransInfo *t, const int mval[2]);

static void initSkinResize(TransInfo *t);
static void applySkinResize(TransInfo *t, const int mval[2]);

static void initTranslation(TransInfo *t);
static void applyTranslation(TransInfo *t, const int mval[2]);

static void initToSphere(TransInfo *t);
static void applyToSphere(TransInfo *t, const int mval[2]);

static void initRotation(TransInfo *t);
static void applyRotation(TransInfo *t, const int mval[2]);

static void initShrinkFatten(TransInfo *t);
static void applyShrinkFatten(TransInfo *t, const int mval[2]);

static void initTilt(TransInfo *t);
static void applyTilt(TransInfo *t, const int mval[2]);

static void initCurveShrinkFatten(TransInfo *t);
static void applyCurveShrinkFatten(TransInfo *t, const int mval[2]);

static void initMaskShrinkFatten(TransInfo *t);
static void applyMaskShrinkFatten(TransInfo *t, const int mval[2]);

static void initTrackball(TransInfo *t);
static void applyTrackball(TransInfo *t, const int mval[2]);

static void initPushPull(TransInfo *t);
static void applyPushPull(TransInfo *t, const int mval[2]);

static void initBevelWeight(TransInfo *t);
static void applyBevelWeight(TransInfo *t, const int mval[2]);

static void initCrease(TransInfo *t);
static void applyCrease(TransInfo *t, const int mval[2]);

static void initBoneSize(TransInfo *t);
static void applyBoneSize(TransInfo *t, const int mval[2]);

static void initBoneEnvelope(TransInfo *t);
static void applyBoneEnvelope(TransInfo *t, const int mval[2]);

static void initBoneRoll(TransInfo *t);
static void applyBoneRoll(TransInfo *t, const int mval[2]);

static void initEdgeSlide(TransInfo *t);
static eRedrawFlag handleEventEdgeSlide(TransInfo *t, const struct wmEvent *event);
static void applyEdgeSlide(TransInfo *t, const int mval[2]);

static void initVertSlide(TransInfo *t);
static eRedrawFlag handleEventVertSlide(TransInfo *t, const struct wmEvent *event);
static void applyVertSlide(TransInfo *t, const int mval[2]);

static void initTimeTranslate(TransInfo *t);
static void applyTimeTranslate(TransInfo *t, const int mval[2]);

static void initTimeSlide(TransInfo *t);
static void applyTimeSlide(TransInfo *t, const int mval[2]);

static void initTimeScale(TransInfo *t);
static void applyTimeScale(TransInfo *t, const int mval[2]);

static void initBakeTime(TransInfo *t);
static void applyBakeTime(TransInfo *t, const int mval[2]);

static void initMirror(TransInfo *t);
static void applyMirror(TransInfo *t, const int mval[2]);

static void initAlign(TransInfo *t);
static void applyAlign(TransInfo *t, const int mval[2]);

static void initSeqSlide(TransInfo *t);
static void applySeqSlide(TransInfo *t, const int mval[2]);
/* end transform callbacks */


static bool transdata_check_local_center(TransInfo *t, short around)
{
	return ((around == V3D_LOCAL) && (
	            (t->flag & (T_OBJECT | T_POSE)) ||
	            (t->obedit && ELEM(t->obedit->type, OB_MESH, OB_CURVE, OB_MBALL, OB_ARMATURE)) ||
	            (t->spacetype == SPACE_IPO) ||
	            (t->options & (CTX_MOVIECLIP | CTX_MASK | CTX_PAINT_CURVE)))
	        );
}

/* ************************** SPACE DEPENDANT CODE **************************** */

void setTransformViewMatrices(TransInfo *t)
{
	if (t->spacetype == SPACE_VIEW3D && t->ar && t->ar->regiontype == RGN_TYPE_WINDOW) {
		RegionView3D *rv3d = t->ar->regiondata;

		copy_m4_m4(t->viewmat, rv3d->viewmat);
		copy_m4_m4(t->viewinv, rv3d->viewinv);
		copy_m4_m4(t->persmat, rv3d->persmat);
		copy_m4_m4(t->persinv, rv3d->persinv);
		t->persp = rv3d->persp;
	}
	else {
		unit_m4(t->viewmat);
		unit_m4(t->viewinv);
		unit_m4(t->persmat);
		unit_m4(t->persinv);
		t->persp = RV3D_ORTHO;
	}

	calculateCenter2D(t);
}

static void convertViewVec2D(View2D *v2d, float r_vec[3], int dx, int dy)
{
	float divx, divy;
	
	divx = BLI_rcti_size_x(&v2d->mask);
	divy = BLI_rcti_size_y(&v2d->mask);

	r_vec[0] = BLI_rctf_size_x(&v2d->cur) * dx / divx;
	r_vec[1] = BLI_rctf_size_y(&v2d->cur) * dy / divy;
	r_vec[2] = 0.0f;
}

static void convertViewVec2D_mask(View2D *v2d, float r_vec[3], int dx, int dy)
{
	float divx, divy;
	float mulx, muly;

	divx = BLI_rcti_size_x(&v2d->mask);
	divy = BLI_rcti_size_y(&v2d->mask);

	mulx = BLI_rctf_size_x(&v2d->cur);
	muly = BLI_rctf_size_y(&v2d->cur);

	/* difference with convertViewVec2D */
	/* clamp w/h, mask only */
	if (mulx / divx < muly / divy) {
		divy = divx;
		muly = mulx;
	}
	else {
		divx = divy;
		mulx = muly;
	}
	/* end difference */

	r_vec[0] = mulx * dx / divx;
	r_vec[1] = muly * dy / divy;
	r_vec[2] = 0.0f;
}

void convertViewVec(TransInfo *t, float r_vec[3], int dx, int dy)
{
	if ((t->spacetype == SPACE_VIEW3D) && (t->ar->regiontype == RGN_TYPE_WINDOW)) {
		if (t->options & CTX_PAINT_CURVE) {
			r_vec[0] = dx;
			r_vec[1] = dy;
		}
		else {	const float mval_f[2] = {(float)dx, (float)dy};
			ED_view3d_win_to_delta(t->ar, mval_f, r_vec, t->zfac);
		}
	}
	else if (t->spacetype == SPACE_IMAGE) {
		float aspx, aspy;

		if (t->options & CTX_MASK) {
			convertViewVec2D_mask(t->view, r_vec, dx, dy);
			ED_space_image_get_aspect(t->sa->spacedata.first, &aspx, &aspy);
		}
		else if (t->options & CTX_PAINT_CURVE) {
			r_vec[0] = dx;
			r_vec[1] = dy;

			aspx = aspy = 1.0;
		}
		else {
			convertViewVec2D(t->view, r_vec, dx, dy);
			ED_space_image_get_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
		}

		r_vec[0] *= aspx;
		r_vec[1] *= aspy;
	}
	else if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		convertViewVec2D(t->view, r_vec, dx, dy);
	}
	else if (ELEM(t->spacetype, SPACE_NODE, SPACE_SEQ)) {
		convertViewVec2D(&t->ar->v2d, r_vec, dx, dy);
	}
	else if (t->spacetype == SPACE_CLIP) {
		float aspx, aspy;

		if (t->options & CTX_MASK) {
			convertViewVec2D_mask(t->view, r_vec, dx, dy);
		}
		else {
			convertViewVec2D(t->view, r_vec, dx, dy);
		}

		if (t->options & CTX_MOVIECLIP) {
			ED_space_clip_get_aspect_dimension_aware(t->sa->spacedata.first, &aspx, &aspy);
		}
		else if (t->options & CTX_MASK) {
			/* TODO - NOT WORKING, this isnt so bad since its only display aspect */
			ED_space_clip_get_aspect(t->sa->spacedata.first, &aspx, &aspy);
		}
		else {
			/* should never happen, quiet warnings */
			BLI_assert(0);
			aspx = aspy = 1.0f;
		}

		r_vec[0] *= aspx;
		r_vec[1] *= aspy;
	}
	else {
		printf("%s: called in an invalid context\n", __func__);
		zero_v3(r_vec);
	}
}

void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], const eV3DProjTest flag)
{
	if (t->spacetype == SPACE_VIEW3D) {
		if (t->ar->regiontype == RGN_TYPE_WINDOW) {
			if (ED_view3d_project_int_global(t->ar, vec, adr, flag) != V3D_PROJ_RET_OK) {
				adr[0] = (int)2140000000.0f;  /* this is what was done in 2.64, perhaps we can be smarter? */
				adr[1] = (int)2140000000.0f;
			}
		}
	}
	else if (t->spacetype == SPACE_IMAGE) {
		SpaceImage *sima = t->sa->spacedata.first;

		if (t->options & CTX_MASK) {
			float aspx, aspy;
			float v[2];

			ED_space_image_get_aspect(sima, &aspx, &aspy);

			copy_v2_v2(v, vec);

			v[0] = v[0] / aspx;
			v[1] = v[1] / aspy;

			BKE_mask_coord_to_image(sima->image, &sima->iuser, v, v);

			ED_image_point_pos__reverse(sima, t->ar, v, v);

			adr[0] = v[0];
			adr[1] = v[1];
		}
		else if (t->options & CTX_PAINT_CURVE) {
			adr[0] = vec[0];
			adr[1] = vec[1];
		}
		else {
			float aspx, aspy, v[2];

			ED_space_image_get_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
			v[0] = vec[0] / aspx;
			v[1] = vec[1] / aspy;

			UI_view2d_view_to_region(t->view, v[0], v[1], &adr[0], &adr[1]);
		}
	}
	else if (t->spacetype == SPACE_ACTION) {
		int out[2] = {0, 0};
#if 0
		SpaceAction *sact = t->sa->spacedata.first;

		if (sact->flag & SACTION_DRAWTIME) {
			//vec[0] = vec[0]/((t->scene->r.frs_sec / t->scene->r.frs_sec_base));
			/* same as below */
			UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
		}
		else
#endif
		{
			UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
		}

		adr[0] = out[0];
		adr[1] = out[1];
	}
	else if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA)) {
		int out[2] = {0, 0};

		UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
		adr[0] = out[0];
		adr[1] = out[1];
	}
	else if (t->spacetype == SPACE_SEQ) { /* XXX not tested yet, but should work */
		int out[2] = {0, 0};

		UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
		adr[0] = out[0];
		adr[1] = out[1];
	}
	else if (t->spacetype == SPACE_CLIP) {
		SpaceClip *sc = t->sa->spacedata.first;

		if (t->options & CTX_MASK) {
			MovieClip *clip = ED_space_clip_get_clip(sc);

			if (clip) {
				float aspx, aspy;
				float v[2];

				ED_space_clip_get_aspect(sc, &aspx, &aspy);

				copy_v2_v2(v, vec);

				v[0] = v[0] / aspx;
				v[1] = v[1] / aspy;

				BKE_mask_coord_to_movieclip(sc->clip, &sc->user, v, v);

				ED_clip_point_stable_pos__reverse(sc, t->ar, v, v);

				adr[0] = v[0];
				adr[1] = v[1];
			}
			else {
				adr[0] = 0;
				adr[1] = 0;
			}
		}
		else if (t->options & CTX_MOVIECLIP) {
			float v[2], aspx, aspy;

			copy_v2_v2(v, vec);
			ED_space_clip_get_aspect_dimension_aware(t->sa->spacedata.first, &aspx, &aspy);

			v[0] /= aspx;
			v[1] /= aspy;

			UI_view2d_view_to_region(t->view, v[0], v[1], &adr[0], &adr[1]);
		}
		else {
			BLI_assert(0);
		}
	}
	else if (t->spacetype == SPACE_NODE) {
		UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &adr[0], &adr[1]);
	}
}
void projectIntView(TransInfo *t, const float vec[3], int adr[2])
{
	projectIntViewEx(t, vec, adr, V3D_PROJ_TEST_NOP);
}

void projectFloatViewEx(TransInfo *t, const float vec[3], float adr[2], const eV3DProjTest flag)
{
	switch (t->spacetype) {
		case SPACE_VIEW3D:
		{
			if (t->options & CTX_PAINT_CURVE) {
				adr[0] = vec[0];
				adr[1] = vec[1];
			}
			else if (t->ar->regiontype == RGN_TYPE_WINDOW) {
				/* allow points behind the view [#33643] */
				if (ED_view3d_project_float_global(t->ar, vec, adr, flag) != V3D_PROJ_RET_OK) {
					/* XXX, 2.64 and prior did this, weak! */
					adr[0] = t->ar->winx / 2.0f;
					adr[1] = t->ar->winy / 2.0f;
				}
				return;
			}
			break;
		}
		default:
		{
			int a[2] = {0, 0};
			projectIntView(t, vec, a);
			adr[0] = a[0];
			adr[1] = a[1];
			break;
		}
	}
}
void projectFloatView(TransInfo *t, const float vec[3], float adr[2])
{
	projectFloatViewEx(t, vec, adr, V3D_PROJ_TEST_NOP);
}

void applyAspectRatio(TransInfo *t, float vec[2])
{
	if ((t->spacetype == SPACE_IMAGE) && (t->mode == TFM_TRANSLATION) && !(t->options & CTX_PAINT_CURVE)) {
		SpaceImage *sima = t->sa->spacedata.first;
		float aspx, aspy;

		if ((sima->flag & SI_COORDFLOATS) == 0) {
			int width, height;
			ED_space_image_get_size(sima, &width, &height);

			vec[0] *= width;
			vec[1] *= height;
		}

		ED_space_image_get_uv_aspect(sima, &aspx, &aspy);
		vec[0] /= aspx;
		vec[1] /= aspy;
	}
	else if ((t->spacetype == SPACE_CLIP) && (t->mode == TFM_TRANSLATION)) {
		if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
			SpaceClip *sc = t->sa->spacedata.first;
			float aspx, aspy;


			if (t->options & CTX_MOVIECLIP) {
				ED_space_clip_get_aspect_dimension_aware(sc, &aspx, &aspy);

				vec[0] /= aspx;
				vec[1] /= aspy;
			}
			else if (t->options & CTX_MASK) {
				ED_space_clip_get_aspect(sc, &aspx, &aspy);

				vec[0] /= aspx;
				vec[1] /= aspy;
			}
		}
	}
}

void removeAspectRatio(TransInfo *t, float vec[2])
{
	if ((t->spacetype == SPACE_IMAGE) && (t->mode == TFM_TRANSLATION)) {
		SpaceImage *sima = t->sa->spacedata.first;
		float aspx, aspy;

		if ((sima->flag & SI_COORDFLOATS) == 0) {
			int width, height;
			ED_space_image_get_size(sima, &width, &height);

			vec[0] /= width;
			vec[1] /= height;
		}

		ED_space_image_get_uv_aspect(sima, &aspx, &aspy);
		vec[0] *= aspx;
		vec[1] *= aspy;
	}
	else if ((t->spacetype == SPACE_CLIP) && (t->mode == TFM_TRANSLATION)) {
		if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
			SpaceClip *sc = t->sa->spacedata.first;
			float aspx = 1.0f, aspy = 1.0f;

			if (t->options & CTX_MOVIECLIP) {
				ED_space_clip_get_aspect_dimension_aware(sc, &aspx, &aspy);
			}
			else if (t->options & CTX_MASK) {
				ED_space_clip_get_aspect(sc, &aspx, &aspy);
			}

			vec[0] *= aspx;
			vec[1] *= aspy;
		}
	}
}

static void viewRedrawForce(const bContext *C, TransInfo *t)
{
	if (t->spacetype == SPACE_VIEW3D) {
		if (t->options & CTX_PAINT_CURVE) {
			wmWindow *window = CTX_wm_window(C);
			WM_paint_cursor_tag_redraw(window, t->ar);
		}
		else {
			/* Do we need more refined tags? */
			if (t->flag & T_POSE)
				WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			else
				WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

			/* for realtime animation record - send notifiers recognised by animation editors */
			// XXX: is this notifier a lame duck?
			if ((t->animtimer) && IS_AUTOKEY_ON(t->scene))
				WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, NULL);

		}
	}
	else if (t->spacetype == SPACE_ACTION) {
		//SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	}
	else if (t->spacetype == SPACE_IPO) {
		//SpaceIpo *sipo = (SpaceIpo *)t->sa->spacedata.first;
		WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	}
	else if (t->spacetype == SPACE_NLA) {
		WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
	}
	else if (t->spacetype == SPACE_NODE) {
		//ED_area_tag_redraw(t->sa);
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);
	}
	else if (t->spacetype == SPACE_SEQ) {
		WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, NULL);
	}
	else if (t->spacetype == SPACE_IMAGE) {
		if (t->options & CTX_MASK) {
			Mask *mask = CTX_data_edit_mask(C);

			WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
		}
		else if (t->options & CTX_PAINT_CURVE) {
			wmWindow *window = CTX_wm_window(C);
			WM_paint_cursor_tag_redraw(window, t->ar);
		}
		else {
			// XXX how to deal with lock?
			SpaceImage *sima = (SpaceImage *)t->sa->spacedata.first;
			if (sima->lock) WM_event_add_notifier(C, NC_GEOM | ND_DATA, t->obedit->data);
			else ED_area_tag_redraw(t->sa);
		}
	}
	else if (t->spacetype == SPACE_CLIP) {
		SpaceClip *sc = (SpaceClip *)t->sa->spacedata.first;

		if (ED_space_clip_check_show_trackedit(sc)) {
			MovieClip *clip = ED_space_clip_get_clip(sc);

			/* objects could be parented to tracking data, so send this for viewport refresh */
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

			WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
		}
		else if (ED_space_clip_check_show_maskedit(sc)) {
			Mask *mask = CTX_data_edit_mask(C);

			WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
		}
	}
}

static void viewRedrawPost(bContext *C, TransInfo *t)
{
	ED_area_headerprint(t->sa, NULL);
	
	if (t->spacetype == SPACE_VIEW3D) {
		/* if autokeying is enabled, send notifiers that keyframes were added */
		if (IS_AUTOKEY_ON(t->scene))
			WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

		/* redraw UV editor */
		if (t->mode == TFM_EDGE_SLIDE && (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT))
			WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
		
		/* XXX temp, first hack to get auto-render in compositor work (ton) */
		WM_event_add_notifier(C, NC_SCENE | ND_TRANSFORM_DONE, CTX_data_scene(C));

	}
	
#if 0 // TRANSFORM_FIX_ME
	if (t->spacetype == SPACE_VIEW3D) {
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else if (t->spacetype == SPACE_IMAGE) {
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_IPO)) {
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWTIME, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
	}

	scrarea_queue_headredraw(curarea);
#endif
}

/* ************************** TRANSFORMATIONS **************************** */

static void view_editmove(unsigned short UNUSED(event))
{
#if 0 // TRANSFORM_FIX_ME
	int refresh = 0;
	/* Regular:   Zoom in */
	/* Shift:     Scroll up */
	/* Ctrl:      Scroll right */
	/* Alt-Shift: Rotate up */
	/* Alt-Ctrl:  Rotate right */

	/* only work in 3D window for now
	 * In the end, will have to send to event to a 2D window handler instead
	 */
	if (Trans.flag & T_2D_EDIT)
		return;

	switch (event) {
		case WHEELUPMOUSE:

			if (G.qual & LR_SHIFTKEY) {
				if (G.qual & LR_ALTKEY) {
					G.qual &= ~LR_SHIFTKEY;
					persptoetsen(PAD2);
					G.qual |= LR_SHIFTKEY;
				}
				else {
					persptoetsen(PAD2);
				}
			}
			else if (G.qual & LR_CTRLKEY) {
				if (G.qual & LR_ALTKEY) {
					G.qual &= ~LR_CTRLKEY;
					persptoetsen(PAD4);
					G.qual |= LR_CTRLKEY;
				}
				else {
					persptoetsen(PAD4);
				}
			}
			else if (U.uiflag & USER_WHEELZOOMDIR)
				persptoetsen(PADMINUS);
			else
				persptoetsen(PADPLUSKEY);

			refresh = 1;
			break;
		case WHEELDOWNMOUSE:
			if (G.qual & LR_SHIFTKEY) {
				if (G.qual & LR_ALTKEY) {
					G.qual &= ~LR_SHIFTKEY;
					persptoetsen(PAD8);
					G.qual |= LR_SHIFTKEY;
				}
				else {
					persptoetsen(PAD8);
				}
			}
			else if (G.qual & LR_CTRLKEY) {
				if (G.qual & LR_ALTKEY) {
					G.qual &= ~LR_CTRLKEY;
					persptoetsen(PAD6);
					G.qual |= LR_CTRLKEY;
				}
				else {
					persptoetsen(PAD6);
				}
			}
			else if (U.uiflag & USER_WHEELZOOMDIR)
				persptoetsen(PADPLUSKEY);
			else
				persptoetsen(PADMINUS);

			refresh = 1;
			break;
	}

	if (refresh)
		setTransformViewMatrices(&Trans);
#endif
}

/* ************************************************* */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
#define TFM_MODAL_CANCEL        1
#define TFM_MODAL_CONFIRM       2
#define TFM_MODAL_TRANSLATE     3
#define TFM_MODAL_ROTATE        4
#define TFM_MODAL_RESIZE        5
#define TFM_MODAL_SNAP_INV_ON   6
#define TFM_MODAL_SNAP_INV_OFF  7
#define TFM_MODAL_SNAP_TOGGLE   8
#define TFM_MODAL_AXIS_X        9
#define TFM_MODAL_AXIS_Y        10
#define TFM_MODAL_AXIS_Z        11
#define TFM_MODAL_PLANE_X       12
#define TFM_MODAL_PLANE_Y       13
#define TFM_MODAL_PLANE_Z       14
#define TFM_MODAL_CONS_OFF      15
#define TFM_MODAL_ADD_SNAP      16
#define TFM_MODAL_REMOVE_SNAP   17
/*	18 and 19 used by numinput, defined in transform.h
 * */
#define TFM_MODAL_PROPSIZE_UP   20
#define TFM_MODAL_PROPSIZE_DOWN 21
#define TFM_MODAL_AUTOIK_LEN_INC 22
#define TFM_MODAL_AUTOIK_LEN_DEC 23

#define TFM_MODAL_EDGESLIDE_UP 24
#define TFM_MODAL_EDGESLIDE_DOWN 25

/* for analog input, like trackpad */
#define TFM_MODAL_PROPSIZE		26

/* called in transform_ops.c, on each regeneration of keymaps */
wmKeyMap *transform_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{TFM_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{TFM_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{TFM_MODAL_TRANSLATE, "TRANSLATE", 0, "Translate", ""},
		{TFM_MODAL_ROTATE, "ROTATE", 0, "Rotate", ""},
		{TFM_MODAL_RESIZE, "RESIZE", 0, "Resize", ""},
		{TFM_MODAL_SNAP_INV_ON, "SNAP_INV_ON", 0, "Invert Snap On", ""},
		{TFM_MODAL_SNAP_INV_OFF, "SNAP_INV_OFF", 0, "Invert Snap Off", ""},
		{TFM_MODAL_SNAP_TOGGLE, "SNAP_TOGGLE", 0, "Snap Toggle", ""},
		{TFM_MODAL_AXIS_X, "AXIS_X", 0, "Orientation X axis", ""},
		{TFM_MODAL_AXIS_Y, "AXIS_Y", 0, "Orientation Y axis", ""},
		{TFM_MODAL_AXIS_Z, "AXIS_Z", 0, "Orientation Z axis", ""},
		{TFM_MODAL_PLANE_X, "PLANE_X", 0, "Orientation X plane", ""},
		{TFM_MODAL_PLANE_Y, "PLANE_Y", 0, "Orientation Y plane", ""},
		{TFM_MODAL_PLANE_Z, "PLANE_Z", 0, "Orientation Z plane", ""},
		{TFM_MODAL_CONS_OFF, "CONS_OFF", 0, "Remove Constraints", ""},
		{TFM_MODAL_ADD_SNAP, "ADD_SNAP", 0, "Add Snap Point", ""},
		{TFM_MODAL_REMOVE_SNAP, "REMOVE_SNAP", 0, "Remove Last Snap Point", ""},
		{NUM_MODAL_INCREMENT_UP, "INCREMENT_UP", 0, "Numinput Increment Up", ""},
		{NUM_MODAL_INCREMENT_DOWN, "INCREMENT_DOWN", 0, "Numinput Increment Down", ""},
		{TFM_MODAL_PROPSIZE_UP, "PROPORTIONAL_SIZE_UP", 0, "Increase Proportional Influence", ""},
		{TFM_MODAL_PROPSIZE_DOWN, "PROPORTIONAL_SIZE_DOWN", 0, "Decrease Proportional Influence", ""},
		{TFM_MODAL_AUTOIK_LEN_INC, "AUTOIK_CHAIN_LEN_UP", 0, "Increase Max AutoIK Chain Length", ""},
		{TFM_MODAL_AUTOIK_LEN_DEC, "AUTOIK_CHAIN_LEN_DOWN", 0, "Decrease Max AutoIK Chain Length", ""},
		{TFM_MODAL_EDGESLIDE_UP, "EDGESLIDE_EDGE_NEXT", 0, "Select next Edge Slide Edge", ""},
		{TFM_MODAL_EDGESLIDE_DOWN, "EDGESLIDE_PREV_NEXT", 0, "Select previous Edge Slide Edge", ""},
		{TFM_MODAL_PROPSIZE, "PROPORTIONAL_SIZE", 0, "Adjust Proportional Influence", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Transform Modal Map");
	
	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items) return NULL;
	
	keymap = WM_modalkeymap_add(keyconf, "Transform Modal Map", modal_items);
	
	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY,    KM_PRESS, KM_ANY, 0, TFM_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, TFM_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, GKEY, KM_PRESS, 0, 0, TFM_MODAL_TRANSLATE);
	WM_modalkeymap_add_item(keymap, RKEY, KM_PRESS, 0, 0, TFM_MODAL_ROTATE);
	WM_modalkeymap_add_item(keymap, SKEY, KM_PRESS, 0, 0, TFM_MODAL_RESIZE);
	
	WM_modalkeymap_add_item(keymap, TABKEY, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_SNAP_TOGGLE);

	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_SNAP_INV_ON);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, TFM_MODAL_SNAP_INV_OFF);

	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_PRESS, KM_ANY, 0, TFM_MODAL_SNAP_INV_ON);
	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_RELEASE, KM_ANY, 0, TFM_MODAL_SNAP_INV_OFF);
	
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, 0, 0, TFM_MODAL_ADD_SNAP);
	WM_modalkeymap_add_item(keymap, AKEY, KM_PRESS, KM_ALT, 0, TFM_MODAL_REMOVE_SNAP);

	WM_modalkeymap_add_item(keymap, PAGEUPKEY, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_UP);
	WM_modalkeymap_add_item(keymap, PAGEDOWNKEY, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_DOWN);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_UP);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, 0, 0, TFM_MODAL_PROPSIZE_DOWN);
	WM_modalkeymap_add_item(keymap, MOUSEPAN, 0, 0, 0, TFM_MODAL_PROPSIZE);

	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, KM_ALT, 0, TFM_MODAL_EDGESLIDE_UP);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, KM_ALT, 0, TFM_MODAL_EDGESLIDE_DOWN);
	
	WM_modalkeymap_add_item(keymap, PAGEUPKEY, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_INC);
	WM_modalkeymap_add_item(keymap, PAGEDOWNKEY, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_DEC);
	WM_modalkeymap_add_item(keymap, WHEELDOWNMOUSE, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_INC);
	WM_modalkeymap_add_item(keymap, WHEELUPMOUSE, KM_PRESS, KM_SHIFT, 0, TFM_MODAL_AUTOIK_LEN_DEC);
	
	return keymap;
}

static void transform_event_xyz_constraint(TransInfo *t, short key_type, char cmode)
{
	if (!(t->flag & T_NO_CONSTRAINT)) {
		int constraint_axis, constraint_plane;
		int edit_2d = (t->flag & T_2D_EDIT);
		const char *msg1 = "", *msg2 = "", *msg3 = "";
		char axis;
	
		/* Initialize */
		switch (key_type) {
			case XKEY:
				msg1 = IFACE_("along X");
				msg2 = IFACE_("along %s X");
				msg3 = IFACE_("locking %s X");
				axis = 'X';
				constraint_axis = CON_AXIS0;
				break;
			case YKEY:
				msg1 = IFACE_("along Y");
				msg2 = IFACE_("along %s Y");
				msg3 = IFACE_("locking %s Y");
				axis = 'Y';
				constraint_axis = CON_AXIS1;
				break;
			case ZKEY:
				msg1 = IFACE_("along Z");
				msg2 = IFACE_("along %s Z");
				msg3 = IFACE_("locking %s Z");
				axis = 'Z';
				constraint_axis = CON_AXIS2;
				break;
			default:
				/* Invalid key */
				return;
		}
		constraint_plane = ((CON_AXIS0 | CON_AXIS1 | CON_AXIS2) & (~constraint_axis));

		if (edit_2d && (key_type != ZKEY)) {
			if (cmode == axis) {
				stopConstraint(t);
			}
			else {
				setUserConstraint(t, V3D_MANIP_GLOBAL, constraint_axis, msg1);
			}
		}
		else if (!edit_2d) {
			if (cmode == axis) {
				if (t->con.orientation != V3D_MANIP_GLOBAL) {
					stopConstraint(t);
				}
				else {
					short orientation = (t->current_orientation != V3D_MANIP_GLOBAL ?
					                     t->current_orientation : V3D_MANIP_LOCAL);
					if (!(t->modifiers & MOD_CONSTRAINT_PLANE))
						setUserConstraint(t, orientation, constraint_axis, msg2);
					else if (t->modifiers & MOD_CONSTRAINT_PLANE)
						setUserConstraint(t, orientation, constraint_plane, msg3);
				}
			}
			else {
				if (!(t->modifiers & MOD_CONSTRAINT_PLANE))
					setUserConstraint(t, V3D_MANIP_GLOBAL, constraint_axis, msg2);
				else if (t->modifiers & MOD_CONSTRAINT_PLANE)
					setUserConstraint(t, V3D_MANIP_GLOBAL, constraint_plane, msg3);
			}
		}
		t->redraw |= TREDRAW_HARD;
	}
}

int transformEvent(TransInfo *t, const wmEvent *event)
{
	char cmode = constraintModeToChar(t);
	bool handled = false;

	t->redraw |= handleMouseInput(t, &t->mouse, event);

	/* Handle modal numinput events first, if already activated. */
	if (((event->val == KM_PRESS) || (event->type == EVT_MODAL_MAP)) &&
	    hasNumInput(&t->num) && handleNumInput(t->context, &(t->num), event))
	{
		t->redraw |= TREDRAW_HARD;
		handled = true;
	}
	else if (event->type == MOUSEMOVE) {
		if (t->modifiers & MOD_CONSTRAINT_SELECT)
			t->con.mode |= CON_SELECT;

		copy_v2_v2_int(t->mval, event->mval);

		// t->redraw |= TREDRAW_SOFT; /* Use this for soft redraw. Might cause flicker in object mode */
		t->redraw |= TREDRAW_HARD;

		if (t->state == TRANS_STARTING) {
			t->state = TRANS_RUNNING;
		}

		applyMouseInput(t, &t->mouse, t->mval, t->values);

		// Snapping mouse move events
		t->redraw |= handleSnapping(t, event);
		handled = true;
	}
	/* handle modal keymap first */
	else if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case TFM_MODAL_CANCEL:
				t->state = TRANS_CANCEL;
				handled = true;
				break;
			case TFM_MODAL_CONFIRM:
				t->state = TRANS_CONFIRM;
				handled = true;
				break;
			case TFM_MODAL_TRANSLATE:
				/* only switch when... */
				if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {
					resetTransModal(t);
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initTranslation(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
					WM_event_add_mousemove(t->context);
					handled = true;
				}
				else if (t->mode == TFM_SEQ_SLIDE) {
					t->flag ^= T_ALT_TRANSFORM;
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				else {
					if (t->obedit && t->obedit->type == OB_MESH) {
						if ((t->mode == TFM_TRANSLATION) && (t->spacetype == SPACE_VIEW3D)) {
							resetTransModal(t);
							resetTransRestrictions(t);
							restoreTransObjects(t);

							/* first try edge slide */
							initEdgeSlide(t);
							/* if that fails, do vertex slide */
							if (t->state == TRANS_CANCEL) {
								t->state = TRANS_STARTING;
								initVertSlide(t);
							}
							/* vert slide can fail on unconnected vertices (rare but possible) */
							if (t->state == TRANS_CANCEL) {
								t->state = TRANS_STARTING;
								resetTransRestrictions(t);
								restoreTransObjects(t);
								initTranslation(t);
							}
							initSnapping(t, NULL); // need to reinit after mode change
							t->redraw |= TREDRAW_HARD;
							handled = true;
							WM_event_add_mousemove(t->context);
						}
					}
					else if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
						if (t->mode == TFM_TRANSLATION) {
							restoreTransObjects(t);

							t->flag ^= T_ALT_TRANSFORM;
							t->redraw |= TREDRAW_HARD;
							handled = true;
						}
					}
				}
				break;
			case TFM_MODAL_ROTATE:
				/* only switch when... */
				if (!(t->options & CTX_TEXTURE) && !(t->options & (CTX_MOVIECLIP | CTX_MASK))) {
					if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {
						resetTransModal(t);
						resetTransRestrictions(t);
						
						if (t->mode == TFM_ROTATION) {
							restoreTransObjects(t);
							initTrackball(t);
						}
						else {
							restoreTransObjects(t);
							initRotation(t);
						}
						initSnapping(t, NULL); // need to reinit after mode change
						t->redraw |= TREDRAW_HARD;
						handled = true;
					}
				}
				break;
			case TFM_MODAL_RESIZE:
				/* only switch when... */
				if (ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {

					/* Scale isn't normally very useful after extrude along normals, see T39756 */
					if ((t->con.mode & CON_APPLY) && (t->con.orientation == V3D_MANIP_NORMAL)) {
						stopConstraint(t);
					}

					resetTransModal(t);
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initResize(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				else if (t->mode == TFM_SHRINKFATTEN) {
					t->flag ^= T_ALT_TRANSFORM;
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				else if (t->mode == TFM_RESIZE) {
					if (t->options & CTX_MOVIECLIP) {
						restoreTransObjects(t);

						t->flag ^= T_ALT_TRANSFORM;
						t->redraw |= TREDRAW_HARD;
						handled = true;
					}
				}
				break;
				
			case TFM_MODAL_SNAP_INV_ON:
				t->modifiers |= MOD_SNAP_INVERT;
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;
			case TFM_MODAL_SNAP_INV_OFF:
				t->modifiers &= ~MOD_SNAP_INVERT;
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;
			case TFM_MODAL_SNAP_TOGGLE:
				t->modifiers ^= MOD_SNAP;
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;
			case TFM_MODAL_AXIS_X:
				if ((t->flag & T_NO_CONSTRAINT) == 0) {
					if (cmode == 'X') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS0), IFACE_("along X"));
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS0), IFACE_("along %s X"));
						}
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_AXIS_Y:
				if ((t->flag & T_NO_CONSTRAINT) == 0) {
					if (cmode == 'Y') {
						stopConstraint(t);
					}
					else {
						if (t->flag & T_2D_EDIT) {
							setUserConstraint(t, V3D_MANIP_GLOBAL, (CON_AXIS1), IFACE_("along Y"));
						}
						else {
							setUserConstraint(t, t->current_orientation, (CON_AXIS1), IFACE_("along %s Y"));
						}
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_AXIS_Z:
				if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
					if (cmode == 'Z') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS2), IFACE_("along %s Z"));
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_PLANE_X:
				if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
					if (cmode == 'X') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS1 | CON_AXIS2), IFACE_("locking %s X"));
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_PLANE_Y:
				if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
					if (cmode == 'Y') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS0 | CON_AXIS2), IFACE_("locking %s Y"));
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_PLANE_Z:
				if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
					if (cmode == 'Z') {
						stopConstraint(t);
					}
					else {
						setUserConstraint(t, t->current_orientation, (CON_AXIS0 | CON_AXIS1), IFACE_("locking %s Z"));
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_CONS_OFF:
				if ((t->flag & T_NO_CONSTRAINT) == 0) {
					stopConstraint(t);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_ADD_SNAP:
				addSnapPoint(t);
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;
			case TFM_MODAL_REMOVE_SNAP:
				removeSnapPoint(t);
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;
			case TFM_MODAL_PROPSIZE:
				/* MOUSEPAN usage... */
				if (t->flag & T_PROP_EDIT) {
					float fac = 1.0f + 0.005f *(event->y - event->prevy);
					t->prop_size *= fac;
					if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO)
						t->prop_size = min_ff(t->prop_size, ((View3D *)t->view)->far);
					calculatePropRatio(t);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_PROPSIZE_UP:
				if (t->flag & T_PROP_EDIT) {
					t->prop_size *= 1.1f;
					if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO)
						t->prop_size = min_ff(t->prop_size, ((View3D *)t->view)->far);
					calculatePropRatio(t);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_PROPSIZE_DOWN:
				if (t->flag & T_PROP_EDIT) {
					t->prop_size *= 0.90909090f;
					calculatePropRatio(t);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_EDGESLIDE_UP:
			case TFM_MODAL_EDGESLIDE_DOWN:
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;
			case TFM_MODAL_AUTOIK_LEN_INC:
				if (t->flag & T_AUTOIK) {
					transform_autoik_update(t, 1);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case TFM_MODAL_AUTOIK_LEN_DEC:
				if (t->flag & T_AUTOIK) {
					transform_autoik_update(t, -1);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			default:
				break;
		}
	}
	/* else do non-mapped events */
	else if (event->val == KM_PRESS) {
		switch (event->type) {
			case RIGHTMOUSE:
				t->state = TRANS_CANCEL;
				handled = true;
				break;
			/* enforce redraw of transform when modifiers are used */
			case LEFTSHIFTKEY:
			case RIGHTSHIFTKEY:
				t->modifiers |= MOD_CONSTRAINT_PLANE;
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;

			case SPACEKEY:
				t->state = TRANS_CONFIRM;
				handled = true;
				break;

			case MIDDLEMOUSE:
				if ((t->flag & T_NO_CONSTRAINT) == 0) {
					/* exception for switching to dolly, or trackball, in camera view */
					if (t->flag & T_CAMERA) {
						if (t->mode == TFM_TRANSLATION)
							setLocalConstraint(t, (CON_AXIS2), IFACE_("along local Z"));
						else if (t->mode == TFM_ROTATION) {
							restoreTransObjects(t);
							initTrackball(t);
						}
					}
					else {
						t->modifiers |= MOD_CONSTRAINT_SELECT;
						if (t->con.mode & CON_APPLY) {
							stopConstraint(t);
						}
						else {
							if (event->shift) {
								initSelectConstraint(t, t->spacemtx);
							}
							else {
								/* bit hackish... but it prevents mmb select to print the orientation from menu */
								float mati[3][3];
								strcpy(t->spacename, "global");
								unit_m3(mati);
								initSelectConstraint(t, mati);
							}
							postSelectConstraint(t);
						}
					}
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case ESCKEY:
				t->state = TRANS_CANCEL;
				handled = true;
				break;
			case PADENTER:
			case RETKEY:
				t->state = TRANS_CONFIRM;
				handled = true;
				break;
			case GKEY:
				/* only switch when... */
				if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL)) {
					resetTransModal(t);
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initTranslation(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case SKEY:
				/* only switch when... */
				if (ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL)) {
					resetTransModal(t);
					resetTransRestrictions(t);
					restoreTransObjects(t);
					initResize(t);
					initSnapping(t, NULL); // need to reinit after mode change
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case RKEY:
				/* only switch when... */
				if (!(t->options & CTX_TEXTURE)) {
					if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION)) {
						resetTransModal(t);
						resetTransRestrictions(t);

						if (t->mode == TFM_ROTATION) {
							restoreTransObjects(t);
							initTrackball(t);
						}
						else {
							restoreTransObjects(t);
							initRotation(t);
						}
						initSnapping(t, NULL); // need to reinit after mode change
						t->redraw |= TREDRAW_HARD;
						handled = true;
					}
				}
				break;
			case CKEY:
				if (event->alt) {
					if (!(t->options & CTX_NO_PET)) {
						t->flag ^= T_PROP_CONNECTED;
						sort_trans_data_dist(t);
						calculatePropRatio(t);
						t->redraw = TREDRAW_HARD;
						handled = true;
					}
				}
				else {
					if (!(t->flag & T_NO_CONSTRAINT)) {
						stopConstraint(t);
						t->redraw |= TREDRAW_HARD;
						handled = true;
					}
				}
				break;
			case XKEY:
			case YKEY:
			case ZKEY:
				if (!(t->flag & T_NO_CONSTRAINT)) {
					transform_event_xyz_constraint(t, event->type, cmode);
					handled = true;
				}
				break;
			case OKEY:
				if (t->flag & T_PROP_EDIT && event->shift) {
					t->prop_mode = (t->prop_mode + 1) % PROP_MODE_MAX;
					calculatePropRatio(t);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case PADPLUSKEY:
				if (event->alt && t->flag & T_PROP_EDIT) {
					t->prop_size *= 1.1f;
					if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO)
						t->prop_size = min_ff(t->prop_size, ((View3D *)t->view)->far);
					calculatePropRatio(t);
					t->redraw = TREDRAW_HARD;
					handled = true;
				}
				break;
			case PAGEUPKEY:
			case WHEELDOWNMOUSE:
				if (t->flag & T_AUTOIK) {
					transform_autoik_update(t, 1);
				}
				else {
					view_editmove(event->type);
				}
				t->redraw = TREDRAW_HARD;
				handled = true;
				break;
			case PADMINUS:
				if (event->alt && t->flag & T_PROP_EDIT) {
					t->prop_size *= 0.90909090f;
					calculatePropRatio(t);
					t->redraw = TREDRAW_HARD;
					handled = true;
				}
				break;
			case PAGEDOWNKEY:
			case WHEELUPMOUSE:
				if (t->flag & T_AUTOIK) {
					transform_autoik_update(t, -1);
				}
				else {
					view_editmove(event->type);
				}
				t->redraw = TREDRAW_HARD;
				handled = true;
				break;
			case LEFTALTKEY:
			case RIGHTALTKEY:
				if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D)) {
					t->flag |= T_ALT_TRANSFORM;
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			default:
				break;
		}

		/* Snapping key events */
		t->redraw |= handleSnapping(t, event);
	}
	else if (event->val == KM_RELEASE) {
		switch (event->type) {
			case LEFTSHIFTKEY:
			case RIGHTSHIFTKEY:
				t->modifiers &= ~MOD_CONSTRAINT_PLANE;
				t->redraw |= TREDRAW_HARD;
				handled = true;
				break;

			case MIDDLEMOUSE:
				if ((t->flag & T_NO_CONSTRAINT) == 0) {
					t->modifiers &= ~MOD_CONSTRAINT_SELECT;
					postSelectConstraint(t);
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			case LEFTALTKEY:
			case RIGHTALTKEY:
				if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D)) {
					t->flag &= ~T_ALT_TRANSFORM;
					t->redraw |= TREDRAW_HARD;
					handled = true;
				}
				break;
			default:
				break;
		}

		/* confirm transform if launch key is released after mouse move */
		if (t->flag & T_RELEASE_CONFIRM) {
			/* XXX Keyrepeat bug in Xorg messes this up, will test when fixed */
			if (event->type == t->launch_event && (t->launch_event == LEFTMOUSE || t->launch_event == RIGHTMOUSE)) {
				t->state = TRANS_CONFIRM;
			}
		}
	}

	/* Per transform event, if present */
	if (t->handleEvent &&
	    (!handled ||
	     /* Needed for vertex slide, see [#38756] */
	     (event->type == MOUSEMOVE)))
	{
		t->redraw |= t->handleEvent(t, event);
	}

	/* Try to init modal numinput now, if possible. */
	if (!(handled || t->redraw) && ((event->val == KM_PRESS) || (event->type == EVT_MODAL_MAP)) &&
	    handleNumInput(t->context, &(t->num), event))
	{
		t->redraw |= TREDRAW_HARD;
		handled = true;
	}

	if (handled || t->redraw) {
		return 0;
	}
	else {
		return OPERATOR_PASS_THROUGH;
	}
}

bool calculateTransformCenter(bContext *C, int centerMode, float cent3d[3], float cent2d[2])
{
	TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
	bool success;

	t->state = TRANS_RUNNING;

	/* avoid calculating PET */
	t->options = CTX_NO_PET;

	t->mode = TFM_DUMMY;

	initTransInfo(C, t, NULL, NULL);

	/* avoid doing connectivity lookups (when V3D_LOCAL is set) */
	t->around = V3D_CENTER;

	createTransData(C, t);              // make TransData structs from selection

	t->around = centerMode;             // override userdefined mode

	if (t->total == 0) {
		success = false;
	}
	else {
		success = true;

		calculateCenter(t);

		if (cent2d) {
			copy_v2_v2(cent2d, t->center2d);
		}

		if (cent3d) {
			// Copy center from constraint center. Transform center can be local
			copy_v3_v3(cent3d, t->con.center);
		}
	}


	/* aftertrans does insert keyframes, and clears base flags; doesn't read transdata */
	special_aftertrans_update(C, t);

	postTrans(C, t);

	MEM_freeN(t);

	return success;
}

typedef enum {
	UP,
	DOWN,
	LEFT,
	RIGHT
} ArrowDirection;
static void drawArrow(ArrowDirection d, short offset, short length, short size)
{
	switch (d) {
		case LEFT:
			offset = -offset;
			length = -length;
			size = -size;
			/* fall-through */
		case RIGHT:
			glBegin(GL_LINES);
			glVertex2s(offset, 0);
			glVertex2s(offset + length, 0);
			glVertex2s(offset + length, 0);
			glVertex2s(offset + length - size, -size);
			glVertex2s(offset + length, 0);
			glVertex2s(offset + length - size,  size);
			glEnd();
			break;

		case DOWN:
			offset = -offset;
			length = -length;
			size = -size;
			/* fall-through */
		case UP:
			glBegin(GL_LINES);
			glVertex2s(0, offset);
			glVertex2s(0, offset + length);
			glVertex2s(0, offset + length);
			glVertex2s(-size, offset + length - size);
			glVertex2s(0, offset + length);
			glVertex2s(size, offset + length - size);
			glEnd();
			break;
	}
}

static void drawArrowHead(ArrowDirection d, short size)
{
	switch (d) {
		case LEFT:
			size = -size;
			/* fall-through */
		case RIGHT:
			glBegin(GL_LINES);
			glVertex2s(0, 0);
			glVertex2s(-size, -size);
			glVertex2s(0, 0);
			glVertex2s(-size,  size);
			glEnd();
			break;

		case DOWN:
			size = -size;
			/* fall-through */
		case UP:
			glBegin(GL_LINES);
			glVertex2s(0, 0);
			glVertex2s(-size, -size);
			glVertex2s(0, 0);
			glVertex2s(size, -size);
			glEnd();
			break;
	}
}

static void drawArc(float size, float angle_start, float angle_end, int segments)
{
	float delta = (angle_end - angle_start) / segments;
	float angle;
	int a;

	glBegin(GL_LINE_STRIP);

	for (angle = angle_start, a = 0; a < segments; angle += delta, a++) {
		glVertex2f(cosf(angle) * size, sinf(angle) * size);
	}
	glVertex2f(cosf(angle_end) * size, sinf(angle_end) * size);

	glEnd();
}

static int helpline_poll(bContext *C)
{
	ARegion *ar = CTX_wm_region(C);
	
	if (ar && ar->regiontype == RGN_TYPE_WINDOW)
		return 1;
	return 0;
}

static void drawHelpline(bContext *UNUSED(C), int x, int y, void *customdata)
{
	TransInfo *t = (TransInfo *)customdata;

	if (t->helpline != HLP_NONE && !(t->flag & T_USES_MANIPULATOR)) {
		float vecrot[3], cent[2];
		int mval[2];

		mval[0] = x;
		mval[1] = y;

		copy_v3_v3(vecrot, t->center);
		if (t->flag & T_EDIT) {
			Object *ob = t->obedit;
			if (ob) mul_m4_v3(ob->obmat, vecrot);
		}
		else if (t->flag & T_POSE) {
			Object *ob = t->poseobj;
			if (ob) mul_m4_v3(ob->obmat, vecrot);
		}

		projectFloatViewEx(t, vecrot, cent, V3D_PROJ_TEST_CLIP_ZERO);

		glPushMatrix();

		switch (t->helpline) {
			case HLP_SPRING:
				UI_ThemeColor(TH_VIEW_OVERLAY);

				setlinestyle(3);
				glBegin(GL_LINE_STRIP);
				glVertex2iv(t->mval);
				glVertex2fv(cent);
				glEnd();

				glTranslatef(mval[0], mval[1], 0);
				glRotatef(-RAD2DEGF(atan2f(cent[0] - t->mval[0], cent[1] - t->mval[1])), 0, 0, 1);

				setlinestyle(0);
				glLineWidth(3.0);
				drawArrow(UP, 5, 10, 5);
				drawArrow(DOWN, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_HARROW:
				UI_ThemeColor(TH_VIEW_OVERLAY);

				glTranslatef(mval[0], mval[1], 0);

				glLineWidth(3.0);
				drawArrow(RIGHT, 5, 10, 5);
				drawArrow(LEFT, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_VARROW:
				UI_ThemeColor(TH_VIEW_OVERLAY);

				glTranslatef(mval[0], mval[1], 0);

				glLineWidth(3.0);
				drawArrow(UP, 5, 10, 5);
				drawArrow(DOWN, 5, 10, 5);
				glLineWidth(1.0);
				break;
			case HLP_ANGLE:
			{
				float dx = t->mval[0] - cent[0], dy = t->mval[1] - cent[1];
				float angle = atan2f(dy, dx);
				float dist = hypotf(dx, dy);
				float delta_angle = min_ff(15.0f / dist, (float)M_PI / 4.0f);
				float spacing_angle = min_ff(5.0f / dist, (float)M_PI / 12.0f);
				UI_ThemeColor(TH_VIEW_OVERLAY);

				setlinestyle(3);
				glBegin(GL_LINE_STRIP);
				glVertex2iv(t->mval);
				glVertex2fv(cent);
				glEnd();

				glTranslatef(cent[0] - t->mval[0] + mval[0], cent[1] - t->mval[1] + mval[1], 0);

				setlinestyle(0);
				glLineWidth(3.0);
				drawArc(dist, angle - delta_angle, angle - spacing_angle, 10);
				drawArc(dist, angle + spacing_angle, angle + delta_angle, 10);

				glPushMatrix();

				glTranslatef(cosf(angle - delta_angle) * dist, sinf(angle - delta_angle) * dist, 0);
				glRotatef(RAD2DEGF(angle - delta_angle), 0, 0, 1);

				drawArrowHead(DOWN, 5);

				glPopMatrix();

				glTranslatef(cosf(angle + delta_angle) * dist, sinf(angle + delta_angle) * dist, 0);
				glRotatef(RAD2DEGF(angle + delta_angle), 0, 0, 1);

				drawArrowHead(UP, 5);

				glLineWidth(1.0);
				break;
			}
			case HLP_TRACKBALL:
			{
				unsigned char col[3], col2[3];
				UI_GetThemeColor3ubv(TH_GRID, col);

				glTranslatef(mval[0], mval[1], 0);

				glLineWidth(3.0);

				UI_make_axis_color(col, col2, 'X');
				glColor3ubv((GLubyte *)col2);

				drawArrow(RIGHT, 5, 10, 5);
				drawArrow(LEFT, 5, 10, 5);

				UI_make_axis_color(col, col2, 'Y');
				glColor3ubv((GLubyte *)col2);

				drawArrow(UP, 5, 10, 5);
				drawArrow(DOWN, 5, 10, 5);
				glLineWidth(1.0);
				break;
			}
		}

		glPopMatrix();
	}
}

static void drawTransformView(const struct bContext *C, ARegion *UNUSED(ar), void *arg)
{
	TransInfo *t = arg;

	drawConstraint(t);
	drawPropCircle(C, t);
	drawSnapping(C, t);

	/* edge slide, vert slide */
	drawEdgeSlide(C, t);
	drawVertSlide(C, t);
}

/* just draw a little warning message in the top-right corner of the viewport to warn that autokeying is enabled */
static void drawAutoKeyWarning(TransInfo *UNUSED(t), ARegion *ar)
{
	rcti rect;
	const char *printable = IFACE_("Auto Keying On");
	float      printable_size[2];
	int xco, yco;

	ED_region_visible_rect(ar, &rect);
	
	BLF_width_and_height_default(printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);
	
	xco = (rect.xmax - U.widget_unit) - (int)printable_size[0];
	yco = (rect.ymax - U.widget_unit);
	
	/* warning text (to clarify meaning of overlays)
	 * - original color was red to match the icon, but that clashes badly with a less nasty border
	 */
	UI_ThemeColorShade(TH_TEXT_HI, -50);
#ifdef WITH_INTERNATIONAL
	BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#else
	BLF_draw_default_ascii(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#endif
	
	/* autokey recording icon... */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	xco -= U.widget_unit;
	yco -= (int)printable_size[1] / 2;

	UI_icon_draw(xco, yco, ICON_REC);
	
	glDisable(GL_BLEND);
}

static void drawTransformPixel(const struct bContext *UNUSED(C), ARegion *ar, void *arg)
{	
	TransInfo *t = arg;
	Scene *scene = t->scene;
	Object *ob = OBACT;
	
	/* draw autokeyframing hint in the corner 
	 * - only draw if enabled (advanced users may be distracted/annoyed), 
	 *   for objects that will be autokeyframed (no point ohterwise),
	 *   AND only for the active region (as showing all is too overwhelming)
	 */
	if ((U.autokey_flag & AUTOKEY_FLAG_NOWARNING) == 0) {
		if (ar == t->ar) {
			if (t->flag & (T_OBJECT | T_POSE)) {
				if (ob && autokeyframe_cfra_can_key(scene, &ob->id)) {
					drawAutoKeyWarning(t, ar);
				}
			}
		}
	}
}

void saveTransform(bContext *C, TransInfo *t, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	int constraint_axis[3] = {0, 0, 0};
	int proportional = 0;
	PropertyRNA *prop;

	// Save back mode in case we're in the generic operator
	if ((prop = RNA_struct_find_property(op->ptr, "mode"))) {
		RNA_property_enum_set(op->ptr, prop, t->mode);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "value"))) {
		const float *values = (t->flag & T_AUTOVALUES) ? t->auto_values : t->values;
		if (RNA_property_array_check(prop)) {
			RNA_property_float_set_array(op->ptr, prop, values);
		}
		else {
			RNA_property_float_set(op->ptr, prop, values[0]);
		}
	}

	/* convert flag to enum */
	switch (t->flag & T_PROP_EDIT_ALL) {
		case T_PROP_EDIT:
			proportional = PROP_EDIT_ON;
			break;
		case (T_PROP_EDIT | T_PROP_CONNECTED):
			proportional = PROP_EDIT_CONNECTED;
			break;
		case (T_PROP_EDIT | T_PROP_PROJECTED):
			proportional = PROP_EDIT_PROJECTED;
			break;
		default:
			proportional = PROP_EDIT_OFF;
			break;
	}

	// If modal, save settings back in scene if not set as operator argument
	if (t->flag & T_MODAL) {
		/* save settings if not set in operator */

		/* skip saving proportional edit if it was not actually used */
		if (!(t->options & CTX_NO_PET)) {
			if ((prop = RNA_struct_find_property(op->ptr, "proportional")) &&
			    !RNA_property_is_set(op->ptr, prop))
			{
				if (t->obedit)
					ts->proportional = proportional;
				else if (t->options & CTX_MASK)
					ts->proportional_mask = (proportional != PROP_EDIT_OFF);
				else
					ts->proportional_objects = (proportional != PROP_EDIT_OFF);
			}

			if ((prop = RNA_struct_find_property(op->ptr, "proportional_size")) &&
			    !RNA_property_is_set(op->ptr, prop))
			{
				ts->proportional_size = t->prop_size;
			}

			if ((prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) &&
			    !RNA_property_is_set(op->ptr, prop))
			{
				ts->prop_mode = t->prop_mode;
			}
		}
		
		/* do we check for parameter? */
		if (t->modifiers & MOD_SNAP) {
			ts->snap_flag |= SCE_SNAP;
		}
		else {
			ts->snap_flag &= ~SCE_SNAP;
		}

		if (t->spacetype == SPACE_VIEW3D) {
			if ((prop = RNA_struct_find_property(op->ptr, "constraint_orientation")) &&
			    !RNA_property_is_set(op->ptr, prop))
			{
				View3D *v3d = t->view;

				v3d->twmode = t->current_orientation;
			}
		}
	}
	
	if (RNA_struct_find_property(op->ptr, "proportional")) {
		RNA_enum_set(op->ptr, "proportional", proportional);
		RNA_enum_set(op->ptr, "proportional_edit_falloff", t->prop_mode);
		RNA_float_set(op->ptr, "proportional_size", t->prop_size);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "axis"))) {
		RNA_property_float_set_array(op->ptr, prop, t->axis);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "mirror"))) {
		RNA_property_boolean_set(op->ptr, prop, t->flag & T_MIRROR);
	}

	if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis"))) {
		/* constraint orientation can be global, event if user selects something else
		 * so use the orientation in the constraint if set
		 * */
		if (t->con.mode & CON_APPLY) {
			RNA_enum_set(op->ptr, "constraint_orientation", t->con.orientation);
		}
		else {
			RNA_enum_set(op->ptr, "constraint_orientation", t->current_orientation);
		}

		if (t->con.mode & CON_APPLY) {
			if (t->con.mode & CON_AXIS0) {
				constraint_axis[0] = 1;
			}
			if (t->con.mode & CON_AXIS1) {
				constraint_axis[1] = 1;
			}
			if (t->con.mode & CON_AXIS2) {
				constraint_axis[2] = 1;
			}
		}

		RNA_property_boolean_set_array(op->ptr, prop, constraint_axis);
	}
}

/* note: caller needs to free 't' on a 0 return */
bool initTransform(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event, int mode)
{
	int options = 0;
	PropertyRNA *prop;

	t->context = C;

	/* added initialize, for external calls to set stuff in TransInfo, like undo string */

	t->state = TRANS_STARTING;

	if ((prop = RNA_struct_find_property(op->ptr, "texture_space")) && RNA_property_is_set(op->ptr, prop)) {
		if (RNA_property_boolean_get(op->ptr, prop)) {
			options |= CTX_TEXTURE;
		}
	}

	t->options = options;

	t->mode = mode;

	t->launch_event = event ? event->type : -1;

	if (t->launch_event == EVT_TWEAK_R) {
		t->launch_event = RIGHTMOUSE;
	}
	else if (t->launch_event == EVT_TWEAK_L) {
		t->launch_event = LEFTMOUSE;
	}

	// XXX Remove this when wm_operator_call_internal doesn't use window->eventstate (which can have type = 0)
	// For manipulator only, so assume LEFTMOUSE
	if (t->launch_event == 0) {
		t->launch_event = LEFTMOUSE;
	}

	initTransInfo(C, t, op, event);

	if (t->spacetype == SPACE_VIEW3D) {
		//calc_manipulator_stats(curarea);
		initTransformOrientation(C, t);

		t->draw_handle_apply = ED_region_draw_cb_activate(t->ar->type, drawTransformApply, t, REGION_DRAW_PRE_VIEW);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		t->draw_handle_pixel = ED_region_draw_cb_activate(t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
		t->draw_handle_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), helpline_poll, drawHelpline, t);
	}
	else if (t->spacetype == SPACE_IMAGE) {
		unit_m3(t->spacemtx);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		//t->draw_handle_pixel = ED_region_draw_cb_activate(t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
		t->draw_handle_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), helpline_poll, drawHelpline, t);
	}
	else if (t->spacetype == SPACE_CLIP) {
		unit_m3(t->spacemtx);
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		t->draw_handle_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), helpline_poll, drawHelpline, t);
	}
	else if (t->spacetype == SPACE_NODE) {
		unit_m3(t->spacemtx);
		/*t->draw_handle_apply = ED_region_draw_cb_activate(t->ar->type, drawTransformApply, t, REGION_DRAW_PRE_VIEW);*/
		t->draw_handle_view = ED_region_draw_cb_activate(t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
		t->draw_handle_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), helpline_poll, drawHelpline, t);
	}
	else
		unit_m3(t->spacemtx);

	createTransData(C, t);          // make TransData structs from selection

	if (t->total == 0) {
		postTrans(C, t);
		return 0;
	}

	if (event) {
		/* keymap for shortcut header prints */
		t->keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);

		/* Stupid code to have Ctrl-Click on manipulator work ok
		 *
		 * do this only for translation/rotation/resize due to only this
		 * moded are available from manipulator and doing such check could
		 * lead to keymap conflicts for other modes (see #31584)
		 */
		if (ELEM(mode, TFM_TRANSLATION, TFM_ROTATION, TFM_RESIZE)) {
			wmKeyMapItem *kmi;

			for (kmi = t->keymap->items.first; kmi; kmi = kmi->next) {
				if (kmi->propvalue == TFM_MODAL_SNAP_INV_ON && kmi->val == KM_PRESS) {
					if ((ELEM(kmi->type, LEFTCTRLKEY, RIGHTCTRLKEY) &&   event->ctrl)  ||
					    (ELEM(kmi->type, LEFTSHIFTKEY, RIGHTSHIFTKEY) && event->shift) ||
					    (ELEM(kmi->type, LEFTALTKEY, RIGHTALTKEY) &&     event->alt)   ||
					    ((kmi->type == OSKEY) &&                         event->oskey) )
					{
						t->modifiers |= MOD_SNAP_INVERT;
					}
					break;
				}
			}
		}
	}

	initSnapping(t, op); // Initialize snapping data AFTER mode flags

	/* EVIL! posemode code can switch translation to rotate when 1 bone is selected. will be removed (ton) */
	/* EVIL2: we gave as argument also texture space context bit... was cleared */
	/* EVIL3: extend mode for animation editors also switches modes... but is best way to avoid duplicate code */
	mode = t->mode;

	calculatePropRatio(t);
	calculateCenter(t);

	initMouseInput(t, &t->mouse, t->center2d, t->imval);

	switch (mode) {
		case TFM_TRANSLATION:
			initTranslation(t);
			break;
		case TFM_ROTATION:
			initRotation(t);
			break;
		case TFM_RESIZE:
			initResize(t);
			break;
		case TFM_SKIN_RESIZE:
			initSkinResize(t);
			break;
		case TFM_TOSPHERE:
			initToSphere(t);
			break;
		case TFM_SHEAR:
			initShear(t);
			break;
		case TFM_BEND:
			initBend(t);
			break;
		case TFM_SHRINKFATTEN:
			initShrinkFatten(t);
			break;
		case TFM_TILT:
			initTilt(t);
			break;
		case TFM_CURVE_SHRINKFATTEN:
			initCurveShrinkFatten(t);
			break;
		case TFM_MASK_SHRINKFATTEN:
			initMaskShrinkFatten(t);
			break;
		case TFM_TRACKBALL:
			initTrackball(t);
			break;
		case TFM_PUSHPULL:
			initPushPull(t);
			break;
		case TFM_CREASE:
			initCrease(t);
			break;
		case TFM_BONESIZE:
		{   /* used for both B-Bone width (bonesize) as for deform-dist (envelope) */
			bArmature *arm = t->poseobj->data;
			if (arm->drawtype == ARM_ENVELOPE)
				initBoneEnvelope(t);
			else
				initBoneSize(t);
			break;
		}
		case TFM_BONE_ENVELOPE:
			initBoneEnvelope(t);
			break;
		case TFM_EDGE_SLIDE:
			initEdgeSlide(t);
			break;
		case TFM_VERT_SLIDE:
			initVertSlide(t);
			break;
		case TFM_BONE_ROLL:
			initBoneRoll(t);
			break;
		case TFM_TIME_TRANSLATE:
			initTimeTranslate(t);
			break;
		case TFM_TIME_SLIDE:
			initTimeSlide(t);
			break;
		case TFM_TIME_SCALE:
			initTimeScale(t);
			break;
		case TFM_TIME_DUPLICATE:
			/* same as TFM_TIME_EXTEND, but we need the mode info for later
			 * so that duplicate-culling will work properly
			 */
			if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA))
				initTranslation(t);
			else
				initTimeTranslate(t);
			t->mode = mode;
			break;
		case TFM_TIME_EXTEND:
			/* now that transdata has been made, do like for TFM_TIME_TRANSLATE (for most Animation
			 * Editors because they have only 1D transforms for time values) or TFM_TRANSLATION
			 * (for Graph/NLA Editors only since they uses 'standard' transforms to get 2D movement)
			 * depending on which editor this was called from
			 */
			if (ELEM(t->spacetype, SPACE_IPO, SPACE_NLA))
				initTranslation(t);
			else
				initTimeTranslate(t);
			break;
		case TFM_BAKE_TIME:
			initBakeTime(t);
			break;
		case TFM_MIRROR:
			initMirror(t);
			break;
		case TFM_BWEIGHT:
			initBevelWeight(t);
			break;
		case TFM_ALIGN:
			initAlign(t);
			break;
		case TFM_SEQ_SLIDE:
			initSeqSlide(t);
			break;
	}

	if (t->state == TRANS_CANCEL) {
		postTrans(C, t);
		return 0;
	}


	/* overwrite initial values if operator supplied a non-null vector */
	if ((prop = RNA_struct_find_property(op->ptr, "value")) && RNA_property_is_set(op->ptr, prop)) {
		float values[4] = {0}; /* in case value isn't length 4, avoid uninitialized memory  */

		if (RNA_property_array_check(prop)) {
			RNA_float_get_array(op->ptr, "value", values);
		}
		else {
			values[0] = RNA_float_get(op->ptr, "value");
		}

		copy_v4_v4(t->values, values);
		copy_v4_v4(t->auto_values, values);
		t->flag |= T_AUTOVALUES;
	}

	/* Transformation axis from operator */
	if ((prop = RNA_struct_find_property(op->ptr, "axis")) && RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_get_array(op->ptr, prop, t->axis);
		normalize_v3(t->axis);
		copy_v3_v3(t->axis_orig, t->axis);
	}

	/* Constraint init from operator */
	if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis")) && RNA_property_is_set(op->ptr, prop)) {
		int constraint_axis[3];

		RNA_property_boolean_get_array(op->ptr, prop, constraint_axis);

		if (constraint_axis[0] || constraint_axis[1] || constraint_axis[2]) {
			t->con.mode |= CON_APPLY;

			if (constraint_axis[0]) {
				t->con.mode |= CON_AXIS0;
			}
			if (constraint_axis[1]) {
				t->con.mode |= CON_AXIS1;
			}
			if (constraint_axis[2]) {
				t->con.mode |= CON_AXIS2;
			}

			setUserConstraint(t, t->current_orientation, t->con.mode, "%s");
		}
	}

	t->context = NULL;

	return 1;
}

void transformApply(bContext *C, TransInfo *t)
{
	t->context = C;

	if ((t->redraw & TREDRAW_HARD) || (t->draw_handle_apply == NULL && (t->redraw & TREDRAW_SOFT))) {
		selectConstraint(t);
		if (t->transform) {
			t->transform(t, t->mval);  // calls recalcData()
			viewRedrawForce(C, t);
		}
		t->redraw = TREDRAW_NOTHING;
	}
	else if (t->redraw & TREDRAW_SOFT) {
		viewRedrawForce(C, t);
	}

	/* If auto confirm is on, break after one pass */
	if (t->options & CTX_AUTOCONFIRM) {
		t->state = TRANS_CONFIRM;
	}

	t->context = NULL;
}

static void drawTransformApply(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	TransInfo *t = arg;

	if (t->redraw & TREDRAW_SOFT) {
		t->redraw |= TREDRAW_HARD;
		transformApply((bContext *)C, t);
	}
}

int transformEnd(bContext *C, TransInfo *t)
{
	int exit_code = OPERATOR_RUNNING_MODAL;

	t->context = C;

	if (t->state != TRANS_STARTING && t->state != TRANS_RUNNING) {
		/* handle restoring objects */
		if (t->state == TRANS_CANCEL) {
			/* exception, edge slide transformed UVs too */
			if (t->mode == TFM_EDGE_SLIDE)
				doEdgeSlide(t, 0.0f);
			
			exit_code = OPERATOR_CANCELLED;
			restoreTransObjects(t); // calls recalcData()
		}
		else {
			exit_code = OPERATOR_FINISHED;
		}

		/* aftertrans does insert keyframes, and clears base flags; doesn't read transdata */
		special_aftertrans_update(C, t);

		/* free data */
		postTrans(C, t);

		/* send events out for redraws */
		viewRedrawPost(C, t);

		viewRedrawForce(C, t);
	}

	t->context = NULL;

	return exit_code;
}

/* ************************** TRANSFORM LOCKS **************************** */

static void protectedTransBits(short protectflag, float vec[3])
{
	if (protectflag & OB_LOCK_LOCX)
		vec[0] = 0.0f;
	if (protectflag & OB_LOCK_LOCY)
		vec[1] = 0.0f;
	if (protectflag & OB_LOCK_LOCZ)
		vec[2] = 0.0f;
}

static void protectedSizeBits(short protectflag, float size[3])
{
	if (protectflag & OB_LOCK_SCALEX)
		size[0] = 1.0f;
	if (protectflag & OB_LOCK_SCALEY)
		size[1] = 1.0f;
	if (protectflag & OB_LOCK_SCALEZ)
		size[2] = 1.0f;
}

static void protectedRotateBits(short protectflag, float eul[3], const float oldeul[3])
{
	if (protectflag & OB_LOCK_ROTX)
		eul[0] = oldeul[0];
	if (protectflag & OB_LOCK_ROTY)
		eul[1] = oldeul[1];
	if (protectflag & OB_LOCK_ROTZ)
		eul[2] = oldeul[2];
}


/* this function only does the delta rotation */
/* axis-angle is usually internally stored as quats... */
static void protectedAxisAngleBits(short protectflag, float axis[3], float *angle, float oldAxis[3], float oldAngle)
{
	/* check that protection flags are set */
	if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0)
		return;
	
	if (protectflag & OB_LOCK_ROT4D) {
		/* axis-angle getting limited as 4D entities that they are... */
		if (protectflag & OB_LOCK_ROTW)
			*angle = oldAngle;
		if (protectflag & OB_LOCK_ROTX)
			axis[0] = oldAxis[0];
		if (protectflag & OB_LOCK_ROTY)
			axis[1] = oldAxis[1];
		if (protectflag & OB_LOCK_ROTZ)
			axis[2] = oldAxis[2];
	}
	else {
		/* axis-angle get limited with euler... */
		float eul[3], oldeul[3];
		
		axis_angle_to_eulO(eul, EULER_ORDER_DEFAULT, axis, *angle);
		axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, oldAxis, oldAngle);
		
		if (protectflag & OB_LOCK_ROTX)
			eul[0] = oldeul[0];
		if (protectflag & OB_LOCK_ROTY)
			eul[1] = oldeul[1];
		if (protectflag & OB_LOCK_ROTZ)
			eul[2] = oldeul[2];
		
		eulO_to_axis_angle(axis, angle, eul, EULER_ORDER_DEFAULT);
		
		/* when converting to axis-angle, we need a special exception for the case when there is no axis */
		if (IS_EQF(axis[0], axis[1]) && IS_EQF(axis[1], axis[2])) {
			/* for now, rotate around y-axis then (so that it simply becomes the roll) */
			axis[1] = 1.0f;
		}
	}
}

/* this function only does the delta rotation */
static void protectedQuaternionBits(short protectflag, float quat[4], const float oldquat[4])
{
	/* check that protection flags are set */
	if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0)
		return;
	
	if (protectflag & OB_LOCK_ROT4D) {
		/* quaternions getting limited as 4D entities that they are... */
		if (protectflag & OB_LOCK_ROTW)
			quat[0] = oldquat[0];
		if (protectflag & OB_LOCK_ROTX)
			quat[1] = oldquat[1];
		if (protectflag & OB_LOCK_ROTY)
			quat[2] = oldquat[2];
		if (protectflag & OB_LOCK_ROTZ)
			quat[3] = oldquat[3];
	}
	else {
		/* quaternions get limited with euler... (compatibility mode) */
		float eul[3], oldeul[3], nquat[4], noldquat[4];
		float qlen;

		qlen = normalize_qt_qt(nquat, quat);
		normalize_qt_qt(noldquat, oldquat);

		quat_to_eul(eul, nquat);
		quat_to_eul(oldeul, noldquat);

		if (protectflag & OB_LOCK_ROTX)
			eul[0] = oldeul[0];
		if (protectflag & OB_LOCK_ROTY)
			eul[1] = oldeul[1];
		if (protectflag & OB_LOCK_ROTZ)
			eul[2] = oldeul[2];

		eul_to_quat(quat, eul);

		/* restore original quat size */
		mul_qt_fl(quat, qlen);
		
		/* quaternions flip w sign to accumulate rotations correctly */
		if ((nquat[0] < 0.0f && quat[0] > 0.0f) ||
		    (nquat[0] > 0.0f && quat[0] < 0.0f))
		{
			mul_qt_fl(quat, -1.0f);
		}
	}
}

/* ******************* TRANSFORM LIMITS ********************** */

static void constraintTransLim(TransInfo *t, TransData *td)
{
	if (td->con) {
		bConstraintTypeInfo *ctiLoc = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_LOCLIMIT);
		bConstraintTypeInfo *ctiDist = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_DISTLIMIT);
		
		bConstraintOb cob = {NULL};
		bConstraint *con;
		float ctime = (float)(t->scene->r.cfra);
		
		/* Make a temporary bConstraintOb for using these limit constraints
		 *  - they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		unit_m4(cob.matrix);
		copy_v3_v3(cob.matrix[3], td->loc);
		
		/* Evaluate valid constraints */
		for (con = td->con; con; con = con->next) {
			bConstraintTypeInfo *cti = NULL;
			ListBase targets = {NULL, NULL};
			
			/* only consider constraint if enabled */
			if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) continue;
			if (con->enforce == 0.0f) continue;
			
			/* only use it if it's tagged for this purpose (and the right type) */
			if (con->type == CONSTRAINT_TYPE_LOCLIMIT) {
				bLocLimitConstraint *data = con->data;
				
				if ((data->flag2 & LIMIT_TRANSFORM) == 0)
					continue;
				cti = ctiLoc;
			}
			else if (con->type == CONSTRAINT_TYPE_DISTLIMIT) {
				bDistLimitConstraint *data = con->data;
				
				if ((data->flag & LIMITDIST_TRANSFORM) == 0)
					continue;
				cti = ctiDist;
			}
			
			if (cti) {
				/* do space conversions */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
				}
				else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
					/* skip... incompatable spacetype */
					continue;
				}
				
				/* get constraint targets if needed */
				BKE_constraint_targets_for_solving_get(con, &cob, &targets, ctime);
				
				/* do constraint */
				cti->evaluate_constraint(con, &cob, &targets);
				
				/* convert spaces again */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->smtx (this should be ok) */
					mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
				}
				
				/* free targets list */
				BLI_freelistN(&targets);
			}
		}
		
		/* copy results from cob->matrix */
		copy_v3_v3(td->loc, cob.matrix[3]);
	}
}

static void constraintob_from_transdata(bConstraintOb *cob, TransData *td)
{
	/* Make a temporary bConstraintOb for use by limit constraints
	 *  - they only care that cob->matrix is correctly set ;-)
	 *	- current space should be local
	 */
	memset(cob, 0, sizeof(bConstraintOb));
	if (td->ext) {
		if (td->ext->rotOrder == ROT_MODE_QUAT) {
			/* quats */
			/* objects and bones do normalization first too, otherwise
			 * we don't necessarily end up with a rotation matrix, and
			 * then conversion back to quat gives a different result */
			float quat[4];
			normalize_qt_qt(quat, td->ext->quat);
			quat_to_mat4(cob->matrix, quat);
		}
		else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
			/* axis angle */
			axis_angle_to_mat4(cob->matrix, &td->ext->quat[1], td->ext->quat[0]);
		}
		else {
			/* eulers */
			eulO_to_mat4(cob->matrix, td->ext->rot, td->ext->rotOrder);
		}
	}
}

static void constraintRotLim(TransInfo *UNUSED(t), TransData *td)
{
	if (td->con) {
		bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_ROTLIMIT);
		bConstraintOb cob;
		bConstraint *con;
		bool do_limit = false;

		/* Evaluate valid constraints */
		for (con = td->con; con; con = con->next) {
			/* only consider constraint if enabled */
			if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) continue;
			if (con->enforce == 0.0f) continue;

			/* we're only interested in Limit-Rotation constraints */
			if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
				bRotLimitConstraint *data = con->data;

				/* only use it if it's tagged for this purpose */
				if ((data->flag2 & LIMIT_TRANSFORM) == 0)
					continue;

				/* skip incompatable spacetypes */
				if (!ELEM(con->ownspace, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL))
					continue;

				/* only do conversion if necessary, to preserve quats and eulers */
				if (do_limit == false) {
					constraintob_from_transdata(&cob, td);
					do_limit = true;
				}

				/* do space conversions */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
				}
				
				/* do constraint */
				cti->evaluate_constraint(con, &cob, NULL);
				
				/* convert spaces again */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->smtx (this should be ok) */
					mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
				}
			}
		}
		
		if (do_limit) {
			/* copy results from cob->matrix */
			if (td->ext->rotOrder == ROT_MODE_QUAT) {
				/* quats */
				mat4_to_quat(td->ext->quat, cob.matrix);
			}
			else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
				/* axis angle */
				mat4_to_axis_angle(&td->ext->quat[1], &td->ext->quat[0], cob.matrix);
			}
			else {
				/* eulers */
				mat4_to_eulO(td->ext->rot, td->ext->rotOrder, cob.matrix);
			}
		}
	}
}

static void constraintSizeLim(TransInfo *t, TransData *td)
{
	if (td->con && td->ext) {
		bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_SIZELIMIT);
		bConstraintOb cob = {NULL};
		bConstraint *con;
		float size_sign[3], size_abs[3];
		int i;
		
		/* Make a temporary bConstraintOb for using these limit constraints
		 *  - they only care that cob->matrix is correctly set ;-)
		 *	- current space should be local
		 */
		if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
			/* scale val and reset size */
			return; // TODO: fix this case
		}
		else {
			/* Reset val if SINGLESIZE but using a constraint */
			if (td->flag & TD_SINGLESIZE)
				return;

			/* separate out sign to apply back later */
			for (i = 0; i < 3; i++) {
				size_sign[i] = signf(td->ext->size[i]);
				size_abs[i] = fabsf(td->ext->size[i]);
			}
			
			size_to_mat4(cob.matrix, size_abs);
		}
		
		/* Evaluate valid constraints */
		for (con = td->con; con; con = con->next) {
			/* only consider constraint if enabled */
			if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) continue;
			if (con->enforce == 0.0f) continue;
			
			/* we're only interested in Limit-Scale constraints */
			if (con->type == CONSTRAINT_TYPE_SIZELIMIT) {
				bSizeLimitConstraint *data = con->data;
				
				/* only use it if it's tagged for this purpose */
				if ((data->flag2 & LIMIT_TRANSFORM) == 0)
					continue;
				
				/* do space conversions */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->mtx (this should be ok) */
					mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
				}
				else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
					/* skip... incompatible spacetype */
					continue;
				}
				
				/* do constraint */
				cti->evaluate_constraint(con, &cob, NULL);
				
				/* convert spaces again */
				if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
					/* just multiply by td->smtx (this should be ok) */
					mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
				}
			}
		}

		/* copy results from cob->matrix */
		if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
			/* scale val and reset size */
			return; // TODO: fix this case
		}
		else {
			/* Reset val if SINGLESIZE but using a constraint */
			if (td->flag & TD_SINGLESIZE)
				return;

			/* extrace scale from matrix and apply back sign */
			mat4_to_size(td->ext->size, cob.matrix);
			mul_v3_v3(td->ext->size, size_sign);
		}
	}
}


/* -------------------------------------------------------------------- */
/* Transform (Bend) */

/** \name Transform Bend
 * \{ */

struct BendCustomData {
	float warp_sta[3];
	float warp_end[3];

	float warp_nor[3];
	float warp_tan[3];

	/* for applying the mouse distance */
	float warp_init_dist;
};

static void initBend(TransInfo *t)
{
	const float mval_fl[2] = {UNPACK2(t->mval)};
	const float *curs;
	float tvec[3];
	struct BendCustomData *data;
	
	t->mode = TFM_BEND;
	t->transform = Bend;
	t->handleEvent = handleEventBend;
	
	setInputPostFct(&t->mouse, postInputRotation);
	initMouseInputMode(t, &t->mouse, INPUT_ANGLE_SPRING);
	
	t->idx_max = 1;
	t->num.idx_max = 1;
	t->snap[0] = 0.0f;
	t->snap[1] = DEG2RAD(5.0);
	t->snap[2] = DEG2RAD(1.0);

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
	t->num.unit_type[0] = B_UNIT_ROTATION;
	t->num.unit_type[1] = B_UNIT_LENGTH;

	t->flag |= T_NO_CONSTRAINT;

	//copy_v3_v3(t->center, ED_view3d_cursor3d_get(t->scene, t->view));
	calculateCenterCursor(t, t->center);

	t->val = 0.0f;

	data = MEM_callocN(sizeof(*data), __func__);

	curs = ED_view3d_cursor3d_get(t->scene, t->view);
	copy_v3_v3(data->warp_sta, curs);
	ED_view3d_win_to_3d(t->ar, curs, mval_fl, data->warp_end);

	copy_v3_v3(data->warp_nor, t->viewinv[2]);
	if (t->flag & T_EDIT) {
		sub_v3_v3(data->warp_sta, t->obedit->obmat[3]);
		sub_v3_v3(data->warp_end, t->obedit->obmat[3]);
	}
	normalize_v3(data->warp_nor);

	/* tangent */
	sub_v3_v3v3(tvec, data->warp_end, data->warp_sta);
	cross_v3_v3v3(data->warp_tan, tvec, data->warp_nor);
	normalize_v3(data->warp_tan);

	data->warp_init_dist = len_v3v3(data->warp_end, data->warp_sta);

	t->customData = data;
}

static eRedrawFlag handleEventBend(TransInfo *UNUSED(t), const wmEvent *event)
{
	eRedrawFlag status = TREDRAW_NOTHING;
	
	if (event->type == MIDDLEMOUSE && event->val == KM_PRESS) {
		status = TREDRAW_HARD;
	}
	
	return status;
}

static void Bend(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float vec[3];
	float pivot[3];
	float warp_end_radius[3];
	int i;
	char str[MAX_INFO_LEN];
	const struct BendCustomData *data = t->customData;
	const bool is_clamp = (t->flag & T_ALT_TRANSFORM) == 0;

	union {
		struct { float angle, scale; };
		float vector[2];
	} values;

	/* amount of radians for bend */
	copy_v2_v2(values.vector, t->values);

#if 0
	snapGrid(t, angle_rad);
#else
	/* hrmf, snapping radius is using 'angle' steps, need to convert to something else
	 * this isnt essential but nicer to give reasonable snapping values for radius */
	if (t->tsnap.mode == SCE_SNAP_MODE_INCREMENT) {
		const float radius_snap = 0.1f;
		const float snap_hack = (t->snap[1] * data->warp_init_dist) / radius_snap;
		values.scale *= snap_hack;
		snapGridIncrement(t, values.vector);
		values.scale /= snap_hack;
	}
#endif

	if (applyNumInput(&t->num, values.vector)) {
		values.scale = values.scale / data->warp_init_dist;
	}

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN * 2];

		outputNumInput(&(t->num), c, &t->scene->unit);
		
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Bend Angle: %s Radius: %s Alt, Clamp %s"),
		             &c[0], &c[NUM_STR_REP_LEN],
		             WM_bool_as_string(is_clamp));
	}
	else {
		/* default header print */
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Bend Angle: %.3f Radius: %.4f, Alt, Clamp %s"),
		             RAD2DEGF(values.angle), values.scale * data->warp_init_dist,
		             WM_bool_as_string(is_clamp));
	}
	
	copy_v2_v2(t->values, values.vector);

	values.angle *= -1.0f;
	values.scale *= data->warp_init_dist;
	
	/* calc 'data->warp_end' from 'data->warp_end_init' */
	copy_v3_v3(warp_end_radius, data->warp_end);
	dist_ensure_v3_v3fl(warp_end_radius, data->warp_sta, values.scale);
	/* done */

	/* calculate pivot */
	copy_v3_v3(pivot, data->warp_sta);
	if (values.angle > 0.0f) {
		madd_v3_v3fl(pivot, data->warp_tan, -values.scale * shell_angle_to_dist((float)M_PI_2 - values.angle));
	}
	else {
		madd_v3_v3fl(pivot, data->warp_tan, +values.scale * shell_angle_to_dist((float)M_PI_2 + values.angle));
	}

	for (i = 0; i < t->total; i++, td++) {
		float mat[3][3];
		float delta[3];
		float fac, fac_scaled;

		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;

		if (UNLIKELY(values.angle == 0.0f)) {
			copy_v3_v3(td->loc, td->iloc);
			continue;
		}

		copy_v3_v3(vec, td->iloc);
		mul_m3_v3(td->mtx, vec);

		fac = line_point_factor_v3(vec, data->warp_sta, warp_end_radius);
		if (is_clamp) {
			CLAMP(fac, 0.0f, 1.0f);
		}

		fac_scaled = fac * td->factor;
		axis_angle_normalized_to_mat3(mat, data->warp_nor, values.angle * fac_scaled);
		interp_v3_v3v3(delta, data->warp_sta, warp_end_radius, fac_scaled);
		sub_v3_v3(delta, data->warp_sta);

		/* delta is subtracted, rotation adds back this offset */
		sub_v3_v3(vec, delta);

		sub_v3_v3(vec, pivot);
		mul_m3_v3(mat, vec);
		add_v3_v3(vec, pivot);

		mul_m3_v3(td->smtx, vec);
		copy_v3_v3(td->loc, vec);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Shear) */

/** \name Transform Shear
 * \{ */

static void postInputShear(TransInfo *UNUSED(t), float values[3])
{
	mul_v3_fl(values, 0.05f);
}

static void initShear(TransInfo *t)
{
	t->mode = TFM_SHEAR;
	t->transform = applyShear;
	t->handleEvent = handleEventShear;
	
	setInputPostFct(&t->mouse, postInputShear);
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;  /* Don't think we have any unit here? */

	t->flag |= T_NO_CONSTRAINT;
}

static eRedrawFlag handleEventShear(TransInfo *t, const wmEvent *event)
{
	eRedrawFlag status = TREDRAW_NOTHING;
	
	if (event->type == MIDDLEMOUSE && event->val == KM_PRESS) {
		// Use customData pointer to signal Shear direction
		if (t->customData == NULL) {
			initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);
			t->customData = (void *)1;
		}
		else {
			initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
			t->customData = NULL;
		}

		status = TREDRAW_HARD;
	}
	else if (event->type == XKEY && event->val == KM_PRESS) {
		initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_ABSOLUTE);
		t->customData = NULL;
		
		status = TREDRAW_HARD;
	}
	else if (event->type == YKEY && event->val == KM_PRESS) {
		initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);
		t->customData = (void *)1;
		
		status = TREDRAW_HARD;
	}
	
	return status;
}


static void applyShear(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float vec[3];
	float smat[3][3], tmat[3][3], totmat[3][3], persmat[3][3], persinv[3][3];
	float value;
	int i;
	char str[MAX_INFO_LEN];
	
	copy_m3_m4(persmat, t->viewmat);
	invert_m3_m3(persinv, persmat);
	
	value = t->values[0];
	
	snapGridIncrement(t, &value);
	
	applyNumInput(&t->num, &value);
	
	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];
		
		outputNumInput(&(t->num), c, &t->scene->unit);
		
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Shear: %s %s"), c, t->proptext);
	}
	else {
		/* default header print */
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Shear: %.3f %s (Press X or Y to set shear axis)"), value, t->proptext);
	}
	
	t->values[0] = value;

	unit_m3(smat);
	
	// Custom data signals shear direction
	if (t->customData == NULL)
		smat[1][0] = value;
	else
		smat[0][1] = value;
	
	mul_m3_m3m3(tmat, smat, persmat);
	mul_m3_m3m3(totmat, persinv, tmat);
	
	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if (t->obedit) {
			float mat3[3][3];
			mul_m3_m3m3(mat3, totmat, td->mtx);
			mul_m3_m3m3(tmat, td->smtx, mat3);
		}
		else {
			copy_m3_m3(tmat, totmat);
		}
		sub_v3_v3v3(vec, td->center, t->center);
		
		mul_m3_v3(tmat, vec);
		
		add_v3_v3(vec, t->center);
		sub_v3_v3(vec, td->center);
		
		mul_v3_fl(vec, td->factor);
		
		add_v3_v3v3(td->loc, td->iloc, vec);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Resize) */

/** \name Transform Resize
 * \{ */

static void initResize(TransInfo *t)
{
	t->mode = TFM_RESIZE;
	t->transform = applyResize;
	
	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);
	
	t->flag |= T_NULL_ONE;
	t->num.val_flag[0] |= NUM_NULL_ONE;
	t->num.val_flag[1] |= NUM_NULL_ONE;
	t->num.val_flag[2] |= NUM_NULL_ONE;
	t->num.flag |= NUM_AFFECT_ALL;
	if (!t->obedit) {
		t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
		t->num.val_flag[0] |= NUM_NO_ZERO;
		t->num.val_flag[1] |= NUM_NO_ZERO;
		t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
	}
	
	t->idx_max = 2;
	t->num.idx_max = 2;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;
	t->num.unit_type[1] = B_UNIT_NONE;
	t->num.unit_type[2] = B_UNIT_NONE;
}

static void headerResize(TransInfo *t, float vec[3], char str[MAX_INFO_LEN])
{
	char tvec[NUM_STR_REP_LEN * 3];
	size_t ofs = 0;
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec, &t->scene->unit);
	}
	else {
		BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
		BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
		BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
	}

	if (t->con.mode & CON_APPLY) {
		switch (t->num.idx_max) {
			case 0:
				ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Scale: %s%s %s"),
				                    &tvec[0], t->con.text, t->proptext);
				break;
			case 1:
				ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Scale: %s : %s%s %s"),
				                    &tvec[0], &tvec[NUM_STR_REP_LEN], t->con.text, t->proptext);
				break;
			case 2:
				ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Scale: %s : %s : %s%s %s"), &tvec[0],
				                    &tvec[NUM_STR_REP_LEN], &tvec[NUM_STR_REP_LEN * 2], t->con.text, t->proptext);
				break;
		}
	}
	else {
		if (t->flag & T_2D_EDIT) {
			ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Scale X: %s   Y: %s%s %s"),
			                    &tvec[0], &tvec[NUM_STR_REP_LEN], t->con.text, t->proptext);
		}
		else {
			ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Scale X: %s   Y: %s  Z: %s%s %s"),
			                    &tvec[0], &tvec[NUM_STR_REP_LEN], &tvec[NUM_STR_REP_LEN * 2], t->con.text, t->proptext);
		}
	}

	if (t->flag & T_PROP_EDIT_ALL) {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
	}
}

/* FLT_EPSILON is too small [#29633], 0.0000001f starts to flip */
#define TX_FLIP_EPS 0.00001f
BLI_INLINE int tx_sign(const float a)
{
	return (a < -TX_FLIP_EPS ? 1 : a > TX_FLIP_EPS ? 2 : 3);
}
BLI_INLINE int tx_vec_sign_flip(const float a[3], const float b[3])
{
	return ((tx_sign(a[0]) & tx_sign(b[0])) == 0 ||
	        (tx_sign(a[1]) & tx_sign(b[1])) == 0 ||
	        (tx_sign(a[2]) & tx_sign(b[2])) == 0);
}

/* smat is reference matrix, only scaled */
static void TransMat3ToSize(float mat[3][3], float smat[3][3], float size[3])
{
	float vec[3];
	
	copy_v3_v3(vec, mat[0]);
	size[0] = normalize_v3(vec);
	copy_v3_v3(vec, mat[1]);
	size[1] = normalize_v3(vec);
	copy_v3_v3(vec, mat[2]);
	size[2] = normalize_v3(vec);
	
	/* first tried with dotproduct... but the sign flip is crucial */
	if (tx_vec_sign_flip(mat[0], smat[0]) ) size[0] = -size[0];
	if (tx_vec_sign_flip(mat[1], smat[1]) ) size[1] = -size[1];
	if (tx_vec_sign_flip(mat[2], smat[2]) ) size[2] = -size[2];
}


static void ElementResize(TransInfo *t, TransData *td, float mat[3][3])
{
	float tmat[3][3], smat[3][3], center[3];
	float vec[3];
	
	if (t->flag & T_EDIT) {
		mul_m3_m3m3(smat, mat, td->mtx);
		mul_m3_m3m3(tmat, td->smtx, smat);
	}
	else {
		copy_m3_m3(tmat, mat);
	}
	
	if (t->con.applySize) {
		t->con.applySize(t, td, tmat);
	}
	
	/* local constraint shouldn't alter center */
	if (transdata_check_local_center(t, t->around)) {
		copy_v3_v3(center, td->center);
	}
	else if (t->options & CTX_MOVIECLIP) {
		if (td->flag & TD_INDIVIDUAL_SCALE) {
			copy_v3_v3(center, td->center);
		}
		else {
			copy_v3_v3(center, t->center);
		}
	}
	else {
		copy_v3_v3(center, t->center);
	}

	if (td->ext) {
		float fsize[3];
		
		if (t->flag & (T_OBJECT | T_TEXTURE | T_POSE)) {
			float obsizemat[3][3];
			/* Reorient the size mat to fit the oriented object. */
			mul_m3_m3m3(obsizemat, tmat, td->axismtx);
			/* print_m3("obsizemat", obsizemat); */
			TransMat3ToSize(obsizemat, td->axismtx, fsize);
			/* print_v3("fsize", fsize); */
		}
		else {
			mat3_to_size(fsize, tmat);
		}
		
		protectedSizeBits(td->protectflag, fsize);
		
		if ((t->flag & T_V3D_ALIGN) == 0) {   /* align mode doesn't resize objects itself */
			if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
				/* scale val and reset size */
				*td->val = td->ival * (1 + (fsize[0] - 1) * td->factor);
				
				td->ext->size[0] = td->ext->isize[0];
				td->ext->size[1] = td->ext->isize[1];
				td->ext->size[2] = td->ext->isize[2];
			}
			else {
				/* Reset val if SINGLESIZE but using a constraint */
				if (td->flag & TD_SINGLESIZE)
					*td->val = td->ival;
				
				td->ext->size[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
				td->ext->size[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
				td->ext->size[2] = td->ext->isize[2] * (1 + (fsize[2] - 1) * td->factor);
			}
		}
		
		constraintSizeLim(t, td);
	}

	/* For individual element center, Editmode need to use iloc */
	if (t->flag & T_POINTS)
		sub_v3_v3v3(vec, td->iloc, center);
	else
		sub_v3_v3v3(vec, td->center, center);
	
	mul_m3_v3(tmat, vec);
	
	add_v3_v3(vec, center);
	if (t->flag & T_POINTS)
		sub_v3_v3(vec, td->iloc);
	else
		sub_v3_v3(vec, td->center);
	
	mul_v3_fl(vec, td->factor);
	
	if (t->flag & (T_OBJECT | T_POSE)) {
		mul_m3_v3(td->smtx, vec);
	}
	
	protectedTransBits(td->protectflag, vec);
	add_v3_v3v3(td->loc, td->iloc, vec);
	
	constraintTransLim(t, td);
}

static void applyResize(TransInfo *t, const int mval[2])
{
	TransData *td;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[MAX_INFO_LEN];

	/* for manipulator, center handle, the scaling can't be done relative to center */
	if ((t->flag & T_USES_MANIPULATOR) && t->con.mode == 0) {
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1])) / 100.0f;
	}
	else {
		ratio = t->values[0];
	}
	
	size[0] = size[1] = size[2] = ratio;
	
	snapGridIncrement(t, size);
	
	if (applyNumInput(&t->num, size)) {
		constraintNumInput(t, size);
	}
	
	applySnapping(t, size);
	
	if (t->flag & T_AUTOVALUES) {
		copy_v3_v3(size, t->auto_values);
	}
	
	copy_v3_v3(t->values, size);
	
	size_to_mat3(mat, size);
	
	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}
	
	copy_m3_m3(t->mat, mat);    // used in manipulator
	
	headerResize(t, size, str);
	
	for (i = 0, td = t->data; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		ElementResize(t, td, mat);
	}
	
	/* evil hack - redo resize if cliping needed */
	if (t->flag & T_CLIP_UV && clipUVTransform(t, size, 1)) {
		size_to_mat3(mat, size);
		
		if (t->con.applySize)
			t->con.applySize(t, NULL, mat);
		
		for (i = 0, td = t->data; i < t->total; i++, td++)
			ElementResize(t, td, mat);

		/* In proportional edit it can happen that */
		/* vertices in the radius of the brush end */
		/* outside the clipping area               */
		/* XXX HACK - dg */
		if (t->flag & T_PROP_EDIT_ALL) {
			clipUVData(t);
		}
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Skin) */

/** \name Transform Skin
 * \{ */

static void initSkinResize(TransInfo *t)
{
	t->mode = TFM_SKIN_RESIZE;
	t->transform = applySkinResize;
	
	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);
	
	t->flag |= T_NULL_ONE;
	t->num.val_flag[0] |= NUM_NULL_ONE;
	t->num.val_flag[1] |= NUM_NULL_ONE;
	t->num.val_flag[2] |= NUM_NULL_ONE;
	t->num.flag |= NUM_AFFECT_ALL;
	if (!t->obedit) {
		t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
		t->num.val_flag[0] |= NUM_NO_ZERO;
		t->num.val_flag[1] |= NUM_NO_ZERO;
		t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
	}
	
	t->idx_max = 2;
	t->num.idx_max = 2;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;
	t->num.unit_type[1] = B_UNIT_NONE;
	t->num.unit_type[2] = B_UNIT_NONE;
}

static void applySkinResize(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[MAX_INFO_LEN];
	
	ratio = t->values[0];
	size[0] = size[1] = size[2] = ratio;
	
	snapGridIncrement(t, size);
	
	if (applyNumInput(&t->num, size)) {
		constraintNumInput(t, size);
	}
	
	applySnapping(t, size);
	
	if (t->flag & T_AUTOVALUES) {
		copy_v3_v3(size, t->auto_values);
	}
	
	copy_v3_v3(t->values, size);
	
	size_to_mat3(mat, size);
	
	headerResize(t, size, str);
	
	for (i = 0, td = t->data; i < t->total; i++, td++) {
		float tmat[3][3], smat[3][3];
		float fsize[3];
		
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;

		if (t->flag & T_EDIT) {
			mul_m3_m3m3(smat, mat, td->mtx);
			mul_m3_m3m3(tmat, td->smtx, smat);
		}
		else {
			copy_m3_m3(tmat, mat);
		}
	
		if (t->con.applySize) {
			t->con.applySize(t, NULL, tmat);
		}

		mat3_to_size(fsize, tmat);
		td->val[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
		td->val[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (ToSphere) */

/** \name Transform ToSphere
 * \{ */

static void initToSphere(TransInfo *t)
{
	TransData *td = t->data;
	int i;
	
	t->mode = TFM_TOSPHERE;
	t->transform = applyToSphere;
	
	initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_RATIO);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	
	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->num.val_flag[0] |= NUM_NULL_ONE | NUM_NO_NEGATIVE;
	t->flag |= T_NO_CONSTRAINT;
	
	// Calculate average radius
	for (i = 0; i < t->total; i++, td++) {
		t->val += len_v3v3(t->center, td->iloc);
	}
	
	t->val /= (float)t->total;
}

static void applyToSphere(TransInfo *t, const int UNUSED(mval[2]))
{
	float vec[3];
	float ratio, radius;
	int i;
	char str[MAX_INFO_LEN];
	TransData *td = t->data;
	
	ratio = t->values[0];
	
	snapGridIncrement(t, &ratio);
	
	applyNumInput(&t->num, &ratio);
	
	if (ratio < 0)
		ratio = 0.0f;
	else if (ratio > 1)
		ratio = 1.0f;
	
	t->values[0] = ratio;

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];
		
		outputNumInput(&(t->num), c, &t->scene->unit);
		
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("To Sphere: %s %s"), c, t->proptext);
	}
	else {
		/* default header print */
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("To Sphere: %.4f %s"), ratio, t->proptext);
	}
	
	
	for (i = 0; i < t->total; i++, td++) {
		float tratio;
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		sub_v3_v3v3(vec, td->iloc, t->center);
		
		radius = normalize_v3(vec);
		
		tratio = ratio * td->factor;
		
		mul_v3_fl(vec, radius * (1.0f - tratio) + t->val * tratio);
		
		add_v3_v3v3(td->loc, t->center, vec);
	}
	
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Rotation) */

/** \name Transform Rotation
 * \{ */

static void postInputRotation(TransInfo *t, float values[3])
{
	if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
		t->con.applyRot(t, NULL, t->axis, values);
	}
}

static void initRotation(TransInfo *t)
{
	t->mode = TFM_ROTATION;
	t->transform = applyRotation;
	
	setInputPostFct(&t->mouse, postInputRotation);
	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = DEG2RAD(5.0);
	t->snap[2] = DEG2RAD(1.0);
	
	copy_v3_fl(t->num.val_inc, t->snap[2]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
	t->num.unit_type[0] = B_UNIT_ROTATION;

	if (t->flag & T_2D_EDIT)
		t->flag |= T_NO_CONSTRAINT;

	if (t->options & CTX_PAINT_CURVE) {
		t->axis[0] = 0.0;
		t->axis[1] = 0.0;
		t->axis[2] = -1.0;
	}
	else {
		negate_v3_v3(t->axis, t->viewinv[2]);
		normalize_v3(t->axis);
	}

	copy_v3_v3(t->axis_orig, t->axis);
}

static void ElementRotation(TransInfo *t, TransData *td, float mat[3][3], short around)
{
	float vec[3], totmat[3][3], smat[3][3];
	float eul[3], fmat[3][3], quat[4];
	const float *center;

	/* local constraint shouldn't alter center */
	if (transdata_check_local_center(t, around)) {
		center = td->center;
	}
	else {
		center = t->center;
	}

	if (t->flag & T_POINTS) {
		mul_m3_m3m3(totmat, mat, td->mtx);
		mul_m3_m3m3(smat, td->smtx, totmat);
		
		sub_v3_v3v3(vec, td->iloc, center);
		mul_m3_v3(smat, vec);
		
		add_v3_v3v3(td->loc, vec, center);
		
		sub_v3_v3v3(vec, td->loc, td->iloc);
		protectedTransBits(td->protectflag, vec);
		add_v3_v3v3(td->loc, td->iloc, vec);
		
		
		if (td->flag & TD_USEQUAT) {
			mul_m3_series(fmat, td->smtx, mat, td->mtx);
			mat3_to_quat(quat, fmat);   // Actual transform
			
			if (td->ext->quat) {
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				
				/* is there a reason not to have this here? -jahka */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
			}
		}
	}
	/**
	 * HACK WARNING
	 *
	 * This is some VERY ugly special case to deal with pose mode.
	 *
	 * The problem is that mtx and smtx include each bone orientation.
	 *
	 * That is needed to rotate each bone properly, HOWEVER, to calculate
	 * the translation component, we only need the actual armature object's
	 * matrix (and inverse). That is not all though. Once the proper translation
	 * has been computed, it has to be converted back into the bone's space.
	 */
	else if (t->flag & T_POSE) {
		float pmtx[3][3], imtx[3][3];
		
		// Extract and invert armature object matrix
		copy_m3_m4(pmtx, t->poseobj->obmat);
		invert_m3_m3(imtx, pmtx);
		
		if ((td->flag & TD_NO_LOC) == 0) {
			sub_v3_v3v3(vec, td->center, center);
			
			mul_m3_v3(pmtx, vec);   // To Global space
			mul_m3_v3(mat, vec);        // Applying rotation
			mul_m3_v3(imtx, vec);   // To Local space
			
			add_v3_v3(vec, center);
			/* vec now is the location where the object has to be */
			
			sub_v3_v3v3(vec, vec, td->center); // Translation needed from the initial location
			
			/* special exception, see TD_PBONE_LOCAL_MTX definition comments */
			if (td->flag & TD_PBONE_LOCAL_MTX_P) {
				/* do nothing */
			}
			else if (td->flag & TD_PBONE_LOCAL_MTX_C) {
				mul_m3_v3(pmtx, vec);   // To Global space
				mul_m3_v3(td->ext->l_smtx, vec); // To Pose space (Local Location)
			}
			else {
				mul_m3_v3(pmtx, vec);   // To Global space
				mul_m3_v3(td->smtx, vec); // To Pose space
			}

			protectedTransBits(td->protectflag, vec);
			
			add_v3_v3v3(td->loc, td->iloc, vec);
			
			constraintTransLim(t, td);
		}
		
		/* rotation */
		/* MORE HACK: as in some cases the matrix to apply location and rot/scale is not the same,
		 * and ElementRotation() might be called in Translation context (with align snapping),
		 * we need to be sure to actually use the *rotation* matrix here...
		 * So no other way than storing it in some dedicated members of td->ext! */
		if ((t->flag & T_V3D_ALIGN) == 0) { /* align mode doesn't rotate objects itself */
			/* euler or quaternion/axis-angle? */
			if (td->ext->rotOrder == ROT_MODE_QUAT) {
				mul_m3_series(fmat, td->ext->r_smtx, mat, td->ext->r_mtx);
				
				mat3_to_quat(quat, fmat); /* Actual transform */
				
				mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
				/* this function works on end result */
				protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
				
			}
			else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
				/* calculate effect based on quats */
				float iquat[4], tquat[4];
				
				axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);
				
				mul_m3_series(fmat, td->ext->r_smtx, mat, td->ext->r_mtx);
				mat3_to_quat(quat, fmat); /* Actual transform */
				mul_qt_qtqt(tquat, quat, iquat);
				
				quat_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, tquat);
				
				/* this function works on end result */
				protectedAxisAngleBits(td->protectflag, td->ext->rotAxis, td->ext->rotAngle, td->ext->irotAxis,
				                       td->ext->irotAngle);
			}
			else {
				float eulmat[3][3];
				
				mul_m3_m3m3(totmat, mat, td->ext->r_mtx);
				mul_m3_m3m3(smat, td->ext->r_smtx, totmat);
				
				/* calculate the total rotatation in eulers */
				copy_v3_v3(eul, td->ext->irot);
				eulO_to_mat3(eulmat, eul, td->ext->rotOrder);
				
				/* mat = transform, obmat = bone rotation */
				mul_m3_m3m3(fmat, smat, eulmat);
				
				mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);
				
				/* and apply (to end result only) */
				protectedRotateBits(td->protectflag, eul, td->ext->irot);
				copy_v3_v3(td->ext->rot, eul);
			}
			
			constraintRotLim(t, td);
		}
	}
	else {
		if ((td->flag & TD_NO_LOC) == 0) {
			/* translation */
			sub_v3_v3v3(vec, td->center, center);
			mul_m3_v3(mat, vec);
			add_v3_v3(vec, center);
			/* vec now is the location where the object has to be */
			sub_v3_v3(vec, td->center);
			mul_m3_v3(td->smtx, vec);
			
			protectedTransBits(td->protectflag, vec);
			
			add_v3_v3v3(td->loc, td->iloc, vec);
		}
		
		
		constraintTransLim(t, td);
		
		/* rotation */
		if ((t->flag & T_V3D_ALIGN) == 0) { // align mode doesn't rotate objects itself
			/* euler or quaternion? */
			if ((td->ext->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
				/* can be called for texture space translate for example, then opt out */
				if (td->ext->quat) {
					mul_m3_series(fmat, td->smtx, mat, td->mtx);
					mat3_to_quat(quat, fmat);   // Actual transform
					
					mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
					/* this function works on end result */
					protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
				}
			}
			else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
				/* calculate effect based on quats */
				float iquat[4], tquat[4];
				
				axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);
				
				mul_m3_series(fmat, td->smtx, mat, td->mtx);
				mat3_to_quat(quat, fmat);   // Actual transform
				mul_qt_qtqt(tquat, quat, iquat);
				
				quat_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, tquat);
				
				/* this function works on end result */
				protectedAxisAngleBits(td->protectflag, td->ext->rotAxis, td->ext->rotAngle, td->ext->irotAxis,
				                       td->ext->irotAngle);
			}
			else {
				float obmat[3][3];
				
				mul_m3_m3m3(totmat, mat, td->mtx);
				mul_m3_m3m3(smat, td->smtx, totmat);
				
				/* calculate the total rotatation in eulers */
				add_v3_v3v3(eul, td->ext->irot, td->ext->drot); /* we have to correct for delta rot */
				eulO_to_mat3(obmat, eul, td->ext->rotOrder);
				/* mat = transform, obmat = object rotation */
				mul_m3_m3m3(fmat, smat, obmat);
				
				mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);
				
				/* correct back for delta rot */
				sub_v3_v3v3(eul, eul, td->ext->drot);
				
				/* and apply */
				protectedRotateBits(td->protectflag, eul, td->ext->irot);
				copy_v3_v3(td->ext->rot, eul);
			}
			
			constraintRotLim(t, td);
		}
	}
}

static void applyRotationValue(TransInfo *t, float angle, float axis[3])
{
	TransData *td = t->data;
	float mat[3][3];
	int i;
	
	axis_angle_normalized_to_mat3(mat, axis, angle);
	
	for (i = 0; i < t->total; i++, td++) {
		
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if (t->con.applyRot) {
			t->con.applyRot(t, td, axis, NULL);
			axis_angle_normalized_to_mat3(mat, axis, angle * td->factor);
		}
		else if (t->flag & T_PROP_EDIT) {
			axis_angle_normalized_to_mat3(mat, axis, angle * td->factor);
		}
		
		ElementRotation(t, td, mat, t->around);
	}
}

static void applyRotation(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];
	size_t ofs = 0;

	float final;

	final = t->values[0];

	snapGridIncrement(t, &final);

	if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
		t->con.applyRot(t, NULL, t->axis, NULL);
	}
	else {
		/* reset axis if constraint is not set */
		copy_v3_v3(t->axis, t->axis_orig);
	}

	applySnapping(t, &final);

	if (applyNumInput(&t->num, &final)) {
		/* Clamp between -PI and PI */
		final = angle_wrap_rad(final);
	}

	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];
		
		outputNumInput(&(t->num), c, &t->scene->unit);
		
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Rot: %s %s %s"), &c[0], t->con.text, t->proptext);
	}
	else {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Rot: %.2f%s %s"),
		                    RAD2DEGF(final), t->con.text, t->proptext);
	}
	
	if (t->flag & T_PROP_EDIT_ALL) {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
	}

	t->values[0] = final;
	
	applyRotationValue(t, final, t->axis);
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Rotation - Trackball) */

/** \name Transform Rotation - Trackball
 * \{ */

static void initTrackball(TransInfo *t)
{
	t->mode = TFM_TRACKBALL;
	t->transform = applyTrackball;

	initMouseInputMode(t, &t->mouse, INPUT_TRACKBALL);

	t->idx_max = 1;
	t->num.idx_max = 1;
	t->snap[0] = 0.0f;
	t->snap[1] = DEG2RAD(5.0);
	t->snap[2] = DEG2RAD(1.0);

	copy_v3_fl(t->num.val_inc, t->snap[2]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
	t->num.unit_type[0] = B_UNIT_ROTATION;
	t->num.unit_type[1] = B_UNIT_ROTATION;

	t->flag |= T_NO_CONSTRAINT;
}

static void applyTrackballValue(TransInfo *t, const float axis1[3], const float axis2[3], float angles[2])
{
	TransData *td = t->data;
	float mat[3][3], smat[3][3], totmat[3][3];
	int i;

	axis_angle_normalized_to_mat3(smat, axis1, angles[0]);
	axis_angle_normalized_to_mat3(totmat, axis2, angles[1]);

	mul_m3_m3m3(mat, smat, totmat);

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (t->flag & T_PROP_EDIT) {
			axis_angle_normalized_to_mat3(smat, axis1, td->factor * angles[0]);
			axis_angle_normalized_to_mat3(totmat, axis2, td->factor * angles[1]);

			mul_m3_m3m3(mat, smat, totmat);
		}

		ElementRotation(t, td, mat, t->around);
	}
}

static void applyTrackball(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];
	size_t ofs = 0;
	float axis1[3], axis2[3];
	float mat[3][3], totmat[3][3], smat[3][3];
	float phi[2];

	copy_v3_v3(axis1, t->persinv[0]);
	copy_v3_v3(axis2, t->persinv[1]);
	normalize_v3(axis1);
	normalize_v3(axis2);

	phi[0] = t->values[0];
	phi[1] = t->values[1];

	snapGridIncrement(t, phi);

	applyNumInput(&t->num, phi);

	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN * 2];

		outputNumInput(&(t->num), c, &t->scene->unit);

		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Trackball: %s %s %s"),
		                    &c[0], &c[NUM_STR_REP_LEN], t->proptext);
	}
	else {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Trackball: %.2f %.2f %s"),
		                    RAD2DEGF(phi[0]), RAD2DEGF(phi[1]), t->proptext);
	}

	if (t->flag & T_PROP_EDIT_ALL) {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
	}

	axis_angle_normalized_to_mat3(smat, axis1, phi[0]);
	axis_angle_normalized_to_mat3(totmat, axis2, phi[1]);

	mul_m3_m3m3(mat, smat, totmat);

	// TRANSFORM_FIX_ME
	//copy_m3_m3(t->mat, mat);	// used in manipulator

	applyTrackballValue(t, axis1, axis2, phi);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Translation) */

/** \name Transform Translation
 * \{ */

static void initTranslation(TransInfo *t)
{
	if (t->spacetype == SPACE_ACTION) {
		/* this space uses time translate */
		BKE_report(t->reports, RPT_ERROR, 
		           "Use 'Time_Translate' transform mode instead of 'Translation' mode "
				   "for translating keyframes in Dope Sheet Editor");
		t->state = TRANS_CANCEL;
	}

	t->mode = TFM_TRANSLATION;
	t->transform = applyTranslation;

	initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

	t->idx_max = (t->flag & T_2D_EDIT) ? 1 : 2;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	if (t->spacetype == SPACE_VIEW3D) {
		RegionView3D *rv3d = t->ar->regiondata;

		if (rv3d) {
			t->snap[0] = 0.0f;
			t->snap[1] = rv3d->gridview * 1.0f;
			t->snap[2] = t->snap[1] * 0.1f;
		}
	}
	else if (ELEM(t->spacetype, SPACE_IMAGE, SPACE_CLIP)) {
		t->snap[0] = 0.0f;
		t->snap[1] = 0.125f;
		t->snap[2] = 0.0625f;
	}
	else if (t->spacetype == SPACE_NODE) {
		t->snap[0] = 0.0f;
		t->snap[1] = ED_node_grid_size() * NODE_GRID_STEPS;
		t->snap[2] = ED_node_grid_size();
	}
	else {
		t->snap[0] = 0.0f;
		t->snap[1] = t->snap[2] = 1.0f;
	}

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	if (t->spacetype == SPACE_VIEW3D) {
		/* Handling units makes only sense in 3Dview... See T38877. */
		t->num.unit_type[0] = B_UNIT_LENGTH;
		t->num.unit_type[1] = B_UNIT_LENGTH;
		t->num.unit_type[2] = B_UNIT_LENGTH;
	}
	else {
		/* SPACE_IPO, SPACE_ACTION, etc. could use some time units, when we have them... */
		t->num.unit_type[0] = B_UNIT_NONE;
		t->num.unit_type[1] = B_UNIT_NONE;
		t->num.unit_type[2] = B_UNIT_NONE;
	}
}

static void headerTranslation(TransInfo *t, float vec[3], char str[MAX_INFO_LEN])
{
	size_t ofs = 0;
	char tvec[NUM_STR_REP_LEN * 3];
	char distvec[NUM_STR_REP_LEN];
	char autoik[NUM_STR_REP_LEN];
	float dist;

	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec, &t->scene->unit);
		dist = len_v3(t->num.val);
	}
	else {
		float dvec[3];

		copy_v3_v3(dvec, vec);
		applyAspectRatio(t, dvec);

		dist = len_v3(vec);
		if (!(t->flag & T_2D_EDIT) && t->scene->unit.system) {
			const bool do_split = (t->scene->unit.flag & USER_UNIT_OPT_SPLIT) != 0;
			int i;

			for (i = 0; i < 3; i++) {
				bUnit_AsString(&tvec[NUM_STR_REP_LEN * i], NUM_STR_REP_LEN, dvec[i] * t->scene->unit.scale_length,
				               4, t->scene->unit.system, B_UNIT_LENGTH, do_split, true);
			}
		}
		else {
			BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", dvec[0]);
			BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", dvec[1]);
			BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", dvec[2]);
		}
	}

	if (!(t->flag & T_2D_EDIT) && t->scene->unit.system) {
		const bool do_split = (t->scene->unit.flag & USER_UNIT_OPT_SPLIT) != 0;
		bUnit_AsString(distvec, sizeof(distvec), dist * t->scene->unit.scale_length, 4, t->scene->unit.system,
		               B_UNIT_LENGTH, do_split, false);
	}
	else if (dist > 1e10f || dist < -1e10f)  {
		/* prevent string buffer overflow */
		BLI_snprintf(distvec, NUM_STR_REP_LEN, "%.4e", dist);
	}
	else {
		BLI_snprintf(distvec, NUM_STR_REP_LEN, "%.4f", dist);
	}

	if (t->flag & T_AUTOIK) {
		short chainlen = t->settings->autoik_chainlen;

		if (chainlen)
			BLI_snprintf(autoik, NUM_STR_REP_LEN, IFACE_("AutoIK-Len: %d"), chainlen);
		else
			autoik[0] = '\0';
	}
	else
		autoik[0] = '\0';

	if (t->con.mode & CON_APPLY) {
		switch (t->num.idx_max) {
			case 0:
				ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, "D: %s (%s)%s %s  %s",
				               &tvec[0], distvec, t->con.text, t->proptext, autoik);
				break;
			case 1:
				ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, "D: %s   D: %s (%s)%s %s  %s",
				                    &tvec[0], &tvec[NUM_STR_REP_LEN], distvec, t->con.text, t->proptext, autoik);
				break;
			case 2:
				ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, "D: %s   D: %s  D: %s (%s)%s %s  %s",
				                    &tvec[0], &tvec[NUM_STR_REP_LEN], &tvec[NUM_STR_REP_LEN * 2], distvec,
				                    t->con.text, t->proptext, autoik);
				break;
		}
	}
	else {
		if (t->flag & T_2D_EDIT) {
			ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, "Dx: %s   Dy: %s (%s)%s %s",
			                    &tvec[0], &tvec[NUM_STR_REP_LEN], distvec, t->con.text, t->proptext);
		}
		else {
			ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, "Dx: %s   Dy: %s  Dz: %s (%s)%s %s  %s",
			                    &tvec[0], &tvec[NUM_STR_REP_LEN], &tvec[NUM_STR_REP_LEN * 2], distvec, t->con.text,
			                    t->proptext, autoik);
		}
	}

	if (t->flag & T_PROP_EDIT_ALL) {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_(" Proportional size: %.2f"), t->prop_size);
	}
}

static void applyTranslationValue(TransInfo *t, float vec[3])
{
	TransData *td = t->data;
	float tvec[3];
	int i;

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		/* handle snapping rotation before doing the translation */
		if (usingSnappingNormal(t)) {
			if (validSnappingNormal(t)) {
				const float *original_normal;
				float axis[3];
				float quat[4];
				float mat[3][3];
				float angle;
				
				/* In pose mode, we want to align normals with Y axis of bones... */
				if (t->flag & T_POSE)
					original_normal = td->axismtx[1];
				else
					original_normal = td->axismtx[2];
				
				cross_v3_v3v3(axis, original_normal, t->tsnap.snapNormal);
				angle = saacos(dot_v3v3(original_normal, t->tsnap.snapNormal));
				
				axis_angle_to_quat(quat, axis, angle);
				
				quat_to_mat3(mat, quat);
				
				ElementRotation(t, td, mat, V3D_LOCAL);
			}
			else {
				float mat[3][3];
				
				unit_m3(mat);
				
				ElementRotation(t, td, mat, V3D_LOCAL);
			}
		}
		
		if (t->con.applyVec) {
			float pvec[3];
			t->con.applyVec(t, td, vec, tvec, pvec);
		}
		else {
			copy_v3_v3(tvec, vec);
		}
		
		mul_m3_v3(td->smtx, tvec);
		mul_v3_fl(tvec, td->factor);
		
		protectedTransBits(td->protectflag, tvec);
		
		if (td->loc)
			add_v3_v3v3(td->loc, td->iloc, tvec);
		
		constraintTransLim(t, td);
	}
}

/* uses t->vec to store actual translation in */
static void applyTranslation(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];

	if (t->con.mode & CON_APPLY) {
		float pvec[3] = {0.0f, 0.0f, 0.0f};
		float tvec[3];
		if (applyNumInput(&t->num, t->values)) {
			removeAspectRatio(t, t->values);
		}
		applySnapping(t, t->values);
		t->con.applyVec(t, NULL, t->values, tvec, pvec);
		copy_v3_v3(t->values, tvec);
		headerTranslation(t, pvec, str);
	}
	else {
		snapGridIncrement(t, t->values);
		if (applyNumInput(&t->num, t->values)) {
			removeAspectRatio(t, t->values);
		}
		applySnapping(t, t->values);
		headerTranslation(t, t->values, str);
	}

	applyTranslationValue(t, t->values);

	/* evil hack - redo translation if clipping needed */
	if (t->flag & T_CLIP_UV && clipUVTransform(t, t->values, 0)) {
		applyTranslationValue(t, t->values);

		/* In proportional edit it can happen that */
		/* vertices in the radius of the brush end */
		/* outside the clipping area               */
		/* XXX HACK - dg */
		if (t->flag & T_PROP_EDIT_ALL) {
			clipUVData(t);
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Shrink-Fatten) */

/** \name Transform Shrink-Fatten
 * \{ */

static void initShrinkFatten(TransInfo *t)
{
	// If not in mesh edit mode, fallback to Resize
	if (t->obedit == NULL || t->obedit->type != OB_MESH) {
		initResize(t);
	}
	else {
		t->mode = TFM_SHRINKFATTEN;
		t->transform = applyShrinkFatten;

		initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

		t->idx_max = 0;
		t->num.idx_max = 0;
		t->snap[0] = 0.0f;
		t->snap[1] = 1.0f;
		t->snap[2] = t->snap[1] * 0.1f;

		copy_v3_fl(t->num.val_inc, t->snap[1]);
		t->num.unit_sys = t->scene->unit.system;
		t->num.unit_type[0] = B_UNIT_LENGTH;

		t->flag |= T_NO_CONSTRAINT;
	}
}


static void applyShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
	float distance;
	int i;
	char str[MAX_INFO_LEN];
	size_t ofs = 0;
	TransData *td = t->data;

	distance = -t->values[0];

	snapGridIncrement(t, &distance);

	applyNumInput(&t->num, &distance);

	/* header print for NumInput */
	ofs += BLI_strncpy_rlen(str + ofs, IFACE_("Shrink/Fatten:"), MAX_INFO_LEN - ofs);
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];
		outputNumInput(&(t->num), c, &t->scene->unit);
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, " %s", c);
	}
	else {
		/* default header print */
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, " %.4f", distance);
	}

	if (t->proptext[0]) {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, " %s", t->proptext);
	}
	ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, ", (");

	if (t->keymap) {
		wmKeyMapItem *kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_RESIZE);
		if (kmi) {
			ofs += WM_keymap_item_to_string(kmi, str + ofs, MAX_INFO_LEN - ofs);
		}
	}
	BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_(" or Alt) Even Thickness %s"),
	             WM_bool_as_string((t->flag & T_ALT_TRANSFORM) != 0));
	/* done with header string */


	t->values[0] = -distance;

	for (i = 0; i < t->total; i++, td++) {
		float tdistance;  /* temp dist */
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		/* get the final offset */
		tdistance = distance * td->factor;
		if (td->ext && (t->flag & T_ALT_TRANSFORM)) {
			tdistance *= td->ext->isize[0];  /* shell factor */
		}

		madd_v3_v3v3fl(td->loc, td->iloc, td->axismtx[2], tdistance);
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Tilt) */

/** \name Transform Tilt
 * \{ */

static void initTilt(TransInfo *t)
{
	t->mode = TFM_TILT;
	t->transform = applyTilt;

	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = DEG2RAD(5.0);
	t->snap[2] = DEG2RAD(1.0);

	copy_v3_fl(t->num.val_inc, t->snap[2]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
	t->num.unit_type[0] = B_UNIT_ROTATION;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}


static void applyTilt(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	int i;
	char str[MAX_INFO_LEN];

	float final;

	final = t->values[0];

	snapGridIncrement(t, &final);

	applyNumInput(&t->num, &final);

	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Tilt: %s° %s"), &c[0], t->proptext);

		/* XXX For some reason, this seems needed for this op, else RNA prop is not updated... :/ */
		t->values[0] = final;
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Tilt: %.2f° %s"), RAD2DEGF(final), t->proptext);
	}

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival + final * td->factor;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Curve Shrink/Fatten) */

/** \name Transform Curve Shrink/Fatten
 * \{ */

static void initCurveShrinkFatten(TransInfo *t)
{
	t->mode = TFM_CURVE_SHRINKFATTEN;
	t->transform = applyCurveShrinkFatten;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
	t->num.val_flag[0] |= NUM_NO_ZERO;
#endif

	t->flag |= T_NO_CONSTRAINT;
}

static void applyCurveShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float ratio;
	int i;
	char str[MAX_INFO_LEN];

	ratio = t->values[0];

	snapGridIncrement(t, &ratio);

	applyNumInput(&t->num, &ratio);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Shrink/Fatten: %s"), c);
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Shrink/Fatten: %3f"), ratio);
	}

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival * ratio;
			/* apply PET */
			*td->val = (*td->val * td->factor) + ((1.0f - td->factor) * td->ival);
			if (*td->val <= 0.0f) *td->val = 0.001f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Mask Shrink/Fatten) */

/** \name Transform Mask Shrink/Fatten
 * \{ */

static void initMaskShrinkFatten(TransInfo *t)
{
	t->mode = TFM_MASK_SHRINKFATTEN;
	t->transform = applyMaskShrinkFatten;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
	t->num.val_flag[0] |= NUM_NO_ZERO;
#endif

	t->flag |= T_NO_CONSTRAINT;
}

static void applyMaskShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td;
	float ratio;
	int i;
	bool initial_feather = false;
	char str[MAX_INFO_LEN];

	ratio = t->values[0];

	snapGridIncrement(t, &ratio);

	applyNumInput(&t->num, &ratio);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Feather Shrink/Fatten: %s"), c);
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Feather Shrink/Fatten: %3f"), ratio);
	}

	/* detect if no points have feather yet */
	if (ratio > 1.0f) {
		initial_feather = true;

		for (td = t->data, i = 0; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			if (td->ival >= 0.001f)
				initial_feather = false;
		}
	}

	/* apply shrink/fatten */
	for (td = t->data, i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			if (initial_feather)
				*td->val = td->ival + (ratio - 1.0f) * 0.01f;
			else
				*td->val = td->ival * ratio;

			/* apply PET */
			*td->val = (*td->val * td->factor) + ((1.0f - td->factor) * td->ival);
			if (*td->val <= 0.0f) *td->val = 0.001f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Push/Pull) */

/** \name Transform Push/Pull
 * \{ */

static void initPushPull(TransInfo *t)
{
	t->mode = TFM_PUSHPULL;
	t->transform = applyPushPull;

	initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_LENGTH;
}


static void applyPushPull(TransInfo *t, const int UNUSED(mval[2]))
{
	float vec[3], axis[3];
	float distance;
	int i;
	char str[MAX_INFO_LEN];
	TransData *td = t->data;

	distance = t->values[0];

	snapGridIncrement(t, &distance);

	applyNumInput(&t->num, &distance);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Push/Pull: %s%s %s"), c, t->con.text, t->proptext);
	}
	else {
		/* default header print */
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Push/Pull: %.4f%s %s"), distance, t->con.text, t->proptext);
	}

	t->values[0] = distance;

	if (t->con.applyRot && t->con.mode & CON_APPLY) {
		t->con.applyRot(t, NULL, axis, NULL);
	}

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		sub_v3_v3v3(vec, t->center, td->center);
		if (t->con.applyRot && t->con.mode & CON_APPLY) {
			t->con.applyRot(t, td, axis, NULL);
			if (isLockConstraint(t)) {
				float dvec[3];
				project_v3_v3v3(dvec, vec, axis);
				sub_v3_v3(vec, dvec);
			}
			else {
				project_v3_v3v3(vec, vec, axis);
			}
		}
		normalize_v3(vec);
		mul_v3_fl(vec, distance);
		mul_v3_fl(vec, td->factor);

		add_v3_v3v3(td->loc, td->iloc, vec);
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Bevel Weight) */

/** \name Transform Bevel Weight
 * \{ */

static void initBevelWeight(TransInfo *t)
{
	t->mode = TFM_BWEIGHT;
	t->transform = applyBevelWeight;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING_DELTA);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyBevelWeight(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float weight;
	int i;
	char str[MAX_INFO_LEN];

	weight = t->values[0];

	if (weight > 1.0f) weight = 1.0f;

	snapGridIncrement(t, &weight);

	applyNumInput(&t->num, &weight);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		if (weight >= 0.0f)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Bevel Weight: +%s %s"), c, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Bevel Weight: %s %s"), c, t->proptext);
	}
	else {
		/* default header print */
		if (weight >= 0.0f)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Bevel Weight: +%.3f %s"), weight, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Bevel Weight: %.3f %s"), weight, t->proptext);
	}

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->val) {
			*td->val = td->ival + weight * td->factor;
			if (*td->val < 0.0f) *td->val = 0.0f;
			if (*td->val > 1.0f) *td->val = 1.0f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Crease) */

/** \name Transform Crease
 * \{ */

static void initCrease(TransInfo *t)
{
	t->mode = TFM_CREASE;
	t->transform = applyCrease;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING_DELTA);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyCrease(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float crease;
	int i;
	char str[MAX_INFO_LEN];

	crease = t->values[0];

	if (crease > 1.0f) crease = 1.0f;

	snapGridIncrement(t, &crease);

	applyNumInput(&t->num, &crease);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		if (crease >= 0.0f)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Crease: +%s %s"), c, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Crease: %s %s"), c, t->proptext);
	}
	else {
		/* default header print */
		if (crease >= 0.0f)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Crease: +%.3f %s"), crease, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Crease: %.3f %s"), crease, t->proptext);
	}

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival + crease * td->factor;
			if (*td->val < 0.0f) *td->val = 0.0f;
			if (*td->val > 1.0f) *td->val = 1.0f;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (EditBone (B-bone) width scaling) */

/** \name Transform B-bone width scaling
 * \{ */

static void initBoneSize(TransInfo *t)
{
	t->mode = TFM_BONESIZE;
	t->transform = applyBoneSize;

	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

	t->idx_max = 2;
	t->num.idx_max = 2;
	t->num.val_flag[0] |= NUM_NULL_ONE;
	t->num.val_flag[1] |= NUM_NULL_ONE;
	t->num.val_flag[2] |= NUM_NULL_ONE;
	t->num.flag |= NUM_AFFECT_ALL;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;
	t->num.unit_type[1] = B_UNIT_NONE;
	t->num.unit_type[2] = B_UNIT_NONE;
}

static void headerBoneSize(TransInfo *t, float vec[3], char str[MAX_INFO_LEN])
{
	char tvec[NUM_STR_REP_LEN * 3];
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec, &t->scene->unit);
	}
	else {
		BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
		BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
		BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
	}

	/* hmm... perhaps the y-axis values don't need to be shown? */
	if (t->con.mode & CON_APPLY) {
		if (t->num.idx_max == 0)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("ScaleB: %s%s %s"), &tvec[0], t->con.text, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("ScaleB: %s : %s : %s%s %s"),
			             &tvec[0], &tvec[NUM_STR_REP_LEN], &tvec[NUM_STR_REP_LEN * 2], t->con.text, t->proptext);
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("ScaleB X: %s  Y: %s  Z: %s%s %s"),
		             &tvec[0], &tvec[NUM_STR_REP_LEN], &tvec[NUM_STR_REP_LEN * 2], t->con.text, t->proptext);
	}
}

static void ElementBoneSize(TransInfo *t, TransData *td, float mat[3][3])
{
	float tmat[3][3], smat[3][3], oldy;
	float sizemat[3][3];

	mul_m3_m3m3(smat, mat, td->mtx);
	mul_m3_m3m3(tmat, td->smtx, smat);

	if (t->con.applySize) {
		t->con.applySize(t, td, tmat);
	}

	/* we've tucked the scale in loc */
	oldy = td->iloc[1];
	size_to_mat3(sizemat, td->iloc);
	mul_m3_m3m3(tmat, tmat, sizemat);
	mat3_to_size(td->loc, tmat);
	td->loc[1] = oldy;
}

static void applyBoneSize(TransInfo *t, const int mval[2])
{
	TransData *td = t->data;
	float size[3], mat[3][3];
	float ratio;
	int i;
	char str[MAX_INFO_LEN];
	
	// TRANSFORM_FIX_ME MOVE TO MOUSE INPUT
	/* for manipulator, center handle, the scaling can't be done relative to center */
	if ((t->flag & T_USES_MANIPULATOR) && t->con.mode == 0) {
		ratio = 1.0f - ((t->imval[0] - mval[0]) + (t->imval[1] - mval[1])) / 100.0f;
	}
	else {
		ratio = t->values[0];
	}
	
	size[0] = size[1] = size[2] = ratio;
	
	snapGridIncrement(t, size);
	
	if (applyNumInput(&t->num, size)) {
		constraintNumInput(t, size);
	}
	
	size_to_mat3(mat, size);
	
	if (t->con.applySize) {
		t->con.applySize(t, NULL, mat);
	}
	
	copy_m3_m3(t->mat, mat);    // used in manipulator
	
	headerBoneSize(t, size, str);
	
	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		ElementBoneSize(t, td, mat);
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Bone Envelope) */

/** \name Transform Bone Envelope
 * \{ */

static void initBoneEnvelope(TransInfo *t)
{
	t->mode = TFM_BONE_ENVELOPE;
	t->transform = applyBoneEnvelope;
	
	initMouseInputMode(t, &t->mouse, INPUT_SPRING);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;
	
	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyBoneEnvelope(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float ratio;
	int i;
	char str[MAX_INFO_LEN];
	
	ratio = t->values[0];
	
	snapGridIncrement(t, &ratio);
	
	applyNumInput(&t->num, &ratio);
	
	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];
		
		outputNumInput(&(t->num), c, &t->scene->unit);
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Envelope: %s"), c);
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Envelope: %3f"), ratio);
	}
	
	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		
		if (td->flag & TD_SKIP)
			continue;
		
		if (td->val) {
			/* if the old/original value was 0.0f, then just use ratio */
			if (td->ival)
				*td->val = td->ival * ratio;
			else
				*td->val = ratio;
		}
	}
	
	recalcData(t);
	
	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Edge Slide) */

/** \name Transform Edge Slide
 * \{ */

static BMEdge *get_other_edge(BMVert *v, BMEdge *e)
{
	BMIter iter;
	BMEdge *e_iter;

	BM_ITER_ELEM (e_iter, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_elem_flag_test(e_iter, BM_ELEM_SELECT) && e_iter != e) {
			return e_iter;
		}
	}

	return NULL;
}

/* interpoaltes along a line made up of 2 segments (used for edge slide) */
static void interp_line_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float t)
{
	float t_mid, t_delta;

	/* could be pre-calculated */
	t_mid = line_point_factor_v3(v2, v1, v3);

	t_delta = t - t_mid;
	if (fabsf(t_delta) < FLT_EPSILON) {
		copy_v3_v3(p, v2);
	}
	else if (t_delta < 0.0f) {
		interp_v3_v3v3(p, v1, v2, t / t_mid);
	}
	else {
		interp_v3_v3v3(p, v2, v3, (t - t_mid) / (1.0f - t_mid));
	}
}

static void len_v3_ensure(float v[3], const float length)
{
	normalize_v3(v);
	mul_v3_fl(v, length);
}

/**
 * Find the closest point on the ngon on the opposite side.
 * used to set the edge slide distance for ngons.
 */
static bool bm_loop_calc_opposite_co(BMLoop *l_tmp,
                                     const float plane_no[3],
                                     float r_co[3])
{
	/* skip adjacent edges */
	BMLoop *l_first = l_tmp->next;
	BMLoop *l_last  = l_tmp->prev;
	BMLoop *l_iter;
	float dist = FLT_MAX;

	l_iter = l_first;
	do {
		float tvec[3];
		if (isect_line_plane_v3(tvec,
		                        l_iter->v->co, l_iter->next->v->co,
		                        l_tmp->v->co, plane_no))
		{
			const float fac = line_point_factor_v3(tvec, l_iter->v->co, l_iter->next->v->co);
			/* allow some overlap to avoid missing the intersection because of float precision */
			if ((fac > -FLT_EPSILON) && (fac < 1.0f + FLT_EPSILON)) {
				/* likelihood of multiple intersections per ngon is quite low,
				 * it would have to loop back on its self, but better support it
				 * so check for the closest opposite edge */
				const float tdist = len_v3v3(l_tmp->v->co, tvec);
				if (tdist < dist) {
					copy_v3_v3(r_co, tvec);
					dist = tdist;
				}
			}
		}
	} while ((l_iter = l_iter->next) != l_last);

	return (dist != FLT_MAX);
}

/**
 * Given 2 edges and a loop, step over the loops
 * and calculate a direction to slide along.
 *
 * \param r_slide_vec the direction to slide,
 * the length of the vector defines the slide distance.
 */
static BMLoop *get_next_loop(BMVert *v, BMLoop *l,
                             BMEdge *e_prev, BMEdge *e_next, float r_slide_vec[3])
{
	BMLoop *l_first;
	float vec_accum[3] = {0.0f, 0.0f, 0.0f};
	float vec_accum_len = 0.0f;
	int i = 0;

	BLI_assert(BM_edge_share_vert(e_prev, e_next) == v);
	BLI_assert(BM_vert_in_edge(l->e, v));

	l_first = l;
	do {
		l = BM_loop_other_edge_loop(l, v);
		
		if (l->e == e_next) {
			if (i) {
				len_v3_ensure(vec_accum, vec_accum_len / (float)i);
			}
			else {
				/* When there is no edge to slide along,
				 * we must slide along the vector defined by the face we're attach to */
				BMLoop *l_tmp = BM_face_vert_share_loop(l_first->f, v);

				BLI_assert(ELEM(l_tmp->e, e_prev, e_next) && ELEM(l_tmp->prev->e, e_prev, e_next));

				if (l_tmp->f->len == 4) {
					/* we could use code below, but in this case
					 * sliding diagonally across the quad works well */
					sub_v3_v3v3(vec_accum, l_tmp->next->next->v->co, v->co);
				}
				else {
					float tdir[3];
					BM_loop_calc_face_direction(l_tmp, tdir);
					cross_v3_v3v3(vec_accum, l_tmp->f->no, tdir);
#if 0
					/* rough guess, we can  do better! */
					len_v3_ensure(vec_accum, (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f);
#else
					/* be clever, check the opposite ngon edge to slide into.
					 * this gives best results */
					{
						float tvec[3];
						float dist;

						if (bm_loop_calc_opposite_co(l_tmp, tdir, tvec)) {
							dist = len_v3v3(l_tmp->v->co, tvec);
						}
						else {
							dist = (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f;
						}

						len_v3_ensure(vec_accum, dist);
					}
#endif
				}
			}

			copy_v3_v3(r_slide_vec, vec_accum);
			return l;
		}
		else {
			/* accumulate the normalized edge vector,
			 * normalize so some edges don't skew the result */
			float tvec[3];
			sub_v3_v3v3(tvec, BM_edge_other_vert(l->e, v)->co, v->co);
			vec_accum_len += normalize_v3(tvec);
			add_v3_v3(vec_accum, tvec);
			i += 1;
		}

		if (BM_loop_other_edge_loop(l, v)->e == e_next) {
			if (i) {
				len_v3_ensure(vec_accum, vec_accum_len / (float)i);
			}

			copy_v3_v3(r_slide_vec, vec_accum);
			return BM_loop_other_edge_loop(l, v);
		}

	} while ((l != l->radial_next) &&
	         ((l = l->radial_next) != l_first));

	if (i) {
		len_v3_ensure(vec_accum, vec_accum_len / (float)i);
	}
	
	copy_v3_v3(r_slide_vec, vec_accum);
	
	return NULL;
}

static void calcNonProportionalEdgeSlide(TransInfo *t, EdgeSlideData *sld, const float mval[2])
{
	TransDataEdgeSlideVert *sv = sld->sv;

	if (sld->totsv > 0) {
		ARegion *ar = t->ar;
		RegionView3D *rv3d = NULL;
		float projectMat[4][4];

		int i = 0;

		float v_proj[2];
		float dist_sq = 0;
		float dist_min_sq = FLT_MAX;

		if (t->spacetype == SPACE_VIEW3D) {
			/* background mode support */
			rv3d = t->ar ? t->ar->regiondata : NULL;
		}

		if (!rv3d) {
			/* ok, let's try to survive this */
			unit_m4(projectMat);
		}
		else {
			ED_view3d_ob_project_mat_get(rv3d, t->obedit, projectMat);
		}

		for (i = 0; i < sld->totsv; i++, sv++) {
			/* Set length */
			sv->edge_len = len_v3v3(sv->dir_a, sv->dir_b);

			ED_view3d_project_float_v2_m4(ar, sv->v->co, v_proj, projectMat);
			dist_sq = len_squared_v2v2(mval, v_proj);
			if (dist_sq < dist_min_sq) {
				dist_min_sq = dist_sq;
				sld->curr_sv_index = i;
			}
		}
	}
	else {
		sld->curr_sv_index = 0;
	}
}

static bool createEdgeSlideVerts(TransInfo *t)
{
	BMEditMesh *em = BKE_editmesh_from_object(t->obedit);
	BMesh *bm = em->bm;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	TransDataEdgeSlideVert *sv_array;
	int sv_tot;
	BMBVHTree *btree;
	int *sv_table;  /* BMVert -> sv_array index */
	EdgeSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
	View3D *v3d = NULL;
	RegionView3D *rv3d = NULL;
	ARegion *ar = t->ar;
	float projectMat[4][4];
	float mval[2] = {(float)t->mval[0], (float)t->mval[1]};
	float mval_start[2], mval_end[2];
	float mval_dir[3], maxdist, (*loop_dir)[3], *loop_maxdist;
	int numsel, i, j, loop_nr, l_nr;
	int use_btree_disp;

	if (t->spacetype == SPACE_VIEW3D) {
		/* background mode support */
		v3d = t->sa ? t->sa->spacedata.first : NULL;
		rv3d = t->ar ? t->ar->regiondata : NULL;
	}

	if ((t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) &&
	    /* don't do this at all for non-basis shape keys, too easy to
	     * accidentally break uv maps or vertex colors then */
	    (bm->shapenr <= 1))
	{
		sld->use_origfaces = true;
	}
	else {
		sld->use_origfaces = false;
	}

	sld->is_proportional = true;
	sld->curr_sv_index = 0;
	sld->flipped_vtx = false;

	if (!rv3d) {
		/* ok, let's try to survive this */
		unit_m4(projectMat);
	}
	else {
		ED_view3d_ob_project_mat_get(rv3d, t->obedit, projectMat);
	}

	/*ensure valid selection*/
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			BMIter iter2;
			numsel = 0;
			BM_ITER_ELEM (e, &iter2, v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
					/* BMESH_TODO: this is probably very evil,
					 * set v->e to a selected edge*/
					v->e = e;

					numsel++;
				}
			}

			if (numsel == 0 || numsel > 2) {
				MEM_freeN(sld);
				return false; /* invalid edge selection */
			}
		}
	}

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			/* note, any edge with loops can work, but we won't get predictable results, so bail out */
			if (!BM_edge_is_manifold(e) && !BM_edge_is_boundary(e)) {
				/* can edges with at least once face user */
				MEM_freeN(sld);
				return false;
			}
		}
	}

	sv_table = MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

	j = 0;
	BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
			sv_table[i] = j;
			j += 1;
		}
		else {
			BM_elem_flag_disable(v, BM_ELEM_TAG);
			sv_table[i] = -1;
		}
		BM_elem_index_set(v, i); /* set_inline */
	}
	bm->elem_index_dirty &= ~BM_VERT;

	if (!j) {
		MEM_freeN(sld);
		MEM_freeN(sv_table);
		return false;
	}

	sv_tot = j;
	sv_array = MEM_callocN(sizeof(TransDataEdgeSlideVert) * sv_tot, "sv_array");
	loop_nr = 0;

	while (1) {
		float vec_a[3], vec_b[3];
		BMLoop *l_a, *l_b;
		BMLoop *l_a_prev, *l_b_prev;
		BMVert *v_first;
		/* If this succeeds call get_next_loop()
		 * which calculates the direction to slide based on clever checks.
		 *
		 * otherwise we simply use 'e_dir' as an edge-rail.
		 * (which is better when the attached edge is a boundary, see: T40422)
		 */
#define EDGESLIDE_VERT_IS_INNER(v, e_dir) \
		((BM_edge_is_boundary(e_dir) == false) && \
		 (BM_vert_edge_count_nonwire(v) == 2))

		v = NULL;
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG))
				break;

		}

		if (!v)
			break;

		if (!v->e)
			continue;
		
		v_first = v;

		/*walk along the edge loop*/
		e = v->e;

		/*first, rewind*/
		do {
			e = get_other_edge(v, e);
			if (!e) {
				e = v->e;
				break;
			}

			if (!BM_elem_flag_test(BM_edge_other_vert(e, v), BM_ELEM_TAG))
				break;

			v = BM_edge_other_vert(e, v);
		} while (e != v_first->e);

		BM_elem_flag_disable(v, BM_ELEM_TAG);

		l_a = e->l;
		l_b = e->l->radial_next;

		/* regarding e_next, use get_next_loop()'s improved interpolation where possible */
		{
			BMEdge *e_next = get_other_edge(v, e);
			if (e_next) {
				get_next_loop(v, l_a, e, e_next, vec_a);
			}
			else {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
				if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
					get_next_loop(v, l_a, e, l_tmp->e, vec_a);
				}
				else {
					sub_v3_v3v3(vec_a, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
				}
			}
		}

		/* !BM_edge_is_boundary(e); */
		if (l_b != l_a) {
			BMEdge *e_next = get_other_edge(v, e);
			if (e_next) {
				get_next_loop(v, l_b, e, e_next, vec_b);
			}
			else {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
				if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
					get_next_loop(v, l_b, e, l_tmp->e, vec_b);
				}
				else {
					sub_v3_v3v3(vec_b, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
				}
			}
		}
		else {
			l_b = NULL;
		}

		l_a_prev = NULL;
		l_b_prev = NULL;

		/*iterate over the loop*/
		v_first = v;
		do {
			bool l_a_ok_prev;
			bool l_b_ok_prev;
			TransDataEdgeSlideVert *sv;
			BMVert *v_prev;
			BMEdge *e_prev;

			/* XXX, 'sv' will initialize multiple times, this is suspicious. see [#34024] */
			BLI_assert(v != NULL);
			BLI_assert(sv_table[BM_elem_index_get(v)] != -1);
			sv = &sv_array[sv_table[BM_elem_index_get(v)]];
			sv->v = v;
			copy_v3_v3(sv->v_co_orig, v->co);
			sv->loop_nr = loop_nr;

			if (l_a || l_a_prev) {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_a ? l_a : l_a_prev, v);
				sv->v_a = BM_edge_other_vert(l_tmp->e, v);
				copy_v3_v3(sv->dir_a, vec_a);
			}

			if (l_b || l_b_prev) {
				BMLoop *l_tmp = BM_loop_other_edge_loop(l_b ? l_b : l_b_prev, v);
				sv->v_b = BM_edge_other_vert(l_tmp->e, v);
				copy_v3_v3(sv->dir_b, vec_b);
			}

			v_prev = v;
			v = BM_edge_other_vert(e, v);

			e_prev = e;
			e = get_other_edge(v, e);

			if (!e) {
				BLI_assert(v != NULL);
				BLI_assert(sv_table[BM_elem_index_get(v)] != -1);
				sv = &sv_array[sv_table[BM_elem_index_get(v)]];
				sv->v = v;
				copy_v3_v3(sv->v_co_orig, v->co);
				sv->loop_nr = loop_nr;

				if (l_a) {
					BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
					sv->v_a = BM_edge_other_vert(l_tmp->e, v);
					if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
						get_next_loop(v, l_a, e_prev, l_tmp->e, sv->dir_a);
					}
					else {
						sub_v3_v3v3(sv->dir_a, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
					}
				}

				if (l_b) {
					BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
					sv->v_b = BM_edge_other_vert(l_tmp->e, v);
					if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
						get_next_loop(v, l_b, e_prev, l_tmp->e, sv->dir_b);
					}
					else {
						sub_v3_v3v3(sv->dir_b, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
					}
				}

				BM_elem_flag_disable(v, BM_ELEM_TAG);
				BM_elem_flag_disable(v_prev, BM_ELEM_TAG);

				break;
			}
			l_a_ok_prev = (l_a != NULL);
			l_b_ok_prev = (l_b != NULL);

			l_a_prev = l_a;
			l_b_prev = l_b;

			if (l_a) {
				l_a = get_next_loop(v, l_a, e_prev, e, vec_a);
			}
			else {
				zero_v3(vec_a);
			}

			if (l_b) {
				l_b = get_next_loop(v, l_b, e_prev, e, vec_b);
			}
			else {
				zero_v3(vec_b);
			}


			if (l_a && l_b) {
				/* pass */
			}
			else {
				if (l_a || l_b) {
					/* find the opposite loop if it was missing previously */
					if      (l_a == NULL && l_b && (l_b->radial_next != l_b)) l_a = l_b->radial_next;
					else if (l_b == NULL && l_a && (l_a->radial_next != l_a)) l_b = l_a->radial_next;
				}
				else if (e->l != NULL) {
					/* if there are non-contiguous faces, we can still recover the loops of the new edges faces */
					/* note!, the behavior in this case means edges may move in opposite directions,
					 * this could be made to work more usefully. */

					if (l_a_ok_prev) {
						l_a = e->l;
						l_b = (l_a->radial_next != l_a) ? l_a->radial_next : NULL;
					}
					else if (l_b_ok_prev) {
						l_b = e->l;
						l_a = (l_b->radial_next != l_b) ? l_b->radial_next : NULL;
					}
				}

				if (!l_a_ok_prev && l_a) {
					get_next_loop(v, l_a, e, e_prev, vec_a);
				}
				if (!l_b_ok_prev && l_b) {
					get_next_loop(v, l_b, e, e_prev, vec_b);
				}
			}

			BM_elem_flag_disable(v, BM_ELEM_TAG);
			BM_elem_flag_disable(v_prev, BM_ELEM_TAG);
		} while ((e != v_first->e) && (l_a || l_b));

		loop_nr++;

#undef EDGESLIDE_VERT_IS_INNER
	}

	/* use for visibility checks */
	use_btree_disp = (v3d && t->obedit->dt > OB_WIRE && v3d->drawtype > OB_WIRE);

	if (use_btree_disp) {
		btree = BKE_bmbvh_new_from_editmesh(em, BMBVH_RESPECT_HIDDEN, NULL, false);
	}
	else {
		btree = NULL;
	}


	/* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

	sld->sv = sv_array;
	sld->totsv = sv_tot;
	
	/* find mouse vectors, the global one, and one per loop in case we have
	 * multiple loops selected, in case they are oriented different */
	zero_v3(mval_dir);
	maxdist = -1.0f;

	loop_dir = MEM_callocN(sizeof(float) * 3 * loop_nr, "sv loop_dir");
	loop_maxdist = MEM_mallocN(sizeof(float) * loop_nr, "sv loop_maxdist");
	fill_vn_fl(loop_maxdist, loop_nr, -1.0f);

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
			BMIter iter2;
			BMEdge *e2;
			float d;

			/* search cross edges for visible edge to the mouse cursor,
			 * then use the shared vertex to calculate screen vector*/
			for (i = 0; i < 2; i++) {
				v = i ? e->v1 : e->v2;
				BM_ITER_ELEM (e2, &iter2, v, BM_EDGES_OF_VERT) {
					/* screen-space coords */
					float sco_a[3], sco_b[3];

					if (BM_elem_flag_test(e2, BM_ELEM_SELECT))
						continue;

					/* This test is only relevant if object is not wire-drawn! See [#32068]. */
					if (use_btree_disp && !BMBVH_EdgeVisible(btree, e2, ar, v3d, t->obedit)) {
						continue;
					}

					BLI_assert(sv_table[BM_elem_index_get(v)] != -1);
					j = sv_table[BM_elem_index_get(v)];

					if (sv_array[j].v_b) {
						ED_view3d_project_float_v3_m4(ar, sv_array[j].v_b->co, sco_b, projectMat);
					}
					else {
						add_v3_v3v3(sco_b, v->co, sv_array[j].dir_b);
						ED_view3d_project_float_v3_m4(ar, sco_b, sco_b, projectMat);
					}
					
					if (sv_array[j].v_a) {
						ED_view3d_project_float_v3_m4(ar, sv_array[j].v_a->co, sco_a, projectMat);
					}
					else {
						add_v3_v3v3(sco_a, v->co, sv_array[j].dir_a);
						ED_view3d_project_float_v3_m4(ar, sco_a, sco_a, projectMat);
					}
					
					/* global direction */
					d = dist_to_line_segment_v2(mval, sco_b, sco_a);
					if ((maxdist == -1.0f) ||
					    /* intentionally use 2d size on 3d vector */
					    (d < maxdist && (len_squared_v2v2(sco_b, sco_a) > 0.1f)))
					{
						maxdist = d;
						sub_v3_v3v3(mval_dir, sco_b, sco_a);
					}

					/* per loop direction */
					l_nr = sv_array[j].loop_nr;
					if (loop_maxdist[l_nr] == -1.0f || d < loop_maxdist[l_nr]) {
						loop_maxdist[l_nr] = d;
						sub_v3_v3v3(loop_dir[l_nr], sco_b, sco_a);
					}
				}
			}
		}
	}

	/* possible all of the edge loops are pointing directly at the view */
	if (UNLIKELY(len_squared_v2(mval_dir) < 0.1f)) {
		mval_dir[0] = 0.0f;
		mval_dir[1] = 100.0f;
	}

	bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);

	if (sld->use_origfaces) {
		sld->origfaces = BLI_ghash_ptr_new(__func__);
		sld->bm_origfaces = BM_mesh_create(&bm_mesh_allocsize_default);
		/* we need to have matching customdata */
		BM_mesh_copy_init_customdata(sld->bm_origfaces, bm, NULL);
	}

	/*create copies of faces for customdata projection*/
	sv_array = sld->sv;
	for (i = 0; i < sld->totsv; i++, sv_array++) {
		BMIter fiter;
		BMFace *f;
		

		if (sld->use_origfaces) {
			BM_ITER_ELEM (f, &fiter, sv_array->v, BM_FACES_OF_VERT) {
				if (!BLI_ghash_haskey(sld->origfaces, f)) {
					BMFace *f_copy = BM_face_copy(sld->bm_origfaces, bm, f, true, true);
					BLI_ghash_insert(sld->origfaces, f, f_copy);
				}
			}
		}

		/* switch a/b if loop direction is different from global direction */
		l_nr = sv_array->loop_nr;
		if (dot_v3v3(loop_dir[l_nr], mval_dir) < 0.0f) {
			swap_v3_v3(sv_array->dir_a, sv_array->dir_b);
			SWAP(BMVert *, sv_array->v_a, sv_array->v_b);
		}
	}

	if (rv3d)
		calcNonProportionalEdgeSlide(t, sld, mval);

	sld->em = em;

	/*zero out start*/
	zero_v2(mval_start);

	/*dir holds a vector along edge loop*/
	copy_v2_v2(mval_end, mval_dir);
	mul_v2_fl(mval_end, 0.5f);
	
	sld->mval_start[0] = t->mval[0] + mval_start[0];
	sld->mval_start[1] = t->mval[1] + mval_start[1];

	sld->mval_end[0] = t->mval[0] + mval_end[0];
	sld->mval_end[1] = t->mval[1] + mval_end[1];
	
	sld->perc = 0.0f;
	
	t->customData = sld;
	
	MEM_freeN(sv_table);
	if (btree) {
		BKE_bmbvh_free(btree);
	}
	MEM_freeN(loop_dir);
	MEM_freeN(loop_maxdist);

	return true;
}

void projectEdgeSlideData(TransInfo *t, bool is_final)
{
	EdgeSlideData *sld = t->customData;
	TransDataEdgeSlideVert *sv;
	BMEditMesh *em = sld->em;
	int i;

	if (sld->use_origfaces == false) {
		return;
	}

	for (i = 0, sv = sld->sv; i < sld->totsv; sv++, i++) {
		BMIter fiter;
		BMLoop *l;

		BM_ITER_ELEM (l, &fiter, sv->v, BM_LOOPS_OF_VERT) {
			BMFace *f_copy;      /* the copy of 'f' */
			BMFace *f_copy_flip; /* the copy of 'f' or detect if we need to flip to the shorter side. */
			
			f_copy = BLI_ghash_lookup(sld->origfaces, l->f);
			
			/* project onto copied projection face */
			f_copy_flip = f_copy;

			if (BM_elem_flag_test(l->e, BM_ELEM_SELECT) || BM_elem_flag_test(l->prev->e, BM_ELEM_SELECT)) {
				/* the loop is attached of the selected edges that are sliding */
				BMLoop *l_ed_sel = l;

				if (!BM_elem_flag_test(l->e, BM_ELEM_SELECT))
					l_ed_sel = l_ed_sel->prev;

				if (sld->perc < 0.0f) {
					if (BM_vert_in_face(l_ed_sel->radial_next->f, sv->v_b)) {
						f_copy_flip = BLI_ghash_lookup(sld->origfaces, l_ed_sel->radial_next->f);
					}
				}
				else if (sld->perc > 0.0f) {
					if (BM_vert_in_face(l_ed_sel->radial_next->f, sv->v_a)) {
						f_copy_flip = BLI_ghash_lookup(sld->origfaces, l_ed_sel->radial_next->f);
					}
				}

				BLI_assert(f_copy_flip != NULL);
				if (!f_copy_flip) {
					continue;  /* shouldn't happen, but protection */
				}
			}
			else {
				/* the loop is attached to only one vertex and not a selected edge,
				 * this means we have to find a selected edges face going in the right direction
				 * to copy from else we get bad distortion see: [#31080] */
				BMIter eiter;
				BMEdge *e_sel;

				BLI_assert(l->v == sv->v);
				BM_ITER_ELEM (e_sel, &eiter, sv->v, BM_EDGES_OF_VERT) {
					if (BM_elem_flag_test(e_sel, BM_ELEM_SELECT)) {
						break;
					}
				}

				if (e_sel) {
					/* warning if the UV's are not contiguous, this will copy from the _wrong_ UVs
					 * in fact whenever the face being copied is not 'f_copy' this can happen,
					 * we could be a lot smarter about this but would need to deal with every UV channel or
					 * add a way to mask out lauers when calling #BM_loop_interp_from_face() */

					/*
					 *        +    +----------------+
					 *         \   |                |
					 * (this) l_adj|                |
					 *           \ |                |
					 *            \|      e_sel     |
					 *  +----------+----------------+  <- the edge we are sliding.
					 *            /|sv->v           |
					 *           / |                |
					 *   (or) l_adj|                |
					 *         /   |                |
					 *        +    +----------------+
					 * (above)
					 * 'other connected loops', attached to sv->v slide faces.
					 *
					 * NOTE: The faces connected to the edge may not have contiguous UV's
					 *       so step around the loops to find l_adj.
					 *       However if the 'other loops' are not cotiguous it will still give problems.
					 *
					 *       A full solution to this would have to store
					 *       per-customdata-layer map of which loops are contiguous
					 *       and take this into account when interpolating.
					 *
					 * NOTE: If l_adj's edge isnt manifold then use then
					 *       interpolate the loop from its own face.
					 *       Can happen when 'other connected loops' are disconnected from the face-fan.
					 */

					BMLoop *l_adj = NULL;
					if (sld->perc < 0.0f) {
						if (BM_vert_in_face(e_sel->l->f, sv->v_b)) {
							l_adj = e_sel->l;
						}
						else if (BM_vert_in_face(e_sel->l->radial_next->f, sv->v_b)) {
							l_adj = e_sel->l->radial_next;
						}
					}
					else if (sld->perc > 0.0f) {
						if (BM_vert_in_face(e_sel->l->f, sv->v_a)) {
							l_adj = e_sel->l;
						}
						else if (BM_vert_in_face(e_sel->l->radial_next->f, sv->v_a)) {
							l_adj = e_sel->l->radial_next;
						}
					}

					/* step across to the face */
					if (l_adj) {
						l_adj = BM_loop_other_edge_loop(l_adj, sv->v);
						if (!BM_edge_is_boundary(l_adj->e)) {
							l_adj = l_adj->radial_next;
						}
						else {
							/* disconnected face-fan, fallback to self */
							l_adj = l;
						}

						f_copy_flip = BLI_ghash_lookup(sld->origfaces, l_adj->f);
					}
				}
			}

			/* only loop data, no vertex data since that contains shape keys,
			 * and we do not want to mess up other shape keys */
			BM_loop_interp_from_face(em->bm, l, f_copy_flip, false, false);

			if (is_final) {
				BM_loop_interp_multires(em->bm, l, f_copy_flip);
				if (f_copy != f_copy_flip) {
					BM_loop_interp_multires(em->bm, l, f_copy);
				}
			}
			
			/* make sure face-attributes are correct (e.g. MTexPoly) */
			BM_elem_attrs_copy(sld->bm_origfaces, em->bm, f_copy, l->f);
		}
	}
}

void freeEdgeSlideTempFaces(EdgeSlideData *sld)
{
	if (sld->use_origfaces) {
		if (sld->bm_origfaces) {
			BM_mesh_free(sld->bm_origfaces);
			sld->bm_origfaces = NULL;
		}

		if (sld->origfaces) {
			BLI_ghash_free(sld->origfaces, NULL, NULL);
			sld->origfaces = NULL;
		}
	}
}


void freeEdgeSlideVerts(TransInfo *t)
{
	EdgeSlideData *sld = t->customData;
	
	if (!sld)
		return;
	
	freeEdgeSlideTempFaces(sld);

	bmesh_edit_end(sld->em->bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
	
	MEM_freeN(sld->sv);
	MEM_freeN(sld);
	
	t->customData = NULL;
	
	recalcData(t);
}

static void initEdgeSlide(TransInfo *t)
{
	EdgeSlideData *sld;

	t->mode = TFM_EDGE_SLIDE;
	t->transform = applyEdgeSlide;
	t->handleEvent = handleEventEdgeSlide;

	if (!createEdgeSlideVerts(t)) {
		t->state = TRANS_CANCEL;
		return;
	}
	
	sld = t->customData;

	if (!sld)
		return;

	t->customFree = freeEdgeSlideVerts;

	/* set custom point first if you want value to be initialized by init */
	setCustomPoints(t, &t->mouse, sld->mval_end, sld->mval_start);
	initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO_FLIP);
	
	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static eRedrawFlag handleEventEdgeSlide(struct TransInfo *t, const struct wmEvent *event)
{
	if (t->mode == TFM_EDGE_SLIDE) {
		EdgeSlideData *sld = t->customData;

		if (sld) {
			switch (event->type) {
				case EKEY:
					if (event->val == KM_PRESS) {
						sld->is_proportional = !sld->is_proportional;
						return TREDRAW_HARD;
					}
					break;
				case FKEY:
				{
					if (event->val == KM_PRESS) {
						if (sld->is_proportional == false) {
							sld->flipped_vtx = !sld->flipped_vtx;
						}
						return TREDRAW_HARD;
					}
					break;
				}
				case EVT_MODAL_MAP:
				{
					switch (event->val) {
						case TFM_MODAL_EDGESLIDE_DOWN:
						{
							sld->curr_sv_index = ((sld->curr_sv_index - 1) + sld->totsv) % sld->totsv;
							break;
						}
						case TFM_MODAL_EDGESLIDE_UP:
						{
							sld->curr_sv_index = (sld->curr_sv_index + 1) % sld->totsv;
							break;
						}
					}
					break;
				}
				default:
					break;
			}
		}
	}
	return TREDRAW_NOTHING;
}

static void drawEdgeSlide(const struct bContext *C, TransInfo *t)
{
	if (t->mode == TFM_EDGE_SLIDE) {
		EdgeSlideData *sld = (EdgeSlideData *)t->customData;
		/* Non-Prop mode */
		if (sld && sld->is_proportional == false) {
			View3D *v3d = CTX_wm_view3d(C);
			float co_a[3], co_b[3], co_mark[3];
			TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
			const float fac = (sld->perc + 1.0f) / 2.0f;
			const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
			const float guide_size = ctrl_size - 0.5f;
			const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;
			const int alpha_shade = -30;

			add_v3_v3v3(co_a, curr_sv->v_co_orig, curr_sv->dir_a);
			add_v3_v3v3(co_b, curr_sv->v_co_orig, curr_sv->dir_b);

			if (v3d && v3d->zbuf)
				glDisable(GL_DEPTH_TEST);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_POINT_BIT);
			glPushMatrix();

			glMultMatrixf(t->obedit->obmat);

			glLineWidth(line_size);
			UI_ThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
			glBegin(GL_LINES);
			if (curr_sv->v_a) {
				glVertex3fv(curr_sv->v_a->co);
				glVertex3fv(curr_sv->v_co_orig);
			}
			if (curr_sv->v_b) {
				glVertex3fv(curr_sv->v_b->co);
				glVertex3fv(curr_sv->v_co_orig);
			}
			bglEnd();


			UI_ThemeColorShadeAlpha(TH_SELECT, -30, alpha_shade);
			glPointSize(ctrl_size);
			bglBegin(GL_POINTS);
			if (sld->flipped_vtx) {
				if (curr_sv->v_b) bglVertex3fv(curr_sv->v_b->co);
			}
			else {
				if (curr_sv->v_a) bglVertex3fv(curr_sv->v_a->co);
			}
			bglEnd();

			UI_ThemeColorShadeAlpha(TH_SELECT, 255, alpha_shade);
			glPointSize(guide_size);
			bglBegin(GL_POINTS);
#if 0
			interp_v3_v3v3(co_mark, co_b, co_a, fac);
			bglVertex3fv(co_mark);
#endif
			interp_line_v3_v3v3v3(co_mark, co_b, curr_sv->v_co_orig, co_a, fac);
			bglVertex3fv(co_mark);
			bglEnd();


			glPopMatrix();
			glPopAttrib();

			glDisable(GL_BLEND);

			if (v3d && v3d->zbuf)
				glEnable(GL_DEPTH_TEST);
		}
	}
}

static int doEdgeSlide(TransInfo *t, float perc)
{
	EdgeSlideData *sld = t->customData;
	TransDataEdgeSlideVert *svlist = sld->sv, *sv;
	int i;

	sld->perc = perc;
	sv = svlist;

	if (sld->is_proportional == true) {
		for (i = 0; i < sld->totsv; i++, sv++) {
			float vec[3];
			if (perc > 0.0f) {
				copy_v3_v3(vec, sv->dir_a);
				mul_v3_fl(vec, perc);
				add_v3_v3v3(sv->v->co, sv->v_co_orig, vec);
			}
			else {
				copy_v3_v3(vec, sv->dir_b);
				mul_v3_fl(vec, -perc);
				add_v3_v3v3(sv->v->co, sv->v_co_orig, vec);
			}
		}
	}
	else {
		/**
		 * Implementation note, non proportional mode ignores the starting positions and uses only the
		 * a/b verts, this could be changed/improved so the distance is still met but the verts are moved along
		 * their original path (which may not be straight), however how it works now is OK and matches 2.4x - Campbell
		 *
		 * \note len_v3v3(curr_sv->dir_a, curr_sv->dir_b)
		 * is the same as the distance between the original vert locations, same goes for the lines below.
		 */
		TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
		const float curr_length_perc = curr_sv->edge_len * (((sld->flipped_vtx ? perc : -perc) + 1.0f) / 2.0f);

		float co_a[3];
		float co_b[3];

		for (i = 0; i < sld->totsv; i++, sv++) {
			if (sv->edge_len > FLT_EPSILON) {
				const float fac = min_ff(sv->edge_len, curr_length_perc) / sv->edge_len;

				add_v3_v3v3(co_a, sv->v_co_orig, sv->dir_a);
				add_v3_v3v3(co_b, sv->v_co_orig, sv->dir_b);

				if (sld->flipped_vtx) {
					interp_line_v3_v3v3v3(sv->v->co, co_b, sv->v_co_orig, co_a, fac);
				}
				else {
					interp_line_v3_v3v3v3(sv->v->co, co_a, sv->v_co_orig, co_b, fac);
				}
			}
		}
	}
	
	projectEdgeSlideData(t, 0);
	
	return 1;
}

static void applyEdgeSlide(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];
	float final;
	EdgeSlideData *sld =  t->customData;
	bool flipped = sld->flipped_vtx;
	bool is_proportional = sld->is_proportional;

	final = t->values[0];

	snapGridIncrement(t, &final);

	/* only do this so out of range values are not displayed */
	CLAMP(final, -1.0f, 1.0f);

	applyNumInput(&t->num, &final);

	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		if (is_proportional) {
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Edge Slide: %s (E)ven: %s"),
			             &c[0], WM_bool_as_string(!is_proportional));
		}
		else {
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Edge Slide: %s (E)ven: %s, (F)lipped: %s"),
			             &c[0], WM_bool_as_string(!is_proportional), WM_bool_as_string(flipped));
		}
	}
	else if (is_proportional) {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Edge Slide: %.4f (E)ven: %s"),
		             final, WM_bool_as_string(!is_proportional));
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Edge Slide: %.4f (E)ven: %s, (F)lipped: %s"),
		             final, WM_bool_as_string(!is_proportional), WM_bool_as_string(flipped));
	}

	CLAMP(final, -1.0f, 1.0f);

	t->values[0] = final;

	/* do stuff here */
	doEdgeSlide(t, final);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Vert Slide) */

/** \name Transform Vert Slide
 * \{ */

static void calcVertSlideCustomPoints(struct TransInfo *t)
{
	VertSlideData *sld = t->customData;
	TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];
	const float *co_orig = sv->co_orig_2d;
	const float *co_curr = sv->co_link_orig_2d[sv->co_link_curr];
	const int mval_start[2] = {co_orig[0], co_orig[1]};
	const int mval_end[2]   = {co_curr[0], co_curr[1]};

	if (sld->flipped_vtx && sld->is_proportional == false) {
		setCustomPoints(t, &t->mouse, mval_start, mval_end);
	}
	else {
		setCustomPoints(t, &t->mouse, mval_end, mval_start);
	}
}

/**
 * Run once when initializing vert slide to find the reference edge
 */
static void calcVertSlideMouseActiveVert(struct TransInfo *t, const int mval[2])
{
	VertSlideData *sld = t->customData;
	float mval_fl[2] = {UNPACK2(mval)};
	TransDataVertSlideVert *sv;

	/* set the vertex to use as a reference for the mouse direction 'curr_sv_index' */
	float dist_sq = 0.0f;
	float dist_min_sq = FLT_MAX;
	int i;

	for (i = 0, sv = sld->sv; i < sld->totsv; i++, sv++) {
		dist_sq = len_squared_v2v2(mval_fl, sv->co_orig_2d);
		if (dist_sq < dist_min_sq) {
			dist_min_sq = dist_sq;
			sld->curr_sv_index = i;
		}
	}
}
/**
 * Run while moving the mouse to slide along the edge matching the mouse direction
 */
static void calcVertSlideMouseActiveEdges(struct TransInfo *t, const int mval[2])
{
	VertSlideData *sld = t->customData;
	float mval_fl[2] = {UNPACK2(mval)};

	float dir[2];
	TransDataVertSlideVert *sv;
	int i;

	/* first get the direction of the original vertex */
	sub_v2_v2v2(dir, sld->sv[sld->curr_sv_index].co_orig_2d, mval_fl);
	normalize_v2(dir);

	for (i = 0, sv = sld->sv; i < sld->totsv; i++, sv++) {
		if (sv->co_link_tot > 1) {
			float dir_dot_best = -FLT_MAX;
			int co_link_curr_best = -1;
			int j;

			for (j = 0; j < sv->co_link_tot; j++) {
				float tdir[2];
				float dir_dot;
				sub_v2_v2v2(tdir, sv->co_orig_2d, sv->co_link_orig_2d[j]);
				normalize_v2(tdir);
				dir_dot = dot_v2v2(dir, tdir);
				if (dir_dot > dir_dot_best) {
					dir_dot_best = dir_dot;
					co_link_curr_best = j;
				}
			}

			if (co_link_curr_best != -1) {
				sv->co_link_curr = co_link_curr_best;
			}
		}
	}
}

static bool createVertSlideVerts(TransInfo *t)
{
	BMEditMesh *em = BKE_editmesh_from_object(t->obedit);
	BMesh *bm = em->bm;
	BMIter iter;
	BMIter eiter;
	BMEdge *e;
	BMVert *v;
	TransDataVertSlideVert *sv_array;
	VertSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
//	View3D *v3d = NULL;
	RegionView3D *rv3d = NULL;
	ARegion *ar = t->ar;
	float projectMat[4][4];
	int j;

	if (t->spacetype == SPACE_VIEW3D) {
		/* background mode support */
//		v3d = t->sa ? t->sa->spacedata.first : NULL;
		rv3d = ar ? ar->regiondata : NULL;
	}

	sld->is_proportional = true;
	sld->curr_sv_index = 0;
	sld->flipped_vtx = false;

	if (!rv3d) {
		/* ok, let's try to survive this */
		unit_m4(projectMat);
	}
	else {
		ED_view3d_ob_project_mat_get(rv3d, t->obedit, projectMat);
	}

	j = 0;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		bool ok = false;
		if (BM_elem_flag_test(v, BM_ELEM_SELECT) && v->e) {
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
					ok = true;
					break;
				}
			}
		}

		if (ok) {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
			j += 1;
		}
		else {
			BM_elem_flag_disable(v, BM_ELEM_TAG);
		}
	}

	if (!j) {
		MEM_freeN(sld);
		return false;
	}

	sv_array = MEM_callocN(sizeof(TransDataVertSlideVert) * j, "sv_array");

	j = 0;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
			int k;
			sv_array[j].v = v;
			copy_v3_v3(sv_array[j].co_orig_3d, v->co);

			k = 0;
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
					k++;
				}
			}

			sv_array[j].co_link_orig_3d = MEM_mallocN(sizeof(*sv_array[j].co_link_orig_3d) * k, __func__);
			sv_array[j].co_link_orig_2d = MEM_mallocN(sizeof(*sv_array[j].co_link_orig_2d) * k, __func__);
			sv_array[j].co_link_tot = k;

			k = 0;
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
					BMVert *v_other = BM_edge_other_vert(e, v);
					copy_v3_v3(sv_array[j].co_link_orig_3d[k], v_other->co);
					if (ar) {
						ED_view3d_project_float_v2_m4(ar,
						                              sv_array[j].co_link_orig_3d[k],
						                              sv_array[j].co_link_orig_2d[k],
						                              projectMat);
					}
					else {
						copy_v2_v2(sv_array[j].co_link_orig_2d[k],
						           sv_array[j].co_link_orig_3d[k]);
					}
					k++;
				}
			}

			if (ar) {
				ED_view3d_project_float_v2_m4(ar,
				                              sv_array[j].co_orig_3d,
				                              sv_array[j].co_orig_2d,
				                              projectMat);
			}
			else {
				copy_v2_v2(sv_array[j].co_orig_2d,
				           sv_array[j].co_orig_3d);
			}

			j++;
		}
	}

	sld->sv = sv_array;
	sld->totsv = j;

	sld->em = em;

	sld->perc = 0.0f;

	t->customData = sld;

	if (rv3d) {
		calcVertSlideMouseActiveVert(t, t->mval);
		calcVertSlideMouseActiveEdges(t, t->mval);
	}

	return true;
}

void freeVertSlideVerts(TransInfo *t)
{
	VertSlideData *sld = t->customData;

	if (!sld)
		return;


	if (sld->totsv > 0) {
		TransDataVertSlideVert *sv = sld->sv;
		int i = 0;
		for (i = 0; i < sld->totsv; i++, sv++) {
			MEM_freeN(sv->co_link_orig_2d);
			MEM_freeN(sv->co_link_orig_3d);
		}
	}

	MEM_freeN(sld->sv);
	MEM_freeN(sld);

	t->customData = NULL;

	recalcData(t);
}

static void initVertSlide(TransInfo *t)
{
	VertSlideData *sld;

	t->mode = TFM_VERT_SLIDE;
	t->transform = applyVertSlide;
	t->handleEvent = handleEventVertSlide;

	if (!createVertSlideVerts(t)) {
		t->state = TRANS_CANCEL;
		return;
	}

	sld = t->customData;

	if (!sld)
		return;

	t->customFree = freeVertSlideVerts;

	/* set custom point first if you want value to be initialized by init */
	calcVertSlideCustomPoints(t);
	initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 0.1f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static eRedrawFlag handleEventVertSlide(struct TransInfo *t, const struct wmEvent *event)
{
	if (t->mode == TFM_VERT_SLIDE) {
		VertSlideData *sld = t->customData;

		if (sld) {
			switch (event->type) {
				case EKEY:
					if (event->val == KM_PRESS) {
						sld->is_proportional = !sld->is_proportional;
						if (sld->flipped_vtx) {
							calcVertSlideCustomPoints(t);
						}
						return TREDRAW_HARD;
					}
					break;
				case FKEY:
				{
					if (event->val == KM_PRESS) {
						sld->flipped_vtx = !sld->flipped_vtx;
						calcVertSlideCustomPoints(t);
						return TREDRAW_HARD;
					}
					break;
				}
				case CKEY:
				{
					/* use like a modifier key */
					if (event->val == KM_PRESS) {
						t->flag ^= T_ALT_TRANSFORM;
						calcVertSlideCustomPoints(t);
						return TREDRAW_HARD;
					}
					break;
				}
#if 0
				case EVT_MODAL_MAP:
				{
					switch (event->val) {
						case TFM_MODAL_EDGESLIDE_DOWN:
						{
							sld->curr_sv_index = ((sld->curr_sv_index - 1) + sld->totsv) % sld->totsv;
							break;
						}
						case TFM_MODAL_EDGESLIDE_UP:
						{
							sld->curr_sv_index = (sld->curr_sv_index + 1) % sld->totsv;
							break;
						}
					}
				}
#endif
				case MOUSEMOVE:
				{
					/* don't recalculat the best edge */
					const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
					if (is_clamp) {
						calcVertSlideMouseActiveEdges(t, event->mval);
					}
					calcVertSlideCustomPoints(t);
					break;
				}
				default:
					break;
			}
		}
	}
	return TREDRAW_NOTHING;
}

static void drawVertSlide(const struct bContext *C, TransInfo *t)
{
	if (t->mode == TFM_VERT_SLIDE) {
		VertSlideData *sld = (VertSlideData *)t->customData;
		/* Non-Prop mode */
		if (sld) {
			View3D *v3d = CTX_wm_view3d(C);
			TransDataVertSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
			TransDataVertSlideVert *sv;
			const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
			const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;
			const int alpha_shade = -160;
			const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
			int i;

			if (v3d && v3d->zbuf)
				glDisable(GL_DEPTH_TEST);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_POINT_BIT);
			glPushMatrix();

			glMultMatrixf(t->obedit->obmat);

			glLineWidth(line_size);
			UI_ThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
			glBegin(GL_LINES);
			if (is_clamp) {
				sv = sld->sv;
				for (i = 0; i < sld->totsv; i++, sv++) {
					glVertex3fv(sv->co_orig_3d);
					glVertex3fv(sv->co_link_orig_3d[sv->co_link_curr]);
				}
			}
			else {
				sv = sld->sv;
				for (i = 0; i < sld->totsv; i++, sv++) {
					float a[3], b[3];
					sub_v3_v3v3(a, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
					mul_v3_fl(a, 100.0f);
					negate_v3_v3(b, a);
					add_v3_v3(a, sv->co_orig_3d);
					add_v3_v3(b, sv->co_orig_3d);

					glVertex3fv(a);
					glVertex3fv(b);
				}
			}
			bglEnd();

			glPointSize(ctrl_size);

			bglBegin(GL_POINTS);
			bglVertex3fv((sld->flipped_vtx && sld->is_proportional == false) ?
			             curr_sv->co_link_orig_3d[curr_sv->co_link_curr] :
			             curr_sv->co_orig_3d);
			bglEnd();

			glPopMatrix();
			glPopAttrib();

			glDisable(GL_BLEND);

			if (v3d && v3d->zbuf)
				glEnable(GL_DEPTH_TEST);
		}
	}
}

static int doVertSlide(TransInfo *t, float perc)
{
	VertSlideData *sld = t->customData;
	TransDataVertSlideVert *svlist = sld->sv, *sv;
	int i;

	sld->perc = perc;
	sv = svlist;

	if (sld->is_proportional == true) {
		for (i = 0; i < sld->totsv; i++, sv++) {
			interp_v3_v3v3(sv->v->co, sv->co_orig_3d, sv->co_link_orig_3d[sv->co_link_curr], perc);
		}
	}
	else {
		TransDataVertSlideVert *sv_curr = &sld->sv[sld->curr_sv_index];
		const float edge_len_curr = len_v3v3(sv_curr->co_orig_3d, sv_curr->co_link_orig_3d[sv_curr->co_link_curr]);
		const float tperc = perc * edge_len_curr;

		for (i = 0; i < sld->totsv; i++, sv++) {
			float edge_len;
			float dir[3];

			sub_v3_v3v3(dir, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
			edge_len = normalize_v3(dir);

			if (edge_len > FLT_EPSILON) {
				if (sld->flipped_vtx) {
					madd_v3_v3v3fl(sv->v->co, sv->co_link_orig_3d[sv->co_link_curr], dir, -tperc);
				}
				else {
					madd_v3_v3v3fl(sv->v->co, sv->co_orig_3d, dir, tperc);
				}
			}
			else {
				copy_v3_v3(sv->v->co, sv->co_orig_3d);
			}
		}
	}

	return 1;
}

static void applyVertSlide(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];
	size_t ofs = 0;
	float final;
	VertSlideData *sld =  t->customData;
	const bool flipped = sld->flipped_vtx;
	const bool is_proportional = sld->is_proportional;
	const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
	const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

	final = t->values[0];

	snapGridIncrement(t, &final);

	/* only do this so out of range values are not displayed */
	if (is_constrained) {
		CLAMP(final, 0.0f, 1.0f);
	}

	applyNumInput(&t->num, &final);

	/* header string */
	ofs += BLI_strncpy_rlen(str + ofs, IFACE_("Vert Slide: "), MAX_INFO_LEN - ofs);
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];
		outputNumInput(&(t->num), c, &t->scene->unit);
		ofs += BLI_strncpy_rlen(str + ofs, &c[0], MAX_INFO_LEN - ofs);
	}
	else {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, "%.4f ", final);
	}
	ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("(E)ven: %s, "), WM_bool_as_string(!is_proportional));
	if (!is_proportional) {
		ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("(F)lipped: %s, "), WM_bool_as_string(flipped));
	}
	ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Alt or (C)lamp: %s"), WM_bool_as_string(is_clamp));
	/* done with header string */

	/* do stuff here */
	doVertSlide(t, final);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (EditBone Roll) */

/** \name Transform EditBone Roll
 * \{ */

static void initBoneRoll(TransInfo *t)
{
	t->mode = TFM_BONE_ROLL;
	t->transform = applyBoneRoll;

	initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = DEG2RAD(5.0);
	t->snap[2] = DEG2RAD(1.0);

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
	t->num.unit_type[0] = B_UNIT_ROTATION;

	t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyBoneRoll(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	int i;
	char str[MAX_INFO_LEN];

	float final;

	final = t->values[0];

	snapGridIncrement(t, &final);

	applyNumInput(&t->num, &final);

	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Roll: %s"), &c[0]);
	}
	else {
		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Roll: %.2f"), RAD2DEGF(final));
	}

	/* set roll values */
	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		*(td->val) = td->ival - final;
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Bake-Time) */

/** \name Transform Bake-Time
 * \{ */

static void initBakeTime(TransInfo *t)
{
	t->transform = applyBakeTime;
	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	t->idx_max = 0;
	t->num.idx_max = 0;
	t->snap[0] = 0.0f;
	t->snap[1] = 1.0f;
	t->snap[2] = t->snap[1] * 0.1f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;  /* Don't think this uses units? */
}

static void applyBakeTime(TransInfo *t, const int mval[2])
{
	TransData *td = t->data;
	float time;
	int i;
	char str[MAX_INFO_LEN];

	float fac = 0.1f;

	if (t->mouse.precision) {
		/* calculate ratio for shiftkey pos, and for total, and blend these for precision */
		time = (float)(t->center2d[0] - t->mouse.precision_mval[0]) * fac;
		time += 0.1f * ((float)(t->center2d[0] * fac - mval[0]) - time);
	}
	else {
		time = (float)(t->center2d[0] - mval[0]) * fac;
	}

	snapGridIncrement(t, &time);

	applyNumInput(&t->num, &time);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[NUM_STR_REP_LEN];

		outputNumInput(&(t->num), c, &t->scene->unit);

		if (time >= 0.0f)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Time: +%s %s"), c, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Time: %s %s"), c, t->proptext);
	}
	else {
		/* default header print */
		if (time >= 0.0f)
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Time: +%.3f %s"), time, t->proptext);
		else
			BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Time: %.3f %s"), time, t->proptext);
	}

	for (i = 0; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		if (td->val) {
			*td->val = td->ival + time * td->factor;
			if (td->ext->size && *td->val < *td->ext->size) *td->val = *td->ext->size;
			if (td->ext->quat && *td->val > *td->ext->quat) *td->val = *td->ext->quat;
		}
	}

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Mirror) */

/** \name Transform Mirror
 * \{ */

static void initMirror(TransInfo *t)
{
	t->transform = applyMirror;
	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	t->flag |= T_NULL_ONE;
	if (!t->obedit) {
		t->flag |= T_NO_ZERO;
	}
}

static void applyMirror(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td;
	float size[3], mat[3][3];
	int i;
	char str[MAX_INFO_LEN];

	/*
	 * OPTIMIZATION:
	 * This still recalcs transformation on mouse move
	 * while it should only recalc on constraint change
	 * */

	/* if an axis has been selected */
	if (t->con.mode & CON_APPLY) {
		size[0] = size[1] = size[2] = -1;

		size_to_mat3(mat, size);

		if (t->con.applySize) {
			t->con.applySize(t, NULL, mat);
		}

		BLI_snprintf(str, MAX_INFO_LEN, IFACE_("Mirror%s"), t->con.text);

		for (i = 0, td = t->data; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			ElementResize(t, td, mat);
		}

		recalcData(t);

		ED_area_headerprint(t->sa, str);
	}
	else {
		size[0] = size[1] = size[2] = 1;

		size_to_mat3(mat, size);

		for (i = 0, td = t->data; i < t->total; i++, td++) {
			if (td->flag & TD_NOACTION)
				break;

			if (td->flag & TD_SKIP)
				continue;

			ElementResize(t, td, mat);
		}

		recalcData(t);

		if (t->flag & T_2D_EDIT)
			ED_area_headerprint(t->sa, IFACE_("Select a mirror axis (X, Y)"));
		else
			ED_area_headerprint(t->sa, IFACE_("Select a mirror axis (X, Y, Z)"));
	}
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Align) */

/** \name Transform Align
 * \{ */

static void initAlign(TransInfo *t)
{
	t->flag |= T_NO_CONSTRAINT;

	t->transform = applyAlign;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);
}

static void applyAlign(TransInfo *t, const int UNUSED(mval[2]))
{
	TransData *td = t->data;
	float center[3];
	int i;

	/* saving original center */
	copy_v3_v3(center, t->center);

	for (i = 0; i < t->total; i++, td++) {
		float mat[3][3], invmat[3][3];

		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		/* around local centers */
		if (t->flag & (T_OBJECT | T_POSE)) {
			copy_v3_v3(t->center, td->center);
		}
		else {
			if (t->settings->selectmode & SCE_SELECT_FACE) {
				copy_v3_v3(t->center, td->center);
			}
		}

		invert_m3_m3(invmat, td->axismtx);

		mul_m3_m3m3(mat, t->spacemtx, invmat);

		ElementRotation(t, td, mat, t->around);
	}

	/* restoring original center */
	copy_v3_v3(t->center, center);

	recalcData(t);

	ED_area_headerprint(t->sa, IFACE_("Align"));
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Sequencer Slide) */

/** \name Transform Sequencer Slide
 * \{ */

static void initSeqSlide(TransInfo *t)
{
	t->transform = applySeqSlide;

	initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

	t->idx_max = 1;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	t->snap[0] = 0.0f;
	t->snap[1] = floor(t->scene->r.frs_sec / t->scene->r.frs_sec_base);
	t->snap[2] = 10.0f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	/* Would be nice to have a time handling in units as well (supporting frames in addition to "natural" time...). */
	t->num.unit_type[0] = B_UNIT_NONE;
	t->num.unit_type[1] = B_UNIT_NONE;
}

static void headerSeqSlide(TransInfo *t, float val[2], char str[MAX_INFO_LEN])
{
	char tvec[NUM_STR_REP_LEN * 3];
	size_t ofs = 0;

	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec, &t->scene->unit);
	}
	else {
		BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.0f, %.0f", val[0], val[1]);
	}

	ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_("Sequence Slide: %s%s, ("), &tvec[0], t->con.text);

	if (t->keymap) {
		wmKeyMapItem *kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_TRANSLATE);
		if (kmi) {
			ofs += WM_keymap_item_to_string(kmi, str + ofs, MAX_INFO_LEN - ofs);
		}
	}
	ofs += BLI_snprintf(str + ofs, MAX_INFO_LEN - ofs, IFACE_(" or Alt) Expand to fit %s"),
	                    WM_bool_as_string((t->flag & T_ALT_TRANSFORM) != 0));
}

static void applySeqSlideValue(TransInfo *t, const float val[2])
{
	TransData *td = t->data;
	int i;

	for (i = 0; i < t->total; i++, td++) {
		float tvec[2];

		if (td->flag & TD_NOACTION)
			break;

		if (td->flag & TD_SKIP)
			continue;

		copy_v2_v2(tvec, val);

		mul_v2_fl(tvec, td->factor);

		td->loc[0] = td->iloc[0] + tvec[0];
		td->loc[1] = td->iloc[1] + tvec[1];
	}
}

static void applySeqSlide(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];

	if (t->con.mode & CON_APPLY) {
		float pvec[3] = {0.0f, 0.0f, 0.0f};
		float tvec[3];
		t->con.applyVec(t, NULL, t->values, tvec, pvec);
		copy_v3_v3(t->values, tvec);
	}
	else {
		snapGridIncrement(t, t->values);
		applyNumInput(&t->num, t->values);
	}

	t->values[0] = floor(t->values[0] + 0.5f);
	t->values[1] = floor(t->values[1] + 0.5f);

	headerSeqSlide(t, t->values, str);
	applySeqSlideValue(t, t->values);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Animation Editors - Transform Utils
 *
 * Special Helpers for Various Settings
 */

/** \name Animation Editor Utils
 * \{ */

/* This function returns the snapping 'mode' for Animation Editors only
 * We cannot use the standard snapping due to NLA-strip scaling complexities.
 */
// XXX these modifier checks should be keymappable
static short getAnimEdit_SnapMode(TransInfo *t)
{
	short autosnap = SACTSNAP_OFF;
	
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
		
		if (saction)
			autosnap = saction->autosnap;
	}
	else if (t->spacetype == SPACE_IPO) {
		SpaceIpo *sipo = (SpaceIpo *)t->sa->spacedata.first;
		
		if (sipo)
			autosnap = sipo->autosnap;
	}
	else if (t->spacetype == SPACE_NLA) {
		SpaceNla *snla = (SpaceNla *)t->sa->spacedata.first;
		
		if (snla)
			autosnap = snla->autosnap;
	}
	else {
		autosnap = SACTSNAP_OFF;
	}
	
	/* toggle autosnap on/off 
	 *  - when toggling on, prefer nearest frame over 1.0 frame increments
	 */
	if (t->modifiers & MOD_SNAP_INVERT) {
		if (autosnap)
			autosnap = SACTSNAP_OFF;
		else
			autosnap = SACTSNAP_FRAME;
	}

	return autosnap;
}

/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
static void doAnimEdit_SnapFrame(TransInfo *t, TransData *td, TransData2D *td2d, AnimData *adt, short autosnap)
{
	/* snap key to nearest frame or second? */
	if (ELEM(autosnap, SACTSNAP_FRAME, SACTSNAP_SECOND)) {
		const Scene *scene = t->scene;
		const double secf = FPS;
		double val;
		
		/* convert frame to nla-action time (if needed) */
		if (adt)
			val = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
		else
			val = *(td->val);
		
		/* do the snapping to nearest frame/second */
		if (autosnap == SACTSNAP_FRAME) {
			val = floorf(val + 0.5);
		}
		else if (autosnap == SACTSNAP_SECOND) {
			val = (float)(floor((val / secf) + 0.5) * secf);
		}
		
		/* convert frame out of nla-action time */
		if (adt)
			*(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
		else
			*(td->val) = val;
	}
	/* snap key to nearest marker? */
	else if (autosnap == SACTSNAP_MARKER) {
		float val;
		
		/* convert frame to nla-action time (if needed) */
		if (adt)
			val = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
		else
			val = *(td->val);
		
		/* snap to nearest marker */
		// TODO: need some more careful checks for where data comes from
		val = (float)ED_markers_find_nearest_marker_time(&t->scene->markers, val);
		
		/* convert frame out of nla-action time */
		if (adt)
			*(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
		else
			*(td->val) = val;
	}
	
	/* if the handles are to be moved too (as side-effect of keyframes moving, to keep the general effect) 
	 * offset them by the same amount so that the general angles are maintained (i.e. won't change while 
	 * handles are free-to-roam and keyframes are snap-locked)
	 */
	if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
		td2d->h1[0] = td2d->ih1[0] + *td->val - td->ival;
	}
	if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
		td2d->h2[0] = td2d->ih2[0] + *td->val - td->ival;
	}
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Animation Translation) */

/** \name Transform Animation Translation
 * \{ */

static void initTimeTranslate(TransInfo *t)
{
	/* this tool is only really available in the Action Editor... */
	if (!ELEM(t->spacetype, SPACE_ACTION, SPACE_SEQ)) {
		t->state = TRANS_CANCEL;
	}

	t->mode = TFM_TIME_TRANSLATE;
	t->transform = applyTimeTranslate;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialize snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	/* No time unit supporting frames currently... */
	t->num.unit_type[0] = B_UNIT_NONE;
}

static void headerTimeTranslate(TransInfo *t, char str[MAX_INFO_LEN])
{
	char tvec[NUM_STR_REP_LEN * 3];

	/* if numeric input is active, use results from that, otherwise apply snapping to result */
	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec, &t->scene->unit);
	}
	else {
		const Scene *scene = t->scene;
		const short autosnap = getAnimEdit_SnapMode(t);
		const double secf = FPS;
		float val = t->values[0];
		
		/* apply snapping + frame->seconds conversions */
		if (autosnap == SACTSNAP_STEP) {
			/* frame step */
			val = floorf(val + 0.5f);
		}
		else if (autosnap == SACTSNAP_TSTEP) {
			/* second step */
			val = floorf((double)val / secf + 0.5);
		}
		else if (autosnap == SACTSNAP_SECOND) {
			/* nearest second */
			val = (float)((double)val / secf);
		}
		
		if (autosnap == SACTSNAP_FRAME)
			BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%d.00 (%.4f)", (int)val, val);
		else if (autosnap == SACTSNAP_SECOND)
			BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%d.00 sec (%.4f)", (int)val, val);
		else if (autosnap == SACTSNAP_TSTEP)
			BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f sec", val);
		else
			BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", val);
	}

	BLI_snprintf(str, MAX_INFO_LEN, IFACE_("DeltaX: %s"), &tvec[0]);
}

static void applyTimeTranslateValue(TransInfo *t, float UNUSED(sval))
{
	TransData *td = t->data;
	TransData2D *td2d = t->data2d;
	Scene *scene = t->scene;
	int i;
	
	const short autosnap = getAnimEdit_SnapMode(t);
	const double secf = FPS;

	float deltax, val /* , valprev */;

	/* it doesn't matter whether we apply to t->data or t->data2d, but t->data2d is more convenient */
	for (i = 0; i < t->total; i++, td++, td2d++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;

		/* valprev = *td->val; */ /* UNUSED */

		/* check if any need to apply nla-mapping */
		if (adt && (t->spacetype != SPACE_SEQ)) {
			deltax = t->values[0];

			if (autosnap == SACTSNAP_TSTEP) {
				deltax = (float)(floor(((double)deltax / secf) + 0.5) * secf);
			}
			else if (autosnap == SACTSNAP_STEP) {
				deltax = (float)(floor(deltax + 0.5f));
			}

			val = BKE_nla_tweakedit_remap(adt, td->ival, NLATIME_CONVERT_MAP);
			val += deltax;
			*(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
		}
		else {
			deltax = val = t->values[0];

			if (autosnap == SACTSNAP_TSTEP) {
				val = (float)(floor(((double)deltax / secf) + 0.5) * secf);
			}
			else if (autosnap == SACTSNAP_STEP) {
				val = (float)(floor(val + 0.5f));
			}

			*(td->val) = td->ival + val;
		}

		/* apply nearest snapping */
		doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
	}
}

static void applyTimeTranslate(TransInfo *t, const int mval[2])
{
	View2D *v2d = (View2D *)t->view;
	float cval[2], sval[2];
	char str[MAX_INFO_LEN];

	/* calculate translation amount from mouse movement - in 'time-grid space' */
	UI_view2d_region_to_view(v2d, mval[0], mval[0], &cval[0], &cval[1]);
	UI_view2d_region_to_view(v2d, t->imval[0], t->imval[0], &sval[0], &sval[1]);

	/* we only need to calculate effect for time (applyTimeTranslate only needs that) */
	t->values[0] = cval[0] - sval[0];

	/* handle numeric-input stuff */
	t->vec[0] = t->values[0];
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = t->vec[0];
	headerTimeTranslate(t, str);

	applyTimeTranslateValue(t, sval[0]);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Animation Time Slide) */

/** \name Transform Animation Time Slide
 * \{ */

static void initTimeSlide(TransInfo *t)
{
	/* this tool is only really available in the Action Editor... */
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;

		/* set flag for drawing stuff */
		saction->flag |= SACTION_MOVING;
	}
	else {
		t->state = TRANS_CANCEL;
	}


	t->mode = TFM_TIME_SLIDE;
	t->transform = applyTimeSlide;
	t->flag |= T_FREE_CUSTOMDATA;

	initMouseInputMode(t, &t->mouse, INPUT_NONE);

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialize snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	/* No time unit supporting frames currently... */
	t->num.unit_type[0] = B_UNIT_NONE;
}

static void headerTimeSlide(TransInfo *t, float sval, char str[MAX_INFO_LEN])
{
	char tvec[NUM_STR_REP_LEN * 3];

	if (hasNumInput(&t->num)) {
		outputNumInput(&(t->num), tvec, &t->scene->unit);
	}
	else {
		float minx = *((float *)(t->customData));
		float maxx = *((float *)(t->customData) + 1);
		float cval = t->values[0];
		float val;

		val = 2.0f * (cval - sval) / (maxx - minx);
		CLAMP(val, -1.0f, 1.0f);

		BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", val);
	}

	BLI_snprintf(str, MAX_INFO_LEN, IFACE_("TimeSlide: %s"), &tvec[0]);
}

static void applyTimeSlideValue(TransInfo *t, float sval)
{
	TransData *td = t->data;
	int i;

	float minx = *((float *)(t->customData));
	float maxx = *((float *)(t->customData) + 1);

	/* set value for drawing black line */
	if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
		float cvalf = t->values[0];

		saction->timeslide = cvalf;
	}

	/* it doesn't matter whether we apply to t->data or t->data2d, but t->data2d is more convenient */
	for (i = 0; i < t->total; i++, td++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;
		float cval = t->values[0];

		/* apply NLA-mapping to necessary values */
		if (adt)
			cval = BKE_nla_tweakedit_remap(adt, cval, NLATIME_CONVERT_UNMAP);

		/* only apply to data if in range */
		if ((sval > minx) && (sval < maxx)) {
			float cvalc = CLAMPIS(cval, minx, maxx);
			float timefac;

			/* left half? */
			if (td->ival < sval) {
				timefac = (sval - td->ival) / (sval - minx);
				*(td->val) = cvalc - timefac * (cvalc - minx);
			}
			else {
				timefac = (td->ival - sval) / (maxx - sval);
				*(td->val) = cvalc + timefac * (maxx - cvalc);
			}
		}
	}
}

static void applyTimeSlide(TransInfo *t, const int mval[2])
{
	View2D *v2d = (View2D *)t->view;
	float cval[2], sval[2];
	float minx = *((float *)(t->customData));
	float maxx = *((float *)(t->customData) + 1);
	char str[MAX_INFO_LEN];

	/* calculate mouse co-ordinates */
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &cval[0], &cval[1]);
	UI_view2d_region_to_view(v2d, t->imval[0], t->imval[1], &sval[0], &sval[1]);

	/* t->values[0] stores cval[0], which is the current mouse-pointer location (in frames) */
	// XXX Need to be able to repeat this
	t->values[0] = cval[0];

	/* handle numeric-input stuff */
	t->vec[0] = 2.0f * (cval[0] - sval[0]) / (maxx - minx);
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = (maxx - minx) * t->vec[0] / 2.0f + sval[0];

	headerTimeSlide(t, sval[0], str);
	applyTimeSlideValue(t, sval[0]);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* -------------------------------------------------------------------- */
/* Transform (Animation Time Scale) */

/** \name Transform Animation Time Scale
 * \{ */

static void initTimeScale(TransInfo *t)
{
	float center[2];

	/* this tool is only really available in the Action Editor
	 * AND NLA Editor (for strip scaling)
	 */
	if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA) == 0) {
		t->state = TRANS_CANCEL;
	}

	t->mode = TFM_TIME_SCALE;
	t->transform = applyTimeScale;

	/* recalculate center2d to use CFRA and mouse Y, since that's
	 * what is used in time scale */
	t->center[0] = t->scene->r.cfra;
	projectFloatView(t, t->center, center);
	center[1] = t->imval[1];

	/* force a reinit with the center2d used here */
	initMouseInput(t, &t->mouse, center, t->imval);

	initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

	t->flag |= T_NULL_ONE;
	t->num.val_flag[0] |= NUM_NULL_ONE;

	/* num-input has max of (n-1) */
	t->idx_max = 0;
	t->num.flag = 0;
	t->num.idx_max = t->idx_max;

	/* initialize snap like for everything else */
	t->snap[0] = 0.0f;
	t->snap[1] = t->snap[2] = 1.0f;

	copy_v3_fl(t->num.val_inc, t->snap[1]);
	t->num.unit_sys = t->scene->unit.system;
	t->num.unit_type[0] = B_UNIT_NONE;
}

static void headerTimeScale(TransInfo *t, char str[MAX_INFO_LEN])
{
	char tvec[NUM_STR_REP_LEN * 3];

	if (hasNumInput(&t->num))
		outputNumInput(&(t->num), tvec, &t->scene->unit);
	else
		BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", t->values[0]);

	BLI_snprintf(str, MAX_INFO_LEN, IFACE_("ScaleX: %s"), &tvec[0]);
}

static void applyTimeScaleValue(TransInfo *t)
{
	Scene *scene = t->scene;
	TransData *td = t->data;
	TransData2D *td2d = t->data2d;
	int i;

	const short autosnap = getAnimEdit_SnapMode(t);
	const double secf = FPS;


	for (i = 0; i < t->total; i++, td++, td2d++) {
		/* it is assumed that td->extra is a pointer to the AnimData,
		 * whose active action is where this keyframe comes from
		 * (this is only valid when not in NLA)
		 */
		AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;
		float startx = CFRA;
		float fac = t->values[0];

		if (autosnap == SACTSNAP_TSTEP) {
			fac = (float)(floor((double)fac / secf + 0.5) * secf);
		}
		else if (autosnap == SACTSNAP_STEP) {
			fac = (float)(floor(fac + 0.5f));
		}

		/* check if any need to apply nla-mapping */
		if (adt)
			startx = BKE_nla_tweakedit_remap(adt, startx, NLATIME_CONVERT_UNMAP);

		/* now, calculate the new value */
		*(td->val) = ((td->ival - startx) * fac) + startx;

		/* apply nearest snapping */
		doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
	}
}

static void applyTimeScale(TransInfo *t, const int UNUSED(mval[2]))
{
	char str[MAX_INFO_LEN];
	
	/* handle numeric-input stuff */
	t->vec[0] = t->values[0];
	applyNumInput(&t->num, &t->vec[0]);
	t->values[0] = t->vec[0];
	headerTimeScale(t, str);

	applyTimeScaleValue(t);

	recalcData(t);

	ED_area_headerprint(t->sa, str);
}
/** \} */


/* TODO, move to: transform_queries.c */
bool checkUseAxisMatrix(TransInfo *t)
{
	/* currently only checks for editmode */
	if (t->flag & T_EDIT) {
		if ((t->around == V3D_LOCAL) && (ELEM(t->obedit->type, OB_MESH, OB_CURVE, OB_MBALL, OB_ARMATURE))) {
			/* not all editmode supports axis-matrix */
			return true;
		}
	}

	return false;
}

#undef MAX_INFO_LEN
