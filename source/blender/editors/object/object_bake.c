/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
	meshtools.c: no editmode (violated already :), tools operating on meshes
*/

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"

#include "PIL_time.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "GPU_draw.h" /* GPU_free_image */

#include "WM_api.h"
#include "WM_types.h"


/* ****************** render BAKING ********************** */

/* threaded break test */
static int thread_break(void *unused)
{
	return G.afbreek;
}

static ScrArea *biggest_image_area(bScreen *screen)
{
	ScrArea *sa, *big= NULL;
	int size, maxsize= 0;

	for(sa= screen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			size= sa->winx*sa->winy;
			if(sa->winx > 10 && sa->winy > 10 && size > maxsize) {
				maxsize= size;
				big= sa;
			}
		}
	}
	return big;
}


typedef struct BakeRender {
	Render *re;
	Scene *scene;
	struct Object *actob;
	int tot, ready;

	ReportList *reports;

	short *stop;
	short *do_update;

	ListBase threads;

	/* backup */
	short prev_wo_amb_occ;
	short prev_r_raytrace;

	/* for redrawing */
	ScrArea *sa;
} BakeRender;

/* use by exec and invoke */
int test_bake_internal(bContext *C, ReportList *reports)
{
	Scene *scene= CTX_data_scene(C);

	if(scene->r.renderer!=R_INTERN) {
		BKE_report(reports, RPT_ERROR, "Bake only supported for Internal Renderer");
	} else if((scene->r.bake_flag & R_BAKE_TO_ACTIVE) && CTX_data_active_object(C)==NULL) {
		BKE_report(reports, RPT_ERROR, "No active object");
	}
	else if(scene->r.bake_mode==RE_BAKE_AO && scene->world==NULL) {
		BKE_report(reports, RPT_ERROR, "No world set up");
	}
	else {
		return 1;
	}

	return 0;
}

static void init_bake_internal(BakeRender *bkr, bContext *C)
{
	Scene *scene= CTX_data_scene(C);

	bkr->sa= biggest_image_area(CTX_wm_screen(C)); /* can be NULL */
	bkr->scene= scene;
	bkr->actob= (scene->r.bake_flag & R_BAKE_TO_ACTIVE) ? OBACT : NULL;
	bkr->re= RE_NewRender("_Bake View_", RE_SLOT_DEFAULT);

	if(scene->r.bake_mode==RE_BAKE_AO) {
		/* If raytracing or AO is disabled, switch it on temporarily for baking. */
		bkr->prev_wo_amb_occ = (scene->world->mode & WO_AMB_OCC) != 0;
		scene->world->mode |= WO_AMB_OCC;
	}
	if(scene->r.bake_mode==RE_BAKE_AO || bkr->actob) {
		bkr->prev_r_raytrace = (scene->r.mode & R_RAYTRACE) != 0;
		scene->r.mode |= R_RAYTRACE;
	}
}

static void finish_bake_internal(BakeRender *bkr)
{
	RE_Database_Free(bkr->re);

	/* restore raytrace and AO */
	if(bkr->scene->r.bake_mode==RE_BAKE_AO)
		if(bkr->prev_wo_amb_occ == 0)
			bkr->scene->world->mode &= ~WO_AMB_OCC;

	if(bkr->scene->r.bake_mode==RE_BAKE_AO || bkr->actob)
		if(bkr->prev_r_raytrace == 0)
			bkr->scene->r.mode &= ~R_RAYTRACE;

	if(bkr->tot) {
		Image *ima;
		/* force OpenGL reload and mipmap recalc */
		for(ima= G.main->image.first; ima; ima= ima->id.next) {
			if(ima->ok==IMA_OK_LOADED) {
				ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
				if(ibuf && (ibuf->userflags & IB_BITMAPDIRTY)) {
					GPU_free_image(ima);
					imb_freemipmapImBuf(ibuf);
				}
			}
		}
	}
}

static void *do_bake_render(void *bake_v)
{
	BakeRender *bkr= bake_v;

	bkr->tot= RE_bake_shade_all_selected(bkr->re, bkr->scene->r.bake_mode, bkr->actob, NULL);
	bkr->ready= 1;

	return NULL;
}

