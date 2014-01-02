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
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Morten Mikkelsen,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_bake.c
 *  \ingroup edobj
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_geom.h"

#include "BKE_blender.h"
#include "BKE_screen.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"
#include "RE_multires_bake.h"

#include "PIL_time.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "GPU_draw.h" /* GPU_free_image */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "object_intern.h"

/* ****************** multires BAKING ********************** */

/* holder of per-object data needed for bake job
 * needed to make job totally thread-safe */
typedef struct MultiresBakerJobData {
	struct MultiresBakerJobData *next, *prev;
	DerivedMesh *lores_dm, *hires_dm;
	bool simple;
	int lvl, tot_lvl;
	ListBase images;
} MultiresBakerJobData;

/* data passing to multires-baker job */
typedef struct {
	ListBase data;
	bool bake_clear;      /* Clear the images before baking */
	int bake_filter;      /* Bake-filter, aka margin */
	short mode;           /* mode of baking (displacement, normals, AO) */
	bool use_lores_mesh;  /* Use low-resolution mesh when baking displacement maps */
	int number_of_rays;   /* Number of rays to be cast when doing AO baking */
	float bias;           /* Bias between object and start ray point when doing AO baking */
	int raytrace_structure;  /* Optimization structure to be used for AO baking */
	int octree_resolution;   /* Reslution of octotree when using octotree optimization structure */
	int threads;             /* Number of threads to be used for baking */
	float user_scale;          /* User scale used to scale displacement when baking derivative map. */
} MultiresBakeJob;

static bool multiresbake_check(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	Mesh *me;
	MultiresModifierData *mmd;
	bool ok = true;
	int a;

	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		ob = base->object;

		if (ob->type != OB_MESH) {
			BKE_report(op->reports, RPT_ERROR, "Baking of multires data only works with an active mesh object");

			ok = false;
			break;
		}

		me = (Mesh *)ob->data;
		mmd = get_multires_modifier(scene, ob, 0);

		/* Multi-resolution should be and be last in the stack */
		if (ok && mmd) {
			ModifierData *md;

			ok = mmd->totlvl > 0;

			for (md = (ModifierData *)mmd->modifier.next; md && ok; md = md->next) {
				if (modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
					ok = false;
				}
			}
		}
		else {
			ok = false;
		}

		if (!ok) {
			BKE_report(op->reports, RPT_ERROR, "Multires data baking requires multi-resolution object");

			break;
		}

		if (!me->mtpoly) {
			BKE_report(op->reports, RPT_ERROR, "Mesh should be unwrapped before multires data baking");

			ok = false;
		}
		else {
			a = me->totpoly;
			while (ok && a--) {
				Image *ima = me->mtpoly[a].tpage;

				if (!ima) {
					BKE_report(op->reports, RPT_ERROR, "You should have active texture to use multires baker");

					ok = false;
				}
				else {
					ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

					if (!ibuf) {
						BKE_report(op->reports, RPT_ERROR, "Baking should happen to image with image buffer");

						ok = false;
					}
					else {
						if (ibuf->rect == NULL && ibuf->rect_float == NULL)
							ok = false;

						if (ibuf->rect_float && !(ibuf->channels == 0 || ibuf->channels == 4))
							ok = false;

						if (!ok)
							BKE_report(op->reports, RPT_ERROR, "Baking to unsupported image type");
					}

					BKE_image_release_ibuf(ima, ibuf, NULL);
				}
			}
		}

		if (!ok)
			break;
	}
	CTX_DATA_END;

	return ok;
}

static DerivedMesh *multiresbake_create_loresdm(Scene *scene, Object *ob, int *lvl)
{
	DerivedMesh *dm;
	MultiresModifierData *mmd = get_multires_modifier(scene, ob, 0);
	Mesh *me = (Mesh *)ob->data;
	MultiresModifierData tmp_mmd = *mmd;
	DerivedMesh *cddm = CDDM_from_mesh(me);

	if (mmd->lvl > 0) {
		*lvl = mmd->lvl;
	}
	else {
		*lvl = 1;
		tmp_mmd.simple = true;
	}

	DM_set_only_copy(cddm, CD_MASK_BAREMESH | CD_MASK_MTFACE);

	tmp_mmd.lvl = *lvl;
	tmp_mmd.sculptlvl = *lvl;
	dm = multires_make_derived_from_derived(cddm, &tmp_mmd, ob, 0);
	cddm->release(cddm);

	return dm;
}

