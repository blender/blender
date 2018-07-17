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
#include <errno.h>

#include "DNA_anim_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_timecode.h"
#include "BLI_fileops.h"
#include "BLI_threads.h"
#include "BLI_rand.h"
#include "BLI_callbacks.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"  /* <------ should this be here?, needed for sequencer update */
#include "BKE_camera.h"
#include "BKE_colortools.h"
#include "BKE_context.h" /* XXX needed by wm_window.h */
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"
#include "BKE_writeavi.h"  /* <------ should be replaced once with generic movie module */
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "PIL_time.h"
#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "RE_engine.h"
#include "RE_pipeline.h"
#include "RE_render_ext.h"

#include "../../../windowmanager/WM_api.h" /* XXX */
#include "../../../windowmanager/wm_window.h" /* XXX */
#include "GPU_context.h"

#ifdef WITH_FREESTYLE
#  include "FRS_freestyle.h"
#endif

#include "DEG_depsgraph.h"

/* internal */
#include "initrender.h"
#include "renderpipeline.h"
#include "render_result.h"
#include "render_types.h"

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

/* ********* alloc and free ******** */

static int do_write_image_or_movie(Render *re, Main *bmain, Scene *scene, bMovieHandle *mh, const int totvideos, const char *name_override);

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
	char info_time_str[32];

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

	BLI_timecode_string_from_time_simple(info_time_str, sizeof(info_time_str), PIL_check_seconds_timer() - rs->starttime);
	fprintf(stdout, IFACE_("| Time:%s | "), info_time_str);

	if (rs->infostr) {
		fprintf(stdout, "%s", rs->infostr);
	}
	else {
		if (rs->tothalo)
			fprintf(stdout, IFACE_("Sce: %s Ve:%d Fa:%d Ha:%d La:%d"),
			        rs->scene_name, rs->totvert, rs->totface, rs->tothalo, rs->totlamp);
		else
			fprintf(stdout, IFACE_("Sce: %s Ve:%d Fa:%d La:%d"), rs->scene_name, rs->totvert, rs->totface, rs->totlamp);
	}

	/* Flush stdout to be sure python callbacks are printing stuff after blender. */
	fflush(stdout);

	/* NOTE: using G_MAIN seems valid here??? Not sure it's actually even used anyway, we could as well pass NULL? */
	BLI_callback_exec(G_MAIN, NULL, BLI_CB_EVT_RENDER_STATS);

	fputc('\n', stdout);
	fflush(stdout);
}

static void render_print_save_message(
        ReportList *reports, const char *name, int ok, int err)
{
	if (ok) {
		/* no need to report, just some helpful console info */
		printf("Saved: '%s'\n", name);
	}
	else {
		/* report on error since users will want to know what failed */
		BKE_reportf(reports, RPT_ERROR, "Render error (%s) cannot save: '%s'", strerror(err), name);
	}
}

static int render_imbuf_write_stamp_test(
        ReportList *reports,
        Scene *scene, struct RenderResult *rr, ImBuf *ibuf, const char *name,
        const ImageFormatData *imf, bool stamp)
{
	int ok;

	if (stamp) {
		/* writes the name of the individual cameras */
		ok = BKE_imbuf_write_stamp(scene, rr, ibuf, name, imf);
	}
	else {
		ok = BKE_imbuf_write(ibuf, name, imf);
	}

	render_print_save_message(reports, name, ok, errno);

	return ok;
}

void RE_FreeRenderResult(RenderResult *res)
{
	render_result_free(res);
}

