/*  
 * $Id$
 *
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

/** \file blender/render/intern/pipeline/engine.c
 *  \ingroup render
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_report.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "render_types.h"
#include "renderpipeline.h"

/************************** External Engines ***************************/

RenderResult *RE_engine_begin_result(RenderEngine *engine, int x, int y, int w, int h)
{
	Render *re= engine->re;
	RenderResult *result;
	rcti disprect;

	/* ensure the coordinates are within the right limits */
	CLAMP(x, 0, re->result->rectx);
	CLAMP(y, 0, re->result->recty);
	CLAMP(w, 0, re->result->rectx);
	CLAMP(h, 0, re->result->recty);

	if(x + w > re->result->rectx)
		w= re->result->rectx - x;
	if(y + h > re->result->recty)
		h= re->result->recty - y;

	/* allocate a render result */
	disprect.xmin= x;
	disprect.xmax= x+w;
	disprect.ymin= y;
	disprect.ymax= y+h;

	result= new_render_result(re, &disprect, 0, RR_USEMEM);
	BLI_addtail(&engine->fullresult, result);

	return result;
}

void RE_engine_update_result(RenderEngine *engine, RenderResult *result)
{
	Render *re= engine->re;

	if(result) {
		result->renlay= result->layers.first; // weak, draws first layer always
		re->display_draw(re->ddh, result, NULL);
	}
}

void RE_engine_end_result(RenderEngine *engine, RenderResult *result)
{
	Render *re= engine->re;

	if(!result)
		return;

	/* merge. on break, don't merge in result for preview renders, looks nicer */
	if(!(re->test_break(re->tbh) && (re->r.scemode & R_PREVIEWBUTS)))
		merge_render_result(re->result, result);

	/* draw */
	if(!re->test_break(re->tbh)) {
		result->renlay= result->layers.first; // weak, draws first layer always
		re->display_draw(re->ddh, result, NULL);
	}

	/* free */
	free_render_result(&engine->fullresult, result);
}

/* Cancel */

int RE_engine_test_break(RenderEngine *engine)
{
	Render *re= engine->re;

	if(re)
		return re->test_break(re->tbh);
	
	return 0;
}

/* Statistics */

void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info)
{
	Render *re= engine->re;

	re->i.statstr= stats;
	re->i.infostr= info;
	re->stats_draw(re->sdh, &re->i);
	re->i.infostr= NULL;
	re->i.statstr= NULL;
}

void RE_engine_report(RenderEngine *engine, int type, const char *msg)
{
	BKE_report(engine->re->reports, type, msg);
}

/* Render */

int RE_engine_render(Render *re, int do_all)
{
	RenderEngineType *type= BLI_findstring(&R_engines, re->r.engine, offsetof(RenderEngineType, idname));
	RenderEngine engine;

	if(!(type && type->render))
		return 0;
	if((re->r.scemode & R_PREVIEWBUTS) && !(type->flag & RE_DO_PREVIEW))
		return 0;
	if(do_all && !(type->flag & RE_DO_ALL))
		return 0;
	if(!do_all && (type->flag & RE_DO_ALL))
		return 0;

	/* create render result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if(re->result==NULL || !(re->r.scemode & R_PREVIEWBUTS)) {
		RE_FreeRenderResult(re->result);
		re->result= new_render_result(re, &re->disprect, 0, 0);
	}
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	if(re->result==NULL)
		return 1;

	/* external */
	memset(&engine, 0, sizeof(engine));
	engine.type= type;
	engine.re= re;

	type->render(&engine, re->scene);

	free_render_result(&engine.fullresult, engine.fullresult.first);

	return 1;
}

