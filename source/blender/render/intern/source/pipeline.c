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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/pipeline.c
 *  \ingroup render
 */

#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_threads.h"
#include "BLI_rand.h"
#include "BLI_callbacks.h"

#include "BLF_translation.h"

#include "BKE_animsys.h"  /* <------ should this be here?, needed for sequencer update */
#include "BKE_camera.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_writeavi.h"  /* <------ should be replaced once with generic movie module */

#include "PIL_time.h"
#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#ifdef WITH_FREESTYLE
#  include "FRS_freestyle.h"
#endif

/* internal */
#include "render_result.h"
#include "render_types.h"
#include "renderpipeline.h"
#include "renderdatabase.h"
#include "rendercore.h"
#include "initrender.h"
#include "pixelblending.h"
#include "zbuf.h"

/* render flow
 *
 * 1) Initialize state
 * - state data, tables
 * - movie/image file init
 * - everything that doesn't change during animation
 *
 * 2) Initialize data
 * - camera, world, matrices
 * - make render verts, faces, halos, strands
 * - everything can change per frame/field
 *
 * 3) Render Processor
 * - multiple layers
 * - tiles, rect, baking
 * - layers/tiles optionally to disk or directly in Render Result
 *
 * 4) Composite Render Result
 * - also read external files etc
 *
 * 5) Image Files
 * - save file or append in movie
 *
 */


/* ********* globals ******** */

/* here we store all renders */
static struct {
	ListBase renderlist;
} RenderGlobal = {{NULL, NULL}}; 

/* hardcopy of current render, used while rendering for speed */
Render R;

/* ********* alloc and free ******** */

static int do_write_image_or_movie(Render *re, Main *bmain, Scene *scene, bMovieHandle *mh, const char *name_override);

static volatile int g_break = 0;
static int thread_break(void *UNUSED(arg))
{
	return g_break;
}

/* default callbacks, set in each new render */
static void result_nothing(void *UNUSED(arg), RenderResult *UNUSED(rr)) {}
static void result_rcti_nothing(void *UNUSED(arg), RenderResult *UNUSED(rr), volatile struct rcti *UNUSED(rect)) {}
static void current_scene_nothing(void *UNUSED(arg), Scene *UNUSED(scene)) {}
static void stats_nothing(void *UNUSED(arg), RenderStats *UNUSED(rs)) {}
static void float_nothing(void *UNUSED(arg), float UNUSED(val)) {}
static int default_break(void *UNUSED(arg)) { return G.is_break == true; }

static void stats_background(void *UNUSED(arg), RenderStats *rs)
{
	uintptr_t mem_in_use, mmap_in_use, peak_memory;
	float megs_used_memory, mmap_used_memory, megs_peak_memory;

	mem_in_use = MEM_get_memory_in_use();
	mmap_in_use = MEM_get_mapped_memory_in_use();
	peak_memory = MEM_get_peak_memory();

	megs_used_memory = (mem_in_use - mmap_in_use) / (1024.0 * 1024.0);
	mmap_used_memory = (mmap_in_use) / (1024.0 * 1024.0);
	megs_peak_memory = (peak_memory) / (1024.0 * 1024.0);

	fprintf(stdout, IFACE_("Fra:%d Mem:%.2fM (%.2fM, Peak %.2fM) "), rs->cfra,
	        megs_used_memory, mmap_used_memory, megs_peak_memory);

	if (rs->curfield)
		fprintf(stdout, IFACE_("Field %d "), rs->curfield);
	if (rs->curblur)
		fprintf(stdout, IFACE_("Blur %d "), rs->curblur);

	if (rs->infostr) {
		fprintf(stdout, "| %s", rs->infostr);
	}
	else {
		if (rs->tothalo)
			fprintf(stdout, IFACE_("Sce: %s Ve:%d Fa:%d Ha:%d La:%d"),
			        rs->scene_name, rs->totvert, rs->totface, rs->tothalo, rs->totlamp);
		else
			fprintf(stdout, IFACE_("Sce: %s Ve:%d Fa:%d La:%d"), rs->scene_name, rs->totvert, rs->totface, rs->totlamp);
	}

	BLI_callback_exec(G.main, NULL, BLI_CB_EVT_RENDER_STATS);

	fputc('\n', stdout);
	fflush(stdout);
}

void RE_FreeRenderResult(RenderResult *res)
{
	render_result_free(res);
}

float *RE_RenderLayerGetPass(RenderLayer *rl, int passtype)
{
	RenderPass *rpass;
	
	for (rpass = rl->passes.first; rpass; rpass = rpass->next)
		if (rpass->passtype == passtype)
			return rpass->rect;
	return NULL;
}

RenderLayer *RE_GetRenderLayer(RenderResult *rr, const char *name)
{
	if (rr == NULL) {
		return NULL;
	}
	else {
		return BLI_findstring(&rr->layers, name, offsetof(RenderLayer, name));
	}
}

RenderResult *RE_MultilayerConvert(void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty)
{
	return render_result_new_from_exr(exrhandle, colorspace, predivide, rectx, recty);
}

RenderLayer *render_get_active_layer(Render *re, RenderResult *rr)
{
	RenderLayer *rl = BLI_findlink(&rr->layers, re->r.actlay);
	
	if (rl)
		return rl;
	else 
		return rr->layers.first;
}

static int render_scene_needs_vector(Render *re)
{
	SceneRenderLayer *srl;
	
	for (srl = re->r.layers.first; srl; srl = srl->next)
		if (!(srl->layflag & SCE_LAY_DISABLE))
			if (srl->passflag & SCE_PASS_VECTOR)
				return 1;

	return 0;
}

static bool render_scene_has_layers_to_render(Scene *scene)
{
	SceneRenderLayer *srl;
	for (srl = scene->r.layers.first; srl; srl = srl->next) {
		if (!(srl->layflag & SCE_LAY_DISABLE)) {
			return true;
		}
	}
	return false;
}

/* *************************************************** */

Render *RE_GetRender(const char *name)
{
	Render *re;

	/* search for existing renders */
	for (re = RenderGlobal.renderlist.first; re; re = re->next)
		if (strncmp(re->name, name, RE_MAXNAME) == 0)
			break;

	return re;
}


/* if you want to know exactly what has been done */
RenderResult *RE_AcquireResultRead(Render *re)
{
	if (re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);
		return re->result;
	}

	return NULL;
}

RenderResult *RE_AcquireResultWrite(Render *re)
{
	if (re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		return re->result;
	}

	return NULL;
}

void RE_SwapResult(Render *re, RenderResult **rr)
{
	/* for keeping render buffers */
	if (re) {
		SWAP(RenderResult *, re->result, *rr);
	}
}


void RE_ReleaseResult(Render *re)
{
	if (re)
		BLI_rw_mutex_unlock(&re->resultmutex);
}

/* displist.c util.... */
Scene *RE_GetScene(Render *re)
{
	if (re)
		return re->scene;
	return NULL;
}

/* fill provided result struct with what's currently active or done */
void RE_AcquireResultImage(Render *re, RenderResult *rr)
{
	memset(rr, 0, sizeof(RenderResult));

	if (re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);

		if (re->result) {
			RenderLayer *rl;
			
			rr->rectx = re->result->rectx;
			rr->recty = re->result->recty;
			
			rr->rectf = re->result->rectf;
			rr->rectz = re->result->rectz;
			rr->rect32 = re->result->rect32;
			
			/* active layer */
			rl = render_get_active_layer(re, re->result);

			if (rl) {
				if (rr->rectf == NULL)
					rr->rectf = rl->rectf;
				if (rr->rectz == NULL)
					rr->rectz = RE_RenderLayerGetPass(rl, SCE_PASS_Z);
			}

			rr->have_combined = (re->result->rectf != NULL);
			rr->layers = re->result->layers;
			
			rr->xof = re->disprect.xmin;
			rr->yof = re->disprect.ymin;
		}
	}
}

void RE_ReleaseResultImage(Render *re)
{
	if (re)
		BLI_rw_mutex_unlock(&re->resultmutex);
}

/* caller is responsible for allocating rect in correct size! */
void RE_ResultGet32(Render *re, unsigned int *rect)
{
	RenderResult rres;
	
	RE_AcquireResultImage(re, &rres);
	render_result_rect_get_pixels(&rres, rect, re->rectx, re->recty, &re->scene->view_settings, &re->scene->display_settings);
	RE_ReleaseResultImage(re);
}

/* caller is responsible for allocating rect in correct size! */
/* Only for acquired results, for lock */
void RE_AcquiredResultGet32(Render *re, RenderResult *result, unsigned int *rect)
{
	render_result_rect_get_pixels(result, rect, re->rectx, re->recty, &re->scene->view_settings, &re->scene->display_settings);
}

RenderStats *RE_GetStats(Render *re)
{
	return &re->i;
}

Render *RE_NewRender(const char *name)
{
	Render *re;

	/* only one render per name exists */
	re = RE_GetRender(name);
	if (re == NULL) {
		
		/* new render data struct */
		re = MEM_callocN(sizeof(Render), "new render");
		BLI_addtail(&RenderGlobal.renderlist, re);
		BLI_strncpy(re->name, name, RE_MAXNAME);
		BLI_rw_mutex_init(&re->resultmutex);
		re->eval_ctx = MEM_callocN(sizeof(EvaluationContext), "re->eval_ctx");
		re->eval_ctx->mode = DAG_EVAL_RENDER;
	}
	
	RE_InitRenderCB(re);

	/* init some variables */
	re->ycor = 1.0f;
	
	return re;
}

/* called for new renders and when finishing rendering so
 * we always have valid callbacks on a render */
void RE_InitRenderCB(Render *re)
{
	/* set default empty callbacks */
	re->display_init = result_nothing;
	re->display_clear = result_nothing;
	re->display_update = result_rcti_nothing;
	re->current_scene_update = current_scene_nothing;
	re->progress = float_nothing;
	re->test_break = default_break;
	if (G.background)
		re->stats_draw = stats_background;
	else
		re->stats_draw = stats_nothing;
	/* clear callback handles */
	re->dih = re->dch = re->duh = re->sdh = re->prh = re->tbh = NULL;
}

/* only call this while you know it will remove the link too */
void RE_FreeRender(Render *re)
{
	if (re->engine)
		RE_engine_free(re->engine);

	BLI_rw_mutex_end(&re->resultmutex);

	BLI_freelistN(&re->r.layers);
	
	/* main dbase can already be invalid now, some database-free code checks it */
	re->main = NULL;
	re->scene = NULL;
	
	RE_Database_Free(re);	/* view render can still have full database */
	free_sample_tables(re);
	
	render_result_free(re->result);
	render_result_free(re->pushedresult);
	
	BLI_remlink(&RenderGlobal.renderlist, re);
	MEM_freeN(re->eval_ctx);
	MEM_freeN(re);
}

/* exit blender */
void RE_FreeAllRender(void)
{
	while (RenderGlobal.renderlist.first) {
		RE_FreeRender(RenderGlobal.renderlist.first);
	}

#ifdef WITH_FREESTYLE
	/* finalize Freestyle */
	FRS_exit();
#endif
}

/* on file load, free all re */
void RE_FreeAllRenderResults(void)
{
	Render *re;

	for (re = RenderGlobal.renderlist.first; re; re = re->next) {
		render_result_free(re->result);
		render_result_free(re->pushedresult);

		re->result = NULL;
		re->pushedresult = NULL;
	}
}

void RE_FreePersistentData(void)
{
	Render *re;

	/* render engines can be kept around for quick re-render, this clears all */
	for (re = RenderGlobal.renderlist.first; re; re = re->next) {
		if (re->engine) {
			/* if engine is currently rendering, just tag it to be freed when render is finished */
			if (!(re->engine->flag & RE_ENGINE_RENDERING))
				RE_engine_free(re->engine);

			re->engine = NULL;
		}
	}
}

/* ********* initialize state ******** */

/* clear full sample and tile flags if needed */
static int check_mode_full_sample(RenderData *rd)
{
	int scemode = rd->scemode;

	if (!STREQ(rd->engine, RE_engine_id_BLENDER_RENDER) &&
	    !STREQ(rd->engine, RE_engine_id_BLENDER_GAME))
	{
		scemode &= ~R_FULL_SAMPLE;
	}

	if ((rd->mode & R_OSA) == 0)
		scemode &= ~R_FULL_SAMPLE;

#ifdef WITH_OPENEXR
	if (scemode & R_FULL_SAMPLE)
		scemode |= R_EXR_TILE_FILE;   /* enable automatic */

	/* Until use_border is made compatible with save_buffers/full_sample, render without the later instead of not rendering at all.*/
	if (rd->mode & R_BORDER) {
		scemode &= ~(R_EXR_TILE_FILE | R_FULL_SAMPLE);
	}

#else
	/* can't do this without openexr support */
	scemode &= ~(R_EXR_TILE_FILE | R_FULL_SAMPLE);
#endif

	return scemode;
}