static DerivedMesh *multiresbake_create_hiresdm(Scene *scene, Object *ob, int *lvl, bool *simple)
{
	Mesh *me = (Mesh *)ob->data;
	MultiresModifierData *mmd = get_multires_modifier(scene, ob, 0);
	MultiresModifierData tmp_mmd = *mmd;
	DerivedMesh *cddm = CDDM_from_mesh(me);
	DerivedMesh *dm;

	DM_set_only_copy(cddm, CD_MASK_BAREMESH);

	/* TODO: DM_set_only_copy wouldn't set mask for loop and poly data,
	 *       but we really need BAREMESH only to save lots of memory
	 */
	CustomData_set_only_copy(&cddm->loopData, CD_MASK_BAREMESH);
	CustomData_set_only_copy(&cddm->polyData, CD_MASK_BAREMESH);

	*lvl = mmd->totlvl;
	*simple = mmd->simple != 0;

	tmp_mmd.lvl = mmd->totlvl;
	tmp_mmd.sculptlvl = mmd->totlvl;
	dm = multires_make_derived_from_derived(cddm, &tmp_mmd, ob, 0);
	cddm->release(cddm);

	return dm;
}

typedef enum ClearFlag {
	CLEAR_TANGENT_NORMAL = 1,
	CLEAR_DISPLACEMENT = 2
} ClearFlag;


static void clear_single_image(Image *image, ClearFlag flag)
{
	const float vec_alpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const float vec_solid[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	const float nor_alpha[4] = {0.5f, 0.5f, 1.0f, 0.0f};
	const float nor_solid[4] = {0.5f, 0.5f, 1.0f, 1.0f};
	const float disp_alpha[4] = {0.5f, 0.5f, 0.5f, 0.0f};
	const float disp_solid[4] = {0.5f, 0.5f, 0.5f, 1.0f};

	if ((image->id.flag & LIB_DOIT) == 0) {
		ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);

		if (flag == CLEAR_TANGENT_NORMAL)
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? nor_alpha : nor_solid);
		else if (flag == CLEAR_DISPLACEMENT)
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? disp_alpha : disp_solid);
		else
			IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? vec_alpha : vec_solid);

		image->id.flag |= LIB_DOIT;

		BKE_image_release_ibuf(image, ibuf, NULL);
	}
}

static void clear_images(MTFace *mtface, int totface, ClearFlag flag)
{
	int a;

	for (a = 0; a < totface; a++) {
		mtface[a].tpage->id.flag &= ~LIB_DOIT;
	}

	for (a = 0; a < totface; a++) {
		clear_single_image(mtface[a].tpage, flag);
	}

	for (a = 0; a < totface; a++) {
		mtface[a].tpage->id.flag &= ~LIB_DOIT;
	}
}

static void clear_images_poly(MTexPoly *mtpoly, int totpoly, ClearFlag flag)
{
	int a;

	for (a = 0; a < totpoly; a++) {
		mtpoly[a].tpage->id.flag &= ~LIB_DOIT;
	}

	for (a = 0; a < totpoly; a++) {
		clear_single_image(mtpoly[a].tpage, flag);
	}

	for (a = 0; a < totpoly; a++) {
		mtpoly[a].tpage->id.flag &= ~LIB_DOIT;
	}
}

