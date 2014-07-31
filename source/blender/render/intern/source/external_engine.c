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

#include "RNA_access.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "RE_engine.h"
#include "RE_pipeline.h"
#include "RE_bake.h"

#include "initrender.h"
#include "renderpipeline.h"
#include "render_types.h"
#include "render_result.h"

/* Render Engine Types */

static RenderEngineType internal_render_type = {
	NULL, NULL,
	"BLENDER_RENDER", N_("Blender Render"), RE_INTERNAL,
	NULL, NULL, NULL, NULL, NULL, NULL,
	{NULL, NULL, NULL}
};

#ifdef WITH_GAMEENGINE

static RenderEngineType internal_game_type = {
	NULL, NULL,
	"BLENDER_GAME", N_("Blender Game"), RE_INTERNAL | RE_GAME,
	NULL, NULL, NULL, NULL, NULL, NULL,
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

bool RE_engine_is_external(Render *re)
{
	RenderEngineType *type = RE_engines_find(re->r.engine);
	return (type && type->render);
}

/* Create, Free */

RenderEngine *RE_engine_create(RenderEngineType *type)
{
	return RE_engine_create_ex(type, false);
}

RenderEngine *RE_engine_create_ex(RenderEngineType *type, bool use_for_viewport)
{
	RenderEngine *engine = MEM_callocN(sizeof(RenderEngine), "RenderEngine");
	engine->type = type;

	if (use_for_viewport) {
		engine->flag |= RE_ENGINE_USED_FOR_VIEWPORT;

		BLI_begin_threaded_malloc();
	}

	return engine;
}

void RE_engine_free(RenderEngine *engine)
{
#ifdef WITH_PYTHON
	if (engine->py_instance) {
		BPY_DECREF_RNA_INVALIDATE(engine->py_instance);
	}
#endif

	if (engine->flag & RE_ENGINE_USED_FOR_VIEWPORT) {
		BLI_end_threaded_malloc();
	}

	MEM_freeN(engine);
}

/* Render Results */

static RenderPart *get_part_from_result(Render *re, RenderResult *result)
{
	RenderPart *pa;

	for (pa = re->parts.first; pa; pa = pa->next) {
		if (result->tilerect.xmin == pa->disprect.xmin - re->disprect.xmin &&
		    result->tilerect.ymin == pa->disprect.ymin - re->disprect.ymin &&
		    result->tilerect.xmax == pa->disprect.xmax - re->disprect.xmin &&
		    result->tilerect.ymax == pa->disprect.ymax - re->disprect.ymin)
		{
			return pa;
		}
	}

	return NULL;
}

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
		RenderPart *pa;

		/* Copy EXR tile settings, so pipeline knows whether this is a result
		 * for Save Buffers enabled rendering.
		 */
		result->do_exr_tile = re->result->do_exr_tile;

		BLI_addtail(&engine->fullresult, result);

		result->tilerect.xmin += re->disprect.xmin;
		result->tilerect.xmax += re->disprect.xmin;
		result->tilerect.ymin += re->disprect.ymin;
		result->tilerect.ymax += re->disprect.ymin;

		pa = get_part_from_result(re, result);

		if (pa)
			pa->status = PART_STATUS_IN_PROGRESS;
	}

	return result;
}

void RE_engine_update_result(RenderEngine *engine, RenderResult *result)
{
	Render *re = engine->re;

	if (result) {
		result->renlay = result->layers.first; /* weak, draws first layer always */
		re->display_update(re->duh, result, NULL);
	}
}

