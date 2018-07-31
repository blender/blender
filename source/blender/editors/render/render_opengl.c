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
#include "BLI_threads.h"
#include "BLI_task.h"

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

#include "DEG_depsgraph.h"

#include "DRW_engine.h"

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

#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_matrix.h"

#include "render_intern.h"

/* Define this to get timing information. */
// #undef DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#endif

// TODO(sergey): Find better approximation of the scheduled frames.
// For really highres renders it might fail still.
#define MAX_SCHEDULED_FRAMES 8

typedef struct OGLRender {
	Main *bmain;
	Render *re;
	Scene *scene;
	WorkSpace *workspace;
	ViewLayer *view_layer;
	Depsgraph *depsgraph;

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

	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	bool pool_ok;
	bool is_animation;
	SpinLock reports_lock;
	unsigned int num_scheduled_frames;
	ThreadMutex task_mutex;
	ThreadCondition task_condition;

#ifdef DEBUG_TIME
	double time_start;
#endif
} OGLRender;

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

			if (rv_del->rect32)
				MEM_freeN(rv_del->rect32);

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

				if (rv_del->rect32)
					MEM_freeN(rv_del->rect32);

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

	if (!(is_multiview && BKE_scene_multiview_is_stereo3d(rd)))
		oglrender->iuser.flag &= ~IMA_SHOW_STEREO;

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

static void screen_opengl_render_doit(const bContext *C, OGLRender *oglrender, RenderResult *rr)
{
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = oglrender->scene;
	ARegion *ar = oglrender->ar;
	View3D *v3d = oglrender->v3d;
	RegionView3D *rv3d = oglrender->rv3d;
	Object *camera = NULL;
	int sizex = oglrender->sizex;
	int sizey = oglrender->sizey;
	const short view_context = (v3d != NULL);
	bool draw_sky = (scene->r.alphamode == R_ADDSKY);
	float *rectf = NULL;
	const char *viewname = RE_GetActiveRenderView(oglrender->re);
	ImBuf *ibuf_result = NULL;

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
		else if (gpd) {
			/* If there are no strips, Grease Pencil still needs a buffer to draw on */
			ImBuf *out = IMB_allocImBuf(oglrender->sizex, oglrender->sizey, 32, IB_rect);
			RE_render_result_rect_from_ibuf(rr, &scene->r, out, oglrender->view_id);
			IMB_freeImBuf(out);
		}

		if (gpd) {
			int i;
			unsigned char *gp_rect;
			unsigned char *render_rect = (unsigned char *)RE_RenderViewGetById(rr, oglrender->view_id)->rect32;

			DRW_opengl_context_enable();
			GPU_offscreen_bind(oglrender->ofs, true);

			GPU_clear_color(0.0f, 0.0f, 0.0f, 0.0f);
			GPU_clear(GPU_COLOR_BIT | GPU_DEPTH_BIT);

			wmOrtho2(0, sizex, 0, sizey);
			GPU_matrix_translate_2f(sizex / 2, sizey / 2);

			G.f |= G_RENDER_OGL;
			ED_gpencil_draw_ex(rv3d, scene, gpd, sizex, sizey, scene->r.cfra, SPACE_SEQ);
			G.f &= ~G_RENDER_OGL;

			gp_rect = MEM_mallocN(sizex * sizey * sizeof(unsigned char) * 4, "offscreen rect");
			GPU_offscreen_read_pixels(oglrender->ofs, GL_UNSIGNED_BYTE, gp_rect);

			for (i = 0; i < sizex * sizey * 4; i += 4) {
				blend_color_mix_byte(&render_rect[i], &render_rect[i], &gp_rect[i]);
			}
			GPU_offscreen_unbind(oglrender->ofs, true);
			DRW_opengl_context_disable();

			MEM_freeN(gp_rect);
		}
	}
	else {
		/* shouldnt suddenly give errors mid-render but possible */
		char err_out[256] = "unknown";
		ImBuf *ibuf_view;
		const int alpha_mode = (draw_sky) ? R_ADDSKY : R_ALPHAPREMUL;

		unsigned int draw_flags = V3D_OFSDRAW_NONE;
		draw_flags |= (oglrender->ofs_full_samples) ? V3D_OFSDRAW_USE_FULL_SAMPLE : 0;

		if (view_context) {
			ibuf_view = ED_view3d_draw_offscreen_imbuf(
			       depsgraph, scene, v3d->shading.type,
			       v3d, ar, sizex, sizey,
			       IB_rectfloat, draw_flags, alpha_mode, oglrender->ofs_samples, viewname,
			       oglrender->ofs, err_out);

			/* for stamp only */
			if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
				camera = BKE_camera_multiview_render(oglrender->scene, v3d->camera, viewname);
			}
		}
		else {
			draw_flags |= V3D_OFSDRAW_USE_GPENCIL;
			ibuf_view = ED_view3d_draw_offscreen_imbuf_simple(
			        depsgraph, scene, OB_SOLID,
			        scene->camera, oglrender->sizex, oglrender->sizey,
			        IB_rectfloat, draw_flags,
			        alpha_mode, oglrender->ofs_samples, viewname,
			        oglrender->ofs, err_out);
			camera = scene->camera;
		}

		if (ibuf_view) {
			ibuf_result = ibuf_view;
			rectf = (float *)ibuf_view->rect_float;
		}
		else {
			fprintf(stderr, "%s: failed to get buffer, %s\n", __func__, err_out);
		}
	}

	if (ibuf_result != NULL) {
		if ((scene->r.stamp & R_STAMP_ALL) && (scene->r.stamp & R_STAMP_DRAW)) {
			BKE_image_stamp_buf(scene, camera, NULL, NULL, rectf, rr->rectx, rr->recty, 4);
		}
		RE_render_result_rect_from_ibuf(rr, &scene->r, ibuf_result, oglrender->view_id);
		IMB_freeImBuf(ibuf_result);
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
	        name, scene->r.pic, BKE_main_blendfile_path(oglrender->bmain), scene->r.cfra,
	        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, false, NULL);

	/* write images as individual images or stereo */
	BKE_render_result_stamp_info(scene, scene->camera, rr, false);
	ok = RE_WriteRenderViewsImage(oglrender->reports, rr, scene, false, name);

	RE_ReleaseResultImage(oglrender->re);

	if (ok) printf("OpenGL Render written to '%s'\n", name);
	else printf("OpenGL Render failed to write '%s'\n", name);
}