static int multiresbake_image_exec_locked(bContext *C, wmOperator *op)
{
	Object *ob;
	Scene *scene = CTX_data_scene(C);
	int objects_baked = 0;

	if (!multiresbake_check(C, op))
		return OPERATOR_CANCELLED;

	if (scene->r.bake_flag & R_BAKE_CLEAR) {  /* clear images */
		CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
		{
			Mesh *me;
			ClearFlag clear_flag = 0;

			ob = base->object;
			me = (Mesh *)ob->data;

			if (scene->r.bake_mode == RE_BAKE_NORMALS) {
				clear_flag = CLEAR_TANGENT_NORMAL;
			}
			else if (ELEM(scene->r.bake_mode, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE)) {
				clear_flag = CLEAR_DISPLACEMENT;
			}

			clear_images(me->mtface, me->totface, clear_flag);
			clear_images_poly(me->mtpoly, me->totpoly, clear_flag);
		}
		CTX_DATA_END;
	}

	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		MultiresBakeRender bkr = {NULL};

		ob = base->object;

		multires_force_update(ob);

		/* copy data stored in job descriptor */
		bkr.bake_filter = scene->r.bake_filter;
		bkr.mode = scene->r.bake_mode;
		bkr.use_lores_mesh = scene->r.bake_flag & R_BAKE_LORES_MESH;
		bkr.bias = scene->r.bake_biasdist;
		bkr.number_of_rays = scene->r.bake_samples;
		bkr.raytrace_structure = scene->r.raytrace_structure;
		bkr.octree_resolution = scene->r.ocres;
		bkr.threads = BKE_scene_num_threads(scene);
		bkr.user_scale = (scene->r.bake_flag & R_BAKE_USERSCALE) ? scene->r.bake_user_scale : -1.0f;
		//bkr.reports= op->reports;

		/* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
		bkr.hires_dm = multiresbake_create_hiresdm(scene, ob, &bkr.tot_lvl, &bkr.simple);
		bkr.lores_dm = multiresbake_create_loresdm(scene, ob, &bkr.lvl);

		RE_multires_bake_images(&bkr);

		BLI_freelistN(&bkr.image);

		bkr.lores_dm->release(bkr.lores_dm);
		bkr.hires_dm->release(bkr.hires_dm);

		objects_baked++;
	}
	CTX_DATA_END;

	if (!objects_baked)
		BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");

	return OPERATOR_FINISHED;
}

/* Multiresbake adopted for job-system executing */
static void init_multiresbake_job(bContext *C, MultiresBakeJob *bkj)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;

	/* backup scene settings, so their changing in UI would take no effect on baker */
	bkj->bake_filter = scene->r.bake_filter;
	bkj->mode = scene->r.bake_mode;
	bkj->use_lores_mesh = scene->r.bake_flag & R_BAKE_LORES_MESH;
	bkj->bake_clear = scene->r.bake_flag & R_BAKE_CLEAR;
	bkj->bias = scene->r.bake_biasdist;
	bkj->number_of_rays = scene->r.bake_samples;
	bkj->raytrace_structure = scene->r.raytrace_structure;
	bkj->octree_resolution = scene->r.ocres;
	bkj->threads = BKE_scene_num_threads(scene);
	bkj->user_scale = (scene->r.bake_flag & R_BAKE_USERSCALE) ? scene->r.bake_user_scale : -1.0f;
	//bkj->reports = op->reports;

	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		MultiresBakerJobData *data;
		int lvl;

		ob = base->object;

		multires_force_update(ob);

		data = MEM_callocN(sizeof(MultiresBakerJobData), "multiresBaker derivedMesh_data");

		/* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
		data->hires_dm = multiresbake_create_hiresdm(scene, ob, &data->tot_lvl, &data->simple);
		data->lores_dm = multiresbake_create_loresdm(scene, ob, &lvl);
		data->lvl = lvl;

		BLI_addtail(&bkj->data, data);
	}
	CTX_DATA_END;
}

static void multiresbake_startjob(void *bkv, short *stop, short *do_update, float *progress)
{
	MultiresBakerJobData *data;
	MultiresBakeJob *bkj = bkv;
	int baked_objects = 0, tot_obj;

	tot_obj = BLI_countlist(&bkj->data);

	if (bkj->bake_clear) {  /* clear images */
		for (data = bkj->data.first; data; data = data->next) {
			DerivedMesh *dm = data->lores_dm;
			MTFace *mtface = CustomData_get_layer(&dm->faceData, CD_MTFACE);
			ClearFlag clear_flag = 0;

			if (bkj->mode == RE_BAKE_NORMALS) {
				clear_flag = CLEAR_TANGENT_NORMAL;
			}
			else if (ELEM(bkj->mode, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE)) {
				clear_flag = CLEAR_DISPLACEMENT;
			}

			clear_images(mtface, dm->getNumTessFaces(dm), clear_flag);
		}
	}

	for (data = bkj->data.first; data; data = data->next) {
		MultiresBakeRender bkr = {NULL};

		/* copy data stored in job descriptor */
		bkr.bake_filter = bkj->bake_filter;
		bkr.mode = bkj->mode;
		bkr.use_lores_mesh = bkj->use_lores_mesh;
		bkr.user_scale = bkj->user_scale;
		//bkr.reports = bkj->reports;

		/* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
		bkr.lores_dm = data->lores_dm;
		bkr.hires_dm = data->hires_dm;
		bkr.tot_lvl = data->tot_lvl;
		bkr.lvl = data->lvl;
		bkr.simple = data->simple;

		/* needed for proper progress bar */
		bkr.tot_obj = tot_obj;
		bkr.baked_objects = baked_objects;

		bkr.stop = stop;
		bkr.do_update = do_update;
		bkr.progress = progress;

		bkr.bias = bkj->bias;
		bkr.number_of_rays = bkj->number_of_rays;
		bkr.raytrace_structure = bkj->raytrace_structure;
		bkr.octree_resolution = bkj->octree_resolution;
		bkr.threads = bkj->threads;

		RE_multires_bake_images(&bkr);

		data->images = bkr.image;

		baked_objects++;
	}
}

