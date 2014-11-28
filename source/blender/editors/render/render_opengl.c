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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/render/render_opengl.c
 *  \ingroup edrend
 */


#include <math.h>
#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_jitter.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_writeavi.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_gpencil.h"

#include "RE_pipeline.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"


#include "render_intern.h"

typedef struct OGLRender {
	Main *bmain;
	Render *re;
	Scene *scene;

	View3D *v3d;
	RegionView3D *rv3d;
	ARegion *ar;

	ScrArea *prevsa;
	ARegion *prevar;

	bool is_sequencer;
	SpaceSeq *sseq;


	Image *ima;
	ImageUser iuser;

	GPUOffScreen *ofs;
	int sizex, sizey;
	int write_still;

	ReportList *reports;
	bMovieHandle *mh;
	int cfrao, nfra;

	/* wm vars for timer and progress cursor */
	wmWindowManager *wm;
	wmWindow *win;

	wmTimer *timer; /* use to check if running modal or not (invoke'd or exec'd)*/
} OGLRender;

/* added because v3d is not always valid */
static unsigned int screen_opengl_layers(OGLRender *oglrender)
{
	if (oglrender->v3d) {
		return oglrender->scene->lay | oglrender->v3d->lay;
	}
	else {
		return oglrender->scene->lay;
	}
}