static void UNUSED_FUNCTION(addAlphaOverFloat)(float dest[4], const float source[4])
{
	/* d = s + (1-alpha_s)d*/
	float mul;

	mul = 1.0f - source[3];

	dest[0] = (mul * dest[0]) + source[0];
	dest[1] = (mul * dest[1]) + source[1];
	dest[2] = (mul * dest[2]) + source[2];
	dest[3] = (mul * dest[3]) + source[3];

}

static void screen_opengl_render_apply(const bContext *C, OGLRender *oglrender)
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
		        oglrender->bmain, oglrender->depsgraph, scene,
		        oglrender->sizex, oglrender->sizey, 100.0f, false,
		        &context);

		for (view_id = 0; view_id < oglrender->views_len; view_id++) {
			context.view_id = view_id;
			context.gpu_offscreen = oglrender->ofs;
			context.gpu_full_samples = oglrender->ofs_full_samples;

			oglrender->seq_data.ibufs_arr[view_id] = BKE_sequencer_give_ibuf(&context, CFRA, chanshown);
		}
	}

	rr = RE_AcquireResultRead(oglrender->re);
	for (rv = rr->views.first, view_id = 0; rv; rv = rv->next, view_id++) {
		BLI_assert(view_id < oglrender->views_len);
		RE_SetActiveRenderView(oglrender->re, rv->name);
		oglrender->view_id = view_id;
		/* render composite */
		screen_opengl_render_doit(C, oglrender, rr);
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

static bool screen_opengl_fullsample_enabled(Scene *scene)
{
	if (scene->r.scemode & R_FULL_SAMPLE) {
		return true;
	}
	else {
		/* XXX TODO:
		 * Technically if the hardware supports MSAA we could keep using Blender 2.7x approach.
		 * However anti-aliasing without full_sample is not playing well even in 2.7x.
		 *
		 * For example, if you enable depth of field, there is aliasing, even if the viewport is fine.
		 * For 2.8x this is more complicated because so many things rely on shader.
		 * So until we fix the gpu_framebuffer anti-aliasing suupport we need to force full sample.
		 */
		return true;
	}
}

static bool screen_opengl_render_init(bContext *C, wmOperator *op)
{
	/* new render clears all callbacks */
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	WorkSpace *workspace = CTX_wm_workspace(C);

	Scene *scene = CTX_data_scene(C);
	ScrArea *prevsa = CTX_wm_area(C);
	ARegion *prevar = CTX_wm_region(C);
	GPUOffScreen *ofs;
	OGLRender *oglrender;
	int sizex, sizey;
	const int samples = (scene->r.mode & R_OSA) ? scene->r.osa : 0;
	const bool full_samples = (samples != 0) && screen_opengl_fullsample_enabled(scene);
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
	DRW_opengl_context_enable(); /* Offscreen creation needs to be done in DRW context. */
	ofs = GPU_offscreen_create(sizex, sizey, full_samples ? 0 : samples, true, true, err_out);
	DRW_opengl_context_disable();

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
	oglrender->workspace = workspace;
	oglrender->view_layer = CTX_data_view_layer(C);
	oglrender->depsgraph = CTX_data_depsgraph(C);
	oglrender->cfrao = scene->r.cfra;

	oglrender->write_still = is_write_still && !is_animation;
	oglrender->is_animation = is_animation;

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
	}

	/* create render */
	oglrender->re = RE_NewSceneRender(scene);

	/* create image and image user */
	oglrender->ima = BKE_image_verify_viewer(oglrender->bmain, IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(oglrender->bmain, oglrender->ima, NULL, IMA_SIGNAL_FREE);
	BKE_image_backup_render(oglrender->scene, oglrender->ima, true);

	oglrender->iuser.scene = scene;
	oglrender->iuser.ok = 1;

	/* create render result */
	RE_InitState(oglrender->re, NULL, &scene->r, &scene->view_layers, NULL, sizex, sizey, NULL);

	/* create render views */
	screen_opengl_views_setup(oglrender);

	/* wm vars */
	oglrender->wm = wm;
	oglrender->win = win;

	oglrender->totvideos = 0;
	oglrender->mh = NULL;
	oglrender->movie_ctx_arr = NULL;

	if (is_animation) {
		TaskScheduler *task_scheduler = BLI_task_scheduler_get();
		if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
			task_scheduler = BLI_task_scheduler_create(1);
			oglrender->task_scheduler = task_scheduler;
			oglrender->task_pool = BLI_task_pool_create_background(task_scheduler,
			                                                       oglrender);
		}
		else {
			oglrender->task_scheduler = NULL;
			oglrender->task_pool = BLI_task_pool_create(task_scheduler,
			                                            oglrender);
		}
		oglrender->pool_ok = true;
		BLI_spin_init(&oglrender->reports_lock);
	}
	else {
		oglrender->task_scheduler = NULL;
		oglrender->task_pool = NULL;
	}
	oglrender->num_scheduled_frames = 0;
	BLI_mutex_init(&oglrender->task_mutex);
	BLI_condition_init(&oglrender->task_condition);

#ifdef DEBUG_TIME
	oglrender->time_start = PIL_check_seconds_timer();
#endif

	return true;
}

