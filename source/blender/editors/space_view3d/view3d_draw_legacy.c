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

/** \file blender/editors/space_view3d/view3d_draw_legacy.c
 *  \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_group_types.h"
#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_jitter.h"
#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_threads.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_unit.h"
#include "BKE_movieclip.h"

#include "DEG_depsgraph.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_screen_types.h"
#include "ED_transform.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "GPU_draw.h"
#include "GPU_framebuffer.h"
#include "GPU_lamp.h"
#include "GPU_material.h"
#include "GPU_compositing.h"
#include "GPU_extensions.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"
#include "GPU_matrix.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "view3d_intern.h"  /* own include */

/* ********* custom clipping *********** */

static void view3d_draw_clipping(RegionView3D *rv3d)
{
	BoundBox *bb = rv3d->clipbb;

	if (bb) {
		const unsigned int clipping_index[6][4] = {
			{0, 1, 2, 3},
			{0, 4, 5, 1},
			{4, 7, 6, 5},
			{7, 3, 2, 6},
			{1, 5, 6, 2},
			{7, 4, 0, 3}
		};

		/* fill in zero alpha for rendering & re-projection [#31530] */
		unsigned char col[4];
		UI_GetThemeColor4ubv(TH_V3D_CLIPPING_BORDER, col);
		glColor4ubv(col);

		glEnable(GL_BLEND);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, bb->vec);
		glDrawElements(GL_QUADS, sizeof(clipping_index) / sizeof(unsigned int), GL_UNSIGNED_INT, clipping_index);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisable(GL_BLEND);
	}
}

void ED_view3d_clipping_set(RegionView3D *rv3d)
{
	double plane[4];
	const unsigned int tot = (rv3d->viewlock & RV3D_BOXCLIP) ? 4 : 6;

	for (unsigned a = 0; a < tot; a++) {
		copy_v4db_v4fl(plane, rv3d->clip[a]);
		glClipPlane(GL_CLIP_PLANE0 + a, plane);
		glEnable(GL_CLIP_PLANE0 + a);
	}
}

/* use these to temp disable/enable clipping when 'rv3d->rflag & RV3D_CLIPPING' is set */
void ED_view3d_clipping_disable(void)
{
	for (unsigned a = 0; a < 6; a++) {
		glDisable(GL_CLIP_PLANE0 + a);
	}
}
void ED_view3d_clipping_enable(void)
{
	for (unsigned a = 0; a < 6; a++) {
		glEnable(GL_CLIP_PLANE0 + a);
	}
}

static bool view3d_clipping_test(const float co[3], const float clip[6][4])
{
	if (plane_point_side_v3(clip[0], co) > 0.0f)
		if (plane_point_side_v3(clip[1], co) > 0.0f)
			if (plane_point_side_v3(clip[2], co) > 0.0f)
				if (plane_point_side_v3(clip[3], co) > 0.0f)
					return false;

	return true;
}

/* for 'local' ED_view3d_clipping_local must run first
 * then all comparisons can be done in localspace */
bool ED_view3d_clipping_test(const RegionView3D *rv3d, const float co[3], const bool is_local)
{
	return view3d_clipping_test(co, is_local ? rv3d->clip_local : rv3d->clip);
}

/* ********* end custom clipping *********** */

static void draw_view_icon(RegionView3D *rv3d, rcti *rect)
{
	BIFIconID icon;
	
	if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM))
		icon = ICON_AXIS_TOP;
	else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK))
		icon = ICON_AXIS_FRONT;
	else if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT))
		icon = ICON_AXIS_SIDE;
	else return;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	
	UI_icon_draw(5.0 + rect->xmin, 5.0 + rect->ymin, icon);
	
	glDisable(GL_BLEND);
}

/* *********************** backdraw for selection *************** */

static void backdrawview3d(const struct EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, wmWindow *win, ARegion *ar, View3D *v3d)
{
	RegionView3D *rv3d = ar->regiondata;
	struct Base *base = view_layer->basact;
	int multisample_enabled;

	BLI_assert(ar->regiontype == RGN_TYPE_WINDOW);

	if (base && (base->object->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT) ||
	             BKE_paint_select_face_test(base->object)))
	{
		/* do nothing */
	}
	/* texture paint mode sampling */
	else if (base && (base->object->mode & OB_MODE_TEXTURE_PAINT) &&
	         (v3d->drawtype > OB_WIRE))
	{
		/* do nothing */
	}
	else if ((base && (base->object->mode & OB_MODE_PARTICLE_EDIT)) &&
	         V3D_IS_ZBUF(v3d))
	{
		/* do nothing */
	}
	else if (scene->obedit &&
	         V3D_IS_ZBUF(v3d))
	{
		/* do nothing */
	}
	else {
		v3d->flag &= ~V3D_INVALID_BACKBUF;
		return;
	}

	if (!(v3d->flag & V3D_INVALID_BACKBUF))
		return;

#if 0
	if (test) {
		if (qtest()) {
			addafterqueue(ar->win, BACKBUFDRAW, 1);
			return;
		}
	}
#endif

	if (v3d->drawtype > OB_WIRE) v3d->zbuf = true;
	
	/* dithering and AA break color coding, so disable */
	glDisable(GL_DITHER);

	multisample_enabled = glIsEnabled(GL_MULTISAMPLE);
	if (multisample_enabled)
		glDisable(GL_MULTISAMPLE);

	if (win->multisamples != USER_MULTISAMPLE_NONE) {
		/* for multisample we use an offscreen FBO. multisample drawing can fail
		 * with color coded selection drawing, and reading back depths from such
		 * a buffer can also cause a few seconds freeze on OS X / NVidia. */
		int w = BLI_rcti_size_x(&ar->winrct);
		int h = BLI_rcti_size_y(&ar->winrct);
		char error[256];

		if (rv3d->gpuoffscreen) {
			if (GPU_offscreen_width(rv3d->gpuoffscreen)  != w ||
			    GPU_offscreen_height(rv3d->gpuoffscreen) != h)
			{
				GPU_offscreen_free(rv3d->gpuoffscreen);
				rv3d->gpuoffscreen = NULL;
			}
		}

		if (!rv3d->gpuoffscreen) {
			rv3d->gpuoffscreen = GPU_offscreen_create(w, h, 0, false, error);

			if (!rv3d->gpuoffscreen)
				fprintf(stderr, "Failed to create offscreen selection buffer for multisample: %s\n", error);
		}
	}

	if (rv3d->gpuoffscreen)
		GPU_offscreen_bind(rv3d->gpuoffscreen, true);
	else
		glScissor(ar->winrct.xmin, ar->winrct.ymin, BLI_rcti_size_x(&ar->winrct), BLI_rcti_size_y(&ar->winrct));

	glClearColor(0.0, 0.0, 0.0, 0.0);
	if (v3d->zbuf) {
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
	}
	
	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_set(rv3d);
	
	G.f |= G_BACKBUFSEL;
	
	if (base && ((base->flag & BASE_VISIBLED) != 0))
		draw_object_backbufsel(eval_ctx, scene, v3d, rv3d, base->object);
	
	if (rv3d->gpuoffscreen)
		GPU_offscreen_unbind(rv3d->gpuoffscreen, true);
	else
		ar->swap = 0; /* mark invalid backbuf for wm draw */

	v3d->flag &= ~V3D_INVALID_BACKBUF;

	G.f &= ~G_BACKBUFSEL;
	v3d->zbuf = false;
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_DITHER);
	if (multisample_enabled)
		glEnable(GL_MULTISAMPLE);

	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();
}

void view3d_opengl_read_pixels(ARegion *ar, int x, int y, int w, int h, int format, int type, void *data)
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d->gpuoffscreen) {
		GPU_offscreen_bind(rv3d->gpuoffscreen, true);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(x, y, w, h, format, type, data);
		GPU_offscreen_unbind(rv3d->gpuoffscreen, true);
	}
	else {
		glReadPixels(ar->winrct.xmin + x, ar->winrct.ymin + y, w, h, format, type, data);
	}
}

/* XXX depth reading exception, for code not using gpu offscreen */
static void view3d_opengl_read_Z_pixels(ARegion *ar, int x, int y, int w, int h, int format, int type, void *data)
{
	glReadPixels(ar->winrct.xmin + x, ar->winrct.ymin + y, w, h, format, type, data);
}

void ED_view3d_backbuf_validate(const struct EvaluationContext *eval_ctx, ViewContext *vc)
{
	if (vc->v3d->flag & V3D_INVALID_BACKBUF) {
		backdrawview3d(eval_ctx, vc->scene, vc->view_layer, vc->win, vc->ar, vc->v3d);
	}
}

/**
 * allow for small values [0.5 - 2.5],
 * and large values, FLT_MAX by clamping by the area size
 */
int ED_view3d_backbuf_sample_size_clamp(ARegion *ar, const float dist)
{
	return (int)min_ff(ceilf(dist), (float)max_ii(ar->winx, ar->winx));
}

