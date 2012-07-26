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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_image/image_editor.c
 *  \ingroup spimage
 */

#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_tessmesh.h"

#include "IMB_imbuf_types.h"

#include "ED_image.h"  /* own include */
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

/* note; image_panel_properties() uses pointer to sima->image directly */
Image *ED_space_image(SpaceImage *sima)
{
	return sima->image;
}

/* called to assign images to UV faces */
void ED_space_image_set(SpaceImage *sima, Scene *scene, Object *obedit, Image *ima)
{
	/* context may be NULL, so use global */
	ED_uvedit_assign_image(G.main, scene, obedit, ima, sima->image);

	/* change the space ima after because uvedit_face_visible_test uses the space ima
	 * to check if the face is displayed in UV-localview */
	sima->image = ima;

	if (ima == NULL || ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE) {
		if (sima->mode == SI_MODE_PAINT) {
			sima->mode = SI_MODE_VIEW;
		}
	}

	if (sima->image)
		BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);

	if (sima->image && sima->image->id.us == 0)
		sima->image->id.us = 1;

	if (obedit)
		WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);

	WM_main_add_notifier(NC_SPACE | ND_SPACE_IMAGE, NULL);
}

Mask *ED_space_image_get_mask(SpaceImage *sima)
{
	return sima->mask_info.mask;
}

void ED_space_image_set_mask(bContext *C, SpaceImage *sima, Mask *mask)
{
	sima->mask_info.mask = mask;

	if (C) {
		WM_event_add_notifier(C, NC_MASK | NA_SELECTED, mask);
	}
}

ImBuf *ED_space_image_acquire_buffer(SpaceImage *sima, void **lock_r)
{
	ImBuf *ibuf;

	if (sima && sima->image) {
#if 0
		if (sima->image->type == IMA_TYPE_R_RESULT && BIF_show_render_spare())
			return BIF_render_spare_imbuf();
		else
#endif
		ibuf = BKE_image_acquire_ibuf(sima->image, &sima->iuser, lock_r);

		if (ibuf && (ibuf->rect || ibuf->rect_float))
			return ibuf;
	}

	return NULL;
}

void ED_space_image_release_buffer(SpaceImage *sima, void *lock)
{
	if (sima && sima->image)
		BKE_image_release_ibuf(sima->image, lock);
}

int ED_space_image_has_buffer(SpaceImage *sima)
{
	ImBuf *ibuf;
	void *lock;
	int has_buffer;

	ibuf = ED_space_image_acquire_buffer(sima, &lock);
	has_buffer = (ibuf != NULL);
	ED_space_image_release_buffer(sima, lock);

	return has_buffer;
}

void ED_image_get_size(Image *ima, int *width, int *height)
{
	ImBuf *ibuf = NULL;
	void *lock;

	if (ima)
		ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

	if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width = ibuf->x;
		*height = ibuf->y;
	}
	else {
		*width  = IMG_SIZE_FALLBACK;
		*height = IMG_SIZE_FALLBACK;
	}

	if (ima)
		BKE_image_release_ibuf(ima, lock);
}

void ED_space_image_get_size(SpaceImage *sima, int *width, int *height)
{
	Scene *scene = sima->iuser.scene;
	ImBuf *ibuf;
	void *lock;

	ibuf = ED_space_image_acquire_buffer(sima, &lock);

	if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width = ibuf->x;
		*height = ibuf->y;
	}
	else if (sima->image && sima->image->type == IMA_TYPE_R_RESULT && scene) {
		/* not very important, just nice */
		*width = (scene->r.xsch * scene->r.size) / 100;
		*height = (scene->r.ysch * scene->r.size) / 100;

		if ((scene->r.mode & R_BORDER) && (scene->r.mode & R_CROP)) {
			*width *= (scene->r.border.xmax - scene->r.border.xmin);
			*height *= (scene->r.border.ymax - scene->r.border.ymin);
		}

	}
	/* I know a bit weak... but preview uses not actual image size */
	// XXX else if (image_preview_active(sima, width, height));
	else {
		*width  = IMG_SIZE_FALLBACK;
		*height = IMG_SIZE_FALLBACK;
	}

	ED_space_image_release_buffer(sima, lock);
}

void ED_image_get_aspect(Image *ima, float *aspx, float *aspy)
{
	*aspx = *aspy = 1.0;

	if ((ima == NULL) || (ima->type == IMA_TYPE_R_RESULT) || (ima->type == IMA_TYPE_COMPOSITE) ||
	    (ima->aspx == 0.0f || ima->aspy == 0.0f))
	{
		return;
	}

	/* x is always 1 */
	*aspy = ima->aspy / ima->aspx;
}

void ED_space_image_get_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	ED_image_get_aspect(ED_space_image(sima), aspx, aspy);
}