static void screen_opengl_render_apply(OGLRender *oglrender)
{
	Scene *scene = oglrender->scene;
	ARegion *ar = oglrender->ar;
	View3D *v3d = oglrender->v3d;
	RegionView3D *rv3d = oglrender->rv3d;
	RenderResult *rr;
	Object *camera = NULL;
	ImBuf *ibuf;
	void *lock;
	float winmat[4][4];
	int sizex = oglrender->sizex;
	int sizey = oglrender->sizey;
	const short view_context = (v3d != NULL);
	bool draw_bgpic = true;
	bool draw_sky = (scene->r.alphamode == R_ADDSKY);
	unsigned char *rect = NULL;

	rr = RE_AcquireResultRead(oglrender->re);

	if (oglrender->is_sequencer) {
		SeqRenderData context;
		SpaceSeq *sseq = oglrender->sseq;
		int chanshown = sseq ? sseq->chanshown : 0;
		struct bGPdata *gpd = (sseq && (sseq->flag & SEQ_SHOW_GPENCIL)) ? sseq->gpd : NULL;

		context = BKE_sequencer_new_render_data(oglrender->bmain->eval_ctx, oglrender->bmain,
		                                        scene, oglrender->sizex, oglrender->sizey, 100.0f);

		ibuf = BKE_sequencer_give_ibuf(&context, CFRA, chanshown);

		if (ibuf) {
			ImBuf *linear_ibuf;

			BLI_assert((oglrender->sizex == ibuf->x) && (oglrender->sizey == ibuf->y));

			linear_ibuf = IMB_dupImBuf(ibuf);
			IMB_freeImBuf(ibuf);

			if (linear_ibuf->rect_float == NULL) {
				/* internally sequencer working in display space and stores both bytes and float buffers in that space.
				 * It is possible that byte->float onversion didn't happen in sequencer (e.g. when adding image sequence/movie
				 * into sequencer) there'll be only byte buffer. Create float buffer from existing byte buffer, making it linear
				 */

				IMB_float_from_rect(linear_ibuf);
			}
			else {
				/* ensure float buffer is in linear space, not in display space */
				BKE_sequencer_imbuf_from_sequencer_space(scene, linear_ibuf);
			}

			memcpy(rr->rectf, linear_ibuf->rect_float, sizeof(float) * 4 * oglrender->sizex * oglrender->sizey);

			IMB_freeImBuf(linear_ibuf);
		}

		if (gpd) {
			int i;
			unsigned char *gp_rect;

			GPU_offscreen_bind(oglrender->ofs);

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			wmOrtho2(0, sizex, 0, sizey);
			glTranslatef(sizex / 2, sizey / 2, 0.0f);

			ED_gpencil_draw_ex(gpd, sizex, sizey, scene->r.cfra);

			gp_rect = MEM_mallocN(sizex * sizey * sizeof(unsigned char) * 4, "offscreen rect");
			GPU_offscreen_read_pixels(oglrender->ofs, GL_UNSIGNED_BYTE, gp_rect);

			for (i = 0; i < sizex * sizey * 4; i += 4) {
				float  col_src[4];
				rgba_uchar_to_float(col_src, &gp_rect[i]);
				blend_color_mix_float(&rr->rectf[i], &rr->rectf[i], col_src);
			}
			GPU_offscreen_unbind(oglrender->ofs);

			MEM_freeN(gp_rect);
		}
	}
	else if (view_context) {
		ED_view3d_draw_offscreen_init(scene, v3d);

		GPU_offscreen_bind(oglrender->ofs); /* bind */

		/* render 3d view */
		if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
			/*int is_ortho = scene->r.mode & R_ORTHO;*/
			camera = v3d->camera;
			RE_GetCameraWindow(oglrender->re, camera, scene->r.cfra, winmat);
			
		}
		else {
			rctf viewplane;
			float clipsta, clipend;

			bool is_ortho = ED_view3d_viewplane_get(v3d, rv3d, sizex, sizey, &viewplane, &clipsta, &clipend, NULL);
			if (is_ortho) orthographic_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, -clipend, clipend);
			else perspective_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
		}

		rect = MEM_mallocN(sizex * sizey * sizeof(unsigned char) * 4, "offscreen rect");

		if ((scene->r.mode & R_OSA) == 0) {
			ED_view3d_draw_offscreen(scene, v3d, ar, sizex, sizey, NULL, winmat, draw_bgpic, draw_sky);
			GPU_offscreen_read_pixels(oglrender->ofs, GL_UNSIGNED_BYTE, rect);
		}
		else {
			/* simple accumulation, less hassle then FSAA FBO's */
			static float jit_ofs[32][2];
			float winmat_jitter[4][4];
			int *accum_buffer = MEM_mallocN(sizex * sizey * sizeof(int) * 4, "accum1");
			int i, j;

			BLI_jitter_init(jit_ofs, scene->r.osa);

			/* first sample buffer, also initializes 'rv3d->persmat' */
			ED_view3d_draw_offscreen(scene, v3d, ar, sizex, sizey, NULL, winmat, draw_bgpic, draw_sky);
			GPU_offscreen_read_pixels(oglrender->ofs, GL_UNSIGNED_BYTE, rect);

			for (i = 0; i < sizex * sizey * 4; i++)
				accum_buffer[i] = rect[i];

			/* skip the first sample */
			for (j = 1; j < scene->r.osa; j++) {
				copy_m4_m4(winmat_jitter, winmat);
				window_translate_m4(winmat_jitter, rv3d->persmat,
				                    (jit_ofs[j][0] * 2.0f) / sizex,
				                    (jit_ofs[j][1] * 2.0f) / sizey);

				ED_view3d_draw_offscreen(scene, v3d, ar, sizex, sizey, NULL, winmat_jitter, draw_bgpic, draw_sky);
				GPU_offscreen_read_pixels(oglrender->ofs, GL_UNSIGNED_BYTE, rect);

				for (i = 0; i < sizex * sizey * 4; i++)
					accum_buffer[i] += rect[i];
			}

			for (i = 0; i < sizex * sizey * 4; i++)
				rect[i] = accum_buffer[i] / scene->r.osa;

			MEM_freeN(accum_buffer);
		}

		GPU_offscreen_unbind(oglrender->ofs); /* unbind */
	}
	else {
		/* shouldnt suddenly give errors mid-render but possible */
		char err_out[256] = "unknown";
		ImBuf *ibuf_view = ED_view3d_draw_offscreen_imbuf_simple(scene, scene->camera, oglrender->sizex, oglrender->sizey,
		                                                         IB_rect, OB_SOLID, false, true,
		                                                         (draw_sky) ? R_ADDSKY : R_ALPHAPREMUL, err_out);
		camera = scene->camera;

		if (ibuf_view) {
			/* steal rect reference from ibuf */
			rect = (unsigned char *)ibuf_view->rect;
			ibuf_view->mall &= ~IB_rect;

			IMB_freeImBuf(ibuf_view);
		}
		else {
			fprintf(stderr, "%s: failed to get buffer, %s\n", __func__, err_out);
		}
	}

	/* note on color management:
	 *
	 * OpenGL renders into sRGB colors, but render buffers are expected to be
	 * linear So we convert to linear here, so the conversion back to bytes can make it
	 * sRGB (or other display space) again, and so that e.g. openexr saving also saves the
	 * correct linear float buffer.
	 */

	if (rect) {
		int profile_to;
		
		if (BKE_scene_check_color_management_enabled(scene))
			profile_to = IB_PROFILE_LINEAR_RGB;
		else
			profile_to = IB_PROFILE_SRGB;

		/* sequencer has got trickier conversion happened above
		 * also assume opengl's space matches byte buffer color space */
		IMB_buffer_float_from_byte(rr->rectf, rect,
		                           profile_to, IB_PROFILE_SRGB, true,
		                           oglrender->sizex, oglrender->sizey, oglrender->sizex, oglrender->sizex);
	}

	/* rr->rectf is now filled with image data */

	if ((scene->r.stamp & R_STAMP_ALL) && (scene->r.stamp & R_STAMP_DRAW))
		BKE_stamp_buf(scene, camera, rect, rr->rectf, rr->rectx, rr->recty, 4);

	RE_ReleaseResult(oglrender->re);

	/* update byte from float buffer */
	ibuf = BKE_image_acquire_ibuf(oglrender->ima, &oglrender->iuser, &lock);

	if (ibuf) {
		ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

		/* write file for animation */
		if (oglrender->write_still) {
			char name[FILE_MAX];
			int ok;

			if (scene->r.im_format.planes == R_IMF_CHAN_DEPTH_8) {
				IMB_color_to_bw(ibuf);
			}

			BKE_makepicstring(name, scene->r.pic, oglrender->bmain->name, scene->r.cfra,
			                  &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, false);
			ok = BKE_imbuf_write_as(ibuf, name, &scene->r.im_format, true); /* no need to stamp here */
			if (ok) printf("OpenGL Render written to '%s'\n", name);
			else printf("OpenGL Render failed to write '%s'\n", name);
		}
	}
	
	BKE_image_release_ibuf(oglrender->ima, ibuf, lock);

	if (rect)
		MEM_freeN(rect);
}