/* samples a single pixel (copied from vpaint) */
unsigned int ED_view3d_backbuf_sample(
        const EvaluationContext *eval_ctx, ViewContext *vc, int x, int y)
{
	if (x >= vc->ar->winx || y >= vc->ar->winy) {
		return 0;
	}

	ED_view3d_backbuf_validate(eval_ctx, vc);

	unsigned int col;
	view3d_opengl_read_pixels(vc->ar, x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
	glReadBuffer(GL_BACK);

	if (ENDIAN_ORDER == B_ENDIAN) {
		BLI_endian_switch_uint32(&col);
	}

	return GPU_select_to_index(col);
}

/* reads full rect, converts indices */
ImBuf *ED_view3d_backbuf_read(
        const EvaluationContext *eval_ctx, ViewContext *vc, int xmin, int ymin, int xmax, int ymax)
{
	/* clip */
	const rcti clip = {
	    max_ii(xmin, 0), min_ii(xmax, vc->ar->winx - 1),
	    max_ii(ymin, 0), min_ii(ymax, vc->ar->winy - 1)};
	const int size_clip[2] = {
	    BLI_rcti_size_x(&clip) + 1,
	    BLI_rcti_size_y(&clip) + 1};

	if (UNLIKELY((clip.xmin > clip.xmax) ||
	             (clip.ymin > clip.ymax)))
	{
		return NULL;
	}

	ImBuf *ibuf_clip = IMB_allocImBuf(size_clip[0], size_clip[1], 32, IB_rect);

	ED_view3d_backbuf_validate(eval_ctx, vc);

	view3d_opengl_read_pixels(vc->ar, clip.xmin, clip.ymin, size_clip[0], size_clip[1], GL_RGBA, GL_UNSIGNED_BYTE, ibuf_clip->rect);

	glReadBuffer(GL_BACK);

	if (ENDIAN_ORDER == B_ENDIAN) {
		IMB_convert_rgba_to_abgr(ibuf_clip);
	}

	GPU_select_to_index_array(ibuf_clip->rect, size_clip[0] * size_clip[1]);
	
	if ((clip.xmin == xmin) &&
	    (clip.xmax == xmax) &&
	    (clip.ymin == ymin) &&
	    (clip.ymax == ymax))
	{
		return ibuf_clip;
	}
	else {
		/* put clipped result into a non-clipped buffer */
		const int size[2] = {
		    (xmax - xmin + 1),
		    (ymax - ymin + 1)};

		ImBuf *ibuf_full = IMB_allocImBuf(size[0], size[1], 32, IB_rect);

		IMB_rectcpy(
		        ibuf_full, ibuf_clip,
		        clip.xmin - xmin, clip.ymin - ymin,
		        0, 0,
		        size_clip[0], size_clip[1]);
		IMB_freeImBuf(ibuf_clip);
		return ibuf_full;
	}
}

/* smart function to sample a rect spiralling outside, nice for backbuf selection */
unsigned int ED_view3d_backbuf_sample_rect(
        const EvaluationContext *eval_ctx, ViewContext *vc, const int mval[2], int size,
        unsigned int min, unsigned int max, float *r_dist)
{
	int dirvec[4][2];

	const int amount = (size - 1) / 2;

	const int minx = mval[0] - (amount + 1);
	const int miny = mval[1] - (amount + 1);
	ImBuf *buf = ED_view3d_backbuf_read(eval_ctx, vc, minx, miny, minx + size - 1, miny + size - 1);
	if (!buf) return 0;

	unsigned index = 0;
	int rc = 0;
	
	dirvec[0][0] = 1; dirvec[0][1] = 0;
	dirvec[1][0] = 0; dirvec[1][1] = -size;
	dirvec[2][0] = -1; dirvec[2][1] = 0;
	dirvec[3][0] = 0; dirvec[3][1] = size;
	
	const unsigned *bufmin = buf->rect;
	const unsigned *tbuf = buf->rect;
	const unsigned *bufmax = buf->rect + size * size;
	tbuf += amount * size + amount;
	
	for (int nr = 1; nr <= size; nr++) {
		for (int a = 0; a < 2; a++) {
			for (int b = 0; b < nr; b++) {
				if (*tbuf && *tbuf >= min && *tbuf < max) {
					/* we got a hit */

					/* get x,y pixel coords from the offset
					 * (manhatten distance in keeping with other screen-based selection) */
					*r_dist = (float)(
					        abs(((int)(tbuf - buf->rect) % size) - (size / 2)) +
					        abs(((int)(tbuf - buf->rect) / size) - (size / 2)));

					/* indices start at 1 here */
					index = (*tbuf - min) + 1;
					goto exit;
				}
				
				tbuf += (dirvec[rc][0] + dirvec[rc][1]);
				
				if (tbuf < bufmin || tbuf >= bufmax) {
					goto exit;
				}
			}
			rc++;
			rc &= 3;
		}
	}

exit:
	IMB_freeImBuf(buf);
	return index;
}


/* ************************************************************* */

static void view3d_stereo_bgpic_setup(Scene *scene, View3D *v3d, Image *ima, ImageUser *iuser)
{
	if (BKE_image_is_stereo(ima)) {
		iuser->flag |= IMA_SHOW_STEREO;

		if ((scene->r.scemode & R_MULTIVIEW) == 0) {
			iuser->multiview_eye = STEREO_LEFT_ID;
		}
		else if (v3d->stereo3d_camera != STEREO_3D_ID) {
			/* show only left or right camera */
			iuser->multiview_eye = v3d->stereo3d_camera;
		}

		BKE_image_multiview_index(ima, iuser);
	}
	else {
		iuser->flag &= ~IMA_SHOW_STEREO;
	}
}

static void view3d_draw_bgpic(Scene *scene, ARegion *ar, View3D *v3d,
                              const bool do_foreground, const bool do_camera_frame)
{
	RegionView3D *rv3d = ar->regiondata;
	int fg_flag = do_foreground ? CAM_BGIMG_FLAG_FOREGROUND : 0;
	if (v3d->camera == NULL || v3d->camera->type != OB_CAMERA) {
		return;
	}
	Camera *cam = v3d->camera->data;

	for (CameraBGImage *bgpic = cam->bg_images.first; bgpic; bgpic = bgpic->next) {
		bgpic->iuser.scene = scene;  /* Needed for render results. */

		if ((bgpic->flag & CAM_BGIMG_FLAG_FOREGROUND) != fg_flag)
			continue;

		{
			float image_aspect[2];
			float x1, y1, x2, y2, centx, centy;

			void *lock;

			Image *ima = NULL;

			/* disable individual images */
			if ((bgpic->flag & CAM_BGIMG_FLAG_DISABLED))
				continue;

			ImBuf *ibuf = NULL;
			ImBuf *freeibuf = NULL;
			ImBuf *releaseibuf = NULL;
			if (bgpic->source == CAM_BGIMG_SOURCE_IMAGE) {
				ima = bgpic->ima;
				if (ima == NULL)
					continue;
				BKE_image_user_frame_calc(&bgpic->iuser, CFRA, 0);
				if (ima->source == IMA_SRC_SEQUENCE && !(bgpic->iuser.flag & IMA_USER_FRAME_IN_RANGE)) {
					ibuf = NULL; /* frame is out of range, dont show */
				}
				else {
					view3d_stereo_bgpic_setup(scene, v3d, ima, &bgpic->iuser);
					ibuf = BKE_image_acquire_ibuf(ima, &bgpic->iuser, &lock);
					releaseibuf = ibuf;
				}

				image_aspect[0] = ima->aspx;
				image_aspect[1] = ima->aspy;
			}
			else if (bgpic->source == CAM_BGIMG_SOURCE_MOVIE) {
				/* TODO: skip drawing when out of frame range (as image sequences do above) */
				MovieClip *clip = NULL;

				if (bgpic->flag & CAM_BGIMG_FLAG_CAMERACLIP) {
					if (scene->camera)
						clip = BKE_object_movieclip_get(scene, scene->camera, true);
				}
				else {
					clip = bgpic->clip;
				}

				if (clip == NULL)
					continue;

				BKE_movieclip_user_set_frame(&bgpic->cuser, CFRA);
				ibuf = BKE_movieclip_get_ibuf(clip, &bgpic->cuser);

				image_aspect[0] = clip->aspx;
				image_aspect[1] = clip->aspy;

				/* working with ibuf from image and clip has got different workflow now.
				 * ibuf acquired from clip is referenced by cache system and should
				 * be dereferenced after usage. */
				freeibuf = ibuf;
			}
			else {
				/* perhaps when loading future files... */
				BLI_assert(0);
				copy_v2_fl(image_aspect, 1.0f);
			}

			if (ibuf == NULL)
				continue;

			if ((ibuf->rect == NULL && ibuf->rect_float == NULL) || ibuf->channels != 4) { /* invalid image format */
				if (freeibuf)
					IMB_freeImBuf(freeibuf);
				if (releaseibuf)
					BKE_image_release_ibuf(ima, releaseibuf, lock);

				continue;
			}

			if (ibuf->rect == NULL)
				IMB_rect_from_float(ibuf);

			BLI_assert(rv3d->persp == RV3D_CAMOB);
			{
				if (do_camera_frame) {
					rctf vb;
					ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &vb, false);
					x1 = vb.xmin;
					y1 = vb.ymin;
					x2 = vb.xmax;
					y2 = vb.ymax;
				}
				else {
					x1 = ar->winrct.xmin;
					y1 = ar->winrct.ymin;
					x2 = ar->winrct.xmax;
					y2 = ar->winrct.ymax;
				}

				/* apply offset last - camera offset is different to offset in blender units */
				/* so this has some sane way of working - this matches camera's shift _exactly_ */
				{
					const float max_dim = max_ff(x2 - x1, y2 - y1);
					const float xof_scale = bgpic->offset[0] * max_dim;
					const float yof_scale = bgpic->offset[1] * max_dim;

					x1 += xof_scale;
					y1 += yof_scale;
					x2 += xof_scale;
					y2 += yof_scale;
				}

				centx = (x1 + x2) * 0.5f;
				centy = (y1 + y2) * 0.5f;

				/* aspect correction */
				if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_ASPECT) {
					/* apply aspect from clip */
					const float w_src = ibuf->x * image_aspect[0];
					const float h_src = ibuf->y * image_aspect[1];

					/* destination aspect is already applied from the camera frame */
					const float w_dst = x1 - x2;
					const float h_dst = y1 - y2;

					const float asp_src = w_src / h_src;
					const float asp_dst = w_dst / h_dst;

					if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
						if ((asp_src > asp_dst) == ((bgpic->flag & CAM_BGIMG_FLAG_CAMERA_CROP) != 0)) {
							/* fit X */
							const float div = asp_src / asp_dst;
							x1 = ((x1 - centx) * div) + centx;
							x2 = ((x2 - centx) * div) + centx;
						}
						else {
							/* fit Y */
							const float div = asp_dst / asp_src;
							y1 = ((y1 - centy) * div) + centy;
							y2 = ((y2 - centy) * div) + centy;
						}
					}
				}
			}

			/* complete clip? */
			rctf clip_rect;
			BLI_rctf_init(&clip_rect, x1, x2, y1, y2);
			if (bgpic->rotation) {
				BLI_rctf_rotate_expand(&clip_rect, &clip_rect, bgpic->rotation);
			}

			if (clip_rect.xmax < 0 || clip_rect.ymax < 0 || clip_rect.xmin > ar->winx || clip_rect.ymin > ar->winy) {
				if (freeibuf)
					IMB_freeImBuf(freeibuf);
				if (releaseibuf)
					BKE_image_release_ibuf(ima, releaseibuf, lock);

				continue;
			}

			float zoomx = (x2 - x1) / ibuf->x;
			float zoomy = (y2 - y1) / ibuf->y;

			/* for some reason; zoomlevels down refuses to use GL_ALPHA_SCALE */
			if (zoomx < 1.0f || zoomy < 1.0f) {
				float tzoom = min_ff(zoomx, zoomy);
				int mip = 0;

				if ((ibuf->userflags & IB_MIPMAP_INVALID) != 0) {
					IMB_remakemipmap(ibuf, 0);
					ibuf->userflags &= ~IB_MIPMAP_INVALID;
				}
				else if (ibuf->mipmap[0] == NULL)
					IMB_makemipmap(ibuf, 0);

				while (tzoom < 1.0f && mip < 8 && ibuf->mipmap[mip]) {
					tzoom *= 2.0f;
					zoomx *= 2.0f;
					zoomy *= 2.0f;
					mip++;
				}
				if (mip > 0)
					ibuf = ibuf->mipmap[mip - 1];
			}

			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);

			gpuPushProjectionMatrix();
			gpuPushMatrix();
			ED_region_pixelspace(ar);

			gpuTranslate2f(centx, centy);
			gpuScaleUniform(bgpic->scale);
			gpuRotate2D(RAD2DEGF(-bgpic->rotation));

			if (bgpic->flag & CAM_BGIMG_FLAG_FLIP_X) {
				zoomx *= -1.0f;
				x1 = x2;
			}
			if (bgpic->flag & CAM_BGIMG_FLAG_FLIP_Y) {
				zoomy *= -1.0f;
				y1 = y2;
			}

			float col[4] = {1.0f, 1.0f, 1.0f, bgpic->alpha};
			IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
			immDrawPixelsTex(&state, x1 - centx, y1 - centy, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR, ibuf->rect,
			                 zoomx, zoomy, col);

			gpuPopProjectionMatrix();
			gpuPopMatrix();

			glDisable(GL_BLEND);

			glDepthMask(GL_TRUE);
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

			if (freeibuf)
				IMB_freeImBuf(freeibuf);
			if (releaseibuf)
				BKE_image_release_ibuf(ima, releaseibuf, lock);
		}
	}
}