static void multiresbake_freejob(void *bkv)
{
	MultiresBakeJob *bkj = bkv;
	MultiresBakerJobData *data, *next;
	LinkData *link;

	data = bkj->data.first;
	while (data) {
		next = data->next;
		data->lores_dm->release(data->lores_dm);
		data->hires_dm->release(data->hires_dm);

		/* delete here, since this delete will be called from main thread */
		for (link = data->images.first; link; link = link->next) {
			Image *ima = (Image *)link->data;
			GPU_free_image(ima);
		}

		BLI_freelistN(&data->images);

		MEM_freeN(data);
		data = next;
	}

	MEM_freeN(bkj);
}

static int multiresbake_image_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	MultiresBakeJob *bkr;
	wmJob *wm_job;

	if (!multiresbake_check(C, op))
		return OPERATOR_CANCELLED;

	bkr = MEM_callocN(sizeof(MultiresBakeJob), "MultiresBakeJob data");
	init_multiresbake_job(C, bkr);

	if (!bkr->data.first) {
		BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");
		return OPERATOR_CANCELLED;
	}

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Multires Bake",
	                     WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS, WM_JOB_TYPE_OBJECT_BAKE_TEXTURE);
	WM_jobs_customdata_set(wm_job, bkr, multiresbake_freejob);
	WM_jobs_timer(wm_job, 0.5, NC_IMAGE, 0); /* TODO - only draw bake image, can we enforce this */
	WM_jobs_callbacks(wm_job, multiresbake_startjob, NULL, NULL, NULL);

	G.is_break = false;

	WM_jobs_start(CTX_wm_manager(C), wm_job);
	WM_cursor_wait(0);

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* ****************** render BAKING ********************** */

/* threaded break test */
static int thread_break(void *UNUSED(arg))
{
	return G.is_break;
}

typedef struct BakeRender {
	Render *re;
	Main *main;
	Scene *scene;
	struct Object *actob;
	int result, ready;

	ReportList *reports;

	short *stop;
	short *do_update;
	float *progress;
	
	ListBase threads;

	/* backup */
	short prev_wo_amb_occ;
	short prev_r_raytrace;

	/* for redrawing */
	ScrArea *sa;
} BakeRender;

/* use by exec and invoke */
static int test_bake_internal(bContext *C, ReportList *reports)
{
	Scene *scene = CTX_data_scene(C);

	if ((scene->r.bake_flag & R_BAKE_TO_ACTIVE) && CTX_data_active_object(C) == NULL) {
		BKE_report(reports, RPT_ERROR, "No active object");
	}
	else if (scene->r.bake_mode == RE_BAKE_AO && scene->world == NULL) {
		BKE_report(reports, RPT_ERROR, "No world set up");
	}
	else {
		return 1;
	}

	return 0;
}

static void init_bake_internal(BakeRender *bkr, bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	bScreen *sc = CTX_wm_screen(C);

	/* get editmode results */
	ED_object_editmode_load(CTX_data_edit_object(C));

	bkr->sa = sc ? BKE_screen_find_big_area(sc, SPACE_IMAGE, 10) : NULL; /* can be NULL */
	bkr->main = CTX_data_main(C);
	bkr->scene = scene;
	bkr->actob = (scene->r.bake_flag & R_BAKE_TO_ACTIVE) ? OBACT : NULL;
	bkr->re = RE_NewRender("_Bake View_");

	if (scene->r.bake_mode == RE_BAKE_AO) {
		/* If raytracing or AO is disabled, switch it on temporarily for baking. */
		bkr->prev_wo_amb_occ = (scene->world->mode & WO_AMB_OCC) != 0;
		scene->world->mode |= WO_AMB_OCC;
	}
	if (scene->r.bake_mode == RE_BAKE_AO || bkr->actob) {
		bkr->prev_r_raytrace = (scene->r.mode & R_RAYTRACE) != 0;
		scene->r.mode |= R_RAYTRACE;
	}
}