static void re_init_resolution(Render *re, Render *source,
                               int winx, int winy, rcti *disprect)
{
	re->winx = winx;
	re->winy = winy;
	if (source && (source->r.mode & R_BORDER)) {
		/* eeh, doesn't seem original bordered disprect is storing anywhere
		 * after insertion on black happening in do_render_fields_blur_3d(),
		 * so for now simply re-calculate disprect using border from source
		 * renderer (sergey)
		 */

		re->disprect.xmin = source->r.border.xmin * winx;
		re->disprect.xmax = source->r.border.xmax * winx;

		re->disprect.ymin = source->r.border.ymin * winy;
		re->disprect.ymax = source->r.border.ymax * winy;

		re->rectx = BLI_rcti_size_x(&re->disprect);
		re->recty = BLI_rcti_size_y(&re->disprect);

		/* copy border itself, since it could be used by external engines */
		re->r.border = source->r.border;
	}
	else if (disprect) {
		re->disprect = *disprect;
		re->rectx = BLI_rcti_size_x(&re->disprect);
		re->recty = BLI_rcti_size_y(&re->disprect);
	}
	else {
		re->disprect.xmin = re->disprect.ymin = 0;
		re->disprect.xmax = winx;
		re->disprect.ymax = winy;
		re->rectx = winx;
		re->recty = winy;
	}

	/* we clip faces with a minimum of 2 pixel boundary outside of image border. see zbuf.c */
	re->clipcrop = 1.0f + 2.0f / (float)(re->winx > re->winy ? re->winy : re->winx);
}

/* what doesn't change during entire render sequence */
/* disprect is optional, if NULL it assumes full window render */
void RE_InitState(Render *re, Render *source, RenderData *rd,
                  SceneRenderLayer *srl,
                  int winx, int winy, rcti *disprect)
{
	bool had_freestyle = (re->r.mode & R_EDGE_FRS) != 0;

	re->ok = true;   /* maybe flag */
	
	re->i.starttime = PIL_check_seconds_timer();

	/* copy render data and render layers for thread safety */
	BLI_freelistN(&re->r.layers);
	re->r = *rd;
	BLI_duplicatelist(&re->r.layers, &rd->layers);

	if (source) {
		/* reuse border flags from source renderer */
		re->r.mode &= ~(R_BORDER | R_CROP);
		re->r.mode |= source->r.mode & (R_BORDER | R_CROP);

		/* dimensions shall be shared between all renderers */
		re->r.xsch = source->r.xsch;
		re->r.ysch = source->r.ysch;
		re->r.size = source->r.size;
	}

	re_init_resolution(re, source, winx, winy, disprect);

	if (re->rectx < 1 || re->recty < 1 || (BKE_imtype_is_movie(rd->im_format.imtype) &&
	                                       (re->rectx < 16 || re->recty < 16) ))
	{
		BKE_report(re->reports, RPT_ERROR, "Image too small");
		re->ok = 0;
		return;
	}

	re->r.scemode = check_mode_full_sample(&re->r);
	
	/* fullsample wants uniform osa levels */
	if (source && (re->r.scemode & R_FULL_SAMPLE)) {
		/* but, if source has no full sample we disable it */
		if ((source->r.scemode & R_FULL_SAMPLE) == 0)
			re->r.scemode &= ~R_FULL_SAMPLE;
		else
			re->r.osa = re->osa = source->osa;
	}
	else {
		/* check state variables, osa? */
		if (re->r.mode & (R_OSA)) {
			re->osa = re->r.osa;
			if (re->osa > 16) re->osa = 16;
		}
		else re->osa = 0;
	}
	
	if (srl) {
		int index = BLI_findindex(&rd->layers, srl);
		if (index != -1) {
			re->r.actlay = index;
			re->r.scemode |= R_SINGLE_LAYER;
		}
	}
		
	/* always call, checks for gamma, gamma tables and jitter too */
	make_sample_tables(re);
	
	/* if preview render, we try to keep old result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if (re->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW)) {
		if (had_freestyle || (re->r.mode & R_EDGE_FRS)) {
			/* freestyle manipulates render layers so always have to free */
			render_result_free(re->result);
			re->result = NULL;
		}
		else if (re->result) {
			SceneRenderLayer *actsrl = BLI_findlink(&re->r.layers, re->r.actlay);
			RenderLayer *rl;
			bool have_layer = false;

			for (rl = re->result->layers.first; rl; rl = rl->next)
				if (STREQ(rl->name, actsrl->name))
					have_layer = true;

			if (re->result->rectx == re->rectx && re->result->recty == re->recty &&
			    have_layer)
			{
				/* keep render result, this avoids flickering black tiles
				 * when the preview changes */
			}
			else {
				/* free because resolution changed */
				render_result_free(re->result);
				re->result = NULL;
			}
		}
	}
	else {
		
		/* make empty render result, so display callbacks can initialize */
		render_result_free(re->result);
		re->result = MEM_callocN(sizeof(RenderResult), "new render result");
		re->result->rectx = re->rectx;
		re->result->recty = re->recty;
	}
	
	if (re->r.scemode & R_VIEWPORT_PREVIEW)
		re->eval_ctx->mode = DAG_EVAL_PREVIEW;
	else
		re->eval_ctx->mode = DAG_EVAL_RENDER;
	
	/* ensure renderdatabase can use part settings correct */
	RE_parts_clamp(re);

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	re->mblur_offs = re->field_offs = 0.f;
	
	RE_init_threadcount(re);
}

static void render_result_rescale(Render *re)
{
	RenderResult *result = re->result;
	int x, y;
	float scale_x, scale_y;
	float *src_rectf;

	src_rectf = result->rectf;
	if (src_rectf == NULL) {
		RenderLayer *rl = render_get_active_layer(re, re->result);
		if (rl != NULL) {
			src_rectf = rl->rectf;
		}
	}

	if (src_rectf != NULL) {
		float *dst_rectf = NULL;
		re->result = render_result_new(re,
		                               &re->disprect,
		                               0,
		                               RR_USE_MEM,
		                               RR_ALL_LAYERS);

		if (re->result != NULL) {
			dst_rectf = re->result->rectf;
			if (dst_rectf == NULL) {
				RenderLayer *rl;
				rl = render_get_active_layer(re, re->result);
				if (rl != NULL) {
					dst_rectf = rl->rectf;
				}
			}

			scale_x = (float) result->rectx / re->result->rectx;
			scale_y = (float) result->recty / re->result->recty;
			for (x = 0; x < re->result->rectx; ++x) {
				for (y = 0; y < re->result->recty; ++y) {
					int src_x = x * scale_x,
					    src_y = y * scale_y;
					int dst_index = y * re->result->rectx + x,
					    src_index = src_y * result->rectx + src_x;
					copy_v4_v4(dst_rectf + dst_index * 4,
					           src_rectf + src_index * 4);
				}
			}
		}
	}

	render_result_free(result);
}

void RE_ChangeResolution(Render *re, int winx, int winy, rcti *disprect)
{
	re_init_resolution(re, NULL, winx, winy, disprect);
	RE_parts_clamp(re);

	if (re->result) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_rescale(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}
}

/* TODO(sergey): This is a bit hackish, used to temporary disable freestyle when
 * doing viewport render. Needs some better integration of BI viewport rendering
 * into the pipeline.
 */
void RE_ChangeModeFlag(Render *re, int flag, bool clear)
{
	if (clear) {
		re->r.mode &= ~flag;
	}
	else {
		re->r.mode |= flag;
	}
}

/* update some variables that can be animated, and otherwise wouldn't be due to
 * RenderData getting copied once at the start of animation render */
void render_update_anim_renderdata(Render *re, RenderData *rd)
{
	/* filter */
	re->r.gauss = rd->gauss;

	/* motion blur */
	re->r.mblur_samples = rd->mblur_samples;
	re->r.blurfac = rd->blurfac;

	/* freestyle */
	re->r.line_thickness_mode = rd->line_thickness_mode;
	re->r.unit_line_thickness = rd->unit_line_thickness;

	/* render layers */
	BLI_freelistN(&re->r.layers);
	BLI_duplicatelist(&re->r.layers, &rd->layers);
}

void RE_SetWindow(Render *re, rctf *viewplane, float clipsta, float clipend)
{
	/* re->ok flag? */
	
	re->viewplane = *viewplane;
	re->clipsta = clipsta;
	re->clipend = clipend;
	re->r.mode &= ~R_ORTHO;

	perspective_m4(re->winmat,
	               re->viewplane.xmin, re->viewplane.xmax,
	               re->viewplane.ymin, re->viewplane.ymax, re->clipsta, re->clipend);
	
}

void RE_SetOrtho(Render *re, rctf *viewplane, float clipsta, float clipend)
{
	/* re->ok flag? */
	
	re->viewplane = *viewplane;
	re->clipsta = clipsta;
	re->clipend = clipend;
	re->r.mode |= R_ORTHO;

	orthographic_m4(re->winmat,
	                re->viewplane.xmin, re->viewplane.xmax,
	                re->viewplane.ymin, re->viewplane.ymax, re->clipsta, re->clipend);
}

void RE_SetView(Render *re, float mat[4][4])
{
	/* re->ok flag? */
	copy_m4_m4(re->viewmat, mat);
	invert_m4_m4(re->viewinv, re->viewmat);
}

void RE_GetViewPlane(Render *re, rctf *viewplane, rcti *disprect)
{
	*viewplane = re->viewplane;
	
	/* make disprect zero when no border render, is needed to detect changes in 3d view render */
	if (re->r.mode & R_BORDER)
		*disprect = re->disprect;
	else
		BLI_rcti_init(disprect, 0, 0, 0, 0);
}

void RE_GetView(Render *re, float mat[4][4])
{
	copy_m4_m4(mat, re->viewmat);
}

/* image and movie output has to move to either imbuf or kernel */
void RE_display_init_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr))
{
	re->display_init = f;
	re->dih = handle;
}
void RE_display_clear_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr))
{
	re->display_clear = f;
	re->dch = handle;
}
void RE_display_update_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr, volatile rcti *rect))
{
	re->display_update = f;
	re->duh = handle;
}
void RE_current_scene_update_cb(Render *re, void *handle, void (*f)(void *handle, Scene *scene))
{
	re->current_scene_update = f;
	re->suh = handle;
}
void RE_stats_draw_cb(Render *re, void *handle, void (*f)(void *handle, RenderStats *rs))
{
	re->stats_draw = f;
	re->sdh = handle;
}
void RE_progress_cb(Render *re, void *handle, void (*f)(void *handle, float))
{
	re->progress = f;
	re->prh = handle;
}

void RE_draw_lock_cb(Render *re, void *handle, void (*f)(void *handle, int i))
{
	re->draw_lock = f;
	re->dlh = handle;
}

void RE_test_break_cb(Render *re, void *handle, int (*f)(void *handle))
{
	re->test_break = f;
	re->tbh = handle;
}


/* ********* add object data (later) ******** */

/* object is considered fully prepared on correct time etc */
/* includes lights */
#if 0
void RE_AddObject(Render *UNUSED(re), Object *UNUSED(ob))
{
	
}
#endif

/* *************************************** */

static int render_display_update_enabled(Render *re)
{
	/* don't show preprocess for previewrender sss */
	if (re->sss_points)
		return !(re->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW));
	else
		return 1;
}

/* the main thread call, renders an entire part */
static void *do_part_thread(void *pa_v)
{
	RenderPart *pa = pa_v;

	pa->status = PART_STATUS_IN_PROGRESS;

	/* need to return nicely all parts on esc */
	if (R.test_break(R.tbh) == 0) {
		
		if (!R.sss_points && (R.r.scemode & R_FULL_SAMPLE))
			pa->result = render_result_new_full_sample(&R, &pa->fullresult, &pa->disprect, pa->crop, RR_USE_MEM);
		else
			pa->result = render_result_new(&R, &pa->disprect, pa->crop, RR_USE_MEM, RR_ALL_LAYERS);

		/* Copy EXR tile settings, so pipeline knows whether this is a result
		 * for Save Buffers enabled rendering.
		 *
		 * TODO(sergey): This actually duplicates logic with external engine, so
		 * worth looking into more generic solution.
		 */
		pa->result->do_exr_tile = R.result->do_exr_tile;

		if (R.sss_points)
			zbufshade_sss_tile(pa);
		else if (R.osa)
			zbufshadeDA_tile(pa);
		else
			zbufshade_tile(pa);
		
		/* we do actually write pixels, but don't allocate/deallocate anything,
		 * so it is safe with other threads reading at the same time */
		BLI_rw_mutex_lock(&R.resultmutex, THREAD_LOCK_READ);
		
		/* merge too on break! */
		if (R.result->do_exr_tile) {
			render_result_exr_file_merge(R.result, pa->result);
		}
		else if (render_display_update_enabled(&R)) {
			/* on break, don't merge in result for preview renders, looks nicer */
			if (R.test_break(R.tbh) && (R.r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))) {
				/* pass */
			}
			else {
				render_result_merge(R.result, pa->result);
			}
		}
		
		BLI_rw_mutex_unlock(&R.resultmutex);
	}
	
	pa->status = PART_STATUS_READY;
	
	return NULL;
}

