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

#include "DNA_camera_types.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_jitter.h"
#include "BLI_threads.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_camera.h"
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

#include "GPU_glew.h"
#include "GPU_compositing.h"
#include "GPU_framebuffer.h"


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

	int views_len;  /* multi-view views */

	bool is_sequencer;
	SpaceSeq *sseq;
	struct {
		ImBuf **ibufs_arr;
	} seq_data;


	Image *ima;
	ImageUser iuser;

	GPUOffScreen *ofs;
	int ofs_samples;
	bool ofs_full_samples;
	GPUFX *fx;
	int sizex, sizey;
	int write_still;

	ReportList *reports;
	bMovieHandle *mh;
	int cfrao, nfra;

	int totvideos;

	/* quick lookup */
	int view_id;

	/* wm vars for timer and progress cursor */
	wmWindowManager *wm;
	wmWindow *win;

	wmTimer *timer; /* use to check if running modal or not (invoke'd or exec'd)*/
	void **movie_ctx_arr;
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

static bool screen_opengl_is_multiview(OGLRender *oglrender)
{
	View3D *v3d = oglrender->v3d;
	RegionView3D *rv3d = oglrender->rv3d;
	RenderData *rd = &oglrender->scene->r;

	if ((rd == NULL) || ((!oglrender->is_sequencer) && ((rv3d == NULL) || (v3d == NULL))))
		return false;

	return (rd->scemode & R_MULTIVIEW) && ((oglrender->is_sequencer) || (rv3d->persp == RV3D_CAMOB && v3d->camera));
}

static void screen_opengl_views_setup(OGLRender *oglrender)
{
	RenderResult *rr;
	RenderView *rv;
	SceneRenderView *srv;
	bool is_multiview;
	Object *camera;
	View3D *v3d = oglrender->v3d;

	RenderData *rd = &oglrender->scene->r;

	rr = RE_AcquireResultWrite(oglrender->re);

	is_multiview = screen_opengl_is_multiview(oglrender);

	if (!is_multiview) {
		/* we only have one view when multiview is off */
		rv = rr->views.first;

		if (rv == NULL) {
			rv = MEM_callocN(sizeof(RenderView), "new opengl render view");
			BLI_addtail(&rr->views, rv);
		}

		while (rv->next) {
			RenderView *rv_del = rv->next;
			BLI_remlink(&rr->views, rv_del);

			if (rv_del->rectf)
				MEM_freeN(rv_del->rectf);

			if (rv_del->rectz)
				MEM_freeN(rv_del->rectz);

			MEM_freeN(rv_del);
		}
	}
	else {
		if (!oglrender->is_sequencer)
			RE_SetOverrideCamera(oglrender->re, V3D_CAMERA_SCENE(oglrender->scene, v3d));

		/* remove all the views that are not needed */
		rv = rr->views.last;
		while (rv) {
			srv = BLI_findstring(&rd->views, rv->name, offsetof(SceneRenderView, name));
			if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
				if (rv->rectf == NULL)
					rv->rectf = MEM_callocN(sizeof(float) * 4 * oglrender->sizex * oglrender->sizey, "screen_opengl_render_init rect");
				rv = rv->prev;
			}
			else {
				RenderView *rv_del = rv;
				rv  = rv_del->prev;

				BLI_remlink(&rr->views, rv_del);

				if (rv_del->rectf)
					MEM_freeN(rv_del->rectf);

				if (rv_del->rectz)
					MEM_freeN(rv_del->rectz);

				MEM_freeN(rv_del);
			}
		}

		/* create all the views that are needed */
		for (srv = rd->views.first; srv; srv = srv->next) {
			if (BKE_scene_multiview_is_render_view_active(rd, srv) == false)
				continue;

			rv = BLI_findstring(&rr->views, srv->name, offsetof(SceneRenderView, name));

			if (rv == NULL) {
				rv = MEM_callocN(sizeof(RenderView), "new opengl render view");
				BLI_strncpy(rv->name, srv->name, sizeof(rv->name));
				BLI_addtail(&rr->views, rv);
			}
		}
	}

	for (rv = rr->views.first; rv; rv = rv->next) {
		if (rv->rectf == NULL) {
			rv->rectf = MEM_callocN(sizeof(float) * 4 * oglrender->sizex * oglrender->sizey, "screen_opengl_render_init rect");
		}
	}

	BLI_lock_thread(LOCK_DRAW_IMAGE);
	if (!(is_multiview && BKE_scene_multiview_is_stereo3d(rd)))
		oglrender->iuser.flag &= ~IMA_SHOW_STEREO;
	BLI_unlock_thread(LOCK_DRAW_IMAGE);

	/* will only work for non multiview correctly */
	if (v3d) {
		camera = BKE_camera_multiview_render(oglrender->scene, v3d->camera, "new opengl render view");
		BKE_render_result_stamp_info(oglrender->scene, camera, rr, false);
	}
	else {
		BKE_render_result_stamp_info(oglrender->scene, oglrender->scene->camera, rr, false);
	}

	RE_ReleaseResult(oglrender->re);
}

