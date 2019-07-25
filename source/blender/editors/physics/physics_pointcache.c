/*
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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 */

/** \file
 * \ingroup edphys
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "DEG_depsgraph.h"

#include "ED_particle.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "physics_intern.h"

static bool ptcache_bake_all_poll(bContext *C)
{
  return CTX_data_scene(C) != NULL;
}

static bool ptcache_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
  return (ptr.data && ptr.id.data);
}

typedef struct PointCacheJob {
  wmWindowManager *wm;
  void *owner;
  short *stop, *do_update;
  float *progress;

  PTCacheBaker *baker;
} PointCacheJob;

static void ptcache_job_free(void *customdata)
{
  PointCacheJob *job = customdata;
  MEM_freeN(job->baker);
  MEM_freeN(job);
}

static int ptcache_job_break(void *customdata)
{
  PointCacheJob *job = customdata;

  if (G.is_break) {
    return 1;
  }

  if (job->stop && *(job->stop)) {
    return 1;
  }

  return 0;
}

static void ptcache_job_update(void *customdata, float progress, int *cancel)
{
  PointCacheJob *job = customdata;

  if (ptcache_job_break(job)) {
    *cancel = 1;
  }

  *(job->do_update) = true;
  *(job->progress) = progress;
}

static void ptcache_job_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
  PointCacheJob *job = customdata;

  job->stop = stop;
  job->do_update = do_update;
  job->progress = progress;

  G.is_break = false;

  /* XXX annoying hack: needed to prevent data corruption when changing
   * scene frame in separate threads
   */
  WM_set_locked_interface(job->wm, true);

  BKE_ptcache_bake(job->baker);

  *do_update = true;
  *stop = 0;
}

static void ptcache_job_endjob(void *customdata)
{
  PointCacheJob *job = customdata;
  Scene *scene = job->baker->scene;

  WM_set_locked_interface(job->wm, false);

  WM_main_add_notifier(NC_SCENE | ND_FRAME, scene);
  WM_main_add_notifier(NC_OBJECT | ND_POINTCACHE, job->baker->pid.ob);
}

static void ptcache_free_bake(PointCache *cache)
{
  if (cache->edit) {
    if (!cache->edit->edited || 1) {  // XXX okee("Lose changes done in particle mode?")) {
      PE_free_ptcache_edit(cache->edit);
      cache->edit = NULL;
      cache->flag &= ~PTCACHE_BAKED;
    }
  }
  else {
    cache->flag &= ~PTCACHE_BAKED;
  }
}

static PTCacheBaker *ptcache_baker_create(bContext *C, wmOperator *op, bool all)
{
  PTCacheBaker *baker = MEM_callocN(sizeof(PTCacheBaker), "PTCacheBaker");

  baker->bmain = CTX_data_main(C);
  baker->scene = CTX_data_scene(C);
  baker->view_layer = CTX_data_view_layer(C);
  /* Depsgraph is used to sweep the frame range and evaluate scene at different times. */
  baker->depsgraph = CTX_data_depsgraph_pointer(C);
  baker->bake = RNA_boolean_get(op->ptr, "bake");
  baker->render = 0;
  baker->anim_init = 0;
  baker->quick_step = 1;

  if (!all) {
    PointerRNA ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
    Object *ob = ptr.id.data;
    PointCache *cache = ptr.data;
    baker->pid = BKE_ptcache_id_find(ob, baker->scene, cache);
  }

  return baker;
}

static int ptcache_bake_exec(bContext *C, wmOperator *op)
{
  bool all = STREQ(op->type->idname, "PTCACHE_OT_bake_all");

  PTCacheBaker *baker = ptcache_baker_create(C, op, all);
  BKE_ptcache_bake(baker);
  MEM_freeN(baker);

  return OPERATOR_FINISHED;
}

static int ptcache_bake_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  bool all = STREQ(op->type->idname, "PTCACHE_OT_bake_all");

  PointCacheJob *job = MEM_mallocN(sizeof(PointCacheJob), "PointCacheJob");
  job->wm = CTX_wm_manager(C);
  job->baker = ptcache_baker_create(C, op, all);
  job->baker->bake_job = job;
  job->baker->update_progress = ptcache_job_update;

  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              CTX_data_scene(C),
                              "Point Cache",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_POINTCACHE);

  WM_jobs_customdata_set(wm_job, job, ptcache_job_free);
  WM_jobs_timer(wm_job, 0.1, NC_OBJECT | ND_POINTCACHE, NC_OBJECT | ND_POINTCACHE);
  WM_jobs_callbacks(wm_job, ptcache_job_startjob, NULL, NULL, ptcache_job_endjob);

  WM_set_locked_interface(CTX_wm_manager(C), true);

  WM_jobs_start(CTX_wm_manager(C), wm_job);

  WM_event_add_modal_handler(C, op);

  /* we must run modal until the bake job is done, otherwise the undo push
   * happens before the job ends, which can lead to race conditions between
   * the baking and file writing code */
  return OPERATOR_RUNNING_MODAL;
}