/* calculus for how much 1 pixel rendered should rotate the 3d geometry */
/* is not that simple, needs to be corrected for errors of larger viewplane sizes */
/* called in initrender.c, RE_parts_init() and convertblender.c, for speedvectors */
float panorama_pixel_rot(Render *re)
{
	float psize, phi, xfac;
	float borderfac = (float)BLI_rcti_size_x(&re->disprect) / (float)re->winx;
	int xparts = (re->rectx + re->partx - 1) / re->partx;
	
	/* size of 1 pixel mapped to viewplane coords */
	psize = BLI_rctf_size_x(&re->viewplane) / (float)re->winx;
	/* angle of a pixel */
	phi = atan(psize / re->clipsta);
	
	/* correction factor for viewplane shifting, first calculate how much the viewplane angle is */
	xfac = borderfac * BLI_rctf_size_x(&re->viewplane) / (float)xparts;
	xfac = atan(0.5f * xfac / re->clipsta);
	/* and how much the same viewplane angle is wrapped */
	psize = 0.5f * phi * ((float)re->partx);
	
	/* the ratio applied to final per-pixel angle */
	phi *= xfac / psize;
	
	return phi;
}

/* for panorama, we render per Y slice, and update
 * camera parameters when we go the next slice */
static bool find_next_pano_slice(Render *re, int *slice, int *minx, rctf *viewplane)
{
	RenderPart *pa, *best = NULL;
	bool found = false;
	
	*minx = re->winx;
	
	if (!(re->r.mode & R_PANORAMA)) {
		/* for regular render, just one 'slice' */
		found = (*slice == 0);
		(*slice)++;
		return found;
	}

	/* most left part of the non-rendering parts */
	for (pa = re->parts.first; pa; pa = pa->next) {
		if (pa->status == PART_STATUS_NONE && pa->nr == 0) {
			if (pa->disprect.xmin < *minx) {
				found = true;
				best = pa;
				*minx = pa->disprect.xmin;
			}
		}
	}
	
	if (best) {
		float phi = panorama_pixel_rot(re);

		R.panodxp = (re->winx - (best->disprect.xmin + best->disprect.xmax) ) / 2;
		R.panodxv = (BLI_rctf_size_x(viewplane) * R.panodxp) / (float)(re->winx);

		/* shift viewplane */
		R.viewplane.xmin = viewplane->xmin + R.panodxv;
		R.viewplane.xmax = viewplane->xmax + R.panodxv;
		RE_SetWindow(re, &R.viewplane, R.clipsta, R.clipend);
		copy_m4_m4(R.winmat, re->winmat);
		
		/* rotate database according to part coordinates */
		project_renderdata(re, projectverto, 1, -R.panodxp * phi, 1);
		R.panosi = sinf(R.panodxp * phi);
		R.panoco = cosf(R.panodxp * phi);
	}
	
	(*slice)++;
	
	return found;
}

static RenderPart *find_next_part(Render *re, int minx)
{
	RenderPart *pa, *best = NULL;

	/* long long int's needed because of overflow [#24414] */
	long long int centx = re->winx / 2, centy = re->winy / 2, tot = 1;
	long long int mindist = (long long int)re->winx * (long long int)re->winy;
	
	/* find center of rendered parts, image center counts for 1 too */
	for (pa = re->parts.first; pa; pa = pa->next) {
		if (pa->status == PART_STATUS_READY) {
			centx += BLI_rcti_cent_x(&pa->disprect);
			centy += BLI_rcti_cent_y(&pa->disprect);
			tot++;
		}
	}
	centx /= tot;
	centy /= tot;
	
	/* closest of the non-rendering parts */
	for (pa = re->parts.first; pa; pa = pa->next) {
		if (pa->status == PART_STATUS_NONE && pa->nr == 0) {
			long long int distx = centx - BLI_rcti_cent_x(&pa->disprect);
			long long int disty = centy - BLI_rcti_cent_y(&pa->disprect);
			distx = (long long int)sqrt(distx * distx + disty * disty);
			if (distx < mindist) {
				if (re->r.mode & R_PANORAMA) {
					if (pa->disprect.xmin == minx) {
						best = pa;
						mindist = distx;
					}
				}
				else {
					best = pa;
					mindist = distx;
				}
			}
		}
	}
	return best;
}

static void print_part_stats(Render *re, RenderPart *pa)
{
	char str[64];
	
	BLI_snprintf(str, sizeof(str), IFACE_("%s, Part %d-%d"), re->scene->id.name + 2, pa->nr, re->i.totpart);
	re->i.infostr = str;
	re->stats_draw(re->sdh, &re->i);
	re->i.infostr = NULL;
}

typedef struct RenderThread {
	ThreadQueue *workqueue;
	ThreadQueue *donequeue;
	
	int number;

	void (*display_update)(void *handle, RenderResult *rr, volatile rcti *rect);
	void *duh;
} RenderThread;

static void *do_render_thread(void *thread_v)
{
	RenderThread *thread = thread_v;
	RenderPart *pa;
	
	while ((pa = BLI_thread_queue_pop(thread->workqueue))) {
		pa->thread = thread->number;
		do_part_thread(pa);

		if (thread->display_update) {
			thread->display_update(thread->duh, pa->result, NULL);
		}

		BLI_thread_queue_push(thread->donequeue, pa);
		
		if (R.test_break(R.tbh))
			break;
	}
	
	return NULL;
}

static void threaded_tile_processor(Render *re)
{
	RenderThread thread[BLENDER_MAX_THREADS];
	ThreadQueue *workqueue, *donequeue;
	ListBase threads;
	RenderPart *pa;
	rctf viewplane = re->viewplane;
	double lastdraw, elapsed, redrawtime = 1.0f;
	int totpart = 0, minx = 0, slice = 0, a, wait;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	/* first step; free the entire render result, make new, and/or prepare exr buffer saving */
	if (re->result == NULL || !(re->r.scemode & (R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))) {
		render_result_free(re->result);

		if (re->sss_points && render_display_update_enabled(re))
			re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);
		else if (re->r.scemode & R_FULL_SAMPLE)
			re->result = render_result_new_full_sample(re, &re->fullresult, &re->disprect, 0, RR_USE_EXR);
		else
			re->result = render_result_new(re, &re->disprect, 0,
				(re->r.scemode & R_EXR_TILE_FILE) ? RR_USE_EXR : RR_USE_MEM, RR_ALL_LAYERS);
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	if (re->result == NULL)
		return;

	/* warning; no return here without closing exr file */
	
	RE_parts_init(re, true);

	if (re->result->do_exr_tile)
		render_result_exr_file_begin(re);
	
	/* assuming no new data gets added to dbase... */
	R = *re;
	
	/* set threadsafe break */
	R.test_break = thread_break;
	
	/* create and fill work queue */
	workqueue = BLI_thread_queue_init();
	donequeue = BLI_thread_queue_init();
	
	/* for panorama we loop over slices */
	while (find_next_pano_slice(re, &slice, &minx, &viewplane)) {
		/* gather parts into queue */
		while ((pa = find_next_part(re, minx))) {
			pa->nr = totpart + 1; /* for nicest part, and for stats */
			totpart++;
			BLI_thread_queue_push(workqueue, pa);
		}
		
		BLI_thread_queue_nowait(workqueue);
		
		/* start all threads */
		BLI_init_threads(&threads, do_render_thread, re->r.threads);
		
		for (a = 0; a < re->r.threads; a++) {
			thread[a].workqueue = workqueue;
			thread[a].donequeue = donequeue;
			thread[a].number = a;

			if (render_display_update_enabled(re)) {
				thread[a].display_update = re->display_update;
				thread[a].duh = re->duh;
			}
			else {
				thread[a].display_update = NULL;
				thread[a].duh = NULL;
			}

			BLI_insert_thread(&threads, &thread[a]);
		}
		
		/* wait for results to come back */
		lastdraw = PIL_check_seconds_timer();
		
		while (1) {
			elapsed = PIL_check_seconds_timer() - lastdraw;
			wait = (redrawtime - elapsed)*1000;
			
			/* handle finished part */
			if ((pa=BLI_thread_queue_pop_timeout(donequeue, wait))) {
				if (pa->result) {
					print_part_stats(re, pa);
					
					render_result_free_list(&pa->fullresult, pa->result);
					pa->result = NULL;
					re->i.partsdone++;
					re->progress(re->prh, re->i.partsdone / (float)re->i.totpart);
				}
				
				totpart--;
			}
			
			/* check for render cancel */
			if ((g_break=re->test_break(re->tbh)))
				break;
			
			/* or done with parts */
			if (totpart == 0)
				break;
			
			/* redraw in progress parts */
			elapsed = PIL_check_seconds_timer() - lastdraw;
			if (elapsed > redrawtime) {
				if (render_display_update_enabled(re))
					for (pa = re->parts.first; pa; pa = pa->next)
						if ((pa->status == PART_STATUS_IN_PROGRESS) && pa->nr && pa->result)
							re->display_update(re->duh, pa->result, &pa->result->renrect);
				
				lastdraw = PIL_check_seconds_timer();
			}
		}
		
		BLI_end_threads(&threads);
		
		if ((g_break=re->test_break(re->tbh)))
			break;
	}

	if (g_break) {
		/* review the done queue and handle all the render parts,
		 * so no unfreed render result are lurking around
		 */
		BLI_thread_queue_nowait(donequeue);
		while ((pa = BLI_thread_queue_pop(donequeue))) {
			if (pa->result) {
				render_result_free_list(&pa->fullresult, pa->result);
				pa->result = NULL;
			}
		}
	}

	BLI_thread_queue_free(donequeue);
	BLI_thread_queue_free(workqueue);
	
	if (re->result->do_exr_tile) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_exr_file_end(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}

	if (re->r.scemode & R_EXR_CACHE_FILE) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_exr_file_cache_write(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}

	/* unset threadsafety */
	g_break = 0;
	
	RE_parts_free(re);
	re->viewplane = viewplane; /* restore viewplane, modified by pano render */
}

#ifdef WITH_FREESTYLE
static void add_freestyle(Render *re, int render);
static void free_all_freestyle_renders(void);
#endif

/* currently only called by preview renders and envmap */
void RE_TileProcessor(Render *re)
{
	threaded_tile_processor(re);
	
	re->i.lastframetime = PIL_check_seconds_timer() - re->i.starttime;
	re->stats_draw(re->sdh, &re->i);

#ifdef WITH_FREESTYLE
	/* Freestyle */
	if (re->r.mode & R_EDGE_FRS) {
		if (!re->test_break(re->tbh)) {
			add_freestyle(re, 1);
	
			free_all_freestyle_renders();
			
			re->i.lastframetime = PIL_check_seconds_timer() - re->i.starttime;
			re->stats_draw(re->sdh, &re->i);
		}
	}
#endif

}

/* ************  This part uses API, for rendering Blender scenes ********** */

static void do_render_3d(Render *re)
{
	int cfra_backup;

	re->current_scene_update(re->suh, re->scene);

	/* try external */
	if (RE_engine_render(re, 0))
		return;

	/* internal */
	RE_parts_clamp(re);
	
	/* add motion blur and fields offset to frames */
	cfra_backup = re->scene->r.cfra;

	BKE_scene_frame_set(re->scene, (double)re->scene->r.cfra + (double)re->mblur_offs + (double)re->field_offs);

	/* lock drawing in UI during data phase */
	if (re->draw_lock)
		re->draw_lock(re->dlh, 1);
	
	/* make render verts/faces/halos/lamps */
	if (render_scene_needs_vector(re)) {
		RE_Database_FromScene_Vectors(re, re->main, re->scene, re->lay);
	}
	else {
		RE_Database_FromScene(re, re->main, re->scene, re->lay, 1);
		RE_Database_Preprocess(re);
	}
	
	/* clear UI drawing locks */
	if (re->draw_lock)
		re->draw_lock(re->dlh, 0);
	
	threaded_tile_processor(re);
	
#ifdef WITH_FREESTYLE
	/* Freestyle */
	if (re->r.mode & R_EDGE_FRS)
		if (!re->test_break(re->tbh))
			add_freestyle(re, 1);
#endif
	
	/* do left-over 3d post effects (flares) */
	if (re->flag & R_HALO)
		if (!re->test_break(re->tbh))
			add_halo_flare(re);
		
	/* free all render verts etc */
	RE_Database_Free(re);
	
	re->scene->r.cfra = cfra_backup;
	re->scene->r.subframe = 0.f;
}