void RE_engine_end_result(RenderEngine *engine, RenderResult *result, int cancel, int merge_results)
{
	Render *re = engine->re;

	if (!result) {
		return;
	}

	/* merge. on break, don't merge in result for preview renders, looks nicer */
	if (!cancel) {
		/* for exr tile render, detect tiles that are done */
		RenderPart *pa = get_part_from_result(re, result);

		if (pa) {
			pa->status = PART_STATUS_READY;
		}
		else if (re->result->do_exr_tile) {
			/* if written result does not match any tile and we are using save
			 * buffers, we are going to get openexr save errors */
			fprintf(stderr, "RenderEngine.end_result: dimensions do not match any OpenEXR tile.\n");
		}
	}

	if (!cancel || merge_results) {
		if (re->result->do_exr_tile) {
			if (!cancel) {
				render_result_exr_file_merge(re->result, result);
			}
		}
		else if (!(re->test_break(re->tbh) && (re->r.scemode & R_BUTS_PREVIEW)))
			render_result_merge(re->result, result);

		/* draw */
		if (!re->test_break(re->tbh)) {
			result->renlay = result->layers.first; /* weak, draws first layer always */
			re->display_update(re->duh, result, NULL);
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
	engine->text[0] = '\0';

	if (stats && stats[0] && info && info[0])
		BLI_snprintf(engine->text, sizeof(engine->text), "%s | %s", stats, info);
	else if (info && info[0])
		BLI_strncpy(engine->text, info, sizeof(engine->text));
	else if (stats && stats[0])
		BLI_strncpy(engine->text, stats, sizeof(engine->text));
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

void RE_engine_get_current_tiles(Render *re, int *total_tiles_r, rcti **tiles_r)
{
	RenderPart *pa;
	int total_tiles = 0;
	rcti *tiles = NULL;
	int allocation_size = 0, allocation_step = BLENDER_MAX_THREADS;

	if (re->engine && (re->engine->flag & RE_ENGINE_HIGHLIGHT_TILES) == 0) {
		*total_tiles_r = 0;
		*tiles_r = NULL;
		return;
	}

	for (pa = re->parts.first; pa; pa = pa->next) {
		if (pa->status == PART_STATUS_IN_PROGRESS) {
			if (total_tiles >= allocation_size) {
				if (tiles == NULL)
					tiles = MEM_mallocN(allocation_step * sizeof(rcti), "current engine tiles");
				else
					tiles = MEM_reallocN(tiles, (total_tiles + allocation_step) * sizeof(rcti));

				allocation_size += allocation_step;
			}

			tiles[total_tiles] = pa->disprect;

			if (pa->crop) {
				tiles[total_tiles].xmin += pa->crop;
				tiles[total_tiles].ymin += pa->crop;
				tiles[total_tiles].xmax -= pa->crop;
				tiles[total_tiles].ymax -= pa->crop;
			}

			total_tiles++;
		}
	}

	*total_tiles_r = total_tiles;
	*tiles_r = tiles;
}

RenderData *RE_engine_get_render_data(Render *re)
{
	return &re->r;
}

/* Bake */
void RE_bake_engine_set_engine_parameters(Render *re, Main *bmain, Scene *scene)
{
	re->scene = scene;
	re->main = bmain;
	re->r = scene->r;

	/* prevent crash when freeing the scene
	 * but it potentially leaves unfreed memory blocks
	 * not sure how to fix this yet -- dfelinto */
	BLI_listbase_clear(&re->r.layers);
}

bool RE_bake_has_engine(Render *re)
{
	RenderEngineType *type = RE_engines_find(re->r.engine);
	return (type->bake != NULL);
}

bool RE_bake_engine(
        Render *re, Object *object, const BakePixel pixel_array[],
        const size_t num_pixels, const int depth,
        const ScenePassType pass_type, float result[])
{
	RenderEngineType *type = RE_engines_find(re->r.engine);
	RenderEngine *engine;
	int persistent_data = re->r.mode & R_PERSISTENT_DATA;

	/* set render info */
	re->i.cfra = re->scene->r.cfra;
	BLI_strncpy(re->i.scene_name, re->scene->id.name + 2, sizeof(re->i.scene_name) - 2);
	re->i.totface = re->i.totvert = re->i.totstrand = re->i.totlamp = re->i.tothalo = 0;

	/* render */
	engine = re->engine;

	if (!engine) {
		engine = RE_engine_create(type);
		re->engine = engine;
	}

	engine->flag |= RE_ENGINE_RENDERING;

	/* TODO: actually link to a parent which shouldn't happen */
	engine->re = re;

	engine->resolution_x = re->winx;
	engine->resolution_y = re->winy;

	RE_parts_init(re, false);
	engine->tile_x = re->r.tilex;
	engine->tile_y = re->r.tiley;

	/* update is only called so we create the engine.session */
	if (type->update)
		type->update(engine, re->main, re->scene);

	if (type->bake)
		type->bake(engine, re->scene, object, pass_type, pixel_array, num_pixels, depth, result);

	engine->tile_x = 0;
	engine->tile_y = 0;
	engine->flag &= ~RE_ENGINE_RENDERING;

	/* re->engine becomes zero if user changed active render engine during render */
	if (!persistent_data || !re->engine) {
		RE_engine_free(engine);
		re->engine = NULL;
	}

	RE_parts_free(re);

	if (BKE_reports_contain(re->reports, RPT_ERROR))
		G.is_break = true;

	return true;
}

void RE_engine_frame_set(RenderEngine *engine, int frame, float subframe)
{
	Render *re = engine->re;
	Scene *scene = re->scene;
	double cfra = (double)frame + (double)subframe;

	CLAMP(cfra, MINAFRAME, MAXFRAME);
	BKE_scene_frame_set(scene, cfra);

#ifdef WITH_PYTHON
	BPy_BEGIN_ALLOW_THREADS;
#endif

	/* It's possible that here we're including layers which were never visible before. */
	BKE_scene_update_for_newframe_ex(re->eval_ctx, re->main, scene, (1 << 20) - 1, true);

#ifdef WITH_PYTHON
	BPy_END_ALLOW_THREADS;
#endif

	BKE_scene_camera_switch_update(scene);
}

/* Render */

static bool render_layer_exclude_animated(Scene *scene, SceneRenderLayer *srl)
{
	PointerRNA ptr;
	PropertyRNA *prop;

	RNA_pointer_create(&scene->id, &RNA_SceneRenderLayer, srl, &ptr);
	prop = RNA_struct_find_property(&ptr, "layers_exclude");

	return RNA_property_animated(&ptr, prop);
}

int RE_engine_render(Render *re, int do_all)
{
	RenderEngineType *type = RE_engines_find(re->r.engine);
	RenderEngine *engine;
	int persistent_data = re->r.mode & R_PERSISTENT_DATA;

	/* verify if we can render */
	if (!type->render)
		return 0;
	if ((re->r.scemode & R_BUTS_PREVIEW) && !(type->flag & RE_USE_PREVIEW))
		return 0;
	if (do_all && !(type->flag & RE_USE_POSTPROCESS))
		return 0;
	if (!do_all && (type->flag & RE_USE_POSTPROCESS))
		return 0;

	/* Lock drawing in UI during data phase. */
	if (re->draw_lock) {
		re->draw_lock(re->dlh, 1);
	}

	/* update animation here so any render layer animation is applied before
	 * creating the render result */
	if ((re->r.scemode & (R_NO_FRAME_UPDATE | R_BUTS_PREVIEW)) == 0) {
		unsigned int lay = re->lay;

		/* don't update layers excluded on all render layers */
		if (type->flag & RE_USE_EXCLUDE_LAYERS) {
			SceneRenderLayer *srl;
			unsigned int non_excluded_lay = 0;

			if (re->r.scemode & R_SINGLE_LAYER) {
				srl = BLI_findlink(&re->r.layers, re->r.actlay);
				if (srl) {
					non_excluded_lay |= ~srl->lay_exclude;

					/* in this case we must update all because animation for
					 * the scene has not been updated yet, and so may not be
					 * up to date until after BKE_scene_update_for_newframe */
					if (render_layer_exclude_animated(re->scene, srl))
						non_excluded_lay |= ~0;
				}
			}
			else {
				for (srl = re->r.layers.first; srl; srl = srl->next) {
					if (!(srl->layflag & SCE_LAY_DISABLE)) {
						non_excluded_lay |= ~srl->lay_exclude;

						if (render_layer_exclude_animated(re->scene, srl))
							non_excluded_lay |= ~0;
					}
				}
			}

			lay &= non_excluded_lay;
		}

		BKE_scene_update_for_newframe(re->eval_ctx, re->main, re->scene, lay);
		render_update_anim_renderdata(re, &re->scene->r);
	}

	/* create render result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if (re->result == NULL || !(re->r.scemode & R_BUTS_PREVIEW)) {
		int savebuffers = RR_USE_MEM;

		if (re->result)
			render_result_free(re->result);

		if ((type->flag & RE_USE_SAVE_BUFFERS) && (re->r.scemode & R_EXR_TILE_FILE))
			savebuffers = RR_USE_EXR;
		re->result = render_result_new(re, &re->disprect, 0, savebuffers, RR_ALL_LAYERS);
	}
	BLI_rw_mutex_unlock(&re->resultmutex);

	if (re->result == NULL) {
		/* Clear UI drawing locks. */
		if (re->draw_lock) {
			re->draw_lock(re->dlh, 0);
		}
		return 1;
	}

	/* set render info */
	re->i.cfra = re->scene->r.cfra;
	BLI_strncpy(re->i.scene_name, re->scene->id.name + 2, sizeof(re->i.scene_name));
	re->i.totface = re->i.totvert = re->i.totstrand = re->i.totlamp = re->i.tothalo = 0;

	/* render */
	engine = re->engine;

	if (!engine) {
		engine = RE_engine_create(type);
		re->engine = engine;
	}

	engine->flag |= RE_ENGINE_RENDERING;

	/* TODO: actually link to a parent which shouldn't happen */
	engine->re = re;

	if (re->flag & R_ANIMATION)
		engine->flag |= RE_ENGINE_ANIMATION;
	if (re->r.scemode & R_BUTS_PREVIEW)
		engine->flag |= RE_ENGINE_PREVIEW;
	engine->camera_override = re->camera_override;
	engine->layer_override = re->layer_override;

	engine->resolution_x = re->winx;
	engine->resolution_y = re->winy;

	RE_parts_init(re, false);
	engine->tile_x = re->partx;
	engine->tile_y = re->party;

	if (re->result->do_exr_tile)
		render_result_exr_file_begin(re);

	if (type->update)
		type->update(engine, re->main, re->scene);

	/* Clear UI drawing locks. */
	if (re->draw_lock) {
		re->draw_lock(re->dlh, 0);
	}

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

	if (re->r.scemode & R_EXR_CACHE_FILE) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		render_result_exr_file_cache_write(re);
		BLI_rw_mutex_unlock(&re->resultmutex);
	}

	RE_parts_free(re);

	if (BKE_reports_contain(re->reports, RPT_ERROR))
		G.is_break = true;
	
#ifdef WITH_FREESTYLE
	if (re->r.mode & R_EDGE_FRS)
		RE_RenderFreestyleExternal(re);
#endif

	return 1;
}