void view3d_draw_bgpic_test(Scene *scene, ARegion *ar, View3D *v3d,
                            const bool do_foreground, const bool do_camera_frame)
{
	RegionView3D *rv3d = ar->regiondata;

	if ((rv3d->persp == RV3D_CAMOB) && v3d->camera && (v3d->camera->type == OB_CAMERA)) {
		Camera *cam = v3d->camera->data;
		if ((cam->flag & CAM_SHOW_BG_IMAGE) == 0) {
			return;
		}
	}
	else {
		return;
	}

	/* disabled - mango request, since footage /w only render is quite useful
	 * and this option is easy to disable all background images at once */
#if 0
	if (v3d->flag2 & V3D_RENDER_OVERRIDE)
		return;
#endif

	if ((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) {
		if (rv3d->persp == RV3D_CAMOB) {
			view3d_draw_bgpic(scene, ar, v3d, do_foreground, do_camera_frame);
		}
	}
	else {
		view3d_draw_bgpic(scene, ar, v3d, do_foreground, do_camera_frame);
	}
}

/* ****************** View3d afterdraw *************** */

typedef struct View3DAfter {
	struct View3DAfter *next, *prev;
	struct Base *base;
	short dflag;
} View3DAfter;

/* temp storage of Objects that need to be drawn as last */
void ED_view3d_after_add(ListBase *lb, Base *base, const short dflag)
{
	View3DAfter *v3da = MEM_callocN(sizeof(View3DAfter), "View 3d after");
	BLI_assert((base->flag_legacy & OB_FROMDUPLI) == 0);
	BLI_addtail(lb, v3da);
	v3da->base = base;
	v3da->dflag = dflag;
}

/* disables write in zbuffer and draws it over */
static void view3d_draw_transp(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d)
{
	View3DAfter *v3da;
	
	glDepthMask(GL_FALSE);
	v3d->transp = true;
	
	while ((v3da = BLI_pophead(&v3d->afterdraw_transp))) {
		draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, v3da->dflag);
		MEM_freeN(v3da);
	}
	v3d->transp = false;
	
	glDepthMask(GL_TRUE);
	
}

/* clears zbuffer and draws it over */
static void view3d_draw_xray(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d, bool *clear)
{
	if (*clear && v3d->zbuf) {
		glClear(GL_DEPTH_BUFFER_BIT);
		*clear = false;
	}

	v3d->xray = true;
	View3DAfter *v3da;
	while ((v3da = BLI_pophead(&v3d->afterdraw_xray))) {
		draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, v3da->dflag);
		MEM_freeN(v3da);
	}
	v3d->xray = false;
}


/* clears zbuffer and draws it over */
static void view3d_draw_xraytransp(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d, const bool clear)
{
	if (clear && v3d->zbuf)
		glClear(GL_DEPTH_BUFFER_BIT);

	v3d->xray = true;
	v3d->transp = true;
	
	glDepthMask(GL_FALSE);

	View3DAfter *v3da;
	while ((v3da = BLI_pophead(&v3d->afterdraw_xraytransp))) {
		draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, v3da->dflag);
		MEM_freeN(v3da);
	}

	v3d->transp = false;
	v3d->xray = false;

	glDepthMask(GL_TRUE);
}

/* clears zbuffer and draws it over,
 * note that in the select version we don't care about transparent flag as with regular drawing */
static void view3d_draw_xray_select(
        const struct EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d, bool *clear)
{
	/* Not ideal, but we need to read from the previous depths before clearing
	 * otherwise we could have a function to load the depths after drawing.
	 *
	 * Clearing the depth buffer isn't all that common between drawing objects so accept this for now.
	 */
	if (U.gpu_select_pick_deph) {
		GPU_select_load_id(-1);
	}

	View3DAfter *v3da;
	if (*clear && v3d->zbuf) {
		glClear(GL_DEPTH_BUFFER_BIT);
		*clear = false;
	}

	v3d->xray = true;
	while ((v3da = BLI_pophead(&v3d->afterdraw_xray))) {
		if (GPU_select_load_id(v3da->base->object->select_color)) {
			draw_object_select(eval_ctx, scene, view_layer, ar, v3d, v3da->base, v3da->dflag);
		}
		MEM_freeN(v3da);
	}
	v3d->xray = false;
}

/* *********************** */

/*
 * In most cases call draw_dupli_objects,
 * draw_dupli_objects_color was added because when drawing set dupli's
 * we need to force the color
 */