static void screen_opengl_render_doit(OGLRender *oglrender, RenderResult *rr)
{
	Scene *scene = oglrender->scene;
	ARegion *ar = oglrender->ar;
	View3D *v3d = oglrender->v3d;
	RegionView3D *rv3d = oglrender->rv3d;
	Object *camera = NULL;
	int sizex = oglrender->sizex;
	int sizey = oglrender->sizey;
	const short view_context = (v3d != NULL);
	bool draw_bgpic = true;
	bool draw_sky = (scene->r.alphamode == R_ADDSKY);
	unsigned char *rect = NULL;
	const char *viewname = RE_GetActiveRenderView(oglrender->re);

	if (oglrender->is_sequencer) {
		SpaceSeq *sseq = oglrender->sseq;
		struct bGPdata *gpd = (sseq && (sseq->flag & SEQ_SHOW_GPENCIL)) ? sseq->gpd : NULL;

		/* use pre-calculated ImBuf (avoids deadlock), see: */
		ImBuf *ibuf = oglrender->seq_data.ibufs_arr[oglrender->view_id];

		if (ibuf) {
			ImBuf *out = IMB_dupImBuf(ibuf);
			IMB_freeImBuf(ibuf);
			/* OpenGL render is considered to be preview and should be
			 * as fast as possible. So currently we're making sure sequencer
			 * result is always byte to simplify color management pipeline.
			 *
			 * TODO(sergey): In the case of output to float container (EXR)
			 * it actually makes sense to keep float buffer instead.
			 */
			if (out->rect_float != NULL) {
				IMB_rect_from_float(out);
				imb_freerectfloatImBuf(out);
			}
			BLI_assert((oglrender->sizex == ibuf->x) && (oglrender->sizey == ibuf->y));
			RE_render_result_rect_from_ibuf(rr, &scene->r, out, oglrender->view_id);
			IMB_freeImBuf(out);
		}

		if (gpd) {
			int i;
			unsigned char *gp_rect;
			unsigned char *render_rect = (unsigned char *)RE_RenderViewGetById(rr, oglrender->view_id)->rect32;

			GPU_offscreen_bind(oglrender->ofs, true);

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			wmOrtho2(0, sizex, 0, sizey);
			glTranslatef(sizex / 2, sizey / 2, 0.0f);

			G.f |= G_RENDER_OGL;
			ED_gpencil_draw_ex(scene, gpd, sizex, sizey, scene->r.cfra, SPACE_SEQ);
			G.f &= ~G_RENDER_OGL;

			gp_rect = MEM_mallocN(sizex * sizey * sizeof(unsigned char) * 4, "offscreen rect");
			GPU_offscreen_read_pixels(oglrender->ofs, GL_UNSIGNED_BYTE, gp_rect);

			for (i = 0; i < sizex * sizey * 4; i += 4) {
				blend_color_mix_byte(&render_rect[i], &render_rect[i], &gp_rect[i]);
			}
			GPU_offscreen_unbind(oglrender->ofs, true);

			MEM_freeN(gp_rect);
		}
	}
	else {
		/* shouldnt suddenly give errors mid-render but possible */
		char err_out[256] = "unknown";
		ImBuf *ibuf_view;
		const int alpha_mode = (draw_sky) ? R_ADDSKY : R_ALPHAPREMUL;

		if (view_context) {
			ibuf_view = ED_view3d_draw_offscreen_imbuf(
			       scene, v3d, ar, sizex, sizey,
			       IB_rect, draw_bgpic,
			       alpha_mode, oglrender->ofs_samples, oglrender->ofs_full_samples, viewname,
			       oglrender->fx, oglrender->ofs, err_out);

			/* for stamp only */
			if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
				camera = BKE_camera_multiview_render(oglrender->scene, v3d->camera, viewname);
			}
		}
		else {
			ibuf_view = ED_view3d_draw_offscreen_imbuf_simple(
			        scene, scene->camera, oglrender->sizex, oglrender->sizey,
			        IB_rect, OB_SOLID, false, true, true,
			        alpha_mode, oglrender->ofs_samples, oglrender->ofs_full_samples, viewname,
			        oglrender->fx, oglrender->ofs, err_out);
			camera = scene->camera;
		}

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
		float *rectf = RE_RenderViewGetById(rr, oglrender->view_id)->rectf;

		if (BKE_scene_check_color_management_enabled(scene))
			profile_to = IB_PROFILE_LINEAR_RGB;
		else
			profile_to = IB_PROFILE_SRGB;

		/* sequencer has got trickier conversion happened above
		 * also assume opengl's space matches byte buffer color space */
		IMB_buffer_float_from_byte(rectf, rect,
		                           profile_to, IB_PROFILE_SRGB, true,
		                           oglrender->sizex, oglrender->sizey, oglrender->sizex, oglrender->sizex);

		/* rr->rectf is now filled with image data */

		if ((scene->r.stamp & R_STAMP_ALL) && (scene->r.stamp & R_STAMP_DRAW))
			BKE_image_stamp_buf(scene, camera, NULL, rect, rectf, rr->rectx, rr->recty, 4);

		MEM_freeN(rect);
	}
}