/* called by blur loop, accumulate RGBA key alpha */
static void addblur_rect_key(RenderResult *rr, float *rectf, float *rectf1, float blurfac)
{
	float mfac = 1.0f - blurfac;
	int a, b, stride = 4 * rr->rectx;
	int len = stride * sizeof(float);
	
	for (a = 0; a < rr->recty; a++) {
		if (blurfac == 1.0f) {
			memcpy(rectf, rectf1, len);
		}
		else {
			float *rf = rectf, *rf1 = rectf1;
			
			for (b = rr->rectx; b > 0; b--, rf += 4, rf1 += 4) {
				if (rf1[3] < 0.01f)
					rf[3] = mfac * rf[3];
				else if (rf[3] < 0.01f) {
					rf[0] = rf1[0];
					rf[1] = rf1[1];
					rf[2] = rf1[2];
					rf[3] = blurfac * rf1[3];
				}
				else {
					rf[0] = mfac * rf[0] + blurfac * rf1[0];
					rf[1] = mfac * rf[1] + blurfac * rf1[1];
					rf[2] = mfac * rf[2] + blurfac * rf1[2];
					rf[3] = mfac * rf[3] + blurfac * rf1[3];
				}
			}
		}
		rectf += stride;
		rectf1 += stride;
	}
}

/* called by blur loop, accumulate renderlayers */
static void addblur_rect(RenderResult *rr, float *rectf, float *rectf1, float blurfac, int channels)
{
	float mfac = 1.0f - blurfac;
	int a, b, stride = channels * rr->rectx;
	int len = stride * sizeof(float);
	
	for (a = 0; a < rr->recty; a++) {
		if (blurfac == 1.0f) {
			memcpy(rectf, rectf1, len);
		}
		else {
			float *rf = rectf, *rf1 = rectf1;
			
			for (b = rr->rectx * channels; b > 0; b--, rf++, rf1++) {
				rf[0] = mfac * rf[0] + blurfac * rf1[0];
			}
		}
		rectf += stride;
		rectf1 += stride;
	}
}


/* called by blur loop, accumulate renderlayers */
static void merge_renderresult_blur(RenderResult *rr, RenderResult *brr, float blurfac, bool key_alpha)
{
	RenderLayer *rl, *rl1;
	RenderPass *rpass, *rpass1;
	
	rl1 = brr->layers.first;
	for (rl = rr->layers.first; rl && rl1; rl = rl->next, rl1 = rl1->next) {
		
		/* combined */
		if (rl->rectf && rl1->rectf) {
			if (key_alpha)
				addblur_rect_key(rr, rl->rectf, rl1->rectf, blurfac);
			else
				addblur_rect(rr, rl->rectf, rl1->rectf, blurfac, 4);
		}
		
		/* passes are allocated in sync */
		rpass1 = rl1->passes.first;
		for (rpass = rl->passes.first; rpass && rpass1; rpass = rpass->next, rpass1 = rpass1->next) {
			addblur_rect(rr, rpass->rect, rpass1->rect, blurfac, rpass->channels);
		}
	}
}

/* main blur loop, can be called by fields too */
static void do_render_blur_3d(Render *re)
{
	RenderResult *rres;
	float blurfac;
	int blur = re->r.mblur_samples;
	
	/* create accumulation render result */
	rres = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);
	
	/* do the blur steps */
	while (blur--) {
		re->mblur_offs = re->r.blurfac * ((float)(re->r.mblur_samples - blur)) / (float)re->r.mblur_samples;
		
		re->i.curblur = re->r.mblur_samples - blur;    /* stats */
		
		do_render_3d(re);
		
		blurfac = 1.0f / (float)(re->r.mblur_samples - blur);
		
		merge_renderresult_blur(rres, re->result, blurfac, false);
		if (re->test_break(re->tbh)) break;
	}
	
	/* swap results */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	render_result_free(re->result);
	re->result = rres;
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	re->mblur_offs = 0.0f;
	re->i.curblur = 0;   /* stats */
	
	/* make sure motion blur changes get reset to current frame */
	if ((re->r.scemode & (R_NO_FRAME_UPDATE|R_BUTS_PREVIEW|R_VIEWPORT_PREVIEW))==0) {
		BKE_scene_update_for_newframe(re->eval_ctx, re->main, re->scene, re->lay);
	}
	
	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay = render_get_active_layer(re, re->result);
	re->display_update(re->duh, re->result, NULL);
}


/* function assumes rectf1 and rectf2 to be half size of rectf */
static void interleave_rect(RenderResult *rr, float *rectf, float *rectf1, float *rectf2, int channels)
{
	int a, stride = channels * rr->rectx;
	int len = stride * sizeof(float);
	
	for (a = 0; a < rr->recty; a += 2) {
		memcpy(rectf, rectf1, len);
		rectf += stride;
		rectf1 += stride;
		memcpy(rectf, rectf2, len);
		rectf += stride;
		rectf2 += stride;
	}
}

/* merge render results of 2 fields */
static void merge_renderresult_fields(RenderResult *rr, RenderResult *rr1, RenderResult *rr2)
{
	RenderLayer *rl, *rl1, *rl2;
	RenderPass *rpass, *rpass1, *rpass2;
	
	rl1 = rr1->layers.first;
	rl2 = rr2->layers.first;
	for (rl = rr->layers.first; rl && rl1 && rl2; rl = rl->next, rl1 = rl1->next, rl2 = rl2->next) {
		
		/* combined */
		if (rl->rectf && rl1->rectf && rl2->rectf)
			interleave_rect(rr, rl->rectf, rl1->rectf, rl2->rectf, 4);
		
		/* passes are allocated in sync */
		rpass1 = rl1->passes.first;
		rpass2 = rl2->passes.first;
		for (rpass = rl->passes.first;
		     rpass && rpass1 && rpass2;
		     rpass = rpass->next, rpass1 = rpass1->next, rpass2 = rpass2->next)
		{
			interleave_rect(rr, rpass->rect, rpass1->rect, rpass2->rect, rpass->channels);
		}
	}
}


/* interleaves 2 frames */
static void do_render_fields_3d(Render *re)
{
	Object *camera = RE_GetCamera(re);
	RenderResult *rr1, *rr2 = NULL;
	
	/* no render result was created, we can safely halve render y */
	re->winy /= 2;
	re->recty /= 2;
	re->disprect.ymin /= 2;
	re->disprect.ymax /= 2;
	
	re->i.curfield = 1;  /* stats */
	
	/* first field, we have to call camera routine for correct aspect and subpixel offset */
	RE_SetCamera(re, camera);
	if (re->r.mode & R_MBLUR && (re->r.scemode & R_FULL_SAMPLE) == 0)
		do_render_blur_3d(re);
	else
		do_render_3d(re);

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	rr1 = re->result;
	re->result = NULL;
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	/* second field */
	if (!re->test_break(re->tbh)) {
		
		re->i.curfield = 2;  /* stats */
		
		re->flag |= R_SEC_FIELD;
		if ((re->r.mode & R_FIELDSTILL) == 0) {
			re->field_offs = 0.5f;
		}
		RE_SetCamera(re, camera);
		if (re->r.mode & R_MBLUR && (re->r.scemode & R_FULL_SAMPLE) == 0)
			do_render_blur_3d(re);
		else
			do_render_3d(re);
		re->flag &= ~R_SEC_FIELD;
		
		re->field_offs = 0.0f;
		
		rr2 = re->result;
	}
	
	/* allocate original height new buffers */
	re->winy *= 2;
	re->recty *= 2;
	re->disprect.ymin *= 2;
	re->disprect.ymax *= 2;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);

	if (rr2) {
		if (re->r.mode & R_ODDFIELD)
			merge_renderresult_fields(re->result, rr2, rr1);
		else
			merge_renderresult_fields(re->result, rr1, rr2);
		
		render_result_free(rr2);
	}

	render_result_free(rr1);
	
	re->i.curfield = 0;  /* stats */
	
	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay = render_get_active_layer(re, re->result);

	BLI_rw_mutex_unlock(&re->resultmutex);

	re->display_update(re->duh, re->result, NULL);
}

/* make sure disprect is not affected by the render border */
static void render_result_disprect_to_full_resolution(Render *re)
{
	re->disprect.xmin = re->disprect.ymin = 0;
	re->disprect.xmax = re->winx;
	re->disprect.ymax = re->winy;
	re->rectx = re->winx;
	re->recty = re->winy;
}

/* main render routine, no compositing */
static void do_render_fields_blur_3d(Render *re)
{
	Object *camera = RE_GetCamera(re);
	/* also check for camera here */
	if (camera == NULL) {
		BKE_report(re->reports, RPT_ERROR, "Cannot render, no camera");
		G.is_break = true;
		return;
	}

	/* now use renderdata and camera to set viewplane */
	RE_SetCamera(re, camera);
	
	if (re->r.mode & R_FIELDS)
		do_render_fields_3d(re);
	else if (re->r.mode & R_MBLUR && (re->r.scemode & R_FULL_SAMPLE) == 0)
		do_render_blur_3d(re);
	else
		do_render_3d(re);
	
	/* when border render, check if we have to insert it in black */
	if (re->result) {
		if (re->r.mode & R_BORDER) {
			if ((re->r.mode & R_CROP) == 0) {
				RenderResult *rres;

				/* backup */
				const rcti orig_disprect = re->disprect;
				const int  orig_rectx = re->rectx, orig_recty = re->recty;
				
				BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

				/* sub-rect for merge call later on */
				re->result->tilerect = re->disprect;
				
				/* weak is: it chances disprect from border */
				render_result_disprect_to_full_resolution(re);
				
				rres = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);
				
				render_result_merge(rres, re->result);
				render_result_free(re->result);
				re->result = rres;
				
				/* weak... the display callback wants an active renderlayer pointer... */
				re->result->renlay = render_get_active_layer(re, re->result);
				
				BLI_rw_mutex_unlock(&re->resultmutex);
		
				re->display_init(re->dih, re->result);
				re->display_update(re->duh, re->result, NULL);

				/* restore the disprect from border */
				re->disprect = orig_disprect;
				re->rectx = orig_rectx;
				re->recty = orig_recty;
			}
			else {
				/* set offset (again) for use in compositor, disprect was manipulated. */
				re->result->xof = 0;
				re->result->yof = 0;
			}
		}
	}
}


/* within context of current Render *re, render another scene.
 * it uses current render image size and disprect, but doesn't execute composite
 */
static void render_scene(Render *re, Scene *sce, int cfra)
{
	Render *resc = RE_NewRender(sce->id.name);
	int winx = re->winx, winy = re->winy;
	
	sce->r.cfra = cfra;

	BKE_scene_camera_switch_update(sce);

	/* exception: scene uses own size (unfinished code) */
	if (0) {
		winx = (sce->r.size * sce->r.xsch) / 100;
		winy = (sce->r.size * sce->r.ysch) / 100;
	}
	
	/* initial setup */
	RE_InitState(resc, re, &sce->r, NULL, winx, winy, &re->disprect);

	/* We still want to use 'rendercache' setting from org (main) scene... */
	resc->r.scemode = (resc->r.scemode & ~R_EXR_CACHE_FILE) | (re->r.scemode & R_EXR_CACHE_FILE);

	/* still unsure entity this... */
	resc->main = re->main;
	resc->scene = sce;
	resc->lay = sce->lay;
	resc->scene_color_manage = BKE_scene_check_color_management_enabled(sce);
	
	/* ensure scene has depsgraph, base flags etc OK */
	BKE_scene_set_background(re->main, sce);

	/* copy callbacks */
	resc->display_update = re->display_update;
	resc->duh = re->duh;
	resc->test_break = re->test_break;
	resc->tbh = re->tbh;
	resc->stats_draw = re->stats_draw;
	resc->sdh = re->sdh;
	resc->current_scene_update = re->current_scene_update;
	resc->suh = re->suh;
	
	do_render_fields_blur_3d(resc);
}

/* helper call to detect if this scene needs a render, or if there's a any render layer to render */
static int composite_needs_render(Scene *sce, int this_scene)
{
	bNodeTree *ntree = sce->nodetree;
	bNode *node;
	
	if (ntree == NULL) return 1;
	if (sce->use_nodes == false) return 1;
	if ((sce->r.scemode & R_DOCOMP) == 0) return 1;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == CMP_NODE_R_LAYERS && (node->flag & NODE_MUTED) == 0)
			if (this_scene == 0 || node->id == NULL || node->id == &sce->id)
				return 1;
	}
	return 0;
}