static bool screen_opengl_render_init(bContext *C, wmOperator *op)
{
	/* new render clears all callbacks */
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);

	Scene *scene = CTX_data_scene(C);
	ScrArea *prevsa = CTX_wm_area(C);
	ARegion *prevar = CTX_wm_region(C);
	RenderResult *rr;
	GPUOffScreen *ofs;
	OGLRender *oglrender;
	int sizex, sizey;
	bool is_view_context = RNA_boolean_get(op->ptr, "view_context");
	const bool is_animation = RNA_boolean_get(op->ptr, "animation");
	const bool is_sequencer = RNA_boolean_get(op->ptr, "sequencer");
	const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
	char err_out[256] = "unknown";

	if (G.background) {
		BKE_report(op->reports, RPT_ERROR, "Cannot use OpenGL render in background mode (no opengl context)");
		return false;
	}

	/* only one render job at a time */
	if (WM_jobs_test(wm, scene, WM_JOB_TYPE_RENDER))
		return false;

	if (is_sequencer) {
		is_view_context = false;
	}
	else {
		/* ensure we have a 3d view */
		if (!ED_view3d_context_activate(C)) {
			RNA_boolean_set(op->ptr, "view_context", false);
			is_view_context = false;
		}

		if (!is_view_context && scene->camera == NULL) {
			BKE_report(op->reports, RPT_ERROR, "Scene has no camera");
			return false;
		}
	}

	if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
		return false;
	}

	/* stop all running jobs, except screen one. currently previews frustrate Render */
	WM_jobs_kill_all_except(wm, CTX_wm_screen(C));

	/* create offscreen buffer */
	sizex = (scene->r.size * scene->r.xsch) / 100;
	sizey = (scene->r.size * scene->r.ysch) / 100;

	/* corrects render size with actual size, not every card supports non-power-of-two dimensions */
	ofs = GPU_offscreen_create(sizex, sizey, err_out);

	if (!ofs) {
		BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL off-screen buffer, %s", err_out);
		return false;
	}

	/* allocate opengl render */
	oglrender = MEM_callocN(sizeof(OGLRender), "OGLRender");
	op->customdata = oglrender;

	oglrender->ofs = ofs;
	oglrender->sizex = sizex;
	oglrender->sizey = sizey;
	oglrender->bmain = CTX_data_main(C);
	oglrender->scene = scene;
	oglrender->cfrao = scene->r.cfra;

	oglrender->write_still = is_write_still && !is_animation;

	oglrender->is_sequencer = is_sequencer;
	if (is_sequencer) {
		oglrender->sseq = CTX_wm_space_seq(C);
	}


	oglrender->prevsa = prevsa;
	oglrender->prevar = prevar;

	if (is_view_context) {
		ED_view3d_context_user_region(C, &oglrender->v3d, &oglrender->ar); /* so quad view renders camera */
		oglrender->rv3d = oglrender->ar->regiondata;

		/* MUST be cleared on exit */
		oglrender->scene->customdata_mask_modal = ED_view3d_datamask(oglrender->scene, oglrender->v3d);

		/* apply immediately in case we're rendering from a script,
		 * running notifiers again will overwrite */
		oglrender->scene->customdata_mask |= oglrender->scene->customdata_mask_modal;

	}

	/* create render */
	oglrender->re = RE_NewRender(scene->id.name);

	/* create image and image user */
	oglrender->ima = BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(oglrender->ima, NULL, IMA_SIGNAL_FREE);
	BKE_image_backup_render(oglrender->scene, oglrender->ima);

	oglrender->iuser.scene = scene;
	oglrender->iuser.ok = 1;

	/* create render result */
	RE_InitState(oglrender->re, NULL, &scene->r, NULL, sizex, sizey, NULL);

	rr = RE_AcquireResultWrite(oglrender->re);
	if (rr->rectf == NULL)
		rr->rectf = MEM_callocN(sizeof(float) * 4 * sizex * sizey, "screen_opengl_render_init rect");
	RE_ReleaseResult(oglrender->re);

	/* wm vars */
	oglrender->wm = wm;
	oglrender->win = win;

	return true;
}