#if 0
int dupli_ob_sort(void *arg1, void *arg2)
{
	void *p1 = ((DupliObject *)arg1)->ob;
	void *p2 = ((DupliObject *)arg2)->ob;
	int val = 0;
	if (p1 < p2) val = -1;
	else if (p1 > p2) val = 1;
	return val;
}
#endif


static DupliObject *dupli_step(DupliObject *dob)
{
	while (dob && dob->no_draw)
		dob = dob->next;
	return dob;
}

static void draw_dupli_objects_color(
        const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d, Base *base,
        const short dflag, const int color)
{
	RegionView3D *rv3d = ar->regiondata;
	ListBase *lb;
	LodLevel *savedlod;
	Base tbase = {NULL};
	BoundBox bb, *bb_tmp; /* use a copy because draw_object, calls clear_mesh_caches */
	unsigned char color_rgb[3];
	const short dflag_dupli = dflag | DRAW_CONSTCOLOR;
	short transflag;
	char dt;
	short dtx;
	DupliApplyData *apply_data;

	if ((base->flag & BASE_VISIBLED) == 0) return;
	if ((base->object->restrictflag & OB_RESTRICT_RENDER) && (v3d->flag2 & V3D_RENDER_OVERRIDE)) return;

	if (dflag & DRAW_CONSTCOLOR) {
		BLI_assert(color == TH_UNDEFINED);
	}
	else {
		UI_GetThemeColorBlend3ubv(color, TH_BACK, 0.5f, color_rgb);
	}

	tbase.flag_legacy = OB_FROMDUPLI | base->flag_legacy;
	tbase.flag = base->flag;
	lb = object_duplilist(eval_ctx, scene, base->object);
	// BLI_listbase_sort(lb, dupli_ob_sort); /* might be nice to have if we have a dupli list with mixed objects. */

	apply_data = duplilist_apply(eval_ctx, base->object, scene, lb);

	DupliObject *dob_next = NULL;
	DupliObject *dob = dupli_step(lb->first);
	if (dob) dob_next = dupli_step(dob->next);

	for (; dob; dob = dob_next, dob_next = dob_next ? dupli_step(dob_next->next) : NULL) {
		bool testbb = false;

		tbase.object = dob->ob;

		/* Make sure lod is updated from dupli's position */
		savedlod = dob->ob->currentlod;

#ifdef WITH_GAMEENGINE
		if (rv3d->rflag & RV3D_IS_GAME_ENGINE) {
			BKE_object_lod_update(dob->ob, rv3d->viewinv[3]);
		}
#endif

		/* extra service: draw the duplicator in drawtype of parent, minimum taken
		 * to allow e.g. boundbox box objects in groups for LOD */
		dt = tbase.object->dt;
		tbase.object->dt = MIN2(tbase.object->dt, base->object->dt);

		/* inherit draw extra, but not if a boundbox under the assumption that this
		 * is intended to speed up drawing, and drawing extra (especially wire) can
		 * slow it down too much */
		dtx = tbase.object->dtx;
		if (tbase.object->dt != OB_BOUNDBOX)
			tbase.object->dtx = base->object->dtx;

		/* negative scale flag has to propagate */
		transflag = tbase.object->transflag;

		if (is_negative_m4(dob->mat))
			tbase.object->transflag |= OB_NEG_SCALE;
		else
			tbase.object->transflag &= ~OB_NEG_SCALE;
		
		/* should move outside the loop but possible color is set in draw_object still */
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			glColor3ubv(color_rgb);
		}
		
		if ((bb_tmp = BKE_object_boundbox_get(dob->ob))) {
			bb = *bb_tmp; /* must make a copy  */
			testbb = true;
		}

		if (!testbb || ED_view3d_boundbox_clip_ex(rv3d, &bb, dob->mat)) {
			copy_m4_m4(dob->ob->obmat, dob->mat);
			GPU_begin_dupli_object(dob);
			draw_object(eval_ctx, scene, view_layer, ar, v3d, &tbase, dflag_dupli);
			GPU_end_dupli_object();
		}
		
		tbase.object->dt = dt;
		tbase.object->dtx = dtx;
		tbase.object->transflag = transflag;
		tbase.object->currentlod = savedlod;
	}

	if (apply_data) {
		duplilist_restore(lb, apply_data);
		duplilist_free_apply_data(apply_data);
	}

	free_object_duplilist(lb);
}

void draw_dupli_objects(const EvaluationContext *eval_ctx, Scene *scene, ViewLayer *view_layer, ARegion *ar, View3D *v3d, Base *base)
{
	/* define the color here so draw_dupli_objects_color can be called
	 * from the set loop */
	
	int color = (base->flag & BASE_SELECTED) ? TH_SELECT : TH_WIRE;
	/* debug */
	if (base->object->dup_group && base->object->dup_group->id.us < 1)
		color = TH_REDALERT;
	
	draw_dupli_objects_color(eval_ctx, scene, view_layer, ar, v3d, base, 0, color);
}

/* XXX warning, not using gpu offscreen here */
void view3d_update_depths_rect(ARegion *ar, ViewDepths *d, rcti *rect)
{
	/* clamp rect by region */
	rcti r = {
		.xmin = 0,
		.xmax = ar->winx - 1,
		.ymin = 0,
		.ymax = ar->winy - 1
	};

	/* Constrain rect to depth bounds */
	BLI_rcti_isect(&r, rect, rect);

	/* assign values to compare with the ViewDepths */
	int x = rect->xmin;
	int y = rect->ymin;

	int w = BLI_rcti_size_x(rect);
	int h = BLI_rcti_size_y(rect);

	if (w <= 0 || h <= 0) {
		if (d->depths)
			MEM_freeN(d->depths);
		d->depths = NULL;

		d->damaged = false;
	}
	else if (d->w != w ||
	         d->h != h ||
	         d->x != x ||
	         d->y != y ||
	         d->depths == NULL
	         )
	{
		d->x = x;
		d->y = y;
		d->w = w;
		d->h = h;

		if (d->depths)
			MEM_freeN(d->depths);

		d->depths = MEM_mallocN(sizeof(float) * d->w * d->h, "View depths Subset");
		
		d->damaged = true;
	}

	if (d->damaged) {
		/* XXX using special function here, it doesn't use the gpu offscreen system */
		view3d_opengl_read_Z_pixels(ar, d->x, d->y, d->w, d->h, GL_DEPTH_COMPONENT, GL_FLOAT, d->depths);
		glGetDoublev(GL_DEPTH_RANGE, d->depth_range);
		d->damaged = false;
	}
}

/* note, with nouveau drivers the glReadPixels() is very slow. [#24339] */
void ED_view3d_depth_update(ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	
	/* Create storage for, and, if necessary, copy depth buffer */
	if (!rv3d->depths) rv3d->depths = MEM_callocN(sizeof(ViewDepths), "ViewDepths");
	if (rv3d->depths) {
		ViewDepths *d = rv3d->depths;
		if (d->w != ar->winx ||
		    d->h != ar->winy ||
		    !d->depths)
		{
			d->w = ar->winx;
			d->h = ar->winy;
			if (d->depths)
				MEM_freeN(d->depths);
			d->depths = MEM_mallocN(sizeof(float) * d->w * d->h, "View depths");
			d->damaged = true;
		}
		
		if (d->damaged) {
			view3d_opengl_read_pixels(ar, 0, 0, d->w, d->h, GL_DEPTH_COMPONENT, GL_FLOAT, d->depths);
			glGetDoublev(GL_DEPTH_RANGE, d->depth_range);
			
			d->damaged = false;
		}
	}
}

/* utility function to find the closest Z value, use for autodepth */
float view3d_depth_near(ViewDepths *d)
{
	/* convert to float for comparisons */
	const float near = (float)d->depth_range[0];
	const float far_real = (float)d->depth_range[1];
	float far = far_real;

	const float *depths = d->depths;
	float depth = FLT_MAX;
	int i = (int)d->w * (int)d->h; /* cast to avoid short overflow */

	/* far is both the starting 'far' value
	 * and the closest value found. */
	while (i--) {
		depth = *depths++;
		if ((depth < far) && (depth > near)) {
			far = depth;
		}
	}

	return far == far_real ? FLT_MAX : far;
}

void ED_view3d_draw_depth_gpencil(
        const EvaluationContext *eval_ctx, Scene *scene, ARegion *ar, View3D *v3d)
{
	bool zbuf = v3d->zbuf;

	/* Setup view matrix. */
	ED_view3d_draw_setup_view(NULL, eval_ctx, scene, ar, v3d, NULL, NULL, NULL);

	glClear(GL_DEPTH_BUFFER_BIT);

	v3d->zbuf = true;
	glEnable(GL_DEPTH_TEST);

	if (v3d->flag2 & V3D_SHOW_GPENCIL) {
		ED_gpencil_draw_view3d(NULL, scene, eval_ctx->view_layer, v3d, ar, true);
	}

	v3d->zbuf = zbuf;
	if (!zbuf) glDisable(GL_DEPTH_TEST);
}