static bool rlayer_node_uses_alpha(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock;

	for (sock = node->outputs.first; sock; sock = sock->next) {
		/* Weak! but how to make it better? */
		if (STREQ(sock->name, "Alpha") && nodeCountSocketLinks(ntree, sock) > 0)
			return true;
	}

	return false;
}

bool RE_allow_render_generic_object(Object *ob)
{
	/* override not showing object when duplis are used with particles */
	if (ob->transflag & OB_DUPLIPARTS) {
		/* pass */  /* let particle system(s) handle showing vs. not showing */
	}
	else if ((ob->transflag & OB_DUPLI) && !(ob->transflag & OB_DUPLIFRAMES)) {
		return false;
	}
	return true;
}

/* Issue here is that it's possible that object which is used by boolean,
 * array or shrinkwrap modifiers weren't displayed in the viewport before
 * rendering. This leads to situations when apply() of this modifiers
 * could not get ob->derivedFinal and modifiers are not being applied.
 *
 * This was worked around by direct call of get_derived_final() from those
 * modifiers, but such approach leads to write conflicts with threaded
 * update.
 *
 * Here we make sure derivedFinal will be calculated by update_for_newframe
 * function later in the pipeline and all the modifiers are applied
 * properly without hacks from their side.
 *                                                  - sergey -
 */
#define DEPSGRAPH_WORKAROUND_HACK

#ifdef DEPSGRAPH_WORKAROUND_HACK
static void tag_dependend_objects_for_render(Scene *scene, int renderlay)
{
	Scene *sce_iter;
	Base *base;
	for (SETLOOPER(scene, sce_iter, base)) {
		Object *object = base->object;

		if ((base->lay & renderlay) == 0) {
			continue;
		}

		if (object->type == OB_MESH) {
			if (RE_allow_render_generic_object(object)) {
				ModifierData *md;
				VirtualModifierData virtualModifierData;

				for (md = modifiers_getVirtualModifierList(object, &virtualModifierData);
				     md;
				     md = md->next)
				{
					if (!modifier_isEnabled(scene, md, eModifierMode_Render)) {
						continue;
					}

					if (md->type == eModifierType_Boolean) {
						BooleanModifierData *bmd = (BooleanModifierData *)md;
						if (bmd->object && bmd->object->type == OB_MESH) {
							DAG_id_tag_update(&bmd->object->id, OB_RECALC_DATA);
						}
					}
					else if (md->type == eModifierType_Array) {
						ArrayModifierData *amd = (ArrayModifierData *)md;
						if (amd->start_cap && amd->start_cap->type == OB_MESH) {
							DAG_id_tag_update(&amd->start_cap->id, OB_RECALC_DATA);
						}
						if (amd->end_cap && amd->end_cap->type == OB_MESH) {
							DAG_id_tag_update(&amd->end_cap->id, OB_RECALC_DATA);
						}
					}
					else if (md->type == eModifierType_Shrinkwrap) {
						ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
						if (smd->target  && smd->target->type == OB_MESH) {
							DAG_id_tag_update(&smd->target->id, OB_RECALC_DATA);
						}
					}
				}
			}
		}
	}
}
#endif

static void tag_scenes_for_render(Render *re)
{
	bNode *node;
	Scene *sce;
#ifdef DEPSGRAPH_WORKAROUND_HACK
	int renderlay = re->lay;
#endif
	
	for (sce = re->main->scene.first; sce; sce = sce->id.next) {
		sce->id.flag &= ~LIB_DOIT;
#ifdef DEPSGRAPH_WORKAROUND_HACK
		tag_dependend_objects_for_render(sce, renderlay);
#endif
	}
	
#ifdef WITH_FREESTYLE
	if (re->freestyle_bmain) {
		for (sce = re->freestyle_bmain->scene.first; sce; sce = sce->id.next) {
			sce->id.flag &= ~LIB_DOIT;
#ifdef DEPSGRAPH_WORKAROUND_HACK
			tag_dependend_objects_for_render(sce, renderlay);
#endif
		}
	}
#endif

	if (RE_GetCamera(re) && composite_needs_render(re->scene, 1)) {
		re->scene->id.flag |= LIB_DOIT;
#ifdef DEPSGRAPH_WORKAROUND_HACK
		tag_dependend_objects_for_render(re->scene, renderlay);
#endif
	}
	
	if (re->scene->nodetree == NULL) return;
	
	/* check for render-layers nodes using other scenes, we tag them LIB_DOIT */
	for (node = re->scene->nodetree->nodes.first; node; node = node->next) {
		node->flag &= ~NODE_TEST;
		if (node->type == CMP_NODE_R_LAYERS && (node->flag & NODE_MUTED) == 0) {
			if (node->id) {
				if (!MAIN_VERSION_ATLEAST(re->main, 265, 5)) {
					if (rlayer_node_uses_alpha(re->scene->nodetree, node)) {
						Scene *scene = (Scene *)node->id;

						if (scene->r.alphamode != R_ALPHAPREMUL) {
							BKE_reportf(re->reports, RPT_WARNING, "Setting scene %s alpha mode to Premul", scene->id.name + 2);

							/* also print, so feedback is immediate */
							printf("2.66 versioning fix: setting scene %s alpha mode to Premul\n", scene->id.name + 2);

							scene->r.alphamode = R_ALPHAPREMUL;
						}
					}
				}

				if (node->id != (ID *)re->scene) {
					if ((node->id->flag & LIB_DOIT) == 0) {
						Scene *scene = (Scene *) node->id;
						if (render_scene_has_layers_to_render(scene)) {
							node->flag |= NODE_TEST;
							node->id->flag |= LIB_DOIT;
#ifdef DEPSGRAPH_WORKAROUND_HACK
							tag_dependend_objects_for_render(scene, renderlay);
#endif
						}
					}
				}
			}
		}
	}
	
}

static void ntree_render_scenes(Render *re)
{
	bNode *node;
	int cfra = re->scene->r.cfra;
	Scene *restore_scene = re->scene;
	bool scene_changed = false;
	
	if (re->scene->nodetree == NULL) return;
	
	tag_scenes_for_render(re);
	
	/* now foreach render-result node tagged we do a full render */
	/* results are stored in a way compisitor will find it */
	for (node = re->scene->nodetree->nodes.first; node; node = node->next) {
		if (node->type == CMP_NODE_R_LAYERS && (node->flag & NODE_MUTED) == 0) {
			if (node->id && node->id != (ID *)re->scene) {
				if (node->flag & NODE_TEST) {
					Scene *scene = (Scene *)node->id;

					scene_changed |= scene != restore_scene;
					render_scene(re, scene, cfra);
					node->flag &= ~NODE_TEST;
					
					nodeUpdate(restore_scene->nodetree, node);
				}
			}
		}
	}

	/* restore scene if we rendered another last */
	if (scene_changed)
		BKE_scene_set_background(re->main, re->scene);
}

/* bad call... need to think over proper method still */
static void render_composit_stats(void *UNUSED(arg), char *str)
{
	R.i.infostr = str;
	R.stats_draw(R.sdh, &R.i);
	R.i.infostr = NULL;
}

#ifdef WITH_FREESTYLE
/* invokes Freestyle stroke rendering */
static void add_freestyle(Render *re, int render)
{
	SceneRenderLayer *srl, *actsrl;
	LinkData *link;
	Render *r;
	const bool do_link = (re->r.mode & R_MBLUR) == 0 || re->i.curblur == re->r.mblur_samples;

	actsrl = BLI_findlink(&re->r.layers, re->r.actlay);

	re->freestyle_bmain = BKE_main_new();

	/* We use the same window manager for freestyle bmain as
	 * real bmain uses. This is needed because freestyle's
	 * bmain could be used to tag scenes for update, which
	 * implies call of ED_render_scene_update in some cases
	 * and that function requires proper window manager
	 * to present (sergey)
	 */
	re->freestyle_bmain->wm = re->main->wm;

	FRS_init_stroke_rendering(re);

	for (srl = (SceneRenderLayer *)re->r.layers.first; srl; srl = srl->next) {
		if (do_link) {
			link = (LinkData *)MEM_callocN(sizeof(LinkData), "LinkData to Freestyle render");
			BLI_addtail(&re->freestyle_renders, link);
		}
		if ((re->r.scemode & R_SINGLE_LAYER) && srl != actsrl)
			continue;
		if (FRS_is_freestyle_enabled(srl)) {
			r = FRS_do_stroke_rendering(re, srl, render);
			if (do_link)
				link->data = (void *)r;
		}
	}

	FRS_finish_stroke_rendering(re);

	/* restore the global R value (invalidated by nested execution of the internal renderer) */
	R = *re;
}

/* merges the results of Freestyle stroke rendering into a given render result */
static void composite_freestyle_renders(Render *re, int sample)
{
	Render *freestyle_render;
	SceneRenderLayer *srl, *actsrl;
	LinkData *link;

	actsrl = BLI_findlink(&re->r.layers, re->r.actlay);

	link = (LinkData *)re->freestyle_renders.first;
	for (srl= (SceneRenderLayer *)re->r.layers.first; srl; srl= srl->next) {
		if ((re->r.scemode & R_SINGLE_LAYER) && srl != actsrl)
			continue;

		if (FRS_is_freestyle_enabled(srl)) {
			freestyle_render = (Render *)link->data;

			/* may be NULL in case of empty render layer */
			if (freestyle_render) {
				render_result_exr_file_read_sample(freestyle_render, sample);
				FRS_composite_result(re, srl, freestyle_render);
				RE_FreeRenderResult(freestyle_render->result);
				freestyle_render->result = NULL;
			}
		}
		link = link->next;
	}
}

/* releases temporary scenes and renders for Freestyle stroke rendering */
static void free_all_freestyle_renders(void)
{
	Render *re1, *freestyle_render;
	Scene *freestyle_scene;
	LinkData *link;

	for (re1= RenderGlobal.renderlist.first; re1; re1= re1->next) {
		for (link = (LinkData *)re1->freestyle_renders.first; link; link = link->next) {
			freestyle_render = (Render *)link->data;

			if (freestyle_render) {
				freestyle_scene = freestyle_render->scene;
				RE_FreeRender(freestyle_render);
				BKE_scene_unlink(re1->freestyle_bmain, freestyle_scene, NULL);
			}
		}
		BLI_freelistN(&re1->freestyle_renders);

		if (re1->freestyle_bmain) {
			/* detach the window manager from freestyle bmain (see comments
			 * in add_freestyle() for more detail)
			 */
			re1->freestyle_bmain->wm.first = re1->freestyle_bmain->wm.last = NULL;

			BKE_main_free(re1->freestyle_bmain);
			re1->freestyle_bmain = NULL;
		}
	}
}
#endif