void ED_space_image_get_zoom(SpaceImage *sima, ARegion *ar, float *zoomx, float *zoomy)
{
	int width, height;

	ED_space_image_get_size(sima, &width, &height);

	*zoomx = (float)(ar->winrct.xmax - ar->winrct.xmin + 1) / (float)((ar->v2d.cur.xmax - ar->v2d.cur.xmin) * width);
	*zoomy = (float)(ar->winrct.ymax - ar->winrct.ymin + 1) / (float)((ar->v2d.cur.ymax - ar->v2d.cur.ymin) * height);
}

void ED_space_image_get_uv_aspect(SpaceImage *sima, float *aspx, float *aspy)
{
	int w, h;

	ED_space_image_get_aspect(sima, aspx, aspy);
	ED_space_image_get_size(sima, &w, &h);

	*aspx *= (float)w;
	*aspy *= (float)h;

	if (*aspx < *aspy) {
		*aspy = *aspy / *aspx;
		*aspx = 1.0f;
	}
	else {
		*aspx = *aspx / *aspy;
		*aspy = 1.0f;
	}
}

void ED_image_get_uv_aspect(Image *ima, float *aspx, float *aspy)
{
	int w, h;

	ED_image_get_aspect(ima, aspx, aspy);
	ED_image_get_size(ima, &w, &h);

	*aspx *= (float)w;
	*aspy *= (float)h;
}

void ED_image_mouse_pos(SpaceImage *sima, ARegion *ar, wmEvent *event, float co[2])
{
	int sx, sy, width, height;
	float zoomx, zoomy;

	ED_space_image_get_zoom(sima, ar, &zoomx, &zoomy);
	ED_space_image_get_size(sima, &width, &height);

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);

	co[0] = ((event->mval[0] - sx) / zoomx) / width;
	co[1] = ((event->mval[1] - sy) / zoomy) / height;
}

void ED_image_point_pos(SpaceImage *sima, ARegion *ar, float x, float y, float *xr, float *yr)
{
	int sx, sy, width, height;
	float zoomx, zoomy;

	ED_space_image_get_zoom(sima, ar, &zoomx, &zoomy);
	ED_space_image_get_size(sima, &width, &height);

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);

	*xr = ((x - sx) / zoomx) / width;
	*yr = ((y - sy) / zoomy) / height;
}

void ED_image_point_pos__reverse(SpaceImage *sima, ARegion *ar, const float co[2], float r_co[2])
{
	float zoomx, zoomy;
	int width, height;
	int sx, sy;

	UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &sx, &sy);
	ED_space_image_get_size(sima, &width, &height);
	ED_space_image_get_zoom(sima, ar, &zoomx, &zoomy);

	r_co[0] = (co[0] * width  * zoomx) + (float)sx;
	r_co[1] = (co[1] * height * zoomy) + (float)sy;
}

int ED_space_image_show_render(SpaceImage *sima)
{
	return (sima->image && ELEM(sima->image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE));
}

int ED_space_image_show_paint(SpaceImage *sima)
{
	if (ED_space_image_show_render(sima))
		return 0;

	return (sima->mode == SI_MODE_PAINT);
}

int ED_space_image_show_uvedit(SpaceImage *sima, Object *obedit)
{
	if (sima && (ED_space_image_show_render(sima) || ED_space_image_show_paint(sima)))
		return 0;

	if (obedit && obedit->type == OB_MESH) {
		struct BMEditMesh *em = BMEdit_FromObject(obedit);
		int ret;

		ret = EDBM_mtexpoly_check(em);

		return ret;
	}

	return 0;
}

int ED_space_image_show_uvshadow(SpaceImage *sima, Object *obedit)
{
	if (ED_space_image_show_render(sima))
		return 0;

	if (ED_space_image_show_paint(sima))
		if (obedit && obedit->type == OB_MESH) {
			struct BMEditMesh *em = BMEdit_FromObject(obedit);
			int ret;

			ret = EDBM_mtexpoly_check(em);

			return ret;
		}

	return 0;
}

/* matches clip function */
int ED_space_image_check_show_maskedit(SpaceImage *sima)
{
	return (sima->mode == SI_MODE_MASK);
}

int ED_space_image_maskedit_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);

	if (sima && sima->image) {
		return ED_space_image_check_show_maskedit(sima);
	}

	return FALSE;
}

int ED_space_image_maskedit_mask_poll(bContext *C)
{
	if (ED_space_image_maskedit_poll(C)) {
		Image *ima = CTX_data_edit_image(C);

		if (ima) {
			SpaceImage *sima = CTX_wm_space_image(C);

			return sima->mask_info.mask != NULL;
		}
	}

	return FALSE;
}

/******************** TODO ********************/

/* XXX notifier? */

/* goes over all ImageUsers, and sets frame numbers if auto-refresh is set */

static void image_update_frame(struct Image *UNUSED(ima), struct ImageUser *iuser, void *customdata)
{
	int cfra = *(int *)customdata;

	BKE_image_user_check_frame_calc(iuser, cfra, 0);
}

void ED_image_update_frame(const Main *mainp, int cfra)
{
	BKE_image_walk_all_users(mainp, &cfra, image_update_frame);
}
