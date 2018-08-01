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
#include "BLI_jitter_2d.h"
#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_threads.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
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
#include "DEG_depsgraph_query.h"

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
#include "GPU_material.h"
#include "GPU_extensions.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "view3d_intern.h"  /* own include */

/* ********* custom clipping *********** */

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

/* *********************** backdraw for selection *************** */

static void backdrawview3d(
        struct Depsgraph *depsgraph, Scene *scene,
        ARegion *ar, View3D *v3d,
        Object *obact, Object *obedit,
        short select_mode)
{
	RegionView3D *rv3d = ar->regiondata;
	Scene *scene_eval = (Scene *)DEG_get_evaluated_id(depsgraph, &scene->id);
	Object *obact_eval = DEG_get_evaluated_object(depsgraph, obact);

	BLI_assert(ar->regiontype == RGN_TYPE_WINDOW);

	if (obact_eval && (obact_eval->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT) ||
	             BKE_paint_select_face_test(obact_eval)))
	{
		/* do nothing */
	}
	/* texture paint mode sampling */
	else if (obact_eval && (obact_eval->mode & OB_MODE_TEXTURE_PAINT) &&
	         (v3d->shading.type > OB_WIRE))
	{
		/* do nothing */
	}
	else if ((obact_eval && (obact_eval->mode & OB_MODE_PARTICLE_EDIT)) &&
	         V3D_IS_ZBUF(v3d))
	{
		/* do nothing */
	}
	else if ((obedit && (obedit->mode & OB_MODE_EDIT)) &&
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

#if 0 /* v3d->zbuf deprecated */
	if (v3d->shading.type > OB_WIRE) v3d->zbuf = true;
#endif

	/* dithering and AA break color coding, so disable */
	glDisable(GL_DITHER);

	if (false) {
		/* for multisample we use an offscreen FBO. multisample drawing can fail
		 * with color coded selection drawing, and reading back depths from such
		 * a buffer can also cause a few seconds freeze on OS X / NVidia.
		 *
		 * NOTE: code is no longer used now, but offscreen drawing is likely
		 * what we will always want to do for the new viewport. */
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
			rv3d->gpuoffscreen = GPU_offscreen_create(w, h, 0, true, false, error);

			if (!rv3d->gpuoffscreen)
				fprintf(stderr, "Failed to create offscreen selection buffer for multisample: %s\n", error);
		}
	}

	if (rv3d->gpuoffscreen)
		GPU_offscreen_bind(rv3d->gpuoffscreen, true);
	else
		GPU_scissor(ar->winrct.xmin, ar->winrct.ymin, BLI_rcti_size_x(&ar->winrct), BLI_rcti_size_y(&ar->winrct));

	GPU_clear_color(0.0, 0.0, 0.0, 0.0);
	GPU_depth_test(true);
	GPU_clear(GPU_COLOR_BIT | GPU_DEPTH_BIT);

	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_set(rv3d);

	G.f |= G_BACKBUFSEL;

	if (obact_eval && ((obact_eval->base_flag & BASE_VISIBLE) != 0)) {
		draw_object_backbufsel(depsgraph, scene_eval, v3d, rv3d, obact_eval, select_mode);
	}

	if (rv3d->gpuoffscreen)
		GPU_offscreen_unbind(rv3d->gpuoffscreen, true);

	v3d->flag &= ~V3D_INVALID_BACKBUF;

	G.f &= ~G_BACKBUFSEL;
	GPU_depth_test(false);
	glEnable(GL_DITHER);

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

void ED_view3d_backbuf_validate_with_select_mode(ViewContext *vc, short select_mode)
{
	if (vc->v3d->flag & V3D_INVALID_BACKBUF) {
		backdrawview3d(vc->depsgraph, vc->scene, vc->ar, vc->v3d, vc->obact, vc->obedit, select_mode);
	}
}

void ED_view3d_backbuf_validate(ViewContext *vc)
{
	ED_view3d_backbuf_validate_with_select_mode(vc, -1);
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
        ViewContext *vc, int x, int y)
{
	if (x >= vc->ar->winx || y >= vc->ar->winy) {
		return 0;
	}

	ED_view3d_backbuf_validate(vc);

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
        ViewContext *vc, int xmin, int ymin, int xmax, int ymax)
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

	ED_view3d_backbuf_validate(vc);

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
        ViewContext *vc, const int mval[2], int size,
        unsigned int min, unsigned int max, float *r_dist)
{
	int dirvec[4][2];

	const int amount = (size - 1) / 2;

	const int minx = mval[0] - (amount + 1);
	const int miny = mval[1] - (amount + 1);
	ImBuf *buf = ED_view3d_backbuf_read(vc, minx, miny, minx + size - 1, miny + size - 1);
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

static void view3d_draw_bgpic(Scene *scene, Depsgraph *depsgraph,
                              ARegion *ar, View3D *v3d,
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
					ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &vb, false);
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

			GPU_depth_test(false);
			glDepthMask(GL_FALSE);

			GPU_blend(true);
			GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

			GPU_matrix_push_projection();
			GPU_matrix_push();
			ED_region_pixelspace(ar);

			GPU_matrix_translate_2f(centx, centy);
			GPU_matrix_scale_1f(bgpic->scale);
			GPU_matrix_rotate_2d(RAD2DEGF(-bgpic->rotation));

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

			GPU_matrix_pop_projection();
			GPU_matrix_pop();

			GPU_blend(false);

			glDepthMask(GL_TRUE);
			GPU_depth_test(true);

			if (freeibuf)
				IMB_freeImBuf(freeibuf);
			if (releaseibuf)
				BKE_image_release_ibuf(ima, releaseibuf, lock);
		}
	}
}

void ED_view3d_draw_bgpic_test(
        Scene *scene, Depsgraph *depsgraph,
        ARegion *ar, View3D *v3d,
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
			view3d_draw_bgpic(scene, depsgraph, ar, v3d, do_foreground, do_camera_frame);
		}
	}
	else {
		view3d_draw_bgpic(scene, depsgraph, ar, v3d, do_foreground, do_camera_frame);
	}
}

/* *********************** */

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
        Depsgraph *depsgraph, Scene *scene, ARegion *ar, View3D *v3d)
{
	ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);

	/* Setup view matrix. */
	ED_view3d_draw_setup_view(NULL, depsgraph, scene, ar, v3d, NULL, NULL, NULL);

	GPU_clear(GPU_DEPTH_BIT);

	GPU_depth_test(true);

	if (v3d->flag2 & V3D_SHOW_ANNOTATION) {
		ED_gpencil_draw_view3d(NULL, scene, view_layer, depsgraph, v3d, ar, true);
	}

	GPU_depth_test(false);
}

/* *********************** customdata **************** */

CustomDataMask ED_view3d_datamask(const Scene *UNUSED(scene), const View3D *v3d)
{
	CustomDataMask mask = 0;
	const int drawtype = view3d_effective_drawtype(v3d);

	if (ELEM(drawtype, OB_TEXTURE, OB_MATERIAL) ||
	    ((drawtype == OB_SOLID) && (v3d->flag2 & V3D_SOLID_TEX)))
	{
		mask |= CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL;

		if (drawtype == OB_MATERIAL)
			mask |= CD_MASK_ORCO;
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
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->view_update && type->view_draw);
}

bool ED_view3d_calc_render_border(const Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *ar, rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	bool use_border;

	/* test if there is a 3d view rendering */
	if (v3d->shading.type != OB_RENDER || !view3d_main_region_do_render_draw(scene))
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
		ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewborder, false);

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