void ED_view3d_draw_depth_loop(const EvaluationContext *eval_ctx, Scene *scene, ARegion *ar, View3D *v3d)
{
	Base *base;
	ViewLayer *view_layer = eval_ctx->view_layer;
	/* no need for color when drawing depth buffer */
	const short dflag_depth = DRAW_CONSTCOLOR;

	/* draw set first */
	if (scene->set) {
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			if ((base->flag & BASE_VISIBLED) != 0) {
				draw_object(eval_ctx, scene, view_layer, ar, v3d, base, 0);
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(eval_ctx, scene, view_layer, ar, v3d, base, dflag_depth, TH_UNDEFINED);
				}
			}
		}
	}
	
	for (base = view_layer->object_bases.first; base; base = base->next) {
		if ((base->flag & BASE_VISIBLED) != 0) {
			/* dupli drawing */
			if (base->object->transflag & OB_DUPLI) {
				draw_dupli_objects_color(eval_ctx, scene, view_layer, ar, v3d, base, dflag_depth, TH_UNDEFINED);
			}
			draw_object(eval_ctx, scene, view_layer, ar, v3d, base, dflag_depth);
		}
	}
	
	/* this isn't that nice, draw xray objects as if they are normal */
	if (v3d->afterdraw_transp.first ||
	    v3d->afterdraw_xray.first ||
	    v3d->afterdraw_xraytransp.first)
	{
		View3DAfter *v3da;
		int mask_orig;

		v3d->xray = true;
		
		/* transp materials can change the depth mask, see #21388 */
		glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);


		if (v3d->afterdraw_xray.first || v3d->afterdraw_xraytransp.first) {
			glDepthFunc(GL_ALWAYS); /* always write into the depth bufer, overwriting front z values */
			for (v3da = v3d->afterdraw_xray.first; v3da; v3da = v3da->next) {
				draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, dflag_depth);
			}
			glDepthFunc(GL_LEQUAL); /* Now write the depth buffer normally */
		}

		/* draw 3 passes, transp/xray/xraytransp */
		v3d->xray = false;
		v3d->transp = true;
		while ((v3da = BLI_pophead(&v3d->afterdraw_transp))) {
			draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, dflag_depth);
			MEM_freeN(v3da);
		}

		v3d->xray = true;
		v3d->transp = false;
		while ((v3da = BLI_pophead(&v3d->afterdraw_xray))) {
			draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, dflag_depth);
			MEM_freeN(v3da);
		}

		v3d->xray = true;
		v3d->transp = true;
		while ((v3da = BLI_pophead(&v3d->afterdraw_xraytransp))) {
			draw_object(eval_ctx, scene, view_layer, ar, v3d, v3da->base, dflag_depth);
			MEM_freeN(v3da);
		}

		
		v3d->xray = false;
		v3d->transp = false;

		glDepthMask(mask_orig);
	}
}

void ED_view3d_draw_select_loop(
        const struct EvaluationContext *eval_ctx, ViewContext *vc, Scene *scene, ViewLayer *view_layer,
        View3D *v3d, ARegion *ar, bool use_obedit_skip, bool use_nearest)
{
	struct bThemeState theme_state;

	short code = 1;
	const short dflag = DRAW_PICKING | DRAW_CONSTCOLOR;

	/* Tools may request depth outside of regular drawing code. */
	UI_Theme_Store(&theme_state);
	UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

	if (vc->obedit && vc->obedit->type == OB_MBALL) {
		draw_object(eval_ctx, scene, view_layer, ar, v3d, BASACT(view_layer), dflag);
	}
	else if ((vc->obedit && vc->obedit->type == OB_ARMATURE)) {
		/* if not drawing sketch, draw bones */
		if (!BDR_drawSketchNames(vc)) {
			draw_object(eval_ctx, scene, view_layer, ar, v3d, BASACT(view_layer), dflag);
		}
	}
	else {
		Base *base;

		for (base = view_layer->object_bases.first; base; base = base->next) {
			if ((base->flag & BASE_VISIBLED) != 0) {
				if (((base->flag & BASE_SELECTABLED) == 0) ||
				    (use_obedit_skip && (scene->obedit->data == base->object->data)))
				{
					base->object->select_color = 0;
				}
				else {
					base->object->select_color = code;

					if (use_nearest && (base->object->dtx & OB_DRAWXRAY)) {
						ED_view3d_after_add(&v3d->afterdraw_xray, base, dflag);
					}
					else {
						if (GPU_select_load_id(code)) {
							draw_object(eval_ctx, scene, view_layer, ar, v3d, base, dflag);
						}
					}
					code++;
				}
			}
		}

		if (use_nearest) {
			bool xrayclear = true;
			if (v3d->afterdraw_xray.first) {
				view3d_draw_xray_select(eval_ctx, scene, view_layer, ar, v3d, &xrayclear);
			}
		}
	}

	UI_Theme_Restore(&theme_state);
}

typedef struct View3DShadow {
	struct View3DShadow *next, *prev;
	GPULamp *lamp;
} View3DShadow;

static void gpu_render_lamp_update(Scene *scene, View3D *v3d,
                                   Object *ob, Object *par,
                                   float obmat[4][4], unsigned int lay,
                                   ListBase *shadows)
{
	GPULamp *lamp = GPU_lamp_from_blender(scene, ob, par);
	
	if (lamp) {
		Lamp *la = (Lamp *)ob->data;

		GPU_lamp_update(lamp, lay, (ob->restrictflag & OB_RESTRICT_RENDER), obmat);
		GPU_lamp_update_colors(lamp, la->r, la->g, la->b, la->energy);
		
		unsigned int layers = lay & v3d->lay;

		if (layers &&
		    GPU_lamp_has_shadow_buffer(lamp) &&
		    /* keep last, may do string lookup */
		    GPU_lamp_visible(lamp, NULL))
		{
			View3DShadow *shadow = MEM_callocN(sizeof(View3DShadow), "View3DShadow");
			shadow->lamp = lamp;
			BLI_addtail(shadows, shadow);
		}
	}
}

static void gpu_update_lamps_shadows_world(const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d)
{
	ListBase shadows;
	Scene *sce_iter;
	Base *base;
	World *world = scene->world;
	
	BLI_listbase_clear(&shadows);
	
	/* update lamp transform and gather shadow lamps */
	for (SETLOOPER(scene, sce_iter, base)) {
		Object *ob = base->object;
		
		if (ob->type == OB_LAMP)
			gpu_render_lamp_update(scene, v3d, ob, NULL, ob->obmat, ob->lay, &shadows);
		
		if (ob->transflag & OB_DUPLI) {
			DupliObject *dob;
			ListBase *lb = object_duplilist(G.main->eval_ctx, scene, ob);
			
			for (dob = lb->first; dob; dob = dob->next)
				if (dob->ob->type == OB_LAMP)
					gpu_render_lamp_update(scene, v3d, dob->ob, ob, dob->mat, ob->lay, &shadows);
			
			free_object_duplilist(lb);
		}
	}
	
	/* render shadows after updating all lamps, nested object_duplilist
	 * don't work correct since it's replacing object matrices */
	for (View3DShadow *shadow = shadows.first; shadow; shadow = shadow->next) {
		/* this needs to be done better .. */
		float viewmat[4][4], winmat[4][4];
		ARegion ar = {NULL};
		RegionView3D rv3d = {{{0}}};

		int drawtype = v3d->drawtype;
		int lay = v3d->lay;
		int flag2 = v3d->flag2;

		v3d->drawtype = OB_SOLID;
		v3d->lay &= GPU_lamp_shadow_layer(shadow->lamp);
		v3d->flag2 &= ~(V3D_SOLID_TEX | V3D_SHOW_SOLID_MATCAP);
		v3d->flag2 |= V3D_RENDER_OVERRIDE | V3D_RENDER_SHADOW;
		
		int winsize;
		GPU_lamp_shadow_buffer_bind(shadow->lamp, viewmat, &winsize, winmat);

		ar.regiondata = &rv3d;
		ar.regiontype = RGN_TYPE_WINDOW;
		rv3d.persp = RV3D_CAMOB;
		copy_m4_m4(rv3d.winmat, winmat);
		copy_m4_m4(rv3d.viewmat, viewmat);
		invert_m4_m4(rv3d.viewinv, rv3d.viewmat);
		mul_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
		invert_m4_m4(rv3d.persinv, rv3d.viewinv);

		/* no need to call ED_view3d_draw_offscreen_init since shadow buffers were already updated */
		ED_view3d_draw_offscreen(
		            eval_ctx, scene, eval_ctx->view_layer, v3d, &ar, winsize, winsize, viewmat, winmat,
		            false, false, true,
		            NULL, NULL, NULL, NULL, NULL);
		GPU_lamp_shadow_buffer_unbind(shadow->lamp);
		
		v3d->drawtype = drawtype;
		v3d->lay = lay;
		v3d->flag2 = flag2;
	}

	BLI_freelistN(&shadows);

	/* update world values */
	if (world) {
		GPU_mist_update_enable(world->mode & WO_MIST);
		GPU_mist_update_values(world->mistype, world->miststa, world->mistdist, world->misi, &world->horr);
		GPU_horizon_update_color(&world->horr);
		GPU_ambient_update_color(&world->ambr);
		GPU_zenith_update_color(&world->zenr);
	}
}