/* reads all buffers, calls optional composite, merges in first result->rectf */
static void do_merge_fullsample(Render *re, bNodeTree *ntree)
{
	float *rectf, filt[3][3];
	int x, y, sample;
	
	/* interaction callbacks */
	if (ntree) {
		ntree->stats_draw = render_composit_stats;
		ntree->test_break = re->test_break;
		ntree->progress = re->progress;
		ntree->sdh = re->sdh;
		ntree->tbh = re->tbh;
		ntree->prh = re->prh;
	}
	
	/* filtmask needs it */
	R = *re;
	
	/* we accumulate in here */
	rectf = MEM_mapallocN(re->rectx * re->recty * sizeof(float) * 4, "fullsample rgba");
	
	for (sample = 0; sample < re->r.osa; sample++) {
		Scene *sce;
		Render *re1;
		RenderResult rres;
		int mask;
		
		/* enable full sample print */
		R.i.curfsa = sample + 1;
		
		/* set all involved renders on the samplebuffers (first was done by render itself, but needs tagged) */
		/* also function below assumes this */
			
		tag_scenes_for_render(re);
		for (sce = re->main->scene.first; sce; sce = sce->id.next) {
			if (sce->id.flag & LIB_DOIT) {
				re1 = RE_GetRender(sce->id.name);

				if (re1 && (re1->r.scemode & R_FULL_SAMPLE)) {
					if (sample) {
						BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
						render_result_exr_file_read_sample(re1, sample);
#ifdef WITH_FREESTYLE
						if (re1->r.mode & R_EDGE_FRS)
							composite_freestyle_renders(re1, sample);
#endif
						BLI_rw_mutex_unlock(&re->resultmutex);
					}
					ntreeCompositTagRender(re1->scene); /* ensure node gets exec to put buffers on stack */
				}
			}
		}
		
		/* composite */
		if (ntree) {
			ntreeCompositTagRender(re->scene);
			ntreeCompositTagAnimated(ntree);
			
			ntreeCompositExecTree(re->scene, ntree, &re->r, true, G.background == 0, &re->scene->view_settings, &re->scene->display_settings);
		}
		
		/* ensure we get either composited result or the active layer */
		RE_AcquireResultImage(re, &rres);
		
		/* accumulate with filter, and clip */
		mask = (1 << sample);
		mask_array(mask, filt);

		for (y = 0; y < re->recty; y++) {
			float *rf = rectf + 4 * y * re->rectx;
			float *col = rres.rectf + 4 * y * re->rectx;
				
			for (x = 0; x < re->rectx; x++, rf += 4, col += 4) {
				/* clamping to 1.0 is needed for correct AA */
				if (col[0] < 0.0f) col[0] = 0.0f; else if (col[0] > 1.0f) col[0] = 1.0f;
				if (col[1] < 0.0f) col[1] = 0.0f; else if (col[1] > 1.0f) col[1] = 1.0f;
				if (col[2] < 0.0f) col[2] = 0.0f; else if (col[2] > 1.0f) col[2] = 1.0f;
				
				add_filt_fmask_coord(filt, col, rf, re->rectx, re->recty, x, y);
			}
		}
		
		RE_ReleaseResultImage(re);

		/* show stuff */
		if (sample != re->osa - 1) {
			/* weak... the display callback wants an active renderlayer pointer... */
			re->result->renlay = render_get_active_layer(re, re->result);
			re->display_update(re->duh, re->result, NULL);
		}
		
		if (re->test_break(re->tbh))
			break;
	}

	/* clamp alpha and RGB to 0..1 and 0..inf, can go outside due to filter */
	for (y = 0; y < re->recty; y++) {
		float *rf = rectf + 4 * y * re->rectx;
			
		for (x = 0; x < re->rectx; x++, rf += 4) {
			rf[0] = MAX2(rf[0], 0.0f);
			rf[1] = MAX2(rf[1], 0.0f);
			rf[2] = MAX2(rf[2], 0.0f);
			CLAMP(rf[3], 0.0f, 1.0f);
		}
	}
	
	/* clear interaction callbacks */
	if (ntree) {
		ntree->stats_draw = NULL;
		ntree->test_break = NULL;
		ntree->progress = NULL;
		ntree->tbh = ntree->sdh = ntree->prh = NULL;
	}
	
	/* disable full sample print */
	R.i.curfsa = 0;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if (re->result->rectf)
		MEM_freeN(re->result->rectf);
	re->result->rectf = rectf;
	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* called externally, via compositor */
void RE_MergeFullSample(Render *re, Main *bmain, Scene *sce, bNodeTree *ntree)
{
	Scene *scene;
	bNode *node;

	/* default start situation */
	G.is_break = false;
	
	re->main = bmain;
	re->scene = sce;
	re->scene_color_manage = BKE_scene_check_color_management_enabled(sce);
	
	/* first call RE_ReadRenderResult on every renderlayer scene. this creates Render structs */
	
	/* tag scenes unread */
	for (scene = re->main->scene.first; scene; scene = scene->id.next)
		scene->id.flag |= LIB_DOIT;
	
#ifdef WITH_FREESTYLE
	if (re->freestyle_bmain) {
		for (scene = re->freestyle_bmain->scene.first; scene; scene = scene->id.next)
			scene->id.flag &= ~LIB_DOIT;
	}
#endif

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->type == CMP_NODE_R_LAYERS && (node->flag & NODE_MUTED) == 0) {
			Scene *nodescene = (Scene *)node->id;
			
			if (nodescene == NULL) nodescene = sce;
			if (nodescene->id.flag & LIB_DOIT) {
				nodescene->r.mode |= R_OSA; /* render struct needs tables */
				RE_ReadRenderResult(sce, nodescene);
				nodescene->id.flag &= ~LIB_DOIT;
			}
		}
	}
	
	/* own render result should be read/allocated */
	if (re->scene->id.flag & LIB_DOIT) {
		RE_ReadRenderResult(re->scene, re->scene);
		re->scene->id.flag &= ~LIB_DOIT;
	}
	
	/* and now we can draw (result is there) */
	re->display_init(re->dih, re->result);
	re->display_clear(re->dch, re->result);
	
#ifdef WITH_FREESTYLE
	if (re->r.mode & R_EDGE_FRS)
		add_freestyle(re, 0);
#endif

	do_merge_fullsample(re, ntree);

#ifdef WITH_FREESTYLE
	free_all_freestyle_renders();
#endif
}

/* returns fully composited render-result on given time step (in RenderData) */
static void do_render_composite_fields_blur_3d(Render *re)
{
	bNodeTree *ntree = re->scene->nodetree;
	int update_newframe = 0;
	
	/* INIT seeding, compositor can use random texture */
	BLI_srandom(re->r.cfra);
	
	if (composite_needs_render(re->scene, 1)) {
		/* save memory... free all cached images */
		ntreeFreeCache(ntree);
		
		do_render_fields_blur_3d(re);
	}
	else {
		re->i.cfra = re->r.cfra;

		/* ensure new result gets added, like for regular renders */
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		
		render_result_free(re->result);
		if ((re->r.mode & R_CROP) == 0) {
			render_result_disprect_to_full_resolution(re);
		}
		re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);

		BLI_rw_mutex_unlock(&re->resultmutex);
		
		/* scene render process already updates animsys */
		update_newframe = 1;
	}
	
	/* swap render result */
	if (re->r.scemode & R_SINGLE_LAYER) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_single_layer_end(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}
	
	if (!re->test_break(re->tbh)) {
		
		if (ntree) {
			ntreeCompositTagRender(re->scene);
			ntreeCompositTagAnimated(ntree);
		}
		
		if (ntree && re->scene->use_nodes && re->r.scemode & R_DOCOMP) {
			/* checks if there are render-result nodes that need scene */
			if ((re->r.scemode & R_SINGLE_LAYER) == 0)
				ntree_render_scenes(re);
			
			if (!re->test_break(re->tbh)) {
				ntree->stats_draw = render_composit_stats;
				ntree->test_break = re->test_break;
				ntree->progress = re->progress;
				ntree->sdh = re->sdh;
				ntree->tbh = re->tbh;
				ntree->prh = re->prh;
				
				/* in case it was never initialized */
				R.sdh = re->sdh;
				R.stats_draw = re->stats_draw;
				
				if (update_newframe)
					BKE_scene_update_for_newframe(re->eval_ctx, re->main, re->scene, re->lay);
				
				if (re->r.scemode & R_FULL_SAMPLE)
					do_merge_fullsample(re, ntree);
				else {
					ntreeCompositExecTree(re->scene, ntree, &re->r, true, G.background == 0, &re->scene->view_settings, &re->scene->display_settings);
				}
				
				ntree->stats_draw = NULL;
				ntree->test_break = NULL;
				ntree->progress = NULL;
				ntree->tbh = ntree->sdh = ntree->prh = NULL;
			}
		}
		else if (re->r.scemode & R_FULL_SAMPLE)
			do_merge_fullsample(re, NULL);
	}

#ifdef WITH_FREESTYLE
	free_all_freestyle_renders();
#endif

	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay = render_get_active_layer(re, re->result);
	re->display_update(re->duh, re->result, NULL);
}

static void renderresult_stampinfo(Render *re)
{
	RenderResult rres;

	/* this is the basic trick to get the displayed float or char rect from render result */
	RE_AcquireResultImage(re, &rres);
	BKE_stamp_buf(re->scene, RE_GetCamera(re), (unsigned char *)rres.rect32, rres.rectf, rres.rectx, rres.recty, 4);
	RE_ReleaseResultImage(re);
}

int RE_seq_render_active(Scene *scene, RenderData *rd)
{
	Editing *ed;
	Sequence *seq;

	ed = scene->ed;
	
	if (!(rd->scemode & R_DOSEQ) || !ed || !ed->seqbase.first)
		return 0;
	
	for (seq = ed->seqbase.first; seq; seq = seq->next) {
		if (seq->type != SEQ_TYPE_SOUND_RAM)
			return 1;
	}
	
	return 0;
}