static void bake_startjob(void *bkv, short *stop, short *do_update)
{
	BakeRender *bkr= bkv;
	Scene *scene= bkr->scene;

	bkr->stop= stop;
	bkr->do_update= do_update;

	RE_test_break_cb(bkr->re, NULL, thread_break);
	G.afbreek= 0;	/* blender_test_break uses this global */

	RE_Database_Baking(bkr->re, scene, scene->r.bake_mode, bkr->actob);

	/* baking itself is threaded, cannot use test_break in threads. we also update optional imagewindow */
	bkr->tot= RE_bake_shade_all_selected(bkr->re, scene->r.bake_mode, bkr->actob, bkr->do_update);
}

static void bake_update(void *bkv)
{
	BakeRender *bkr= bkv;

	if(bkr->sa && bkr->sa->spacetype==SPACE_IMAGE) { /* incase the user changed while baking */
		SpaceImage *sima= bkr->sa->spacedata.first;
		if(sima)
			sima->image= RE_bake_shade_get_image();
	}
}

static void bake_freejob(void *bkv)
{
	BakeRender *bkr= bkv;
	BLI_end_threads(&bkr->threads);
	finish_bake_internal(bkr);

	if(bkr->tot==0) BKE_report(bkr->reports, RPT_ERROR, "No Images found to bake to");
	MEM_freeN(bkr);
}

/* catch esc */
static int objects_bake_render_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if(0==WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C)))
		return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;

	/* running render */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
			break;
	}
	return OPERATOR_PASS_THROUGH;
}

static int objects_bake_render_invoke(bContext *C, wmOperator *op, wmEvent *_event)
{
	Scene *scene= CTX_data_scene(C);

	if(test_bake_internal(C, op->reports)==0) {
		return OPERATOR_CANCELLED;
	}
	else {
		BakeRender *bkr= MEM_callocN(sizeof(BakeRender), "render bake");
		wmJob *steve;

		init_bake_internal(bkr, C);
		bkr->reports= op->reports;

		/* setup job */
		steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, WM_JOB_EXCL_RENDER|WM_JOB_PRIORITY);
		WM_jobs_customdata(steve, bkr, bake_freejob);
		WM_jobs_timer(steve, 0.2, NC_IMAGE, 0); /* TODO - only draw bake image, can we enforce this */
		WM_jobs_callbacks(steve, bake_startjob, NULL, bake_update);

		G.afbreek= 0;

		WM_jobs_start(CTX_wm_manager(C), steve);

		WM_cursor_wait(0);
		WM_event_add_notifier(C, NC_SCENE|ND_RENDER_RESULT, scene);

		/* add modal handler for ESC */
		WM_event_add_modal_handler(C, op);
	}

	return OPERATOR_RUNNING_MODAL;
}


static int bake_image_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);


	if(test_bake_internal(C, op->reports)==0) {
		return OPERATOR_CANCELLED;
	}
	else {
		ListBase threads;
		BakeRender bkr;

		memset(&bkr, 0, sizeof(bkr));

		init_bake_internal(&bkr, C);
		bkr.reports= op->reports;

		RE_test_break_cb(bkr.re, NULL, thread_break);
		G.afbreek= 0;	/* blender_test_break uses this global */

		RE_Database_Baking(bkr.re, scene, scene->r.bake_mode, (scene->r.bake_flag & R_BAKE_TO_ACTIVE)? OBACT: NULL);

		/* baking itself is threaded, cannot use test_break in threads  */
		BLI_init_threads(&threads, do_bake_render, 1);
		bkr.ready= 0;
		BLI_insert_thread(&threads, &bkr);

		while(bkr.ready==0) {
			PIL_sleep_ms(50);
			if(bkr.ready)
				break;

			/* used to redraw in 2.4x but this is just for exec in 2.5 */
			if (!G.background)
				blender_test_break();
		}
		BLI_end_threads(&threads);

		if(bkr.tot==0) BKE_report(op->reports, RPT_ERROR, "No Images found to bake to");

		finish_bake_internal(&bkr);
	}

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_RESULT, scene);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_bake_image(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Bake";
	ot->description= "Bake image textures of selected objects";
	ot->idname= "OBJECT_OT_bake_image";

	/* api callbacks */
	ot->exec= bake_image_exec;
	ot->invoke= objects_bake_render_invoke;
	ot->modal= objects_bake_render_modal;
}