/* *********************** customdata **************** */

CustomDataMask ED_view3d_datamask(const Scene *scene, const View3D *v3d)
{
	CustomDataMask mask = 0;
	const int drawtype = view3d_effective_drawtype(v3d);

	if (ELEM(drawtype, OB_TEXTURE, OB_MATERIAL) ||
	    ((drawtype == OB_SOLID) && (v3d->flag2 & V3D_SOLID_TEX)))
	{
		mask |= CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL;

		if (BKE_scene_use_new_shading_nodes(scene)) {
			if (drawtype == OB_MATERIAL)
				mask |= CD_MASK_ORCO;
		}
		else {
			if ((scene->gm.matmode == GAME_MAT_GLSL && drawtype == OB_TEXTURE) || 
			    (drawtype == OB_MATERIAL))
			{
				mask |= CD_MASK_ORCO;
			}
		}
	}

	return mask;
}

/* goes over all modes and view3d settings */
CustomDataMask ED_view3d_screen_datamask(const Scene *scene, const bScreen *screen)
{
	CustomDataMask mask = CD_MASK_BAREMESH;
	
	/* check if we need tfaces & mcols due to view mode */
	for (const ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_VIEW3D) {
			mask |= ED_view3d_datamask(scene, sa->spacedata.first);
		}
	}

	return mask;
}

/**
 * Shared by #ED_view3d_draw_offscreen and #view3d_main_region_draw_objects
 *
 * \note \a C and \a grid_unit will be NULL when \a draw_offscreen is set.
 * \note Drawing lamps and opengl render uses this, so dont do grease pencil or view widgets here.
 */
static void view3d_draw_objects(
        const bContext *C,
        const EvaluationContext *eval_ctx,
        Scene *scene, View3D *v3d, ARegion *ar,
        const char **grid_unit,
        const bool do_bgpic, const bool draw_offscreen, GPUFX *fx)
{
	ViewLayer *view_layer = C ? CTX_data_view_layer(C) : BKE_view_layer_from_scene_get(scene);
	RegionView3D *rv3d = ar->regiondata;
	Base *base;
	const bool do_camera_frame = !draw_offscreen;
	const bool draw_grids = !draw_offscreen && (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0;
	const bool draw_floor = (rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO);
	/* only draw grids after in solid modes, else it hovers over mesh wires */
	const bool draw_grids_after = draw_grids && draw_floor && (v3d->drawtype > OB_WIRE) && fx;
	bool do_composite_xray = false;
	bool xrayclear = true;

	if (!draw_offscreen) {
		ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);
	}

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_draw_clipping(rv3d);

	/* set zbuffer after we draw clipping region */
	v3d->zbuf = VP_legacy_use_depth(scene, v3d);

	if (v3d->zbuf) {
		glEnable(GL_DEPTH_TEST);
	}

	/* ortho grid goes first, does not write to depth buffer and doesn't need depth test so it will override
	 * objects if done last */
	if (draw_grids) {
		/* needs to be done always, gridview is adjusted in drawgrid() now, but only for ortho views. */
		rv3d->gridview = ED_view3d_grid_scale(scene, v3d, grid_unit);

		if (!draw_floor) {
			ED_region_pixelspace(ar);
			*grid_unit = NULL;  /* drawgrid need this to detect/affect smallest valid unit... */
			VP_legacy_drawgrid(&scene->unit, ar, v3d, grid_unit);
			gpuLoadProjectionMatrix(rv3d->winmat);
			gpuLoadMatrix(rv3d->viewmat);
		}
		else if (!draw_grids_after) {
			VP_legacy_drawfloor(scene, v3d, grid_unit, true);
		}
	}

	/* important to do before clipping */
	if (do_bgpic) {
		view3d_draw_bgpic_test(scene, ar, v3d, false, do_camera_frame);
	}

	if (rv3d->rflag & RV3D_CLIPPING) {
		ED_view3d_clipping_set(rv3d);
	}

	/* draw set first */
	if (scene->set) {
		const short dflag = DRAW_CONSTCOLOR | DRAW_SCENESET;
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			if ((base->flag & BASE_VISIBLED) != 0) {
				UI_ThemeColorBlend(TH_WIRE, TH_BACK, 0.6f);
				draw_object(eval_ctx, scene, view_layer, ar, v3d, base, dflag);

				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(eval_ctx, scene, view_layer, ar, v3d, base, dflag, TH_UNDEFINED);
				}
			}
		}

		/* Transp and X-ray afterdraw stuff for sets is done later */
	}

	if (draw_offscreen) {
		for (base = view_layer->object_bases.first; base; base = base->next) {
			if ((base->flag & BASE_VISIBLED) != 0) {
				/* dupli drawing */
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects(eval_ctx, scene, view_layer, ar, v3d, base);
				}

				draw_object(eval_ctx, scene, view_layer, ar, v3d, base, 0);
			}
		}
	}
	else {
		unsigned int lay_used = 0;

		/* then draw not selected and the duplis, but skip editmode object */
		for (base = view_layer->object_bases.first; base; base = base->next) {
			lay_used |= base->lay;

			if ((base->flag & BASE_VISIBLED) != 0) {

				/* dupli drawing */
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects(eval_ctx, scene, view_layer, ar, v3d, base);
				}
				if ((base->flag & BASE_SELECTED) == 0) {
					if (base->object != scene->obedit)
						draw_object(eval_ctx, scene, view_layer, ar, v3d, base, 0);
				}
			}
		}

		/* mask out localview */
		v3d->lay_used = lay_used & ((1 << 20) - 1);

		/* draw selected and editmode */
		for (base = view_layer->object_bases.first; base; base = base->next) {
			if ((base->flag & BASE_VISIBLED) != 0) {
				if (base->object == scene->obedit || (base->flag & BASE_SELECTED)) {
					draw_object(eval_ctx, scene, view_layer, ar, v3d, base, 0);
				}
			}
		}
	}

	/* perspective floor goes last to use scene depth and avoid writing to depth buffer */
	if (draw_grids_after) {
		VP_legacy_drawfloor(scene, v3d, grid_unit, false);
	}

	/* must be before xray draw which clears the depth buffer */
	if (v3d->flag2 & V3D_SHOW_GPENCIL) {
		wmWindowManager *wm = (C != NULL) ? CTX_wm_manager(C) : NULL;
		
		/* must be before xray draw which clears the depth buffer */
		if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
		ED_gpencil_draw_view3d(wm, scene, view_layer, v3d, ar, true);
		if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	}

	/* transp and X-ray afterdraw stuff */
	if (v3d->afterdraw_transp.first)     view3d_draw_transp(eval_ctx, scene, view_layer, ar, v3d);

	/* always do that here to cleanup depth buffers if none needed */
	if (fx) {
		do_composite_xray = v3d->zbuf && (v3d->afterdraw_xray.first || v3d->afterdraw_xraytransp.first);
		GPU_fx_compositor_setup_XRay_pass(fx, do_composite_xray);
	}

	if (v3d->afterdraw_xray.first)       view3d_draw_xray(eval_ctx, scene, view_layer, ar, v3d, &xrayclear);
	if (v3d->afterdraw_xraytransp.first) view3d_draw_xraytransp(eval_ctx, scene, view_layer, ar, v3d, xrayclear);

	if (fx && do_composite_xray) {
		GPU_fx_compositor_XRay_resolve(fx);
	}

	if (!draw_offscreen) {
		ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
	}

	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();

	/* important to do after clipping */
	if (do_bgpic) {
		view3d_draw_bgpic_test(scene, ar, v3d, true, do_camera_frame);
	}

	/* cleanup */
	if (v3d->zbuf) {
		v3d->zbuf = false;
		glDisable(GL_DEPTH_TEST);
	}

	if ((v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		GPU_free_images_old();
	}
}

/**
 * Store values from #RegionView3D, set when drawing.
 * This is needed when we draw with to a viewport using a different matrix (offscreen drawing for example).
 *
 * Values set by #ED_view3d_update_viewmat should be handled here.
 */
struct RV3DMatrixStore {
	float winmat[4][4];
	float viewmat[4][4];
	float viewinv[4][4];
	float persmat[4][4];
	float persinv[4][4];
	float viewcamtexcofac[4];
	float pixsize;
};

struct RV3DMatrixStore *ED_view3d_mats_rv3d_backup(struct RegionView3D *rv3d)
{
	struct RV3DMatrixStore *rv3dmat = MEM_mallocN(sizeof(*rv3dmat), __func__);
	copy_m4_m4(rv3dmat->winmat, rv3d->winmat);
	copy_m4_m4(rv3dmat->viewmat, rv3d->viewmat);
	copy_m4_m4(rv3dmat->persmat, rv3d->persmat);
	copy_m4_m4(rv3dmat->persinv, rv3d->persinv);
	copy_m4_m4(rv3dmat->viewinv, rv3d->viewinv);
	copy_v4_v4(rv3dmat->viewcamtexcofac, rv3d->viewcamtexcofac);
	rv3dmat->pixsize = rv3d->pixsize;
	return (void *)rv3dmat;
}