static void screen_opengl_render_end(bContext *C, OGLRender *oglrender)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = oglrender->scene;

	if (oglrender->mh) {
		if (BKE_imtype_is_movie(scene->r.im_format.imtype))
			oglrender->mh->end_movie();
	}

	if (oglrender->timer) { /* exec will not have a timer */
		scene->r.cfra = oglrender->cfrao;
		BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, scene, screen_opengl_layers(oglrender));

		WM_event_remove_timer(oglrender->wm, oglrender->win, oglrender->timer);
	}

	WM_cursor_modal_restore(oglrender->win);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);

	GPU_offscreen_free(oglrender->ofs);

	oglrender->scene->customdata_mask_modal = 0;

	CTX_wm_area_set(C, oglrender->prevsa);
	CTX_wm_region_set(C, oglrender->prevar);

	MEM_freeN(oglrender);
}

static void screen_opengl_render_cancel(bContext *C, wmOperator *op)
{
	screen_opengl_render_end(C, op->customdata);
}

/* share between invoke and exec */
static int screen_opengl_render_anim_initialize(bContext *C, wmOperator *op)
{
	/* initialize animation */
	OGLRender *oglrender;
	Scene *scene;

	oglrender = op->customdata;
	scene = oglrender->scene;

	oglrender->reports = op->reports;
	oglrender->mh = BKE_movie_handle_get(scene->r.im_format.imtype);
	if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		if (!oglrender->mh->start_movie(scene, &scene->r, oglrender->sizex, oglrender->sizey, oglrender->reports)) {
			screen_opengl_render_end(C, oglrender);
			return 0;
		}
	}

	oglrender->cfrao = scene->r.cfra;
	oglrender->nfra = PSFRA;
	scene->r.cfra = PSFRA;

	return 1;
}
static bool screen_opengl_render_anim_step(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	OGLRender *oglrender = op->customdata;
	Scene *scene = oglrender->scene;
	ImBuf *ibuf, *ibuf_save = NULL;
	void *lock;
	char name[FILE_MAX];
	bool ok = false;
	const bool view_context = (oglrender->v3d != NULL);
	Object *camera = NULL;
	bool is_movie;

	/* go to next frame */
	if (CFRA < oglrender->nfra)
		CFRA++;
	while (CFRA < oglrender->nfra) {
		unsigned int lay = screen_opengl_layers(oglrender);

		if (lay & 0xFF000000)
			lay &= 0xFF000000;

		BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, scene, lay);
		CFRA++;
	}

	is_movie = BKE_imtype_is_movie(scene->r.im_format.imtype);

	if (!is_movie) {
		BKE_makepicstring(name, scene->r.pic, oglrender->bmain->name, scene->r.cfra,
		                  &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true);

		if ((scene->r.mode & R_NO_OVERWRITE) && BLI_exists(name)) {
			BKE_reportf(op->reports, RPT_INFO, "Skipping existing frame \"%s\"", name);
			ok = true;
			goto finally;
		}
	}

	WM_cursor_time(oglrender->win, scene->r.cfra);

	BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, scene, screen_opengl_layers(oglrender));

	if (view_context) {
		if (oglrender->rv3d->persp == RV3D_CAMOB && oglrender->v3d->camera && oglrender->v3d->scenelock) {
			/* since BKE_scene_update_for_newframe() is used rather
			 * then ED_update_for_newframe() the camera needs to be set */
			if (BKE_scene_camera_switch_update(scene)) {
				oglrender->v3d->camera = scene->camera;
			}

			camera = oglrender->v3d->camera;
		}
	}
	else {
		BKE_scene_camera_switch_update(scene);

		camera = scene->camera;
	}

	/* render into offscreen buffer */
	screen_opengl_render_apply(oglrender);

	/* save to disk */
	ibuf = BKE_image_acquire_ibuf(oglrender->ima, &oglrender->iuser, &lock);

	if (ibuf) {
		bool needs_free = false;

		ibuf_save = ibuf;

		if (is_movie || !BKE_imtype_requires_linear_float(scene->r.im_format.imtype)) {
			ibuf_save = IMB_colormanagement_imbuf_for_write(ibuf, true, true, &scene->view_settings,
			                                                &scene->display_settings, &scene->r.im_format);

			needs_free = true;
		}

		/* color -> grayscale */
		/* editing directly would alter the render view */
		if (scene->r.im_format.planes == R_IMF_PLANES_BW) {
			ImBuf *ibuf_bw = IMB_dupImBuf(ibuf_save);
			IMB_color_to_bw(ibuf_bw);

			if (needs_free)
				IMB_freeImBuf(ibuf_save);

			ibuf_save = ibuf_bw;
		}
		else {
			/* this is lightweight & doesnt re-alloc the buffers, only do this
			 * to save the correct bit depth since the image is always RGBA */
			ImBuf *ibuf_cpy = IMB_allocImBuf(ibuf_save->x, ibuf_save->y, scene->r.im_format.planes, 0);

			ibuf_cpy->rect = ibuf_save->rect;
			ibuf_cpy->rect_float = ibuf_save->rect_float;
			ibuf_cpy->zbuf_float = ibuf_save->zbuf_float;

			if (needs_free) {
				ibuf_cpy->mall = ibuf_save->mall;
				ibuf_save->mall = 0;
				IMB_freeImBuf(ibuf_save);
			}

			ibuf_save = ibuf_cpy;
		}

		if (is_movie) {
			ok = oglrender->mh->append_movie(&scene->r, PSFRA, CFRA, (int *)ibuf_save->rect,
			                                 oglrender->sizex, oglrender->sizey, oglrender->reports);
			if (ok) {
				printf("Append frame %d", scene->r.cfra);
				BKE_reportf(op->reports, RPT_INFO, "Appended frame: %d", scene->r.cfra);
			}
		}
		else {
			ok = BKE_imbuf_write_stamp(scene, camera, ibuf_save, name, &scene->r.im_format);

			if (ok == 0) {
				printf("Write error: cannot save %s\n", name);
				BKE_reportf(op->reports, RPT_ERROR, "Write error: cannot save %s", name);
			}
			else {
				printf("Saved: %s", name);
				BKE_reportf(op->reports, RPT_INFO, "Saved file: %s", name);
			}
		}

		if (needs_free)
			IMB_freeImBuf(ibuf_save);
	}

	BKE_image_release_ibuf(oglrender->ima, ibuf, lock);

	/* movie stats prints have no line break */
	printf("\n");