static void do_render_seq(Render *re)
{
	static int recurs_depth = 0;
	struct ImBuf *ibuf, *out;
	RenderResult *rr; /* don't assign re->result here as it might change during give_ibuf_seq */
	int cfra = re->r.cfra;
	SeqRenderData context;

	re->i.cfra = cfra;

	if (recurs_depth == 0) {
		/* otherwise sequencer animation isn't updated */
		BKE_animsys_evaluate_all_animation(re->main, re->scene, (float)cfra); // XXX, was BKE_scene_frame_get(re->scene)
	}

	recurs_depth++;

	if ((re->r.mode & R_BORDER) && (re->r.mode & R_CROP) == 0) {
		/* if border rendering is used and cropping is disabled, final buffer should
		 * be as large as the whole frame */
		context = BKE_sequencer_new_render_data(re->eval_ctx, re->main, re->scene,
		                                        re->winx, re->winy, 100);
	}
	else {
		context = BKE_sequencer_new_render_data(re->eval_ctx, re->main, re->scene,
		                                        re->result->rectx, re->result->recty, 100);
	}

	out = BKE_sequencer_give_ibuf(&context, cfra, 0);

	if (out) {
		ibuf = IMB_dupImBuf(out);
		IMB_freeImBuf(out);
		BKE_sequencer_imbuf_from_sequencer_space(re->scene, ibuf);
	}
	else {
		ibuf = NULL;
	}

	recurs_depth--;

	rr = re->result;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if (ibuf) {
		/* copy ibuf into combined pixel rect */
		render_result_rect_from_ibuf(rr, &re->r, ibuf);
		
		if (recurs_depth == 0) { /* with nested scenes, only free on toplevel... */
			Editing *ed = re->scene->ed;
			if (ed)
				BKE_sequencer_free_imbuf(re->scene, &ed->seqbase, true);
		}
		IMB_freeImBuf(ibuf);
	}
	else {
		/* render result is delivered empty in most cases, nevertheless we handle all cases */
		render_result_rect_fill_zero(rr);
	}

	BLI_rw_mutex_unlock(&re->resultmutex);

	/* just in case this flag went missing at some point */
	re->r.scemode |= R_DOSEQ;

	/* set overall progress of sequence rendering */
	if (re->r.efra != re->r.sfra)
		re->progress(re->prh, (float)(cfra - re->r.sfra) / (re->r.efra - re->r.sfra));
	else
		re->progress(re->prh, 1.0f);

	/* would mark display buffers as invalid */
	re->display_update(re->duh, re->result, NULL);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* main loop: doing sequence + fields + blur + 3d render + compositing */
static void do_render_all_options(Render *re)
{
	re->current_scene_update(re->suh, re->scene);

	BKE_scene_camera_switch_update(re->scene);

	re->i.starttime = PIL_check_seconds_timer();

	/* ensure no images are in memory from previous animated sequences */
	BKE_image_all_free_anim_ibufs(re->r.cfra);

	if (RE_engine_render(re, 1)) {
		/* in this case external render overrides all */
	}
	else if (RE_seq_render_active(re->scene, &re->r)) {
		/* note: do_render_seq() frees rect32 when sequencer returns float images */
		if (!re->test_break(re->tbh))
			do_render_seq(re);
		
		re->stats_draw(re->sdh, &re->i);
		re->display_update(re->duh, re->result, NULL);
	}
	else {
		re->pool = BKE_image_pool_new();

		do_render_composite_fields_blur_3d(re);

		BKE_image_pool_free(re->pool);
		re->pool = NULL;
	}
	
	re->i.lastframetime = PIL_check_seconds_timer() - re->i.starttime;
	
	re->stats_draw(re->sdh, &re->i);
	
	/* stamp image info here */
	if ((re->r.stamp & R_STAMP_ALL) && (re->r.stamp & R_STAMP_DRAW)) {
		renderresult_stampinfo(re);
		re->display_update(re->duh, re->result, NULL);
	}
}

bool RE_force_single_renderlayer(Scene *scene)
{
	int scemode = check_mode_full_sample(&scene->r);
	if (scemode & R_SINGLE_LAYER) {
		SceneRenderLayer *srl = BLI_findlink(&scene->r.layers, scene->r.actlay);
		/* force layer to be enabled */
		if (srl->layflag & SCE_LAY_DISABLE) {
			srl->layflag &= ~SCE_LAY_DISABLE;
			return true;
		}
	}
	return false;
}

static bool check_valid_compositing_camera(Scene *scene, Object *camera_override)
{
	if (scene->r.scemode & R_DOCOMP && scene->use_nodes) {
		bNode *node = scene->nodetree->nodes.first;

		while (node) {
			if (node->type == CMP_NODE_R_LAYERS && (node->flag & NODE_MUTED) == 0) {
				Scene *sce = node->id ? (Scene *)node->id : scene;

				if (!sce->camera && !BKE_scene_camera_find(sce)) {
					/* all render layers nodes need camera */
					return false;
				}
			}

			node = node->next;
		}

		return true;
	}
	else {
		return (camera_override != NULL || scene->camera != NULL);
	}
}

static int check_valid_camera(Scene *scene, Object *camera_override, ReportList *reports)
{
	if (camera_override == NULL && scene->camera == NULL)
		scene->camera = BKE_scene_camera_find(scene);

	if (RE_seq_render_active(scene, &scene->r)) {
		if (scene->ed) {
			Sequence *seq = scene->ed->seqbase.first;

			while (seq) {
				if (seq->type == SEQ_TYPE_SCENE && seq->scene) {
					if (!seq->scene_camera) {
						if (!seq->scene->camera && !BKE_scene_camera_find(seq->scene)) {
							/* camera could be unneeded due to composite nodes */
							Object *override = (seq->scene == scene) ? camera_override : NULL;

							if (!check_valid_compositing_camera(seq->scene, override)) {
								BKE_reportf(reports, RPT_ERROR, "No camera found in scene \"%s\"", seq->scene->id.name+2);
								return false;
							}
						}
					}
				}

				seq = seq->next;
			}
		}
	}
	else if (!check_valid_compositing_camera(scene, camera_override)) {
		BKE_report(reports, RPT_ERROR, "No camera found in scene");
		return false;
	}

	return true;
}

static bool node_tree_has_composite_output(bNodeTree *ntree)
{
	bNode *node;

	for (node = ntree->nodes.first; node; node = node->next) {
		if (ELEM(node->type, CMP_NODE_COMPOSITE, CMP_NODE_OUTPUT_FILE)) {
			return true;
		}
		else if (node->type == NODE_GROUP) {
			if (node->id) {
				if (node_tree_has_composite_output((bNodeTree *)node->id)) {
					return true;
				}
			}
		}
	}

	return false;
}

static int check_composite_output(Scene *scene)
{
	return node_tree_has_composite_output(scene->nodetree);
}

bool RE_is_rendering_allowed(Scene *scene, Object *camera_override, ReportList *reports)
{
	int scemode = check_mode_full_sample(&scene->r);
	
	if (scene->r.mode & R_BORDER) {
		if (scene->r.border.xmax <= scene->r.border.xmin ||
		    scene->r.border.ymax <= scene->r.border.ymin)
		{
			BKE_report(reports, RPT_ERROR, "No border area selected");
			return 0;
		}
	}
	
	if (scemode & (R_EXR_TILE_FILE | R_FULL_SAMPLE)) {
		char str[FILE_MAX];
		
		render_result_exr_file_path(scene, "", 0, str);
		
		if (!BLI_file_is_writable(str)) {
			BKE_report(reports, RPT_ERROR, "Cannot save render buffers, check the temp default path");
			return 0;
		}
		
		/* no fullsample and edge */
		if ((scemode & R_FULL_SAMPLE) && (scene->r.mode & R_EDGE)) {
			BKE_report(reports, RPT_ERROR, "Full sample does not support edge enhance");
			return 0;
		}
		
	}
	
	if (scemode & R_DOCOMP) {
		if (scene->use_nodes) {
			if (!scene->nodetree) {
				BKE_report(reports, RPT_ERROR, "No node tree in scene");
				return 0;
			}
			
			if (!check_composite_output(scene)) {
				BKE_report(reports, RPT_ERROR, "No render output node in scene");
				return 0;
			}
			
			if (scemode & R_FULL_SAMPLE) {
				if (composite_needs_render(scene, 0) == 0) {
					BKE_report(reports, RPT_ERROR, "Full sample AA not supported without 3D rendering");
					return 0;
				}
			}
		}
	}
	
	/* check valid camera, without camera render is OK (compo, seq) */
	if (!check_valid_camera(scene, camera_override, reports)) {
		return 0;
	}
	
	/* get panorama & ortho, only after camera is set */
	BKE_camera_object_mode(&scene->r, camera_override ? camera_override : scene->camera);

	/* forbidden combinations */
	if (scene->r.mode & R_PANORAMA) {
		if (scene->r.mode & R_ORTHO) {
			BKE_report(reports, RPT_ERROR, "No ortho render possible for panorama");
			return 0;
		}

#ifdef WITH_FREESTYLE
		if (scene->r.mode & R_EDGE_FRS) {
			BKE_report(reports, RPT_ERROR, "Panoramic camera not supported in Freestyle");
			return 0;
		}
#endif
	}

#ifdef WITH_FREESTYLE
	if (scene->r.mode & R_EDGE_FRS) {
		if (scene->r.mode & R_FIELDS) {
			BKE_report(reports, RPT_ERROR, "Fields not supported in Freestyle");
			return false;
		}
	}
#endif

	/* layer flag tests */
	if (!render_scene_has_layers_to_render(scene)) {
		BKE_report(reports, RPT_ERROR, "All render layers are disabled");
		return 0;
	}

	return 1;
}

static void validate_render_settings(Render *re)
{
	if (re->r.scemode & (R_EXR_TILE_FILE | R_FULL_SAMPLE)) {
		/* no osa + fullsample won't work... */
		if (re->r.osa == 0)
			re->r.scemode &= ~R_FULL_SAMPLE;
	}

	if (RE_engine_is_external(re)) {
		/* not supported yet */
		re->r.scemode &= ~(R_FULL_SAMPLE);
		re->r.mode &= ~(R_FIELDS | R_MBLUR);
	}
}

static void update_physics_cache(Render *re, Scene *scene, int UNUSED(anim_init))
{
	PTCacheBaker baker;

	baker.main = re->main;
	baker.scene = scene;
	baker.pid = NULL;
	baker.bake = 0;
	baker.render = 1;
	baker.anim_init = 1;
	baker.quick_step = 1;
	baker.break_test = re->test_break;
	baker.break_data = re->tbh;
	baker.progressbar = NULL;

	BKE_ptcache_bake(&baker);
}
/* evaluating scene options for general Blender render */
static int render_initialize_from_main(Render *re, RenderData *rd, Main *bmain, Scene *scene, SceneRenderLayer *srl,
                                       Object *camera_override, unsigned int lay_override, int anim, int anim_init)
{
	int winx, winy;
	rcti disprect;
	
	/* r.xsch and r.ysch has the actual view window size
	 * r.border is the clipping rect */
	
	/* calculate actual render result and display size */
	winx = (rd->size * rd->xsch) / 100;
	winy = (rd->size * rd->ysch) / 100;
	
	/* we always render smaller part, inserting it in larger image is compositor bizz, it uses disprect for it */
	if (scene->r.mode & R_BORDER) {
		disprect.xmin = rd->border.xmin * winx;
		disprect.xmax = rd->border.xmax * winx;
		
		disprect.ymin = rd->border.ymin * winy;
		disprect.ymax = rd->border.ymax * winy;
	}
	else {
		disprect.xmin = disprect.ymin = 0;
		disprect.xmax = winx;
		disprect.ymax = winy;
	}
	
	re->main = bmain;
	re->scene = scene;
	re->scene_color_manage = BKE_scene_check_color_management_enabled(scene);
	re->camera_override = camera_override;
	re->lay = lay_override ? lay_override : scene->lay;
	re->layer_override = lay_override;
	re->i.localview = (re->lay & 0xFF000000) != 0;
	
	/* not too nice, but it survives anim-border render */
	if (anim) {
		render_update_anim_renderdata(re, &scene->r);
		re->disprect = disprect;
		return 1;
	}
	
	/* check all scenes involved */
	tag_scenes_for_render(re);

	/*
	 * Disabled completely for now,
	 * can be later set as render profile option
	 * and default for background render.
	 */
	if (0) {
		/* make sure dynamics are up to date */
		update_physics_cache(re, scene, anim_init);
	}
	
	if (srl || scene->r.scemode & R_SINGLE_LAYER) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_single_layer_begin(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}
	
	RE_InitState(re, NULL, &scene->r, srl, winx, winy, &disprect);
	if (!re->ok)  /* if an error was printed, abort */
		return 0;
	
	/* initstate makes new result, have to send changed tags around */
	ntreeCompositTagRender(re->scene);

	validate_render_settings(re);

	re->display_init(re->dih, re->result);
	re->display_clear(re->dch, re->result);
	
	return 1;
}

void RE_SetReports(Render *re, ReportList *reports)
{
	re->reports = reports;
}

/* general Blender frame render call */
void RE_BlenderFrame(Render *re, Main *bmain, Scene *scene, SceneRenderLayer *srl, Object *camera_override,
                     unsigned int lay_override, int frame, const bool write_still)
{
	BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_INIT);

	/* ugly global still... is to prevent preview events and signal subsurfs etc to make full resol */
	G.is_rendering = true;
	
	scene->r.cfra = frame;
	
	if (render_initialize_from_main(re, &scene->r, bmain, scene, srl, camera_override, lay_override, 0, 0)) {
		MEM_reset_peak_memory();

		BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);

		do_render_all_options(re);

		if (write_still && !G.is_break) {
			if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
				/* operator checks this but in case its called from elsewhere */
				printf("Error: cant write single images with a movie format!\n");
			}
			else {
				char name[FILE_MAX];
				BKE_makepicstring(name, scene->r.pic, bmain->name, scene->r.cfra,
				                  &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, false);

				/* reports only used for Movie */
				do_write_image_or_movie(re, bmain, scene, NULL, name);
			}
		}

		BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
		if (write_still) {
			BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_WRITE);
		}
	}

	BLI_callback_exec(re->main, (ID *)scene, G.is_break ? BLI_CB_EVT_RENDER_CANCEL : BLI_CB_EVT_RENDER_COMPLETE);

	/* UGLY WARNING */
	G.is_rendering = false;
}

#ifdef WITH_FREESTYLE
void RE_RenderFreestyleStrokes(Render *re, Main *bmain, Scene *scene, int render)
{
	re->result_ok= 0;
	if (render_initialize_from_main(re, &scene->r, bmain, scene, NULL, NULL, scene->lay, 0, 0)) {
		if (render)
			do_render_fields_blur_3d(re);
	}
	re->result_ok = 1;
}

void RE_RenderFreestyleExternal(Render *re)
{
	if (!re->test_break(re->tbh)) {
		RE_Database_FromScene(re, re->main, re->scene, re->lay, 1);
		RE_Database_Preprocess(re);
		add_freestyle(re, 1);
		RE_Database_Free(re);
	}
}
#endif

static int do_write_image_or_movie(Render *re, Main *bmain, Scene *scene, bMovieHandle *mh, const char *name_override)
{
	char name[FILE_MAX];
	RenderResult rres;
	Object *camera = RE_GetCamera(re);
	double render_time;
	int ok = 1;
	
	RE_AcquireResultImage(re, &rres);

	/* write movie or image */
	if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		bool do_free = false;
		ImBuf *ibuf = render_result_rect_to_ibuf(&rres, &scene->r);

		/* note; the way it gets 32 bits rects is weak... */
		if (ibuf->rect == NULL) {
			ibuf->rect = MEM_mapallocN(sizeof(int) * rres.rectx * rres.recty, "temp 32 bits rect");
			ibuf->mall |= IB_rect;
			RE_AcquiredResultGet32(re, &rres, ibuf->rect);
			do_free = true;
		}


		IMB_colormanagement_imbuf_for_write(ibuf, true, false, &scene->view_settings,
		                                    &scene->display_settings, &scene->r.im_format);

		ok = mh->append_movie(&re->r, scene->r.sfra, scene->r.cfra, (int *) ibuf->rect,
		                      ibuf->x, ibuf->y, re->reports);
		if (do_free) {
			MEM_freeN(ibuf->rect);
			ibuf->rect = NULL;
			ibuf->mall &= ~IB_rect;
		}

		/* imbuf knows which rects are not part of ibuf */
		IMB_freeImBuf(ibuf);

		printf("Append frame %d", scene->r.cfra);
	}
	else {
		if (name_override)
			BLI_strncpy(name, name_override, sizeof(name));
		else
			BKE_makepicstring(name, scene->r.pic, bmain->name, scene->r.cfra,
			                  &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true);
		
		if (re->r.im_format.imtype == R_IMF_IMTYPE_MULTILAYER) {
			if (re->result) {
				RE_WriteRenderResult(re->reports, re->result, name, scene->r.im_format.exr_codec);
				printf("Saved: %s", name);
			}
		}
		else {
			ImBuf *ibuf = render_result_rect_to_ibuf(&rres, &scene->r);

			IMB_colormanagement_imbuf_for_write(ibuf, true, false, &scene->view_settings,
			                                    &scene->display_settings, &scene->r.im_format);

			ok = BKE_imbuf_write_stamp(scene, camera, ibuf, name, &scene->r.im_format);
			
			if (ok == 0) {
				printf("Render error: cannot save %s\n", name);
			}
			else printf("Saved: %s", name);
			
			/* optional preview images for exr */
			if (ok && scene->r.im_format.imtype == R_IMF_IMTYPE_OPENEXR && (scene->r.im_format.flag & R_IMF_FLAG_PREVIEW_JPG)) {
				ImageFormatData imf = scene->r.im_format;
				imf.imtype = R_IMF_IMTYPE_JPEG90;

				if (BLI_testextensie(name, ".exr"))
					name[strlen(name) - 4] = 0;
				BKE_add_image_extension(name, &imf);
				ibuf->planes = 24;

				IMB_colormanagement_imbuf_for_write(ibuf, true, false, &scene->view_settings,
				                                    &scene->display_settings, &imf);

				BKE_imbuf_write_stamp(scene, camera, ibuf, name, &imf);
				printf("\nSaved: %s", name);
			}
			
			/* imbuf knows which rects are not part of ibuf */
			IMB_freeImBuf(ibuf);
		}
	}
	
	RE_ReleaseResultImage(re);

	render_time = re->i.lastframetime;
	re->i.lastframetime = PIL_check_seconds_timer() - re->i.starttime;
	
	BLI_timestr(re->i.lastframetime, name, sizeof(name));
	printf(" Time: %s", name);
	
	BLI_callback_exec(G.main, NULL, BLI_CB_EVT_RENDER_STATS);

	BLI_timestr(re->i.lastframetime - render_time, name, sizeof(name));
	printf(" (Saving: %s)\n", name);
	
	fputc('\n', stdout);
	fflush(stdout); /* needed for renderd !! (not anymore... (ton)) */

	return ok;
}