void ED_view3d_mats_rv3d_restore(struct RegionView3D *rv3d, struct RV3DMatrixStore *rv3dmat_pt)
{
	struct RV3DMatrixStore *rv3dmat = rv3dmat_pt;
	copy_m4_m4(rv3d->winmat, rv3dmat->winmat);
	copy_m4_m4(rv3d->viewmat, rv3dmat->viewmat);
	copy_m4_m4(rv3d->persmat, rv3dmat->persmat);
	copy_m4_m4(rv3d->persinv, rv3dmat->persinv);
	copy_m4_m4(rv3d->viewinv, rv3dmat->viewinv);
	copy_v4_v4(rv3d->viewcamtexcofac, rv3dmat->viewcamtexcofac);
	rv3d->pixsize = rv3dmat->pixsize;
}

/**
 * \note The info that this uses is updated in #ED_refresh_viewport_fps,
 * which currently gets called during #SCREEN_OT_animation_step.
 */
void ED_scene_draw_fps(Scene *scene, const rcti *rect)
{
	ScreenFrameRateInfo *fpsi = scene->fps_info;
	char printable[16];
	
	if (!fpsi || !fpsi->lredrawtime || !fpsi->redrawtime)
		return;
	
	printable[0] = '\0';
	
#if 0
	/* this is too simple, better do an average */
	fps = (float)(1.0 / (fpsi->lredrawtime - fpsi->redrawtime))
#else
	fpsi->redrawtimes_fps[fpsi->redrawtime_index] = (float)(1.0 / (fpsi->lredrawtime - fpsi->redrawtime));
	
	float fps = 0.0f;
	int tot = 0;
	for (int i = 0; i < REDRAW_FRAME_AVERAGE; i++) {
		if (fpsi->redrawtimes_fps[i]) {
			fps += fpsi->redrawtimes_fps[i];
			tot++;
		}
	}
	if (tot) {
		fpsi->redrawtime_index = (fpsi->redrawtime_index + 1) % REDRAW_FRAME_AVERAGE;
		
		//fpsi->redrawtime_index++;
		//if (fpsi->redrawtime >= REDRAW_FRAME_AVERAGE)
		//	fpsi->redrawtime = 0;
		
		fps = fps / tot;
	}
#endif

	const int font_id = BLF_default();

	/* is this more than half a frame behind? */
	if (fps + 0.5f < (float)(FPS)) {
		UI_FontThemeColor(font_id, TH_REDALERT);
		BLI_snprintf(printable, sizeof(printable), IFACE_("fps: %.2f"), fps);
	}
	else {
		UI_FontThemeColor(font_id, TH_TEXT_HI);
		BLI_snprintf(printable, sizeof(printable), IFACE_("fps: %i"), (int)(fps + 0.5f));
	}

#ifdef WITH_INTERNATIONAL
	BLF_draw_default(rect->xmin + U.widget_unit,  rect->ymax - U.widget_unit, 0.0f, printable, sizeof(printable));
#else
	BLF_draw_default_ascii(rect->xmin + U.widget_unit,  rect->ymax - U.widget_unit, 0.0f, printable, sizeof(printable));
#endif
}

static bool view3d_main_region_do_render_draw(const Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->view_render.engine_id);
	return (type && type->view_update && type->render_to_view);
}

bool ED_view3d_calc_render_border(const Scene *scene, View3D *v3d, ARegion *ar, rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	bool use_border;

	/* test if there is a 3d view rendering */
	if (v3d->drawtype != OB_RENDER || !view3d_main_region_do_render_draw(scene))
		return false;

	/* test if there is a border render */
	if (rv3d->persp == RV3D_CAMOB)
		use_border = (scene->r.mode & R_BORDER) != 0;
	else
		use_border = (v3d->flag2 & V3D_RENDER_BORDER) != 0;
	
	if (!use_border)
		return false;

	/* compute border */
	if (rv3d->persp == RV3D_CAMOB) {
		rctf viewborder;
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &viewborder, false);

		rect->xmin = viewborder.xmin + scene->r.border.xmin * BLI_rctf_size_x(&viewborder);
		rect->ymin = viewborder.ymin + scene->r.border.ymin * BLI_rctf_size_y(&viewborder);
		rect->xmax = viewborder.xmin + scene->r.border.xmax * BLI_rctf_size_x(&viewborder);
		rect->ymax = viewborder.ymin + scene->r.border.ymax * BLI_rctf_size_y(&viewborder);
	}
	else {
		rect->xmin = v3d->render_border.xmin * ar->winx;
		rect->xmax = v3d->render_border.xmax * ar->winx;
		rect->ymin = v3d->render_border.ymin * ar->winy;
		rect->ymax = v3d->render_border.ymax * ar->winy;
	}

	BLI_rcti_translate(rect, ar->winrct.xmin, ar->winrct.ymin);
	BLI_rcti_isect(&ar->winrct, rect, rect);

	return true;
}

/**
  * IMPORTANT: this is deprecated, any changes made in this function should
  * be mirrored in view3d_draw_render_draw() in view3d_draw.c
  */
static bool view3d_main_region_draw_engine(
        const bContext *C, const EvaluationContext *eval_ctx, Scene *scene,
        ARegion *ar, View3D *v3d,
        bool clip_border, const rcti *border_rect)
{
	RegionView3D *rv3d = ar->regiondata;
	RenderEngineType *type;
	GLint scissor[4];


	/* create render engine */
	if (!rv3d->render_engine) {
		RenderEngine *engine;
		type = RE_engines_find(scene->view_render.engine_id);

		if (!(type->view_update && type->render_to_view))
			return false;

		engine = RE_engine_create_ex(type, true);

		engine->tile_x = scene->r.tilex;
		engine->tile_y = scene->r.tiley;

		type->view_update(engine, C);

		rv3d->render_engine = engine;
	}

	/* setup view matrices */
	VP_legacy_view3d_main_region_setup_view(eval_ctx, scene, v3d, ar, NULL, NULL);

	/* background draw */
	ED_region_pixelspace(ar);

	if (clip_border) {
		/* for border draw, we only need to clear a subset of the 3d view */
		if (border_rect->xmax > border_rect->xmin && border_rect->ymax > border_rect->ymin) {
			glGetIntegerv(GL_SCISSOR_BOX, scissor);
			glScissor(border_rect->xmin, border_rect->ymin,
			          BLI_rcti_size_x(border_rect), BLI_rcti_size_y(border_rect));
		}
		else {
			return false;
		}
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	bool show_image = false;
	{
		Camera *cam = ED_view3d_camera_data_get(v3d, rv3d);
		if (cam->flag & CAM_SHOW_BG_IMAGE) {
			show_image = true;
			view3d_draw_bgpic_test(scene, ar, v3d, false, true);
		}
		else {
			imm_draw_box_checker_2d(0, 0, ar->winx, ar->winy);
		}
	}

	if (show_image) {
		view3d_draw_bgpic_test(scene, ar, v3d, false, true);
	}
	else {
		imm_draw_box_checker_2d(0, 0, ar->winx, ar->winy);
	}

	/* render result draw */
	type = rv3d->render_engine->type;
	type->render_to_view(rv3d->render_engine, C);

	if (show_image) {
		view3d_draw_bgpic_test(scene, ar, v3d, true, true);
	}

	if (clip_border) {
		/* restore scissor as it was before */
		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	}

	return true;
}

static void view3d_main_region_draw_engine_info(View3D *v3d, RegionView3D *rv3d, ARegion *ar, bool render_border)
{
	float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};

	if (!rv3d->render_engine || !rv3d->render_engine->text[0])
		return;
	
	if (render_border) {
		/* draw darkened background color. no alpha because border render does
		 * partial redraw and will not redraw the region behind this info bar */
		float alpha = 1.0f - fill_color[3];
		Camera *camera = ED_view3d_camera_data_get(v3d, rv3d);

		if (camera) {
			if (camera->flag & CAM_SHOWPASSEPARTOUT) {
				alpha *= (1.0f - camera->passepartalpha);
			}
		}

		UI_GetThemeColor3fv(TH_HIGH_GRAD, fill_color);
		mul_v3_fl(fill_color, alpha);
		fill_color[3] = 1.0f;
	}

	ED_region_info_draw(ar, rv3d->render_engine->text, fill_color, true);
}

#ifdef WITH_GAMEENGINE
static void update_lods(Scene *scene, float camera_pos[3])
{
	Scene *sce_iter;
	Base *base;

	for (SETLOOPER(scene, sce_iter, base)) {
		Object *ob = base->object;
		BKE_object_lod_update(ob, camera_pos);
	}
}
#endif

