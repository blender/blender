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

/** \file blender/render/intern/source/external_engine.c
 *  \ingroup render
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLF_translation.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "initrender.h"
#include "render_types.h"
#include "render_result.h"

/* Render Engine Types */

static RenderEngineType internal_render_type = {
	NULL, NULL,
	"BLENDER_RENDER", N_("Blender Render"), RE_INTERNAL,
	NULL, NULL, NULL, NULL, NULL,
	{NULL, NULL, NULL}
};

#ifdef WITH_GAMEENGINE

static RenderEngineType internal_game_type = {
	NULL, NULL,
	"BLENDER_GAME", N_("Blender Game"), RE_INTERNAL | RE_GAME,
	NULL, NULL, NULL, NULL, NULL,
	{NULL, NULL, NULL}
};

#endif

ListBase R_engines = {NULL, NULL};

void RE_engines_init(void)
{
	BLI_addtail(&R_engines, &internal_render_type);
#ifdef WITH_GAMEENGINE
	BLI_addtail(&R_engines, &internal_game_type);
#endif
}

void RE_engines_exit(void)
{
	RenderEngineType *type, *next;

	for (type = R_engines.first; type; type = next) {
		next = type->next;

		BLI_remlink(&R_engines, type);

		if (!(type->flag & RE_INTERNAL)) {
			if (type->ext.free)
				type->ext.free(type->ext.data);

			MEM_freeN(type);
		}
	}
}

RenderEngineType *RE_engines_find(const char *idname)
{
	RenderEngineType *type;
	
	type = BLI_findstring(&R_engines, idname, offsetof(RenderEngineType, idname));
	if (!type)
		type = &internal_render_type;

	return type;
}

int RE_engine_is_external(Render *re)
{
	RenderEngineType *type = RE_engines_find(re->r.engine);
	return (type && type->render);
}

/* Create, Free */

RenderEngine *RE_engine_create(RenderEngineType *type)
{
	RenderEngine *engine = MEM_callocN(sizeof(RenderEngine), "RenderEngine");
	engine->type = type;

	return engine;
}

void RE_engine_free(RenderEngine *engine)
{
#ifdef WITH_PYTHON
	if (engine->py_instance) {
		BPY_DECREF_RNA_INVALIDATE(engine->py_instance);
	}
#endif

	if (engine->text)
		MEM_freeN(engine->text);

	MEM_freeN(engine);
}

/* Render Results */

RenderResult *RE_engine_begin_result(RenderEngine *engine, int x, int y, int w, int h, const char *layername)
{
	Render *re = engine->re;
	RenderResult *result;
	rcti disprect;

	/* ensure the coordinates are within the right limits */
	CLAMP(x, 0, re->result->rectx);
	CLAMP(y, 0, re->result->recty);
	CLAMP(w, 0, re->result->rectx);
	CLAMP(h, 0, re->result->recty);

	if (x + w > re->result->rectx)
		w = re->result->rectx - x;
	if (y + h > re->result->recty)
		h = re->result->recty - y;

	/* allocate a render result */
	disprect.xmin = x;
	disprect.xmax = x + w;
	disprect.ymin = y;
	disprect.ymax = y + h;

	result = render_result_new(re, &disprect, 0, RR_USE_MEM, layername);

	/* todo: make this thread safe */

	/* can be NULL if we CLAMP the width or height to 0 */
	if (result) {
		BLI_addtail(&engine->fullresult, result);

		result->tilerect.xmin += re->disprect.xmin;
		result->tilerect.xmax += re->disprect.xmin;
		result->tilerect.ymin += re->disprect.ymin;
		result->tilerect.ymax += re->disprect.ymin;
	}

	return result;
}

void RE_engine_update_result(RenderEngine *engine, RenderResult *result)
{
	Render *re = engine->re;

	if (result) {
		result->renlay = result->layers.first; /* weak, draws first layer always */
		re->display_draw(re->ddh, result, NULL);
	}
}

void RE_engine_end_result(RenderEngine *engine, RenderResult *result, int cancel)
{
	Render *re = engine->re;
	RenderPart *pa;

	if (!result) {
		return;
	}

	/* merge. on break, don't merge in result for preview renders, looks nicer */
	if (!cancel) {
		/* for exr tile render, detect tiles that are done */
		for (pa = re->parts.first; pa; pa = pa->next) {
			if (result->tilerect.xmin == pa->disprect.xmin &&
			    result->tilerect.ymin == pa->disprect.ymin &&
			    result->tilerect.xmax == pa->disprect.xmax &&
			    result->tilerect.ymax == pa->disprect.ymax)
			{
				pa->ready = 1;
			}
		}

		if (re->result->do_exr_tile)
			render_result_exr_file_merge(re->result, result);
		else if (!(re->test_break(re->tbh) && (re->r.scemode & R_PREVIEWBUTS)))
			render_result_merge(re->result, result);

		/* draw */
		if (!re->test_break(re->tbh)) {
			result->renlay = result->layers.first; /* weak, draws first layer always */
			re->display_draw(re->ddh, result, NULL);
		}
	}

	/* free */
	BLI_remlink(&engine->fullresult, result);
	render_result_free(result);
}