static void screen_opengl_render_write(OGLRender *oglrender)
{
	Scene *scene = oglrender->scene;
	RenderResult *rr;
	bool ok;
	char name[FILE_MAX];

	rr = RE_AcquireResultRead(oglrender->re);

	BKE_image_path_from_imformat(
	        name, scene->r.pic, oglrender->bmain->name, scene->r.cfra,
	        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, false, NULL);

	/* write images as individual images or stereo */
	BKE_render_result_stamp_info(scene, scene->camera, rr, false);
	ok = RE_WriteRenderViewsImage(oglrender->reports, rr, scene, false, name);

	RE_ReleaseResultImage(oglrender->re);

	if (ok) printf("OpenGL Render written to '%s'\n", name);
	else printf("OpenGL Render failed to write '%s'\n", name);
}

static void addAlphaOverFloat(float dest[4], const float source[4])
{
	/* d = s + (1-alpha_s)d*/
	float mul;

	mul = 1.0f - source[3];

	dest[0] = (mul * dest[0]) + source[0];
	dest[1] = (mul * dest[1]) + source[1];
	dest[2] = (mul * dest[2]) + source[2];
	dest[3] = (mul * dest[3]) + source[3];

}

/* add renderlayer and renderpass for each grease pencil layer for using in composition */
static void add_gpencil_renderpass(OGLRender *oglrender, RenderResult *rr, RenderView *rv)
{
	bGPdata *gpd = oglrender->scene->gpd;
	Scene *scene = oglrender->scene;

	/* sanity checks */
	if (gpd == NULL) {
		return;
	}
	if (scene == NULL) {
		return;
	}
	if (BLI_listbase_is_empty(&gpd->layers)) {
		return;
	}
	if (oglrender->v3d != NULL && (oglrender->v3d->flag2 & V3D_SHOW_GPENCIL) == 0) {
		return;
	}

	/* save old alpha mode */
	short oldalphamode = scene->r.alphamode;
	/* set alpha transparent for gp */
	scene->r.alphamode = R_ALPHAPREMUL;
	
	/* saves layer status */
	short *oldsts = MEM_mallocN(BLI_listbase_count(&gpd->layers) * sizeof(short), "temp_gplayers_flag");
	int i = 0;
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		oldsts[i] = gpl->flag;
		++i;
	}
	/* loop all layers to create separate render */
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* dont draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE)
			continue;
		/* hide all layer except current */
		for (bGPDlayer *gph = gpd->layers.first; gph; gph = gph->next) {
			if (gpl != gph) {
				gph->flag |= GP_LAYER_HIDE;
			}
		}

		/* render this gp layer */
		screen_opengl_render_doit(oglrender, rr);
		
		/* add RendePass composite */
		RenderPass *rp = RE_create_gp_pass(rr, gpl->info, rv->name);

		/* copy image data from rectf */
		float *src = RE_RenderViewGetById(rr, oglrender->view_id)->rectf;
		float *dest = rp->rect;

		float *pixSrc, *pixDest;
		int x, y, rectx, recty;
		rectx = rr->rectx;
		recty = rr->recty;
		for (y = 0; y < recty; y++) {
			for (x = 0; x < rectx; x++) {
				pixSrc = src + 4 * (rectx * y + x);
				if (pixSrc[3] > 0.0) {
					pixDest = dest + 4 * (rectx * y + x);
					addAlphaOverFloat(pixDest, pixSrc);
				}
			}
		}

		/* back layer status */
		i = 0;
		for (bGPDlayer *gph = gpd->layers.first; gph; gph = gph->next) {
			gph->flag = oldsts[i];
			++i;
		}
	}
	/* free memory */
	MEM_freeN(oldsts);

	/* back default alpha mode */
	scene->r.alphamode = oldalphamode;
}