static int ptcache_bake_modal(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Scene *scene = (Scene *)op->customdata;

  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_POINTCACHE)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_PASS_THROUGH;
}

static void ptcache_bake_cancel(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = (Scene *)op->customdata;

  /* kill on cancel, because job is using op->reports */
  WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_POINTCACHE);
}

static int ptcache_free_bake_all_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  PTCacheID *pid;
  ListBase pidlist;

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    BKE_ptcache_ids_from_object(&pidlist, ob, scene, MAX_DUPLI_RECUR);

    for (pid = pidlist.first; pid; pid = pid->next) {
      ptcache_free_bake(pid->cache);
    }

    BLI_freelistN(&pidlist);

    WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);
  }
  FOREACH_SCENE_OBJECT_END;

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

void PTCACHE_OT_bake_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake All Physics";
  ot->description = "Bake all physics";
  ot->idname = "PTCACHE_OT_bake_all";

  /* api callbacks */
  ot->exec = ptcache_bake_exec;
  ot->invoke = ptcache_bake_invoke;
  ot->modal = ptcache_bake_modal;
  ot->cancel = ptcache_bake_cancel;
  ot->poll = ptcache_bake_all_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "bake", 1, "Bake", "");
}
void PTCACHE_OT_free_bake_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete All Physics Bakes";
  ot->idname = "PTCACHE_OT_free_bake_all";
  ot->description = "Delete all baked caches of all objects in the current scene";

  /* api callbacks */
  ot->exec = ptcache_free_bake_all_exec;
  ot->poll = ptcache_bake_all_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int ptcache_free_bake_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
  PointCache *cache = ptr.data;
  Object *ob = ptr.id.data;

  ptcache_free_bake(cache);

  WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);

  return OPERATOR_FINISHED;
}
static int ptcache_bake_from_cache_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
  PointCache *cache = ptr.data;
  Object *ob = ptr.id.data;

  cache->flag |= PTCACHE_BAKED;

  WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);

  return OPERATOR_FINISHED;
}
void PTCACHE_OT_bake(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Physics";
  ot->description = "Bake physics";
  ot->idname = "PTCACHE_OT_bake";

  /* api callbacks */
  ot->exec = ptcache_bake_exec;
  ot->invoke = ptcache_bake_invoke;
  ot->modal = ptcache_bake_modal;
  ot->cancel = ptcache_bake_cancel;
  ot->poll = ptcache_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "bake", 0, "Bake", "");
}
void PTCACHE_OT_free_bake(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Physics Bake";
  ot->description = "Delete physics bake";
  ot->idname = "PTCACHE_OT_free_bake";

  /* api callbacks */
  ot->exec = ptcache_free_bake_exec;
  ot->poll = ptcache_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
void PTCACHE_OT_bake_from_cache(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake From Cache";
  ot->description = "Bake from cache";
  ot->idname = "PTCACHE_OT_bake_from_cache";

  /* api callbacks */
  ot->exec = ptcache_bake_from_cache_exec;
  ot->poll = ptcache_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int ptcache_add_new_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  PointerRNA ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
  Object *ob = ptr.id.data;
  PointCache *cache = ptr.data;
  PTCacheID pid = BKE_ptcache_id_find(ob, scene, cache);

  if (pid.cache) {
    PointCache *cache_new = BKE_ptcache_add(pid.ptcaches);
    cache_new->step = pid.default_step;
    *(pid.cache_ptr) = cache_new;

    DEG_id_tag_update(&ob->id, ID_RECALC_POINT_CACHE);
    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
    WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);
  }

  return OPERATOR_FINISHED;
}
static int ptcache_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "point_cache", &RNA_PointCache);
  Scene *scene = CTX_data_scene(C);
  Object *ob = ptr.id.data;
  PointCache *cache = ptr.data;
  PTCacheID pid = BKE_ptcache_id_find(ob, scene, cache);

  /* don't delete last cache */
  if (pid.cache && pid.ptcaches->first != pid.ptcaches->last) {
    BLI_remlink(pid.ptcaches, pid.cache);
    BKE_ptcache_free(pid.cache);
    *(pid.cache_ptr) = pid.ptcaches->first;

    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
    WM_event_add_notifier(C, NC_OBJECT | ND_POINTCACHE, ob);
  }

  return OPERATOR_FINISHED;
}
void PTCACHE_OT_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Cache";
  ot->description = "Add new cache";
  ot->idname = "PTCACHE_OT_add";

  /* api callbacks */
  ot->exec = ptcache_add_new_exec;
  ot->poll = ptcache_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
void PTCACHE_OT_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Current Cache";
  ot->description = "Delete current cache";
  ot->idname = "PTCACHE_OT_remove";

  /* api callbacks */
  ot->exec = ptcache_remove_exec;
  ot->poll = ptcache_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