finally:  /* Step the frame and bail early if needed */

	/* go to next frame */
	oglrender->nfra += scene->r.frame_step;

	/* stop at the end or on error */
	if (CFRA >= PEFRA || !ok) {
		screen_opengl_render_end(C, op->customdata);
		return 0;
	}

	return 1;
}


static int screen_opengl_render_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	OGLRender *oglrender = op->customdata;
	const bool anim = RNA_boolean_get(op->ptr, "animation");
	bool ret;

	switch (event->type) {
		case ESCKEY:
			/* cancel */
			screen_opengl_render_end(C, op->customdata);
			return OPERATOR_FINISHED;
		case TIMER:
			/* render frame? */
			if (oglrender->timer == event->customdata)
				break;
			/* fall-through */
		default:
			/* nothing to do */
			return OPERATOR_RUNNING_MODAL;
	}

	/* run first because screen_opengl_render_anim_step can free oglrender */
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);
	
	if (anim == 0) {
		screen_opengl_render_apply(op->customdata);
		screen_opengl_render_end(C, op->customdata);
		return OPERATOR_FINISHED;
	}
	else {
		ret = screen_opengl_render_anim_step(C, op);
	}

	/* stop at the end or on error */
	if (ret == false) {
		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int screen_opengl_render_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	OGLRender *oglrender;
	const bool anim = RNA_boolean_get(op->ptr, "animation");

	if (!screen_opengl_render_init(C, op))
		return OPERATOR_CANCELLED;

	if (anim) {
		if (!screen_opengl_render_anim_initialize(C, op))
			return OPERATOR_CANCELLED;
	}
	
	oglrender = op->customdata;
	render_view_open(C, event->x, event->y);
	
	/* view may be changed above (R_OUTPUT_WINDOW) */
	oglrender->win = CTX_wm_window(C);

	WM_event_add_modal_handler(C, op);
	oglrender->timer = WM_event_add_timer(oglrender->wm, oglrender->win, TIMER, 0.01f);
	
	return OPERATOR_RUNNING_MODAL;
}