static void screen_opengl_render_end(bContext *C, OGLRender *oglrender)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = oglrender->scene;
	int i;

	if (oglrender->is_animation) {
		/* Trickery part for movie output:
		 *
		 * We MUST write frames in an exact order, so we only let background
		 * thread to work on that, and main thread is simply waits for that
		 * thread to do all the dirty work.
		 *
		 * After this loop is done work_and_wait() will have nothing to do,
		 * so we don't run into wrong order of frames written to the stream.
		 */
		if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
			BLI_mutex_lock(&oglrender->task_mutex);
			while (oglrender->num_scheduled_frames > 0) {
				BLI_condition_wait(&oglrender->task_condition,
				                   &oglrender->task_mutex);
			}
			BLI_mutex_unlock(&oglrender->task_mutex);
		}
		BLI_task_pool_work_and_wait(oglrender->task_pool);
		BLI_task_pool_free(oglrender->task_pool);
		/* Depending on various things we might or might not use global scheduler. */
		if (oglrender->task_scheduler != NULL) {
			BLI_task_scheduler_free(oglrender->task_scheduler);
		}
		BLI_spin_end(&oglrender->reports_lock);
	}
	BLI_mutex_end(&oglrender->task_mutex);
	BLI_condition_end(&oglrender->task_condition);