static void screen_opengl_render_apply(OGLRender *oglrender)
{
	RenderResult *rr;
	RenderView *rv;
	int view_id;
	ImBuf *ibuf;
	void *lock;

	if (oglrender->is_sequencer) {
		Scene *scene = oglrender->scene;

		SeqRenderData context;
		SpaceSeq *sseq = oglrender->sseq;
		int chanshown = sseq ? sseq->chanshown : 0;

		BKE_sequencer_new_render_data(
		        oglrender->bmain->eval_ctx, oglrender->bmain, scene,
		        oglrender->sizex, oglrender->sizey, 100.0f,
		        &context);

		for (view_id = 0; view_id < oglrender->views_len; view_id++) {
			context.view_id = view_id;
			context.gpu_offscreen = oglrender->ofs;
			context.gpu_fx = oglrender->fx;
			context.gpu_full_samples = oglrender->ofs_full_samples;

			oglrender->seq_data.ibufs_arr[view_id] = BKE_sequencer_give_ibuf(&context, CFRA, chanshown);
		}
	}

	rr = RE_AcquireResultRead(oglrender->re);
	for (rv = rr->views.first, view_id = 0; rv; rv = rv->next, view_id++) {
		BLI_assert(view_id < oglrender->views_len);
		RE_SetActiveRenderView(oglrender->re, rv->name);
		oglrender->view_id = view_id;
		/* add grease pencil passes */
		add_gpencil_renderpass(oglrender, rr, rv);
		/* render composite */
		screen_opengl_render_doit(oglrender, rr);
	}

	RE_ReleaseResult(oglrender->re);

	ibuf = BKE_image_acquire_ibuf(oglrender->ima, &oglrender->iuser, &lock);
	if (ibuf) {
		ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
	}
	BKE_image_release_ibuf(oglrender->ima, ibuf, lock);

	if (oglrender->write_still) {
		screen_opengl_render_write(oglrender);
	}
}