/* Cancel */

int RE_engine_test_break(RenderEngine *engine)
{
	Render *re = engine->re;

	if (re)
		return re->test_break(re->tbh);
	
	return 0;
}

/* Statistics */

void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info)
{
	Render *re = engine->re;

	/* stats draw callback */
	if (re) {
		re->i.statstr = stats;
		re->i.infostr = info;
		re->stats_draw(re->sdh, &re->i);
		re->i.infostr = NULL;
		re->i.statstr = NULL;
	}

	/* set engine text */
	if (engine->text) {
		MEM_freeN(engine->text);
		engine->text = NULL;
	}

	if (stats && stats[0] && info && info[0])
		engine->text = BLI_sprintfN("%s | %s", stats, info);
	else if (info && info[0])
		engine->text = BLI_strdup(info);
	else if (stats && stats[0])
		engine->text = BLI_strdup(stats);
}

void RE_engine_update_progress(RenderEngine *engine, float progress)
{
	Render *re = engine->re;

	if (re) {
		CLAMP(progress, 0.0f, 1.0f);
		re->progress(re->prh, progress);
	}
}

void RE_engine_update_memory_stats(RenderEngine *engine, float mem_used, float mem_peak)
{
	Render *re = engine->re;

	if (re) {
		re->i.mem_used = mem_used;
		re->i.mem_peak = mem_peak;
	}
}

void RE_engine_report(RenderEngine *engine, int type, const char *msg)
{
	Render *re = engine->re;

	if (re)
		BKE_report(engine->re->reports, type, msg);
	else if (engine->reports)
		BKE_report(engine->reports, type, msg);
}

/* Render */

int RE_engine_render(Render *re, int do_all)
{
	RenderEngineType *type = RE_engines_find(re->r.engine);
	RenderEngine *engine;
	int persistent_data = re->r.mode & R_PERSISTENT_DATA;

	/* verify if we can render */
	if (!type->render)
		return 0;
	if ((re->r.scemode & R_PREVIEWBUTS) && !(type->flag & RE_USE_PREVIEW))
		return 0;
	if (do_all && !(type->flag & RE_USE_POSTPROCESS))
		return 0;
	if (!do_all && (type->flag & RE_USE_POSTPROCESS))
		return 0;

	/* create render result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if (re->result == NULL || !(re->r.scemode & R_PREVIEWBUTS)) {
		int savebuffers;

		if (re->result)
			render_result_free(re->result);

		savebuffers = (re->r.scemode & R_EXR_TILE_FILE) ? RR_USE_EXR : RR_USE_MEM;
		re->result = render_result_new(re, &re->disprect, 0, savebuffers, RR_ALL_LAYERS);
	}
	BLI_rw_mutex_unlock(&re->resultmutex);

	if (re->result == NULL)
		return 1;

	/* set render info */
	re->i.cfra = re->scene->r.cfra;
	BLI_strncpy(re->i.scene_name, re->scene->id.name + 2, sizeof(re->i.scene_name));
	re->i.totface = re->i.totvert = re->i.totstrand = re->i.totlamp = re->i.tothalo = 0;

	/* render */
	engine = re->engine;

	if (!engine) {
		engine = RE_engine_create(type);

		if (persistent_data)
			re->engine = engine;
	}

	engine->flag |= RE_ENGINE_RENDERING;

	/* TODO: actually link to a parent which shouldn't happen */
	engine->re = re;

	if (re->flag & R_ANIMATION)
		engine->flag |= RE_ENGINE_ANIMATION;
	if (re->r.scemode & R_PREVIEWBUTS)
		engine->flag |= RE_ENGINE_PREVIEW;
	engine->camera_override = re->camera_override;

	engine->resolution_x = re->winx;
	engine->resolution_y = re->winy;

	if ((re->r.scemode & (R_NO_FRAME_UPDATE | R_PREVIEWBUTS)) == 0)
		BKE_scene_update_for_newframe(re->main, re->scene, re->lay);

	initparts(re, FALSE);
	engine->tile_x = re->partx;
	engine->tile_y = re->party;

	if (re->result->do_exr_tile)
		render_result_exr_file_begin(re);

	if (type->update)
		type->update(engine, re->main, re->scene);
	
	if (type->render)
		type->render(engine, re->scene);

	engine->tile_x = 0;
	engine->tile_y = 0;
	engine->flag &= ~RE_ENGINE_RENDERING;

	render_result_free_list(&engine->fullresult, engine->fullresult.first);

	/* re->engine becomes zero if user changed active render engine during render */
	if (!persistent_data || !re->engine) {
		RE_engine_free(engine);
		re->engine = NULL;
	}

	if (re->result->do_exr_tile) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_exr_file_end(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}

	freeparts(re);

	if (BKE_reports_contain(re->reports, RPT_ERROR))
		G.is_break = TRUE;
	
	return 1;
}