static void view3d_main_region_draw_objects(const bContext *C, Scene *scene, ViewLayer *view_layer, View3D *v3d,
                                          ARegion *ar, const char **grid_unit)
{
	wmWindow *win = CTX_wm_window(C);
	EvaluationContext eval_ctx;
	RegionView3D *rv3d = ar->regiondata;
	unsigned int lay_used = v3d->lay_used;
	
	CTX_data_eval_ctx(C, &eval_ctx);

	/* post processing */
	bool do_compositing = false;
	
	/* shadow buffers, before we setup matrices */
	if (draw_glsl_material(scene, view_layer, NULL, v3d, v3d->drawtype))
		gpu_update_lamps_shadows_world(&eval_ctx, scene, v3d);

	/* reset default OpenGL lights if needed (i.e. after preferences have been altered) */
	if (rv3d->rflag & RV3D_GPULIGHT_UPDATE) {
		rv3d->rflag &= ~RV3D_GPULIGHT_UPDATE;
		GPU_default_lights();
	}

	/* setup the view matrix */
	if (VP_legacy_view3d_stereo3d_active(win, scene, v3d, rv3d)) {
		VP_legacy_view3d_stereo3d_setup(&eval_ctx, scene, v3d, ar);
	}
	else {
		VP_legacy_view3d_main_region_setup_view(&eval_ctx, scene, v3d, ar, NULL, NULL);
	}

	rv3d->rflag &= ~RV3D_IS_GAME_ENGINE;
#ifdef WITH_GAMEENGINE
	if (STREQ(scene->view_render.engine_id, RE_engine_id_BLENDER_GAME)) {
		rv3d->rflag |= RV3D_IS_GAME_ENGINE;

		/* Make sure LoDs are up to date */
		update_lods(scene, rv3d->viewinv[3]);
	}
#endif

	/* framebuffer fx needed, we need to draw offscreen first */
	if (v3d->fx_settings.fx_flag && v3d->drawtype >= OB_SOLID) {
		BKE_screen_gpu_fx_validate(&v3d->fx_settings);
		GPUFXSettings fx_settings = v3d->fx_settings;
		if (!rv3d->compositor)
			rv3d->compositor = GPU_fx_compositor_create();
		
		if (rv3d->persp == RV3D_CAMOB && v3d->camera)
			BKE_camera_to_gpu_dof(v3d->camera, &fx_settings);
		else {
			fx_settings.dof = NULL;
		}

		do_compositing = GPU_fx_compositor_initialize_passes(rv3d->compositor, &ar->winrct, &ar->drawrct, &fx_settings);
	}
	
	/* enables anti-aliasing for 3D view drawing */
	if (win->multisamples != USER_MULTISAMPLE_NONE) {
		glEnable(GL_MULTISAMPLE);
	}

	/* main drawing call */
	view3d_draw_objects(C, &eval_ctx, scene, v3d, ar, grid_unit, true, false, do_compositing ? rv3d->compositor : NULL);

	/* post process */
	if (do_compositing) {
		GPU_fx_do_composite_pass(rv3d->compositor, rv3d->winmat, rv3d->is_persp, scene, NULL);
	}

	/* Disable back anti-aliasing */
	if (win->multisamples != USER_MULTISAMPLE_NONE) {
		glDisable(GL_MULTISAMPLE);
	}

	if (v3d->lay_used != lay_used) { /* happens when loading old files or loading with UI load */
		/* find header and force tag redraw */
		ScrArea *sa = CTX_wm_area(C);
		ARegion *ar_header = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);
		ED_region_tag_redraw(ar_header); /* can be NULL */
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
		BDR_drawSketch(C);
	}
}

static void view3d_main_region_draw_info(const bContext *C, Scene *scene,
                                       ARegion *ar, View3D *v3d,
                                       const char *grid_unit, bool render_border)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	RegionView3D *rv3d = ar->regiondata;
	rcti rect;
	
	/* local coordinate visible rect inside region, to accomodate overlapping ui */
	ED_region_visible_rect(ar, &rect);

	if (rv3d->persp == RV3D_CAMOB) {
		VP_drawviewborder(scene, ar, v3d);
	}
	else if (v3d->flag2 & V3D_RENDER_BORDER) {
		VP_drawrenderborder(ar, v3d);
	}

	if (v3d->flag2 & V3D_SHOW_GPENCIL) {
		/* draw grease-pencil stuff - needed to get paint-buffer shown too (since it's 2D) */
		ED_gpencil_draw_view3d(wm, scene, view_layer, v3d, ar, false);
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
		VP_legacy_drawcursor(scene, view_layer, ar, v3d); /* 3D cursor */

		if (U.uiflag & USER_SHOW_ROTVIEWICON)
			VP_legacy_draw_view_axis(rv3d, &rect);
		else
			draw_view_icon(rv3d, &rect);

		if (U.uiflag & USER_DRAWVIEWINFO) {
			Object *ob = OBACT(view_layer);
			VP_legacy_draw_selected_name(scene, ob, &rect);
		}
	}

	if (rv3d->render_engine) {
		view3d_main_region_draw_engine_info(v3d, rv3d, ar, render_border);
		return;
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
		if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(wm)) {
			ED_scene_draw_fps(scene, &rect);
		}
		else if (U.uiflag & USER_SHOW_VIEWPORTNAME) {
			VP_legacy_draw_viewport_name(ar, v3d, &rect);
		}

		if (grid_unit) { /* draw below the viewport name */
			char numstr[32] = "";

			UI_FontThemeColor(BLF_default(), TH_TEXT_HI);
			if (v3d->grid != 1.0f) {
				BLI_snprintf(numstr, sizeof(numstr), "%s x %.4g", grid_unit, v3d->grid);
			}

			BLF_draw_default_ascii(rect.xmin + U.widget_unit,
			                       rect.ymax - (USER_SHOW_VIEWPORTNAME ? 2 * U.widget_unit : U.widget_unit), 0.0f,
			                       numstr[0] ? numstr : grid_unit, sizeof(numstr));
		}
	}
}

void view3d_main_region_draw_legacy(const bContext *C, ARegion *ar)
{
	EvaluationContext eval_ctx;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);
	const char *grid_unit = NULL;
	rcti border_rect;

	/* if we only redraw render border area, skip opengl draw and also
	 * don't do scissor because it's already set */
	bool render_border = ED_view3d_calc_render_border(scene, v3d, ar, &border_rect);
	bool clip_border = (render_border && !BLI_rcti_compare(&ar->drawrct, &border_rect));

	gpuPushProjectionMatrix();
	gpuLoadIdentityProjectionMatrix();
	gpuPushMatrix();
	gpuLoadIdentity();

	CTX_data_eval_ctx(C, &eval_ctx);

	/* draw viewport using opengl */
	if (v3d->drawtype != OB_RENDER || !view3d_main_region_do_render_draw(scene) || clip_border) {
		VP_view3d_main_region_clear(scene, v3d, ar); /* background */
		view3d_main_region_draw_objects(C, scene, view_layer, v3d, ar, &grid_unit);

		if (G.debug & G_DEBUG_SIMDATA)
			draw_sim_debug_data(scene, v3d, ar);

		glDisable(GL_DEPTH_TEST);
		ED_region_pixelspace(ar);
	}

	/* draw viewport using external renderer */
	if (v3d->drawtype == OB_RENDER) {
		view3d_main_region_draw_engine(C, &eval_ctx, scene, ar, v3d, clip_border, &border_rect);
	}

	VP_legacy_view3d_main_region_setup_view(&eval_ctx, scene, v3d, ar, NULL, NULL);
	glClear(GL_DEPTH_BUFFER_BIT);

	WM_manipulatormap_draw(ar->manipulator_map, C, WM_MANIPULATORMAP_DRAWSTEP_3D);

	ED_region_pixelspace(ar);

	view3d_main_region_draw_info(C, scene, ar, v3d, grid_unit, render_border);

	WM_manipulatormap_draw(ar->manipulator_map, C, WM_MANIPULATORMAP_DRAWSTEP_2D);

	gpuPopProjectionMatrix();
	gpuPopMatrix();

	v3d->flag |= V3D_INVALID_BACKBUF;

	BLI_assert(BLI_listbase_is_empty(&v3d->afterdraw_transp));
	BLI_assert(BLI_listbase_is_empty(&v3d->afterdraw_xray));
	BLI_assert(BLI_listbase_is_empty(&v3d->afterdraw_xraytransp));
}


/* -------------------------------------------------------------------- */

/** \name Deprecated Interface
 *
 * New viewport sometimes has a check for new/old viewport code.
 * Use these functions so new viewport can *optionally* call.
 *
 * \{ */


void VP_deprecated_view3d_draw_objects(
        const bContext *C,
        const EvaluationContext *eval_ctx,
        Scene *scene, View3D *v3d, ARegion *ar,
        const char **grid_unit,
        const bool do_bgpic, const bool draw_offscreen, GPUFX *fx)
{
	view3d_draw_objects(C, eval_ctx, scene, v3d, ar, grid_unit, do_bgpic, draw_offscreen, fx);
}

void VP_deprecated_gpu_update_lamps_shadows_world(const EvaluationContext *eval_ctx, Scene *scene, View3D *v3d)
{
	gpu_update_lamps_shadows_world(eval_ctx, scene, v3d);
}

/** \} */