/* saves images to disk */
void RE_BlenderAnim(Render *re, Main *bmain, Scene *scene, Object *camera_override,
                    unsigned int lay_override, int sfra, int efra, int tfra)
{
	RenderData rd = scene->r;
	bMovieHandle *mh = BKE_movie_handle_get(scene->r.im_format.imtype);
	int cfrao = scene->r.cfra;
	int nfra, totrendered = 0, totskipped = 0;
	
	BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_INIT);

	/* do not fully call for each frame, it initializes & pops output window */
	if (!render_initialize_from_main(re, &rd, bmain, scene, NULL, camera_override, lay_override, 0, 1))
		return;
	
	/* ugly global still... is to prevent renderwin events and signal subsurfs etc to make full resol */
	/* is also set by caller renderwin.c */
	G.is_rendering = true;

	re->flag |= R_ANIMATION;

	if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		int width, height;
		if (re->r.mode & R_BORDER) {
			if ((re->r.mode & R_CROP) == 0) {
				width = re->winx;
				height = re->winy;
			}
			else {
				width = re->rectx;
				height = re->recty;
			}
		}
		else {
			width = re->rectx;
			height = re->recty;
		}

		if (!mh->start_movie(scene, &re->r, width, height, re->reports))
			G.is_break = true;
	}

	if (mh->get_next_frame) {
		while (!(G.is_break == 1)) {
			int nf = mh->get_next_frame(&re->r, re->reports);
			if (nf >= 0 && nf >= scene->r.sfra && nf <= scene->r.efra) {
				scene->r.cfra = re->r.cfra = nf;

				BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);

				do_render_all_options(re);
				totrendered++;

				if (re->test_break(re->tbh) == 0) {
					if (!do_write_image_or_movie(re, bmain, scene, mh, NULL))
						G.is_break = true;
				}

				if (G.is_break == false) {
					BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
					BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_WRITE);
				}
			}
			else {
				if (re->test_break(re->tbh)) {
					G.is_break = true;
				}
			}
		}
	}
	else {
		for (nfra = sfra, scene->r.cfra = sfra; scene->r.cfra <= efra; scene->r.cfra++) {
			char name[FILE_MAX];
			
			/* only border now, todo: camera lens. (ton) */
			render_initialize_from_main(re, &rd, bmain, scene, NULL, camera_override, lay_override, 1, 0);

			if (nfra != scene->r.cfra) {
				/*
				 * Skip this frame, but update for physics and particles system.
				 * From convertblender.c:
				 * in localview, lamps are using normal layers, objects only local bits.
				 */
				unsigned int updatelay;

				if (re->lay & 0xFF000000)
					updatelay = re->lay & 0xFF000000;
				else
					updatelay = re->lay;

				BKE_scene_update_for_newframe(re->eval_ctx, bmain, scene, updatelay);
				continue;
			}
			else
				nfra += tfra;

			/* Touch/NoOverwrite options are only valid for image's */
			if (BKE_imtype_is_movie(scene->r.im_format.imtype) == 0) {
				if (scene->r.mode & (R_NO_OVERWRITE | R_TOUCH))
					BKE_makepicstring(name, scene->r.pic, bmain->name, scene->r.cfra,
					                  &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true);

				if (scene->r.mode & R_NO_OVERWRITE && BLI_exists(name)) {
					printf("skipping existing frame \"%s\"\n", name);
					totskipped++;
					continue;
				}
				if (scene->r.mode & R_TOUCH && !BLI_exists(name)) {
					BLI_make_existing_file(name); /* makes the dir if its not there */
					BLI_file_touch(name);
				}
			}

			re->r.cfra = scene->r.cfra;     /* weak.... */

			/* run callbacs before rendering, before the scene is updated */
			BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);

			
			do_render_all_options(re);
			totrendered++;
			
			if (re->test_break(re->tbh) == 0) {
				if (!G.is_break)
					if (!do_write_image_or_movie(re, bmain, scene, mh, NULL))
						G.is_break = true;
			}
			else
				G.is_break = true;
		
			if (G.is_break == true) {
				/* remove touched file */
				if (BKE_imtype_is_movie(scene->r.im_format.imtype) == 0) {
					if ((scene->r.mode & R_TOUCH) && (BLI_file_size(name) == 0)) {
						/* BLI_exists(name) is implicit */
						BLI_delete(name, false, false);
					}
				}
				
				break;
			}

			if (G.is_break == false) {
				BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
				BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_WRITE);
			}
		}
	}
	
	/* end movie */
	if (BKE_imtype_is_movie(scene->r.im_format.imtype))
		mh->end_movie();
	
	if (totskipped && totrendered == 0)
		BKE_report(re->reports, RPT_INFO, "No frames rendered, skipped to not overwrite");

	scene->r.cfra = cfrao;

	re->flag &= ~R_ANIMATION;

	BLI_callback_exec(re->main, (ID *)scene, G.is_break ? BLI_CB_EVT_RENDER_CANCEL : BLI_CB_EVT_RENDER_COMPLETE);

	/* UGLY WARNING */
	G.is_rendering = false;
}

void RE_PreviewRender(Render *re, Main *bmain, Scene *sce)
{
	Object *camera;
	int winx, winy;

	winx = (sce->r.size * sce->r.xsch) / 100;
	winy = (sce->r.size * sce->r.ysch) / 100;

	RE_InitState(re, NULL, &sce->r, NULL, winx, winy, NULL);

	re->pool = BKE_image_pool_new();

	re->main = bmain;
	re->scene = sce;
	re->scene_color_manage = BKE_scene_check_color_management_enabled(sce);
	re->lay = sce->lay;

	camera = RE_GetCamera(re);
	RE_SetCamera(re, camera);

	do_render_3d(re);

	BKE_image_pool_free(re->pool);
	re->pool = NULL;
}

/* note; repeated win/disprect calc... solve that nicer, also in compo */

/* only the temp file! */
bool RE_ReadRenderResult(Scene *scene, Scene *scenode)
{
	Render *re;
	int winx, winy;
	bool success;
	rcti disprect;
	
	/* calculate actual render result and display size */
	winx = (scene->r.size * scene->r.xsch) / 100;
	winy = (scene->r.size * scene->r.ysch) / 100;
	
	/* only in movie case we render smaller part */
	if (scene->r.mode & R_BORDER) {
		disprect.xmin = scene->r.border.xmin * winx;
		disprect.xmax = scene->r.border.xmax * winx;
		
		disprect.ymin = scene->r.border.ymin * winy;
		disprect.ymax = scene->r.border.ymax * winy;
	}
	else {
		disprect.xmin = disprect.ymin = 0;
		disprect.xmax = winx;
		disprect.ymax = winy;
	}
	
	if (scenode)
		scene = scenode;
	
	/* get render: it can be called from UI with draw callbacks */
	re = RE_GetRender(scene->id.name);
	if (re == NULL)
		re = RE_NewRender(scene->id.name);
	RE_InitState(re, NULL, &scene->r, NULL, winx, winy, &disprect);
	re->scene = scene;
	re->scene_color_manage = BKE_scene_check_color_management_enabled(scene);
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	success = render_result_exr_file_cache_read(re);
	BLI_rw_mutex_unlock(&re->resultmutex);

	return success;
}

void RE_init_threadcount(Render *re) 
{
	re->r.threads = BKE_render_num_threads(&re->r);
}

/* loads in image into a result, size must match
 * x/y offsets are only used on a partial copy when dimensions don't match */
void RE_layer_load_from_file(RenderLayer *layer, ReportList *reports, const char *filename, int x, int y)
{
	/* OCIO_TODO: assume layer was saved in defaule color space */
	ImBuf *ibuf = IMB_loadiffname(filename, IB_rect, NULL);

	if (ibuf && (ibuf->rect || ibuf->rect_float)) {
		if (ibuf->x == layer->rectx && ibuf->y == layer->recty) {
			if (ibuf->rect_float == NULL)
				IMB_float_from_rect(ibuf);

			memcpy(layer->rectf, ibuf->rect_float, sizeof(float) * 4 * layer->rectx * layer->recty);
		}
		else {
			if ((ibuf->x - x >= layer->rectx) && (ibuf->y - y >= layer->recty)) {
				ImBuf *ibuf_clip;

				if (ibuf->rect_float == NULL)
					IMB_float_from_rect(ibuf);

				ibuf_clip = IMB_allocImBuf(layer->rectx, layer->recty, 32, IB_rectfloat);
				if (ibuf_clip) {
					IMB_rectcpy(ibuf_clip, ibuf, 0, 0, x, y, layer->rectx, layer->recty);

					memcpy(layer->rectf, ibuf_clip->rect_float, sizeof(float) * 4 * layer->rectx * layer->recty);
					IMB_freeImBuf(ibuf_clip);
				}
				else {
					BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to allocate clip buffer '%s'", filename);
				}
			}
			else {
				BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: incorrect dimensions for partial copy '%s'", filename);
			}
		}

		IMB_freeImBuf(ibuf);
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to load '%s'", filename);
	}
}

void RE_result_load_from_file(RenderResult *result, ReportList *reports, const char *filename)
{
	if (!render_result_exr_file_read_path(result, NULL, filename)) {
		BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to load '%s'", filename);
		return;
	}
}

const float default_envmap_layout[] = { 0, 0, 1, 0, 2, 0, 0, 1, 1, 1, 2, 1 };

bool RE_WriteEnvmapResult(struct ReportList *reports, Scene *scene, EnvMap *env, const char *relpath, const char imtype, float layout[12])
{
	ImageFormatData imf;
	ImBuf *ibuf = NULL;
	int ok;
	int dx;
	int maxX = 0, maxY = 0, i = 0;
	char filepath[FILE_MAX];

	if (env->cube[1] == NULL) {
		BKE_report(reports, RPT_ERROR, "There is no generated environment map available to save");
		return 0;
	}

	imf = scene->r.im_format;
	imf.imtype = imtype;

	dx = env->cube[1]->x;

	if (env->type == ENV_CUBE) {
		for (i = 0; i < 12; i += 2) {
			maxX = max_ii(maxX, (int)layout[i] + 1);
			maxY = max_ii(maxY, (int)layout[i + 1] + 1);
		}

		ibuf = IMB_allocImBuf(maxX * dx, maxY * dx, 24, IB_rectfloat);

		for (i = 0; i < 12; i += 2)
			if (layout[i] > -1 && layout[i + 1] > -1)
				IMB_rectcpy(ibuf, env->cube[i / 2], layout[i] * dx, layout[i + 1] * dx, 0, 0, dx, dx);
	}
	else if (env->type == ENV_PLANE) {
		ibuf = IMB_allocImBuf(dx, dx, 24, IB_rectfloat);
		IMB_rectcpy(ibuf, env->cube[1], 0, 0, 0, 0, dx, dx);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Invalid environment map type");
		return 0;
	}

	IMB_colormanagement_imbuf_for_write(ibuf, true, false, &scene->view_settings, &scene->display_settings, &imf);

	/* to save, we first get absolute path */
	BLI_strncpy(filepath, relpath, sizeof(filepath));
	BLI_path_abs(filepath, G.main->name);

	ok = BKE_imbuf_write(ibuf, filepath, &imf);

	IMB_freeImBuf(ibuf);

	if (ok) {
		return true;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Error writing environment map");
		return false;
	}
}