static bool screen_opengl_render_init(bContext *C, wmOperator *op)
{
	/* new render clears all callbacks */
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);

	Scene *scene = CTX_data_scene(C);
	ScrArea *prevsa = CTX_wm_area(C);
	ARegion *prevar = CTX_wm_region(C);
	GPUOffScreen *ofs;
	OGLRender *oglrender;
	int sizex, sizey;
	const int samples = (scene->r.mode & R_OSA) ? scene->r.osa : 0;
	const bool full_samples = (samples != 0) && (scene->r.scemode & R_FULL_SAMPLE);
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
	ofs = GPU_offscreen_create(sizex, sizey, full_samples ? 0 : samples, err_out);

	if (!ofs) {
		BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL off-screen buffer, %s", err_out);
		return false;
	}

	/* allocate opengl render */
	oglrender = MEM_callocN(sizeof(OGLRender), "OGLRender");
	op->customdata = oglrender;

	oglrender->ofs = ofs;
	oglrender->ofs_samples = samples;
	oglrender->ofs_full_samples = full_samples;
	oglrender->sizex = sizex;
	oglrender->sizey = sizey;
	oglrender->bmain = CTX_data_main(C);
	oglrender->scene = scene;
	oglrender->cfrao = scene->r.cfra;

	oglrender->write_still = is_write_still && !is_animation;

	oglrender->views_len = BKE_scene_multiview_num_views_get(&scene->r);

	oglrender->is_sequencer = is_sequencer;
	if (is_sequencer) {
		oglrender->sseq = CTX_wm_space_seq(C);
		ImBuf **ibufs_arr = MEM_callocN(sizeof(*ibufs_arr) * oglrender->views_len, __func__);
		oglrender->seq_data.ibufs_arr = ibufs_arr;
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

		if (oglrender->v3d->fx_settings.fx_flag & (GPU_FX_FLAG_DOF | GPU_FX_FLAG_SSAO)) {
			oglrender->fx = GPU_fx_compositor_create();
		}
	}

	/* create render */
	oglrender->re = RE_NewRender(scene->id.name);

	/* create image and image user */
	oglrender->ima = BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(oglrender->ima, NULL, IMA_SIGNAL_FREE);
	BKE_image_backup_render(oglrender->scene, oglrender->ima, true);

	oglrender->iuser.scene = scene;
	oglrender->iuser.ok = 1;

	/* create render result */
	RE_InitState(oglrender->re, NULL, &scene->r, NULL, sizex, sizey, NULL);

	/* create render views */
	screen_opengl_views_setup(oglrender);

	/* wm vars */
	oglrender->wm = wm;
	oglrender->win = win;

	oglrender->totvideos = 0;
	oglrender->mh = NULL;
	oglrender->movie_ctx_arr = NULL;

	return true;
}