static void finish_bake_internal(BakeRender *bkr)
{
	Image *ima;

	RE_Database_Free(bkr->re);

	/* restore raytrace and AO */
	if (bkr->scene->r.bake_mode == RE_BAKE_AO)
		if (bkr->prev_wo_amb_occ == 0)
			bkr->scene->world->mode &= ~WO_AMB_OCC;

	if (bkr->scene->r.bake_mode == RE_BAKE_AO || bkr->actob)
		if (bkr->prev_r_raytrace == 0)
			bkr->scene->r.mode &= ~R_RAYTRACE;

	/* force OpenGL reload and mipmap recalc */
	if ((bkr->scene->r.bake_flag & R_BAKE_VCOL) == 0) {
		for (ima = G.main->image.first; ima; ima = ima->id.next) {
			ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

			/* some of the images could have been changed during bake,
			 * so recreate mipmaps regardless bake result status
			 */
			if (ima->ok == IMA_OK_LOADED) {
				if (ibuf) {
					if (ibuf->userflags & IB_BITMAPDIRTY) {
						GPU_free_image(ima);
						imb_freemipmapImBuf(ibuf);
					}

					/* invalidate display buffers for changed images */
					if (ibuf->userflags & IB_BITMAPDIRTY)
						ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
				}
			}

			/* freed when baking is done, but if its canceled we need to free here */
			if (ibuf) {
				if (ibuf->userdata) {
					BakeImBufuserData *userdata = (BakeImBufuserData *) ibuf->userdata;
					if (userdata->mask_buffer)
						MEM_freeN(userdata->mask_buffer);
					if (userdata->displacement_buffer)
						MEM_freeN(userdata->displacement_buffer);
					MEM_freeN(userdata);
					ibuf->userdata = NULL;
				}
			}

			BKE_image_release_ibuf(ima, ibuf, NULL);
		}
	}

	if (bkr->scene->r.bake_flag & R_BAKE_VCOL) {
		/* update all tagged meshes */
		Mesh *me;
		BLI_assert(BLI_thread_is_main());
		for (me = G.main->mesh.first; me; me = me->id.next) {
			if (me->id.flag & LIB_DOIT) {
				DAG_id_tag_update(&me->id, OB_RECALC_DATA);
				BKE_mesh_tessface_clear(me);
			}
		}
	}

}

static void *do_bake_render(void *bake_v)
{
	BakeRender *bkr = bake_v;

	bkr->result = RE_bake_shade_all_selected(bkr->re, bkr->scene->r.bake_mode, bkr->actob, NULL, bkr->progress);
	bkr->ready = 1;

	return NULL;
}

static void bake_startjob(void *bkv, short *stop, short *do_update, float *progress)
{
	BakeRender *bkr = bkv;
	Scene *scene = bkr->scene;
	Main *bmain = bkr->main;

	bkr->stop = stop;
	bkr->do_update = do_update;
	bkr->progress = progress;

	RE_test_break_cb(bkr->re, NULL, thread_break);
	G.is_break = false;   /* blender_test_break uses this global */

	RE_Database_Baking(bkr->re, bmain, scene, scene->lay, scene->r.bake_mode, bkr->actob);

	/* baking itself is threaded, cannot use test_break in threads. we also update optional imagewindow */
	bkr->result = RE_bake_shade_all_selected(bkr->re, scene->r.bake_mode, bkr->actob, bkr->do_update, bkr->progress);
}

static void bake_update(void *bkv)
{
	BakeRender *bkr = bkv;

	if (bkr->sa && bkr->sa->spacetype == SPACE_IMAGE) { /* in case the user changed while baking */
		SpaceImage *sima = bkr->sa->spacedata.first;
		if (sima)
			sima->image = RE_bake_shade_get_image();
	}
}

static void bake_freejob(void *bkv)
{
	BakeRender *bkr = bkv;
	finish_bake_internal(bkr);

	if (bkr->result == BAKE_RESULT_NO_OBJECTS)
		BKE_report(bkr->reports, RPT_ERROR, "No objects or images found to bake to");
	else if (bkr->result == BAKE_RESULT_FEEDBACK_LOOP)
		BKE_report(bkr->reports, RPT_WARNING, "Circular reference in texture stack");

	MEM_freeN(bkr);
	G.is_rendering = false;
}