#ifdef DEBUG_TIME
	printf("Total render time: %f\n", PIL_check_seconds_timer() - oglrender->time_start);
#endif

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
		Depsgraph *depsgraph = oglrender->depsgraph;
		scene->r.cfra = oglrender->cfrao;
		BKE_scene_graph_update_for_newframe(depsgraph, bmain);

		WM_event_remove_timer(oglrender->wm, oglrender->win, oglrender->timer);
	}

	WM_cursor_modal_restore(oglrender->win);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);

	DRW_opengl_context_enable();
	GPU_offscreen_free(oglrender->ofs);
	DRW_opengl_context_disable();

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

typedef struct WriteTaskData {
	RenderResult *rr;
	Scene tmp_scene;
} WriteTaskData;

static void write_result_func(TaskPool * __restrict pool,
                              void *task_data_v,
                              int UNUSED(thread_id))
{
	OGLRender *oglrender = (OGLRender *) BLI_task_pool_userdata(pool);
	WriteTaskData *task_data = (WriteTaskData *) task_data_v;
	Scene *scene = &task_data->tmp_scene;
	RenderResult *rr = task_data->rr;
	const bool is_movie = BKE_imtype_is_movie(scene->r.im_format.imtype);
	const int cfra = scene->r.cfra;
	bool ok;
	/* Don't attempt to write if we've got an error. */
	if (!oglrender->pool_ok) {
		RE_FreeRenderResult(rr);
		BLI_mutex_lock(&oglrender->task_mutex);
		oglrender->num_scheduled_frames--;
		BLI_condition_notify_all(&oglrender->task_condition);
		BLI_mutex_unlock(&oglrender->task_mutex);
		return;
	}
	/* Construct local thread0safe copy of reports structure which we can
	 * safely pass to the underlying functions.
	 */
	ReportList reports;
	BKE_reports_init(&reports, oglrender->reports->flag & ~RPT_PRINT);
	/* Do actual save logic here, depending on the file format.
	 *
	 * NOTE: We have to construct temporary scene with proper scene->r.cfra.
	 * This is because underlying calls do not use r.cfra but use scene
	 * for that.
	 */
	if (is_movie) {
		ok = RE_WriteRenderViewsMovie(&reports,
		                              rr,
		                              scene,
		                              &scene->r,
		                              oglrender->mh,
		                              oglrender->movie_ctx_arr,
		                              oglrender->totvideos,
		                              PRVRANGEON != 0);
	}
	else {
		/* TODO(sergey): We can in theory save some CPU ticks here because we
		 * calculate file name again here.
		 */
		char name[FILE_MAX];
		BKE_image_path_from_imformat(name,
		                             scene->r.pic,
		                             BKE_main_blendfile_path(oglrender->bmain),
		                             cfra,
		                             &scene->r.im_format,
		                             (scene->r.scemode & R_EXTENSION) != 0,
		                             true,
		                             NULL);

		BKE_render_result_stamp_info(scene, scene->camera, rr, false);
		ok = RE_WriteRenderViewsImage(NULL, rr, scene, true, name);
		if (!ok) {
			BKE_reportf(&reports,
			            RPT_ERROR,
			            "Write error: cannot save %s",
			            name);
		}
	}
	if (reports.list.first != NULL) {
		BLI_spin_lock(&oglrender->reports_lock);
		for (Report *report = reports.list.first;
		     report != NULL;
		     report = report->next)
		{
			BKE_report(oglrender->reports,
			           report->type,
			           report->message);
		}
		BLI_spin_unlock(&oglrender->reports_lock);
	}
	if (!ok) {
		oglrender->pool_ok = false;
	}
	RE_FreeRenderResult(rr);
	BLI_mutex_lock(&oglrender->task_mutex);
	oglrender->num_scheduled_frames--;
	BLI_condition_notify_all(&oglrender->task_condition);
	BLI_mutex_unlock(&oglrender->task_mutex);
}