static void screen_opengl_render_end(bContext *C, OGLRender *oglrender)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = oglrender->scene;
	int i;

	if (oglrender->mh) {
		if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
			for (i = 0; i < oglrender->totvideos; i++) {
				oglrender->mh->end_movie(oglrender->movie_ctx_arr[i]);
				oglrender->mh->context_free(oglrender->movie_ctx_arr[i]);
			}
		}

		if (oglrender->movie_ctx_arr) {
			MEM_freeN(oglrender->movie_ctx_arr);
		}
	}

	if (oglrender->timer) { /* exec will not have a timer */
		scene->r.cfra = oglrender->cfrao;
		BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, scene, screen_opengl_layers(oglrender));

		WM_event_remove_timer(oglrender->wm, oglrender->win, oglrender->timer);
	}

	WM_cursor_modal_restore(oglrender->win);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);

	if (oglrender->fx)
		GPU_fx_compositor_destroy(oglrender->fx);

	GPU_offscreen_free(oglrender->ofs);

	if (oglrender->is_sequencer) {
		MEM_freeN(oglrender->seq_data.ibufs_arr);
	}

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
static bool screen_opengl_render_anim_initialize(bContext *C, wmOperator *op)
{
	/* initialize animation */
	OGLRender *oglrender;
	Scene *scene;

	oglrender = op->customdata;
	scene = oglrender->scene;
	oglrender->totvideos = BKE_scene_multiview_num_videos_get(&scene->r);

	oglrender->reports = op->reports;

	if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		size_t width, height;
		int i;

		BKE_scene_multiview_videos_dimensions_get(&scene->r, oglrender->sizex, oglrender->sizey, &width, &height);
		oglrender->mh = BKE_movie_handle_get(scene->r.im_format.imtype);

		if (oglrender->mh == NULL) {
			BKE_report(oglrender->reports, RPT_ERROR, "Movie format unsupported");
			screen_opengl_render_end(C, oglrender);
			return false;
		}

		oglrender->movie_ctx_arr = MEM_mallocN(sizeof(void *) * oglrender->totvideos, "Movies");

		for (i = 0; i < oglrender->totvideos; i++) {
			const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, i);

			oglrender->movie_ctx_arr[i] = oglrender->mh->context_create();
			if (!oglrender->mh->start_movie(oglrender->movie_ctx_arr[i], scene, &scene->r, oglrender->sizex,
			                                oglrender->sizey, oglrender->reports, PRVRANGEON != 0, suffix))
			{
				screen_opengl_render_end(C, oglrender);
				return false;
			}
		}
	}

	oglrender->cfrao = scene->r.cfra;
	oglrender->nfra = PSFRA;
	scene->r.cfra = PSFRA;

	return true;
}

static bool screen_opengl_render_anim_step(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	OGLRender *oglrender = op->customdata;
	Scene *scene = oglrender->scene;
	char name[FILE_MAX];
	bool ok = false;
	const bool view_context = (oglrender->v3d != NULL);
	bool is_movie;
	RenderResult *rr;

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
		BKE_image_path_from_imformat(
		        name, scene->r.pic, oglrender->bmain->name, scene->r.cfra,
		        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true, NULL);

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
		}
	}
	else {
		BKE_scene_camera_switch_update(scene);
	}

	/* render into offscreen buffer */
	screen_opengl_render_apply(oglrender);

	/* save to disk */
	rr = RE_AcquireResultRead(oglrender->re);

	if (is_movie) {
		ok = RE_WriteRenderViewsMovie(oglrender->reports, rr, scene, &scene->r, oglrender->mh,
		                              oglrender->movie_ctx_arr, oglrender->totvideos, PRVRANGEON != 0);
		if (ok) {
			printf("Append frame %d", scene->r.cfra);
			BKE_reportf(op->reports, RPT_INFO, "Appended frame: %d", scene->r.cfra);
		}
	}
	else {
		BKE_render_result_stamp_info(scene, scene->camera, rr, false);
		ok = RE_WriteRenderViewsImage(op->reports, rr, scene, true, name);
		if (ok) {
			printf("Saved: %s", name);
			BKE_reportf(op->reports, RPT_INFO, "Saved file: %s", name);
		}
		else {
			printf("Write error: cannot save %s\n", name);
			BKE_reportf(op->reports, RPT_ERROR, "Write error: cannot save %s", name);
		}
	}

	RE_ReleaseResult(oglrender->re);


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
	render_view_open(C, event->x, event->y, op->reports);
	
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