/* catch esc */
static int objects_bake_render_modal(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	/* no running blender, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_OBJECT_BAKE_TEXTURE))
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;

	/* running render */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
	}
	return OPERATOR_PASS_THROUGH;
}

static bool is_multires_bake(Scene *scene)
{
	if (ELEM4(scene->r.bake_mode, RE_BAKE_NORMALS, RE_BAKE_DISPLACEMENT, RE_BAKE_DERIVATIVE, RE_BAKE_AO))
		return scene->r.bake_flag & R_BAKE_MULTIRES;

	return 0;
}

static int objects_bake_render_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(_event))
{
	Scene *scene = CTX_data_scene(C);
	int result = OPERATOR_CANCELLED;

	if (is_multires_bake(scene)) {
		result = multiresbake_image_exec(C, op);
	}
	else {
		/* only one render job at a time */
		if (WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_OBJECT_BAKE_TEXTURE))
			return OPERATOR_CANCELLED;

		if (test_bake_internal(C, op->reports) == 0) {
			return OPERATOR_CANCELLED;
		}
		else {
			BakeRender *bkr = MEM_callocN(sizeof(BakeRender), "render bake");
			wmJob *wm_job;

			init_bake_internal(bkr, C);
			bkr->reports = op->reports;

			/* setup job */
			wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Texture Bake",
			                     WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS, WM_JOB_TYPE_OBJECT_BAKE_TEXTURE);
			WM_jobs_customdata_set(wm_job, bkr, bake_freejob);
			WM_jobs_timer(wm_job, 0.5, NC_IMAGE, 0); /* TODO - only draw bake image, can we enforce this */
			WM_jobs_callbacks(wm_job, bake_startjob, NULL, bake_update, NULL);

			G.is_break = false;
			G.is_rendering = true;

			WM_jobs_start(CTX_wm_manager(C), wm_job);

			WM_cursor_wait(0);

			/* add modal handler for ESC */
			WM_event_add_modal_handler(C, op);
		}

		result = OPERATOR_RUNNING_MODAL;
	}

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

	return result;
}


static int bake_image_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int result = OPERATOR_CANCELLED;

	if (is_multires_bake(scene)) {
		result = multiresbake_image_exec_locked(C, op);
	}
	else {
		if (test_bake_internal(C, op->reports) == 0) {
			return OPERATOR_CANCELLED;
		}
		else {
			ListBase threads;
			BakeRender bkr = {NULL};

			init_bake_internal(&bkr, C);
			bkr.reports = op->reports;

			RE_test_break_cb(bkr.re, NULL, thread_break);
			G.is_break = false;   /* blender_test_break uses this global */

			RE_Database_Baking(bkr.re, bmain, scene, scene->lay, scene->r.bake_mode, (scene->r.bake_flag & R_BAKE_TO_ACTIVE) ? OBACT : NULL);

			/* baking itself is threaded, cannot use test_break in threads  */
			BLI_init_threads(&threads, do_bake_render, 1);
			bkr.ready = 0;
			BLI_insert_thread(&threads, &bkr);

			while (bkr.ready == 0) {
				PIL_sleep_ms(50);
				if (bkr.ready)
					break;

				/* used to redraw in 2.4x but this is just for exec in 2.5 */
				if (!G.background)
					blender_test_break();
			}
			BLI_end_threads(&threads);

			if (bkr.result == BAKE_RESULT_NO_OBJECTS)
				BKE_report(op->reports, RPT_ERROR, "No valid images found to bake to");
			else if (bkr.result == BAKE_RESULT_FEEDBACK_LOOP)
				BKE_report(op->reports, RPT_ERROR, "Circular reference in texture stack");

			finish_bake_internal(&bkr);

			result = OPERATOR_FINISHED;
		}
	}

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

	return result;
}

void OBJECT_OT_bake_image(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bake";
	ot->description = "Bake image textures of selected objects";
	ot->idname = "OBJECT_OT_bake_image";

	/* api callbacks */
	ot->exec = bake_image_exec;
	ot->invoke = objects_bake_render_invoke;
	ot->modal = objects_bake_render_modal;
	ot->poll = ED_operator_object_active;
}