/* executes blocking render */
static int screen_opengl_render_exec(bContext *C, wmOperator *op)
{
	const bool is_animation = RNA_boolean_get(op->ptr, "animation");

	if (!screen_opengl_render_init(C, op))
		return OPERATOR_CANCELLED;

	if (!is_animation) { /* same as invoke */
		/* render image */
		screen_opengl_render_apply(op->customdata);
		screen_opengl_render_end(C, op->customdata);

		return OPERATOR_FINISHED;
	}
	else {
		bool ret = true;

		if (!screen_opengl_render_anim_initialize(C, op))
			return OPERATOR_CANCELLED;

		while (ret) {
			ret = screen_opengl_render_anim_step(C, op);
		}
	}

	/* no redraw needed, we leave state as we entered it */
//	ED_update_for_newframe(C, 1);
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

void RENDER_OT_opengl(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "OpenGL Render";
	ot->description = "OpenGL render active viewport";
	ot->idname = "RENDER_OT_opengl";

	/* api callbacks */
	ot->invoke = screen_opengl_render_invoke;
	ot->exec = screen_opengl_render_exec; /* blocking */
	ot->modal = screen_opengl_render_modal;
	ot->cancel = screen_opengl_render_cancel;

	ot->poll = ED_operator_screenactive;

	prop = RNA_def_boolean(ot->srna, "animation", 0, "Animation", "Render files from the animation range of this scene");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "sequencer", 0, "Sequencer", "Render using the sequencer's OpenGL display");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "write_still", 0, "Write Image", "Save rendered the image to the output path (used only when animation is disabled)");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "view_context", 1, "View Context", "Use the current 3D view for rendering, else use scene settings");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

}

/* function for getting an opengl buffer from a View3D, used by sequencer */
// extern void *sequencer_view3d_cb;