float *RE_RenderLayerGetPass(volatile RenderLayer *rl, const char *name, const char *viewname)
{
	RenderPass *rpass = RE_pass_find_by_name(rl, name, viewname);
	return rpass ? rpass->rect : NULL;
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

bool RE_HasSingleLayer(Render *re)
{
	return (re->r.scemode & R_SINGLE_LAYER);
}

RenderResult *RE_MultilayerConvert(void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty)
{
	return render_result_new_from_exr(exrhandle, colorspace, predivide, rectx, recty);
}

RenderLayer *render_get_active_layer(Render *re, RenderResult *rr)
{
	ViewLayer *view_layer = BLI_findlink(&re->view_layers, re->active_view_layer);

	if (view_layer) {
		RenderLayer *rl = BLI_findstring(&rr->layers,
		                                 view_layer->name,
		                                 offsetof(RenderLayer, name));

		if (rl) {
			return rl;
		}
	}

	return rr->layers.first;
}

static bool render_scene_has_layers_to_render(Scene *scene, ViewLayer *single_layer)
{
	if (single_layer) {
		return true;
	}

	ViewLayer *view_layer;
	for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		if (view_layer->flag & VIEW_LAYER_RENDER) {
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
		if (STREQLEN(re->name, name, RE_MAXNAME))
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

void RE_ClearResult(Render *re)
{
	if (re) {
		render_result_free(re->result);
		re->result = NULL;
	}
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

/**
 * Same as #RE_AcquireResultImage but creating the necessary views to store the result
 * fill provided result struct with a copy of thew views of what is done so far the
 * #RenderResult.views #ListBase needs to be freed after with #RE_ReleaseResultImageViews
 */
void RE_AcquireResultImageViews(Render *re, RenderResult *rr)
{
	memset(rr, 0, sizeof(RenderResult));

	if (re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);

		if (re->result) {
			RenderLayer *rl;
			RenderView *rv, *rview;

			rr->rectx = re->result->rectx;
			rr->recty = re->result->recty;

			/* creates a temporary duplication of views */
			render_result_views_shallowcopy(rr, re->result);

			rv = rr->views.first;
			rr->have_combined = (rv->rectf != NULL);

			/* active layer */
			rl = render_get_active_layer(re, re->result);

			if (rl) {
				if (rv->rectf == NULL) {
					for (rview = (RenderView *)rr->views.first; rview; rview = rview->next) {
						rview->rectf = RE_RenderLayerGetPass(rl, RE_PASSNAME_COMBINED, rview->name);
					}
				}

				if (rv->rectz == NULL) {
					for (rview = (RenderView *)rr->views.first; rview; rview = rview->next) {
						rview->rectz = RE_RenderLayerGetPass(rl, RE_PASSNAME_Z, rview->name);
					}
				}
			}

			rr->layers = re->result->layers;
			rr->xof = re->disprect.xmin;
			rr->yof = re->disprect.ymin;
			rr->stamp_data = re->result->stamp_data;
		}
	}
}

/* clear temporary renderresult struct */
void RE_ReleaseResultImageViews(Render *re, RenderResult *rr)
{
	if (re) {
		if (rr) {
			render_result_views_shallowdelete(rr);
		}
		BLI_rw_mutex_unlock(&re->resultmutex);
	}
}

/* fill provided result struct with what's currently active or done */
/* this RenderResult struct is the only exception to the rule of a RenderResult */
/* always having at least one RenderView */
void RE_AcquireResultImage(Render *re, RenderResult *rr, const int view_id)
{
	memset(rr, 0, sizeof(RenderResult));

	if (re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);

		if (re->result) {
			RenderLayer *rl;
			RenderView *rv;

			rr->rectx = re->result->rectx;
			rr->recty = re->result->recty;

			/* actview view */
			rv = RE_RenderViewGetById(re->result, view_id);
			rr->have_combined = (rv->rectf != NULL);

			rr->rectf = rv->rectf;
			rr->rectz = rv->rectz;
			rr->rect32 = rv->rect32;

			/* active layer */
			rl = render_get_active_layer(re, re->result);

			if (rl) {
				if (rv->rectf == NULL)
					rr->rectf = RE_RenderLayerGetPass(rl, RE_PASSNAME_COMBINED, rv->name);

				if (rv->rectz == NULL)
					rr->rectz = RE_RenderLayerGetPass(rl, RE_PASSNAME_Z, rv->name);
			}

			rr->layers = re->result->layers;
			rr->views = re->result->views;

			rr->xof = re->disprect.xmin;
			rr->yof = re->disprect.ymin;

			rr->stamp_data = re->result->stamp_data;
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
	const int view_id = BKE_scene_multiview_view_id_get(&re->r, re->viewname);

	RE_AcquireResultImageViews(re, &rres);
	render_result_rect_get_pixels(&rres, rect, re->rectx, re->recty, &re->scene->view_settings, &re->scene->display_settings, view_id);
	RE_ReleaseResultImageViews(re, &rres);
}

/* caller is responsible for allocating rect in correct size! */
/* Only for acquired results, for lock */
void RE_AcquiredResultGet32(Render *re, RenderResult *result, unsigned int *rect, const int view_id)
{
	render_result_rect_get_pixels(result, rect, re->rectx, re->recty, &re->scene->view_settings, &re->scene->display_settings, view_id);
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
		BLI_rw_mutex_init(&re->partsmutex);
	}

	RE_InitRenderCB(re);

	return re;
}

/* MAX_ID_NAME + sizeof(Library->name) + space + null-terminator. */
#define MAX_SCENE_RENDER_NAME (MAX_ID_NAME + 1024 + 2)

static void scene_render_name_get(const Scene *scene,
                                  const size_t max_size,
                                  char *render_name)
{
	if (ID_IS_LINKED(scene)) {
		BLI_snprintf(render_name, max_size, "%s %s",
		             scene->id.lib->id.name, scene->id.name);
	}
	else {
		BLI_snprintf(render_name, max_size, "%s", scene->id.name);
	}
}

Render *RE_GetSceneRender(const Scene *scene)
{
	char render_name[MAX_SCENE_RENDER_NAME];
	scene_render_name_get(scene, sizeof(render_name), render_name);
	return RE_GetRender(render_name);
}

Render *RE_NewSceneRender(const Scene *scene)
{
	char render_name[MAX_SCENE_RENDER_NAME];
	scene_render_name_get(scene, sizeof(render_name), render_name);
	return RE_NewRender(render_name);
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
	BLI_rw_mutex_end(&re->partsmutex);

	BLI_freelistN(&re->view_layers);
	BLI_freelistN(&re->r.views);

	curvemapping_free_data(&re->r.mblur_shutter_curve);

	/* main dbase can already be invalid now, some database-free code checks it */
	re->main = NULL;
	re->scene = NULL;

	render_result_free(re->result);
	render_result_free(re->pushedresult);

	BLI_remlink(&RenderGlobal.renderlist, re);
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

void RE_FreeAllPersistentData(void)
{
	Render *re;
	for (re = RenderGlobal.renderlist.first; re != NULL; re = re->next) {
		if ((re->r.mode & R_PERSISTENT_DATA) != 0 && re->engine != NULL) {
			RE_engine_free(re->engine);
			re->engine = NULL;
		}
	}
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

	/* not supported by any current renderer */
	scemode &= ~R_FULL_SAMPLE;

#ifdef WITH_OPENEXR
	if (scemode & R_FULL_SAMPLE)
		scemode |= R_EXR_TILE_FILE;   /* enable automatic */
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
		 * after insertion on black happening in do_render(),
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
}

void render_copy_renderdata(RenderData *to, RenderData *from)
{
	BLI_freelistN(&to->views);
	curvemapping_free_data(&to->mblur_shutter_curve);

	*to = *from;

	BLI_duplicatelist(&to->views, &from->views);
	curvemapping_copy_data(&to->mblur_shutter_curve, &from->mblur_shutter_curve);
}

/* what doesn't change during entire render sequence */
/* disprect is optional, if NULL it assumes full window render */
void RE_InitState(Render *re, Render *source, RenderData *rd,
                  ListBase *render_layers, ViewLayer *single_layer,
                  int winx, int winy, rcti *disprect)
{
	bool had_freestyle = (re->r.mode & R_EDGE_FRS) != 0;

	re->ok = true;   /* maybe flag */

	re->i.starttime = PIL_check_seconds_timer();

	/* copy render data and render layers for thread safety */
	render_copy_renderdata(&re->r, rd);
	BLI_freelistN(&re->view_layers);
	BLI_duplicatelist(&re->view_layers, render_layers);
	re->active_view_layer = 0;

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

	/* disable border if it's a full render anyway */
	if (re->r.border.xmin == 0.0f && re->r.border.xmax == 1.0f &&
	    re->r.border.ymin == 0.0f && re->r.border.ymax == 1.0f)
	{
		re->r.mode &= ~R_BORDER;
	}

	if (re->rectx < 1 || re->recty < 1 || (BKE_imtype_is_movie(rd->im_format.imtype) &&
	                                       (re->rectx < 16 || re->recty < 16) ))
	{
		BKE_report(re->reports, RPT_ERROR, "Image too small");
		re->ok = 0;
		return;
	}

	re->r.scemode = check_mode_full_sample(&re->r);

	if (single_layer) {
		int index = BLI_findindex(render_layers, single_layer);
		if (index != -1) {
			re->active_view_layer = index;
			re->r.scemode |= R_SINGLE_LAYER;
		}
	}

	/* if preview render, we try to keep old result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if (re->r.scemode & R_BUTS_PREVIEW) {
		if (had_freestyle || (re->r.mode & R_EDGE_FRS)) {
			/* freestyle manipulates render layers so always have to free */
			render_result_free(re->result);
			re->result = NULL;
		}
		else if (re->result) {
			ViewLayer *active_render_layer = BLI_findlink(&re->view_layers, re->active_view_layer);
			RenderLayer *rl;
			bool have_layer = false;

			for (rl = re->result->layers.first; rl; rl = rl->next)
				if (STREQ(rl->name, active_render_layer->name))
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
		render_result_view_new(re->result, "");
	}

	/* ensure renderdatabase can use part settings correct */
	RE_parts_clamp(re);

	BLI_rw_mutex_unlock(&re->resultmutex);

	RE_init_threadcount(re);

	RE_point_density_fix_linking();
}

/* This function is only called by view3d rendering, which doesn't support
 * multiview at the moment. so handle only one view here */
static void render_result_rescale(Render *re)
{
	RenderResult *result = re->result;
	RenderView *rv;
	int x, y;
	float scale_x, scale_y;
	float *src_rectf;

	rv = RE_RenderViewGetById(result, 0);
	src_rectf = rv->rectf;

	if (src_rectf == NULL) {
		RenderLayer *rl = render_get_active_layer(re, re->result);
		if (rl != NULL) {
			src_rectf = RE_RenderLayerGetPass(rl, RE_PASSNAME_COMBINED, NULL);
		}
	}

	if (src_rectf != NULL) {
		float *dst_rectf = NULL;
		re->result = render_result_new(re,
		                               &re->disprect,
		                               0,
		                               RR_USE_MEM,
		                               RR_ALL_LAYERS,
		                               "");

		if (re->result != NULL) {
			dst_rectf = RE_RenderViewGetById(re->result, 0)->rectf;
			if (dst_rectf == NULL) {
				RenderLayer *rl;
				rl = render_get_active_layer(re, re->result);
				if (rl != NULL) {
					dst_rectf = RE_RenderLayerGetPass(rl, RE_PASSNAME_COMBINED, NULL);
				}
			}

			scale_x = (float) result->rectx / re->result->rectx;
			scale_y = (float) result->recty / re->result->recty;
			for (x = 0; x < re->result->rectx; ++x) {
				for (y = 0; y < re->result->recty; ++y) {
					int src_x = x * scale_x;
					int src_y = y * scale_y;
					int dst_index = y * re->result->rectx + x;
					int src_index = src_y * result->rectx + src_x;
					copy_v4_v4(dst_rectf + dst_index * 4,
					           src_rectf + src_index * 4);
				}
			}
		}
		render_result_free(result);
	}
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
void render_update_anim_renderdata(Render *re, RenderData *rd, ListBase *render_layers)
{
	/* filter */
	re->r.gauss = rd->gauss;

	/* motion blur */
	re->r.blurfac = rd->blurfac;

	/* freestyle */
	re->r.line_thickness_mode = rd->line_thickness_mode;
	re->r.unit_line_thickness = rd->unit_line_thickness;

	/* render layers */
	BLI_freelistN(&re->view_layers);
	BLI_duplicatelist(&re->view_layers, render_layers);

	/* render views */
	BLI_freelistN(&re->r.views);
	BLI_duplicatelist(&re->r.views, &rd->views);
}

void RE_SetWindow(Render *re, const rctf *viewplane, float clipsta, float clipend)
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

void RE_SetOrtho(Render *re, const rctf *viewplane, float clipsta, float clipend)
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

void RE_GetViewPlane(Render *re, rctf *r_viewplane, rcti *r_disprect)
{
	*r_viewplane = re->viewplane;

	/* make disprect zero when no border render, is needed to detect changes in 3d view render */
	if (re->r.mode & R_BORDER) {
		*r_disprect = re->disprect;
	}
	else {
		BLI_rcti_init(r_disprect, 0, 0, 0, 0);
	}
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

/* ********* GL Context ******** */

void RE_gl_context_create(Render *re)
{
	/* Needs to be created in the main ogl thread. */
	re->gl_context = WM_opengl_context_create();
	/* So we activate the window's one afterwards. */
	wm_window_reset_drawable();
}

void RE_gl_context_destroy(Render *re)
{
	/* Needs to be called from the thread which used the ogl context for rendering. */
	if (re->gwn_context) {
		GWN_context_active_set(re->gwn_context);
		GWN_context_discard(re->gwn_context);
		re->gwn_context = NULL;
	}
	if (re->gl_context) {
		WM_opengl_context_dispose(re->gl_context);
		re->gl_context = NULL;
	}
}

void *RE_gl_context_get(Render *re)
{
	return re->gl_context;
}

void *RE_gwn_context_get(Render *re)
{
	if (re->gwn_context == NULL) {
		re->gwn_context = GWN_context_create();
	}
	return re->gwn_context;
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

#ifdef WITH_FREESTYLE
static void init_freestyle(Render *re);
static void add_freestyle(Render *re, int render);
static void free_all_freestyle_renders(void);
#endif


/* ************  This part uses API, for rendering Blender scenes ********** */

static void do_render_3d(Render *re)
{
	re->current_scene_update(re->suh, re->scene);
	RE_engine_render(re, 0);
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

static void render_result_uncrop(Render *re)
{
	/* when using border render with crop disabled, insert render result into
	 * full size with black pixels outside */
	if (re->result && (re->r.mode & R_BORDER)) {
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

			rres = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS, RR_ALL_VIEWS);

			render_result_clone_passes(re, rres, NULL);

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

/* main render routine, no compositing */
static void do_render(Render *re)
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

	do_render_3d(re);

	/* when border render, check if we have to insert it in black */
	render_result_uncrop(re);
}


/* within context of current Render *re, render another scene.
 * it uses current render image size and disprect, but doesn't execute composite
 */
static void render_scene(Render *re, Scene *sce, int cfra)
{
	Render *resc = RE_NewSceneRender(sce);
	int winx = re->winx, winy = re->winy;

	sce->r.cfra = cfra;

	BKE_scene_camera_switch_update(sce);

	/* exception: scene uses own size (unfinished code) */
	if (0) {
		winx = (sce->r.size * sce->r.xsch) / 100;
		winy = (sce->r.size * sce->r.ysch) / 100;
	}

	/* initial setup */
	RE_InitState(resc, re, &sce->r, &sce->view_layers, NULL, winx, winy, &re->disprect);

	/* We still want to use 'rendercache' setting from org (main) scene... */
	resc->r.scemode = (resc->r.scemode & ~R_EXR_CACHE_FILE) | (re->r.scemode & R_EXR_CACHE_FILE);

	/* still unsure entity this... */
	resc->main = re->main;
	resc->scene = sce;
	resc->lay = sce->lay;

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

	do_render(resc);
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

static void ntree_render_scenes(Render *re)
{
	bNode *node;
	int cfra = re->scene->r.cfra;
	Scene *restore_scene = re->scene;
	bool scene_changed = false;

	if (re->scene->nodetree == NULL) return;

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
static void render_composit_stats(void *arg, const char *str)
{
	Render *re = (Render*)arg;

	RenderStats i;
	memcpy(&i, &re->i, sizeof(i));
	i.infostr = str;
	re->stats_draw(re->sdh, &i);
}

#ifdef WITH_FREESTYLE
/* init Freestyle renderer */
static void init_freestyle(Render *re)
{
	re->freestyle_bmain = BKE_main_new();

	/* We use the same window manager for freestyle bmain as
	 * real bmain uses. This is needed because freestyle's
	 * bmain could be used to tag scenes for update, which
	 * implies call of ED_render_scene_update in some cases
	 * and that function requires proper window manager
	 * to present (sergey)
	 */
	re->freestyle_bmain->wm = re->main->wm;

	FRS_init_stroke_renderer(re);
}

/* invokes Freestyle stroke rendering */
static void add_freestyle(Render *re, int render)
{
	ViewLayer *view_layer, *active_view_layer;
	LinkData *link;
	Render *r;

	active_view_layer = BLI_findlink(&re->view_layers, re->active_view_layer);

	FRS_begin_stroke_rendering(re);

	for (view_layer = (ViewLayer *)re->view_layers.first; view_layer; view_layer = view_layer->next) {
		link = (LinkData *)MEM_callocN(sizeof(LinkData), "LinkData to Freestyle render");
		BLI_addtail(&re->freestyle_renders, link);

		if ((re->r.scemode & R_SINGLE_LAYER) && view_layer != active_view_layer)
			continue;
		if (FRS_is_freestyle_enabled(view_layer)) {
			r = FRS_do_stroke_rendering(re, view_layer, render);
			link->data = (void *)r;
		}
	}

	FRS_end_stroke_rendering(re);
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
				BKE_libblock_unlink(re1->freestyle_bmain, freestyle_scene, false, false);
				BKE_libblock_free(re1->freestyle_bmain, freestyle_scene);
			}
		}
		BLI_freelistN(&re1->freestyle_renders);

		if (re1->freestyle_bmain) {
			/* detach the window manager from freestyle bmain (see comments
			 * in add_freestyle() for more detail)
			 */
			BLI_listbase_clear(&re1->freestyle_bmain->wm);

			BKE_main_free(re1->freestyle_bmain);
			re1->freestyle_bmain = NULL;
		}
	}
}
#endif

/* returns fully composited render-result on given time step (in RenderData) */
static void do_render_composite(Render *re)
{
	bNodeTree *ntree = re->scene->nodetree;
	int update_newframe = 0;

	if (composite_needs_render(re->scene, 1)) {
		/* save memory... free all cached images */
		ntreeFreeCache(ntree);

		/* render the frames
		 * it could be optimized to render only the needed view
		 * but what if a scene has a different number of views
		 * than the main scene? */
		do_render(re);
	}
	else {
		re->i.cfra = re->r.cfra;

		/* ensure new result gets added, like for regular renders */
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

		render_result_free(re->result);
		if ((re->r.mode & R_CROP) == 0) {
			render_result_disprect_to_full_resolution(re);
		}
		re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS, RR_ALL_VIEWS);

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
				ntree->sdh = re;
				ntree->tbh = re->tbh;
				ntree->prh = re->prh;

				if (update_newframe) {
					/* If we have consistent depsgraph now would be a time to update them. */
				}

				RenderView *rv;
				for (rv = re->result->views.first; rv; rv = rv->next) {
					ntreeCompositExecTree(re->scene, ntree, &re->r, true, G.background == 0, &re->scene->view_settings, &re->scene->display_settings, rv->name);
				}

				ntree->stats_draw = NULL;
				ntree->test_break = NULL;
				ntree->progress = NULL;
				ntree->tbh = ntree->sdh = ntree->prh = NULL;
			}
		}
	}

#ifdef WITH_FREESTYLE
	free_all_freestyle_renders();
#endif

	/* weak... the display callback wants an active renderlayer pointer... */
	if (re->result != NULL) {
		re->result->renlay = render_get_active_layer(re, re->result);
		re->display_update(re->duh, re->result, NULL);
	}
}

static void renderresult_stampinfo(Render *re)
{
	RenderResult rres;
	RenderView *rv;
	int nr;

	/* this is the basic trick to get the displayed float or char rect from render result */
	nr = 0;
	for (rv = re->result->views.first;rv;rv = rv->next, nr++) {
		RE_SetActiveRenderView(re, rv->name);
		RE_AcquireResultImage(re, &rres, nr);
		BKE_image_stamp_buf(re->scene,
		                    RE_GetCamera(re),
		                    (re->r.stamp & R_STAMP_STRIPMETA) ? rres.stamp_data : NULL,
		                    (unsigned char *)rres.rect32,
		                    rres.rectf,
		                    rres.rectx, rres.recty,
		                    4);
		RE_ReleaseResultImage(re);
	}
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
	struct ImBuf *out;
	RenderResult *rr; /* don't assign re->result here as it might change during give_ibuf_seq */
	int cfra = re->r.cfra;
	SeqRenderData context;
	int view_id, tot_views;
	struct ImBuf **ibuf_arr;
	int re_x, re_y;

	re->i.cfra = cfra;

	if (recurs_depth == 0) {
		/* otherwise sequencer animation isn't updated */
		/* TODO(sergey): Currently depsgraph is only used to check whether it is an active
		 * edit window or not to deal with unkeyed changes. We don't have depsgraph here yet,
		 * but we also dont' deal with unkeyed changes. But still nice to get proper depsgraph
		 * within tjhe render pipeline, somehow.
		 */
		BKE_animsys_evaluate_all_animation(re->main, NULL, re->scene, (float)cfra); // XXX, was BKE_scene_frame_get(re->scene)
	}

	recurs_depth++;

	if ((re->r.mode & R_BORDER) && (re->r.mode & R_CROP) == 0) {
		/* if border rendering is used and cropping is disabled, final buffer should
		 * be as large as the whole frame */
		re_x = re->winx;
		re_y = re->winy;
	}
	else {
		re_x = re->result->rectx;
		re_y = re->result->recty;
	}

	tot_views = BKE_scene_multiview_num_views_get(&re->r);
	ibuf_arr = MEM_mallocN(sizeof(ImBuf *) * tot_views, "Sequencer Views ImBufs");

	/* TODO(sergey): Currently depsgraph is only used to check whether it is an active
	 * edit window or not to deal with unkeyed changes. We don't have depsgraph here yet,
	 * but we also dont' deal with unkeyed changes. But still nice to get proper depsgraph
	 * within tjhe render pipeline, somehow.
	 */
	BKE_sequencer_new_render_data(
	        re->main, NULL, re->scene,
	        re_x, re_y, 100, true,
	        &context);

	/* the renderresult gets destroyed during the rendering, so we first collect all ibufs
	 * and then we populate the final renderesult */

	for (view_id = 0; view_id < tot_views; view_id++) {
		context.view_id = view_id;
		out = BKE_sequencer_give_ibuf(&context, cfra, 0);

		if (out) {
			ibuf_arr[view_id] = IMB_dupImBuf(out);
			IMB_metadata_copy(ibuf_arr[view_id], out);
			IMB_freeImBuf(out);
			BKE_sequencer_imbuf_from_sequencer_space(re->scene, ibuf_arr[view_id]);
		}
		else {
			ibuf_arr[view_id] = NULL;
		}
	}

	rr = re->result;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	render_result_views_new(rr, &re->r);
	BLI_rw_mutex_unlock(&re->resultmutex);

	for (view_id = 0; view_id < tot_views; view_id++) {
		RenderView *rv = RE_RenderViewGetById(rr, view_id);
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

		if (ibuf_arr[view_id]) {
			/* copy ibuf into combined pixel rect */
			RE_render_result_rect_from_ibuf(rr, &re->r, ibuf_arr[view_id], view_id);

			if (ibuf_arr[view_id]->metadata && (re->r.stamp & R_STAMP_STRIPMETA)) {
				/* ensure render stamp info first */
				BKE_render_result_stamp_info(NULL, NULL, rr, true);
				BKE_stamp_info_from_imbuf(rr, ibuf_arr[view_id]);
			}

			if (recurs_depth == 0) { /* with nested scenes, only free on toplevel... */
				Editing *ed = re->scene->ed;
				if (ed)
					BKE_sequencer_free_imbuf(re->scene, &ed->seqbase, true);
			}
			IMB_freeImBuf(ibuf_arr[view_id]);
		}
		else {
			/* render result is delivered empty in most cases, nevertheless we handle all cases */
			render_result_rect_fill_zero(rr, view_id);
		}

		BLI_rw_mutex_unlock(&re->resultmutex);

		/* would mark display buffers as invalid */
		RE_SetActiveRenderView(re, rv->name);
		re->display_update(re->duh, re->result, NULL);
	}

	MEM_freeN(ibuf_arr);

	recurs_depth--;

	/* just in case this flag went missing at some point */
	re->r.scemode |= R_DOSEQ;

	/* set overall progress of sequence rendering */
	if (re->r.efra != re->r.sfra)
		re->progress(re->prh, (float)(cfra - re->r.sfra) / (re->r.efra - re->r.sfra));
	else
		re->progress(re->prh, 1.0f);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* main loop: doing sequence + 3d render + compositing */
static void do_render_all_options(Render *re)
{
	Object *camera;
	bool render_seq = false;

	re->current_scene_update(re->suh, re->scene);

	BKE_scene_camera_switch_update(re->scene);

	re->i.starttime = PIL_check_seconds_timer();

	/* ensure no images are in memory from previous animated sequences */
	BKE_image_all_free_anim_ibufs(re->main, re->r.cfra);
	BKE_sequencer_all_free_anim_ibufs(re->main, re->r.cfra);

	if (RE_engine_render(re, 1)) {
		/* in this case external render overrides all */
	}
	else if (RE_seq_render_active(re->scene, &re->r)) {
		/* note: do_render_seq() frees rect32 when sequencer returns float images */
		if (!re->test_break(re->tbh)) {
			do_render_seq(re);
			render_seq = true;
		}

		re->stats_draw(re->sdh, &re->i);
		re->display_update(re->duh, re->result, NULL);
	}
	else {
		do_render_composite(re);
	}

	re->i.lastframetime = PIL_check_seconds_timer() - re->i.starttime;

	re->stats_draw(re->sdh, &re->i);

	/* save render result stamp if needed */
	if (re->result != NULL) {
		camera = RE_GetCamera(re);
		/* sequence rendering should have taken care of that already */
		if (!(render_seq && (re->r.stamp & R_STAMP_STRIPMETA)))
			BKE_render_result_stamp_info(re->scene, camera, re->result, false);

		/* stamp image info here */
		if ((re->r.stamp & R_STAMP_ALL) && (re->r.stamp & R_STAMP_DRAW)) {
			renderresult_stampinfo(re);
			re->display_update(re->duh, re->result, NULL);
		}
	}
}

static bool check_valid_compositing_camera(Scene *scene, Object *camera_override)
{
	if (scene->r.scemode & R_DOCOMP && scene->use_nodes) {
		bNode *node = scene->nodetree->nodes.first;

		while (node) {
			if (node->type == CMP_NODE_R_LAYERS && (node->flag & NODE_MUTED) == 0) {
				Scene *sce = node->id ? (Scene *)node->id : scene;
				if (sce->camera == NULL) {
					sce->camera = BKE_view_layer_camera_find(BKE_view_layer_default_render(sce));
				}
				if (sce->camera == NULL) {
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

static bool check_valid_camera_multiview(Scene *scene, Object *camera, ReportList *reports)
{
	SceneRenderView *srv;
	bool active_view = false;

	if (camera == NULL || (scene->r.scemode & R_MULTIVIEW) == 0)
		return true;

	for (srv = scene->r.views.first; srv; srv = srv->next) {
		if (BKE_scene_multiview_is_render_view_active(&scene->r, srv)) {
			active_view = true;

			if (scene->r.views_format == SCE_VIEWS_FORMAT_MULTIVIEW) {
				Object *view_camera;
				view_camera = BKE_camera_multiview_render(scene, camera, srv->name);

				if (view_camera == camera) {
					/* if the suffix is not in the camera, means we are using the fallback camera */
					if (!BLI_str_endswith(view_camera->id.name + 2, srv->suffix)) {
						BKE_reportf(reports, RPT_ERROR, "Camera \"%s\" is not a multi-view camera",
						            camera->id.name + 2);
						return false;
					}
				}
			}
		}
	}

	if (!active_view) {
		BKE_reportf(reports, RPT_ERROR, "No active view found in scene \"%s\"", scene->id.name + 2);
		return false;
	}

	return true;
}

static int check_valid_camera(Scene *scene, Object *camera_override, ReportList *reports)
{
	const char *err_msg = "No camera found in scene \"%s\"";

	if (camera_override == NULL && scene->camera == NULL)
		scene->camera = BKE_view_layer_camera_find(BKE_view_layer_default_render(scene));

	if (!check_valid_camera_multiview(scene, scene->camera, reports))
		return false;

	if (RE_seq_render_active(scene, &scene->r)) {
		if (scene->ed) {
			Sequence *seq = scene->ed->seqbase.first;

			while (seq) {
				if ((seq->type == SEQ_TYPE_SCENE) &&
				    ((seq->flag & SEQ_SCENE_STRIPS) == 0) &&
				    (seq->scene != NULL))
				{
					if (!seq->scene_camera) {
						if (!seq->scene->camera &&
						    !BKE_view_layer_camera_find(BKE_view_layer_default_render(seq->scene)))
						{
							/* camera could be unneeded due to composite nodes */
							Object *override = (seq->scene == scene) ? camera_override : NULL;

							if (!check_valid_compositing_camera(seq->scene, override)) {
								BKE_reportf(reports, RPT_ERROR, err_msg, seq->scene->id.name + 2);
								return false;
							}
						}
					}
					else if (!check_valid_camera_multiview(seq->scene, seq->scene_camera, reports))
						return false;
				}

				seq = seq->next;
			}
		}
	}
	else if (!check_valid_compositing_camera(scene, camera_override)) {
		BKE_reportf(reports, RPT_ERROR, err_msg, scene->id.name + 2);
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

bool RE_is_rendering_allowed(Scene *scene, ViewLayer *single_layer, Object *camera_override, ReportList *reports)
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

	if (RE_seq_render_active(scene, &scene->r)) {
		if (scene->r.mode & R_BORDER) {
			BKE_report(reports, RPT_ERROR, "Border rendering is not supported by sequencer");
			return false;
		}
	}

	/* layer flag tests */
	if (!render_scene_has_layers_to_render(scene, single_layer)) {
		BKE_report(reports, RPT_ERROR, "All render layers are disabled");
		return 0;
	}

	return 1;
}

static void validate_render_settings(Render *re)
{
	if (RE_engine_is_external(re)) {
		/* not supported yet */
		re->r.scemode &= ~(R_FULL_SAMPLE);
	}
}

static void update_physics_cache(Render *re, Scene *scene, ViewLayer *view_layer, int UNUSED(anim_init))
{
	PTCacheBaker baker;

	memset(&baker, 0, sizeof(baker));
	baker.bmain = re->main;
	baker.scene = scene;
	baker.view_layer = view_layer;
	baker.depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
	baker.bake = 0;
	baker.render = 1;
	baker.anim_init = 1;
	baker.quick_step = 1;

	BKE_ptcache_bake(&baker);
}

void RE_SetActiveRenderView(Render *re, const char *viewname)
{
	BLI_strncpy(re->viewname, viewname, sizeof(re->viewname));
}

const char *RE_GetActiveRenderView(Render *re)
{
	return re->viewname;
}

/* evaluating scene options for general Blender render */
static int render_initialize_from_main(Render *re, RenderData *rd, Main *bmain, Scene *scene,
                                       ViewLayer *single_layer, Object *camera_override, unsigned int lay_override,
                                       int anim, int anim_init)
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
	re->camera_override = camera_override;
	re->lay = lay_override ? lay_override : scene->lay;
	re->layer_override = lay_override;
	re->i.localview = (re->lay & 0xFF000000) != 0;
	re->viewname[0] = '\0';

	/* not too nice, but it survives anim-border render */
	if (anim) {
		render_update_anim_renderdata(re, &scene->r, &scene->view_layers);
		re->disprect = disprect;
		return 1;
	}

	/*
	 * Disabled completely for now,
	 * can be later set as render profile option
	 * and default for background render.
	 */
	if (0) {
		/* make sure dynamics are up to date */
		ViewLayer *view_layer = BKE_view_layer_context_active_PLACEHOLDER(scene);
		update_physics_cache(re, scene, view_layer, anim_init);
	}

	if (single_layer || scene->r.scemode & R_SINGLE_LAYER) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_single_layer_begin(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}

	RE_InitState(re, NULL, &scene->r, &scene->view_layers, single_layer, winx, winy, &disprect);
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
void RE_BlenderFrame(Render *re, Main *bmain, Scene *scene, ViewLayer *single_layer, Object *camera_override,
                     unsigned int lay_override, int frame, const bool write_still)
{
	BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_INIT);

	/* ugly global still... is to prevent preview events and signal subsurfs etc to make full resol */
	G.is_rendering = true;

	scene->r.cfra = frame;

	if (render_initialize_from_main(re, &scene->r, bmain, scene, single_layer,
	                                camera_override, lay_override, 0, 0))
	{
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
				BKE_image_path_from_imformat(
				        name, scene->r.pic, BKE_main_blendfile_path(bmain), scene->r.cfra,
				        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, false, NULL);

				/* reports only used for Movie */
				do_write_image_or_movie(re, bmain, scene, NULL, 0, name);
			}
		}

		BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
		if (write_still) {
			BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_WRITE);
		}
	}

	BLI_callback_exec(re->main, (ID *)scene, G.is_break ? BLI_CB_EVT_RENDER_CANCEL : BLI_CB_EVT_RENDER_COMPLETE);

	/* Destroy the opengl context in the correct thread. */
	RE_gl_context_destroy(re);

	/* UGLY WARNING */
	G.is_rendering = false;
}

#ifdef WITH_FREESTYLE
void RE_RenderFreestyleStrokes(Render *re, Main *bmain, Scene *scene, int render)
{
	re->result_ok= 0;
	if (render_initialize_from_main(re, &scene->r, bmain, scene, NULL, NULL, scene->lay, 0, 0)) {
		if (render)
			do_render_3d(re);
	}
	re->result_ok = 1;
}

void RE_RenderFreestyleExternal(Render *re)
{
	if (!re->test_break(re->tbh)) {
		RenderView *rv;

		init_freestyle(re);

		for (rv = re->result->views.first; rv; rv = rv->next) {
			RE_SetActiveRenderView(re, rv->name);

			/* scene needs to be set to get camera */
			Object *camera = RE_GetCamera(re);

			/* if no camera, viewmat should have been set! */
			if (camera) {
				/* called before but need to call again in case of lens animation from the
				 * above call to BKE_scene_graph_update_for_newframe, fixes bug. [#22702].
				 * following calls don't depend on 'RE_SetCamera' */
				float mat[4][4];

				RE_SetCamera(re, camera);
				RE_GetCameraModelMatrix(re, camera, mat);
				invert_m4(mat);
				RE_SetView(re, mat);

				/* force correct matrix for scaled cameras */
				DEG_id_tag_update_ex(re->main, &camera->id, OB_RECALC_OB);
			}

			printf("add freestyle\n");

			add_freestyle(re, 1);
		}
	}
}
#endif

bool RE_WriteRenderViewsImage(ReportList *reports, RenderResult *rr, Scene *scene, const bool stamp, char *name)
{
	bool ok = true;
	RenderData *rd = &scene->r;

	if (!rr)
		return false;

	bool is_mono = BLI_listbase_count_at_most(&rr->views, 2) < 2;
	bool is_exr_rr = ELEM(rd->im_format.imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER) &&
	                 RE_HasFloatPixels(rr);

	if (rd->im_format.views_format == R_IMF_VIEWS_MULTIVIEW && is_exr_rr)
	{
		ok = RE_WriteRenderResult(reports, rr, name, &rd->im_format, NULL, -1);
		render_print_save_message(reports, name, ok, errno);
	}

	/* mono, legacy code */
	else if (is_mono || (rd->im_format.views_format == R_IMF_VIEWS_INDIVIDUAL))
	{
		RenderView *rv;
		int view_id;
		char filepath[FILE_MAX];

		BLI_strncpy(filepath, name, sizeof(filepath));

		for (view_id = 0, rv = rr->views.first; rv; rv = rv->next, view_id++) {
			if (!is_mono) {
				BKE_scene_multiview_view_filepath_get(&scene->r, filepath, rv->name, name);
			}

			if (is_exr_rr) {
				ok = RE_WriteRenderResult(reports, rr, name, &rd->im_format, rv->name, -1);
				render_print_save_message(reports, name, ok, errno);

				/* optional preview images for exr */
				if (ok && (rd->im_format.flag & R_IMF_FLAG_PREVIEW_JPG)) {
					ImageFormatData imf = rd->im_format;
					imf.imtype = R_IMF_IMTYPE_JPEG90;

					if (BLI_path_extension_check(name, ".exr"))
						name[strlen(name) - 4] = 0;
					BKE_image_path_ensure_ext_from_imformat(name, &imf);

					ImBuf *ibuf = render_result_rect_to_ibuf(rr, rd, view_id);
					ibuf->planes = 24;

					ok = render_imbuf_write_stamp_test(reports, scene, rr, ibuf, name, &imf, stamp);

					IMB_freeImBuf(ibuf);
				}
			}
			else {
				ImBuf *ibuf = render_result_rect_to_ibuf(rr, rd, view_id);

				IMB_colormanagement_imbuf_for_write(ibuf, true, false, &scene->view_settings,
				                                    &scene->display_settings, &rd->im_format);

				ok = render_imbuf_write_stamp_test(reports, scene, rr, ibuf, name, &rd->im_format, stamp);

				/* imbuf knows which rects are not part of ibuf */
				IMB_freeImBuf(ibuf);
			}
		}
	}
	else { /* R_IMF_VIEWS_STEREO_3D */
		BLI_assert(scene->r.im_format.views_format == R_IMF_VIEWS_STEREO_3D);

		if (rd->im_format.imtype == R_IMF_IMTYPE_MULTILAYER) {
			printf("Stereo 3D not supported for MultiLayer image: %s\n", name);
		}
		else {
			ImBuf *ibuf_arr[3] = {NULL};
			const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
			int i;

			for (i = 0; i < 2; i++) {
				int view_id = BLI_findstringindex(&rr->views, names[i], offsetof(RenderView, name));
				ibuf_arr[i] = render_result_rect_to_ibuf(rr, rd, view_id);
				IMB_colormanagement_imbuf_for_write(ibuf_arr[i], true, false, &scene->view_settings,
				                                    &scene->display_settings, &scene->r.im_format);
				IMB_prepare_write_ImBuf(IMB_isfloat(ibuf_arr[i]), ibuf_arr[i]);
			}

			ibuf_arr[2] = IMB_stereo3d_ImBuf(&scene->r.im_format, ibuf_arr[0], ibuf_arr[1]);

			ok = render_imbuf_write_stamp_test(reports, scene, rr, ibuf_arr[2], name, &rd->im_format, stamp);

			/* optional preview images for exr */
			if (ok && is_exr_rr &&
			    (rd->im_format.flag & R_IMF_FLAG_PREVIEW_JPG))
			{
				ImageFormatData imf = rd->im_format;
				imf.imtype = R_IMF_IMTYPE_JPEG90;

				if (BLI_path_extension_check(name, ".exr"))
					name[strlen(name) - 4] = 0;

				BKE_image_path_ensure_ext_from_imformat(name, &imf);
				ibuf_arr[2]->planes = 24;

				ok = render_imbuf_write_stamp_test(reports, scene, rr, ibuf_arr[2], name, &rd->im_format, stamp);
			}

			/* imbuf knows which rects are not part of ibuf */
			for (i = 0; i < 3; i++) {
				IMB_freeImBuf(ibuf_arr[i]);
			}
		}
	}

	return ok;
}

bool RE_WriteRenderViewsMovie(
        ReportList *reports, RenderResult *rr, Scene *scene, RenderData *rd, bMovieHandle *mh,
        void **movie_ctx_arr, const int totvideos, bool preview)
{
	bool is_mono;
	bool ok = true;

	if (!rr)
		return false;

	is_mono = BLI_listbase_count_at_most(&rr->views, 2) < 2;

	if (is_mono || (scene->r.im_format.views_format == R_IMF_VIEWS_INDIVIDUAL)) {
		int view_id;
		for (view_id = 0; view_id < totvideos; view_id++) {
			const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, view_id);
			ImBuf *ibuf = render_result_rect_to_ibuf(rr, &scene->r, view_id);

			IMB_colormanagement_imbuf_for_write(ibuf, true, false, &scene->view_settings,
			                                    &scene->display_settings, &scene->r.im_format);

			ok &= mh->append_movie(movie_ctx_arr[view_id], rd, preview ? scene->r.psfra : scene->r.sfra, scene->r.cfra,
			                       (int *) ibuf->rect, ibuf->x, ibuf->y, suffix, reports);

			/* imbuf knows which rects are not part of ibuf */
			IMB_freeImBuf(ibuf);
		}
		printf("Append frame %d\n", scene->r.cfra);
	}
	else { /* R_IMF_VIEWS_STEREO_3D */
		const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
		ImBuf *ibuf_arr[3] = {NULL};
		int i;

		BLI_assert((totvideos == 1) && (scene->r.im_format.views_format == R_IMF_VIEWS_STEREO_3D));

		for (i = 0; i < 2; i++) {
			int view_id = BLI_findstringindex(&rr->views, names[i], offsetof(RenderView, name));
			ibuf_arr[i] = render_result_rect_to_ibuf(rr, &scene->r, view_id);

			IMB_colormanagement_imbuf_for_write(ibuf_arr[i], true, false, &scene->view_settings,
			                                    &scene->display_settings, &scene->r.im_format);
		}

		ibuf_arr[2] = IMB_stereo3d_ImBuf(&scene->r.im_format, ibuf_arr[0], ibuf_arr[1]);

		ok = mh->append_movie(movie_ctx_arr[0], rd, preview ? scene->r.psfra : scene->r.sfra, scene->r.cfra, (int *) ibuf_arr[2]->rect,
		                      ibuf_arr[2]->x, ibuf_arr[2]->y, "", reports);

		for (i = 0; i < 3; i++) {
			/* imbuf knows which rects are not part of ibuf */
			IMB_freeImBuf(ibuf_arr[i]);
		}
	}

	return ok;
}

static int do_write_image_or_movie(Render *re, Main *bmain, Scene *scene, bMovieHandle *mh, const int totvideos, const char *name_override)
{
	char name[FILE_MAX];
	RenderResult rres;
	double render_time;
	bool ok = true;

	RE_AcquireResultImageViews(re, &rres);

	/* write movie or image */
	if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		RE_WriteRenderViewsMovie(re->reports, &rres, scene, &re->r, mh, re->movie_ctx_arr, totvideos, false);
	}
	else {
		if (name_override)
			BLI_strncpy(name, name_override, sizeof(name));
		else
			BKE_image_path_from_imformat(
			        name, scene->r.pic, BKE_main_blendfile_path(bmain), scene->r.cfra,
			        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true, NULL);

		/* write images as individual images or stereo */
		ok = RE_WriteRenderViewsImage(re->reports, &rres, scene, true, name);
	}

	RE_ReleaseResultImageViews(re, &rres);

	render_time = re->i.lastframetime;
	re->i.lastframetime = PIL_check_seconds_timer() - re->i.starttime;

	BLI_timecode_string_from_time_simple(name, sizeof(name), re->i.lastframetime);
	printf(" Time: %s", name);

	/* Flush stdout to be sure python callbacks are printing stuff after blender. */
	fflush(stdout);

	/* NOTE: using G_MAIN seems valid here??? Not sure it's actually even used anyway, we could as well pass NULL? */
	BLI_callback_exec(G_MAIN, NULL, BLI_CB_EVT_RENDER_STATS);

	BLI_timecode_string_from_time_simple(name, sizeof(name), re->i.lastframetime - render_time);
	printf(" (Saving: %s)\n", name);

	fputc('\n', stdout);
	fflush(stdout); /* needed for renderd !! (not anymore... (ton)) */

	return ok;
}

static void get_videos_dimensions(
        Render *re, RenderData *rd,
        size_t *r_width, size_t *r_height)
{
	size_t width, height;
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

	BKE_scene_multiview_videos_dimensions_get(rd, width, height, r_width, r_height);
}

static void re_movie_free_all(Render *re, bMovieHandle *mh, int totvideos)
{
	int i;

	for (i = 0; i < totvideos; i++) {
		mh->end_movie(re->movie_ctx_arr[i]);
		mh->context_free(re->movie_ctx_arr[i]);
	}

	MEM_SAFE_FREE(re->movie_ctx_arr);
}

/* saves images to disk */
void RE_BlenderAnim(Render *re, Main *bmain, Scene *scene, Object *camera_override,
                    unsigned int lay_override, int sfra, int efra, int tfra)
{
	RenderData rd = scene->r;
	bMovieHandle *mh = NULL;
	int cfrao = scene->r.cfra;
	int nfra, totrendered = 0, totskipped = 0;
	const int totvideos = BKE_scene_multiview_num_videos_get(&rd);
	const bool is_movie = BKE_imtype_is_movie(scene->r.im_format.imtype);
	const bool is_multiview_name = ((scene->r.scemode & R_MULTIVIEW) != 0 &&
	                                (scene->r.im_format.views_format == R_IMF_VIEWS_INDIVIDUAL));

	BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_INIT);

	/* do not fully call for each frame, it initializes & pops output window */
	if (!render_initialize_from_main(re, &rd, bmain, scene, NULL, camera_override, lay_override, 0, 1))
		return;

	if (is_movie) {
		size_t width, height;
		int i;
		bool is_error = false;

		get_videos_dimensions(re, &rd, &width, &height);

		mh = BKE_movie_handle_get(scene->r.im_format.imtype);
		if (mh == NULL) {
			BKE_report(re->reports, RPT_ERROR, "Movie format unsupported");
			return;
		}

		re->movie_ctx_arr = MEM_mallocN(sizeof(void *) * totvideos, "Movies' Context");

		for (i = 0; i < totvideos; i++) {
			const char *suffix = BKE_scene_multiview_view_id_suffix_get(&re->r, i);

			re->movie_ctx_arr[i] = mh->context_create();

			if (!mh->start_movie(re->movie_ctx_arr[i], scene, &re->r, width, height, re->reports, false, suffix)) {
				is_error = true;
				break;
			}
		}

		if (is_error) {
			/* report is handled above */
			re_movie_free_all(re, mh, i + 1);
			return;
		}
	}

	/* ugly global still... is to prevent renderwin events and signal subsurfs etc to make full resol */
	/* is also set by caller renderwin.c */
	G.is_rendering = true;

	re->flag |= R_ANIMATION;

	{
		for (nfra = sfra, scene->r.cfra = sfra; scene->r.cfra <= efra; scene->r.cfra++) {
			char name[FILE_MAX];

			/* Special case for 'mh->get_next_frame'
			 * this overrides regular frame stepping logic */
			if (mh && mh->get_next_frame) {
				while (G.is_break == false) {
					int nfra_test = mh->get_next_frame(re->movie_ctx_arr[0], &re->r, re->reports);
					if (nfra_test >= 0 && nfra_test >= sfra && nfra_test <= efra) {
						nfra = nfra_test;
						break;
					}
					else {
						if (re->test_break(re->tbh)) {
							G.is_break = true;
						}
					}
				}
			}

			/* Here is a feedback loop exists -- render initialization requires updated
			 * render layers settings which could be animated, but scene evaluation for
			 * the frame happens later because it depends on what layers are visible to
			 * render engine.
			 *
			 * The idea here is to only evaluate animation data associated with the scene,
			 * which will make sure render layer settings are up-to-date, initialize the
			 * render database itself and then perform full scene update with only needed
			 * layers.
			 *                                                              -sergey-
			 */
			{
				float ctime = BKE_scene_frame_get(scene);
				AnimData *adt = BKE_animdata_from_id(&scene->id);
				/* TODO(sergey): Currently depsgraph is only used to check whether it is an active
				 * edit window or not to deal with unkeyed changes. We don't have depsgraph here yet,
				 * but we also dont' deal with unkeyed changes. But still nice to get proper depsgraph
				 * within tjhe render pipeline, somehow.
				 */
				BKE_animsys_evaluate_animdata(NULL, scene, &scene->id, adt, ctime, ADT_RECALC_ALL);
			}

			/* only border now, todo: camera lens. (ton) */
			render_initialize_from_main(re, &rd, bmain, scene,
			                            NULL, camera_override, lay_override, 1, 0);

			if (nfra != scene->r.cfra) {
				/* Skip this frame, but could update for physics and particles system. */
				continue;
			}
			else
				nfra += tfra;

			/* Touch/NoOverwrite options are only valid for image's */
			if (is_movie == false) {
				if (scene->r.mode & (R_NO_OVERWRITE | R_TOUCH))
					BKE_image_path_from_imformat(
					        name, scene->r.pic, BKE_main_blendfile_path(bmain), scene->r.cfra,
					        &scene->r.im_format, (scene->r.scemode & R_EXTENSION) != 0, true, NULL);

				if (scene->r.mode & R_NO_OVERWRITE) {
					if (!is_multiview_name) {
						if (BLI_exists(name)) {
							printf("skipping existing frame \"%s\"\n", name);
							totskipped++;
							continue;
						}
					}
					else {
						SceneRenderView *srv;
						bool is_skip = false;
						char filepath[FILE_MAX];

						for (srv = scene->r.views.first; srv; srv = srv->next) {
							if (!BKE_scene_multiview_is_render_view_active(&scene->r, srv))
								continue;

							BKE_scene_multiview_filepath_get(srv, name, filepath);

							if (BLI_exists(filepath)) {
								is_skip = true;
								printf("skipping existing frame \"%s\" for view \"%s\"\n", filepath, srv->name);
							}
						}

						if (is_skip) {
							totskipped++;
							continue;
						}
					}
				}

				if (scene->r.mode & R_TOUCH) {
					if (!is_multiview_name) {
						if (!BLI_exists(name)) {
							BLI_make_existing_file(name); /* makes the dir if its not there */
							BLI_file_touch(name);
						}
					}
					else {
						SceneRenderView *srv;
						char filepath[FILE_MAX];

						for (srv = scene->r.views.first; srv; srv = srv->next) {
							if (!BKE_scene_multiview_is_render_view_active(&scene->r, srv))
								continue;

							BKE_scene_multiview_filepath_get(srv, name, filepath);

							if (!BLI_exists(filepath)) {
								BLI_make_existing_file(filepath); /* makes the dir if its not there */
								BLI_file_touch(filepath);
							}
						}
					}
				}
			}

			re->r.cfra = scene->r.cfra;     /* weak.... */

			/* run callbacs before rendering, before the scene is updated */
			BLI_callback_exec(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);


			do_render_all_options(re);
			totrendered++;

			if (re->test_break(re->tbh) == 0) {
				if (!G.is_break)
					if (!do_write_image_or_movie(re, bmain, scene, mh, totvideos, NULL))
						G.is_break = true;
			}
			else
				G.is_break = true;

			if (G.is_break == true) {
				/* remove touched file */
				if (is_movie == false) {
					if ((scene->r.mode & R_TOUCH)) {
						if (!is_multiview_name) {
							if ((BLI_file_size(name) == 0)) {
								/* BLI_exists(name) is implicit */
								BLI_delete(name, false, false);
							}
						}
						else {
							SceneRenderView *srv;
							char filepath[FILE_MAX];

							for (srv = scene->r.views.first; srv; srv = srv->next) {
								if (!BKE_scene_multiview_is_render_view_active(&scene->r, srv))
									continue;

								BKE_scene_multiview_filepath_get(srv, name, filepath);

								if ((BLI_file_size(filepath) == 0)) {
									/* BLI_exists(filepath) is implicit */
									BLI_delete(filepath, false, false);
								}
							}
						}
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
	if (is_movie) {
		re_movie_free_all(re, mh, totvideos);
	}

	if (totskipped && totrendered == 0)
		BKE_report(re->reports, RPT_INFO, "No frames rendered, skipped to not overwrite");

	scene->r.cfra = cfrao;

	re->flag &= ~R_ANIMATION;

	BLI_callback_exec(re->main, (ID *)scene, G.is_break ? BLI_CB_EVT_RENDER_CANCEL : BLI_CB_EVT_RENDER_COMPLETE);
	BKE_sound_reset_scene_specs(scene);

	/* Destroy the opengl context in the correct thread. */
	RE_gl_context_destroy(re);

	/* UGLY WARNING */
	G.is_rendering = false;
}

void RE_PreviewRender(Render *re, Main *bmain, Scene *sce)
{
	Object *camera;
	int winx, winy;

	winx = (sce->r.size * sce->r.xsch) / 100;
	winy = (sce->r.size * sce->r.ysch) / 100;

	RE_InitState(re, NULL, &sce->r, &sce->view_layers, NULL, winx, winy, NULL);

	re->main = bmain;
	re->scene = sce;
	re->lay = sce->lay;

	camera = RE_GetCamera(re);
	RE_SetCamera(re, camera);

	do_render_3d(re);
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
	re = RE_GetSceneRender(scene);
	if (re == NULL)
		re = RE_NewSceneRender(scene);
	RE_InitState(re, NULL, &scene->r, &scene->view_layers, NULL, winx, winy, &disprect);
	re->scene = scene;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	success = render_result_exr_file_cache_read(re);
	BLI_rw_mutex_unlock(&re->resultmutex);

	render_result_uncrop(re);

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
	RenderPass *rpass = NULL;

	/* multiview: since the API takes no 'view', we use the first combined pass found */
	for (rpass = layer->passes.first; rpass; rpass = rpass->next)
		if (STREQ(rpass->name, RE_PASSNAME_COMBINED))
			break;

	if (rpass == NULL)
		BKE_reportf(reports, RPT_ERROR, "%s: no Combined pass found in the render layer '%s'", __func__, filename);

	if (ibuf && (ibuf->rect || ibuf->rect_float)) {
		if (ibuf->x == layer->rectx && ibuf->y == layer->recty) {
			if (ibuf->rect_float == NULL)
				IMB_float_from_rect(ibuf);

			memcpy(rpass->rect, ibuf->rect_float, sizeof(float) * 4 * layer->rectx * layer->recty);
		}
		else {
			if ((ibuf->x - x >= layer->rectx) && (ibuf->y - y >= layer->recty)) {
				ImBuf *ibuf_clip;

				if (ibuf->rect_float == NULL)
					IMB_float_from_rect(ibuf);

				ibuf_clip = IMB_allocImBuf(layer->rectx, layer->recty, 32, IB_rectfloat);
				if (ibuf_clip) {
					IMB_rectcpy(ibuf_clip, ibuf, 0, 0, x, y, layer->rectx, layer->recty);

					memcpy(rpass->rect, ibuf_clip->rect_float, sizeof(float) * 4 * layer->rectx * layer->recty);
					IMB_freeImBuf(ibuf_clip);
				}
				else {
					BKE_reportf(reports, RPT_ERROR, "%s: failed to allocate clip buffer '%s'", __func__, filename);
				}
			}
			else {
				BKE_reportf(reports, RPT_ERROR, "%s: incorrect dimensions for partial copy '%s'", __func__, filename);
			}
		}

		IMB_freeImBuf(ibuf);
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "%s: failed to load '%s'", __func__, filename);
	}
}

void RE_result_load_from_file(RenderResult *result, ReportList *reports, const char *filename)
{
	if (!render_result_exr_file_read_path(result, NULL, filename)) {
		BKE_reportf(reports, RPT_ERROR, "%s: failed to load '%s'", __func__, filename);
		return;
	}
}

/* Used in the interface to decide whether to show layers or passes. */
bool RE_layers_have_name(struct RenderResult *rr)
{
	switch (BLI_listbase_count_at_most(&rr->layers, 2)) {
		case 0:
			return false;
		case 1:
			return (((RenderLayer *)rr->layers.first)->name[0] != '\0');
		default:
			return true;
	}
	return false;
}

bool RE_passes_have_name(struct RenderLayer *rl)
{
	for (RenderPass *rp = rl->passes.first; rp; rp = rp->next) {
		if (!STREQ(rp->name, "Combined")) {
			return true;
		}
	}

	return false;
}

RenderPass *RE_pass_find_by_name(volatile RenderLayer *rl, const char *name, const char *viewname)
{
	RenderPass *rp = NULL;

	for (rp = rl->passes.last; rp; rp = rp->prev) {
		if (STREQ(rp->name, name)) {
			if (viewname == NULL || viewname[0] == '\0')
				break;
			else if (STREQ(rp->view, viewname))
				break;
		}
	}
	return rp;
}

/* Only provided for API compatibility, don't use this in new code! */
RenderPass *RE_pass_find_by_type(volatile RenderLayer *rl, int passtype, const char *viewname)
{
#define CHECK_PASS(NAME) \
	if (passtype == SCE_PASS_ ## NAME) \
		return RE_pass_find_by_name(rl, RE_PASSNAME_ ## NAME, viewname);

	CHECK_PASS(COMBINED);
	CHECK_PASS(Z);
	CHECK_PASS(VECTOR);
	CHECK_PASS(NORMAL);
	CHECK_PASS(UV);
	CHECK_PASS(RGBA);
	CHECK_PASS(EMIT);
	CHECK_PASS(DIFFUSE);
	CHECK_PASS(SPEC);
	CHECK_PASS(SHADOW);
	CHECK_PASS(AO);
	CHECK_PASS(ENVIRONMENT);
	CHECK_PASS(INDIRECT);
	CHECK_PASS(REFLECT);
	CHECK_PASS(REFRACT);
	CHECK_PASS(INDEXOB);
	CHECK_PASS(INDEXMA);
	CHECK_PASS(MIST);
	CHECK_PASS(RAYHITS);
	CHECK_PASS(DIFFUSE_DIRECT);
	CHECK_PASS(DIFFUSE_INDIRECT);
	CHECK_PASS(DIFFUSE_COLOR);
	CHECK_PASS(GLOSSY_DIRECT);
	CHECK_PASS(GLOSSY_INDIRECT);
	CHECK_PASS(GLOSSY_COLOR);
	CHECK_PASS(TRANSM_DIRECT);
	CHECK_PASS(TRANSM_INDIRECT);
	CHECK_PASS(TRANSM_COLOR);
	CHECK_PASS(SUBSURFACE_DIRECT);
	CHECK_PASS(SUBSURFACE_INDIRECT);
	CHECK_PASS(SUBSURFACE_COLOR);

#undef CHECK_PASS

	return NULL;
}

/* create a renderlayer and renderpass for grease pencil layer */
RenderPass *RE_create_gp_pass(RenderResult *rr, const char *layername, const char *viewname)
{
	RenderLayer *rl = BLI_findstring(&rr->layers, layername, offsetof(RenderLayer, name));
	/* only create render layer if not exist */
	if (!rl) {
		rl = MEM_callocN(sizeof(RenderLayer), layername);
		BLI_addtail(&rr->layers, rl);
		BLI_strncpy(rl->name, layername, sizeof(rl->name));
		rl->layflag = SCE_LAY_SOLID;
		rl->passflag = SCE_PASS_COMBINED;
		rl->rectx = rr->rectx;
		rl->recty = rr->recty;
	}

	/* clear previous pass if exist or the new image will be over previous one*/
	RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_COMBINED, viewname);
	if (rp) {
		if (rp->rect) {
			MEM_freeN(rp->rect);
		}
		BLI_freelinkN(&rl->passes, rp);
	}
	/* create a totally new pass */
	return gp_add_pass(rr, rl, 4, RE_PASSNAME_COMBINED, viewname);
}