static bool schedule_write_result(OGLRender *oglrender, RenderResult *rr)
{
	if (!oglrender->pool_ok) {
		RE_FreeRenderResult(rr);
		return false;
	}
	Scene *scene = oglrender->scene;
	WriteTaskData *task_data = MEM_mallocN(sizeof(WriteTaskData), "write task data");
	task_data->rr = rr;
	task_data->tmp_scene = *scene;
	BLI_mutex_lock(&oglrender->task_mutex);
	oglrender->num_scheduled_frames++;
	if (oglrender->num_scheduled_frames > MAX_SCHEDULED_FRAMES) {
		BLI_condition_wait(&oglrender->task_condition,
		                   &oglrender->task_mutex);
	}
	BLI_mutex_unlock(&oglrender->task_mutex);
	BLI_task_pool_push(oglrender->task_pool,
	                   write_result_func,
	                   task_data,
	                   true,
	                   TASK_PRIORITY_LOW);
	return true;
}

static bool screen_opengl_render_anim_step(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	OGLRender *oglrender = op->customdata;
	Scene *scene = oglrender->scene;
	Depsgraph *depsgraph = oglrender->depsgraph;
	char name[FILE_MAX];
	bool ok = false;
	const bool view_context = (oglrender->v3d != NULL);
	bool is_movie;
	RenderResult *rr;

	/* go to next frame */
	if (CFRA < oglrender->nfra)
		CFRA++;
	while (CFRA < oglrender->nfra) {
		BKE_scene_graph_update_for_newframe(depsgraph, bmain);
		CFRA++;
	}

	is_movie = BKE_imtype_is_movie(scene->r.im_format.imtype);

	if (!is_movie) {
		BKE_image_path_from_imformat(
		        name, scene->r.pic, BKE_main_blendfile_path(oglrender->bmain), scene->r.cfra,
		        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true, NULL);

		if ((scene->r.mode & R_NO_OVERWRITE) && BLI_exists(name)) {
			BLI_spin_lock(&oglrender->reports_lock);
			BKE_reportf(op->reports, RPT_INFO, "Skipping existing frame \"%s\"", name);
			BLI_spin_unlock(&oglrender->reports_lock);
			ok = true;
			goto finally;
		}
	}

	WM_cursor_time(oglrender->win, scene->r.cfra);

	BKE_scene_graph_update_for_newframe(depsgraph, bmain);

	if (view_context) {
		if (oglrender->rv3d->persp == RV3D_CAMOB && oglrender->v3d->camera && oglrender->v3d->scenelock) {
			/* since BKE_scene_graph_update_for_newframe() is used rather
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
	screen_opengl_render_apply(C, oglrender);

	/* save to disk */
	rr = RE_AcquireResultRead(oglrender->re);
	RenderResult *new_rr = RE_DuplicateRenderResult(rr);
	RE_ReleaseResult(oglrender->re);

	ok = schedule_write_result(oglrender, new_rr);

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
			oglrender->pool_ok = false;  /* Flag pool for cancel. */
			screen_opengl_render_end(C, op->customdata);
			return OPERATOR_FINISHED;
		case TIMER:
			/* render frame? */
			if (oglrender->timer == event->customdata)
				break;
			ATTR_FALLTHROUGH;
		default:
			/* nothing to do */
			return OPERATOR_RUNNING_MODAL;
	}

	/* run first because screen_opengl_render_anim_step can free oglrender */
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);

	if (anim == 0) {
		screen_opengl_render_apply(C, op->customdata);
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
		screen_opengl_render_apply(C, op->customdata);
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
//	ED_update_for_newframe(C);
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
