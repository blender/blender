/* SPDX-FileCopyrightText: 2007 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edphys
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.hh"
#include "ED_particle.hh"
#include "ED_physics.hh"
#include "ED_undo.hh"

#include "particle_edit_utildefines.h"

#include "physics_intern.h"

/** Only needed this locally. */
static CLG_LogRef LOG = {"ed.undo.particle_edit"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

static void undoptcache_from_editcache(PTCacheUndo *undo, PTCacheEdit *edit)
{
  PTCacheEditPoint *point;

  size_t mem_used_prev = MEM_get_memory_in_use();

  undo->totpoint = edit->totpoint;

  if (edit->psys) {
    ParticleData *pa;

    pa = undo->particles = static_cast<ParticleData *>(MEM_dupallocN(edit->psys->particles));

    for (int i = 0; i < edit->totpoint; i++, pa++) {
      pa->hair = static_cast<HairKey *>(MEM_dupallocN(pa->hair));
    }

    undo->psys_flag = edit->psys->flag;
  }
  else {
    PTCacheMem *pm;

    BLI_duplicatelist(&undo->mem_cache, &edit->pid.cache->mem_cache);
    pm = static_cast<PTCacheMem *>(undo->mem_cache.first);

    for (; pm; pm = pm->next) {
      for (int i = 0; i < BPHYS_TOT_DATA; i++) {
        pm->data[i] = MEM_dupallocN(pm->data[i]);
      }
    }
  }

  point = undo->points = static_cast<PTCacheEditPoint *>(MEM_dupallocN(edit->points));
  undo->totpoint = edit->totpoint;

  for (int i = 0; i < edit->totpoint; i++, point++) {
    point->keys = static_cast<PTCacheEditKey *>(MEM_dupallocN(point->keys));
    /* no need to update edit key->co & key->time pointers here */
  }

  size_t mem_used_curr = MEM_get_memory_in_use();

  undo->undo_size = mem_used_prev < mem_used_curr ? mem_used_curr - mem_used_prev :
                                                    sizeof(PTCacheUndo);
}

static void undoptcache_to_editcache(PTCacheUndo *undo, PTCacheEdit *edit)
{
  ParticleSystem *psys = edit->psys;
  ParticleData *pa;
  HairKey *hkey;
  POINT_P;
  KEY_K;

  LOOP_POINTS {
    if (psys && psys->particles[p].hair) {
      MEM_freeN(psys->particles[p].hair);
    }

    if (point->keys) {
      MEM_freeN(point->keys);
    }
  }
  if (psys && psys->particles) {
    MEM_freeN(psys->particles);
  }
  if (edit->points) {
    MEM_freeN(edit->points);
  }
  MEM_SAFE_FREE(edit->mirror_cache);

  edit->points = static_cast<PTCacheEditPoint *>(MEM_dupallocN(undo->points));
  edit->totpoint = undo->totpoint;

  LOOP_POINTS {
    point->keys = static_cast<PTCacheEditKey *>(MEM_dupallocN(point->keys));
  }

  if (psys) {
    psys->particles = static_cast<ParticleData *>(MEM_dupallocN(undo->particles));

    psys->totpart = undo->totpoint;

    LOOP_POINTS {
      pa = psys->particles + p;
      hkey = pa->hair = static_cast<HairKey *>(MEM_dupallocN(pa->hair));

      LOOP_KEYS {
        key->co = hkey->co;
        key->time = &hkey->time;
        hkey++;
      }
    }

    psys->flag = undo->psys_flag;
  }
  else {
    PTCacheMem *pm;
    int i;

    BKE_ptcache_free_mem(&edit->pid.cache->mem_cache);

    BLI_duplicatelist(&edit->pid.cache->mem_cache, &undo->mem_cache);

    pm = static_cast<PTCacheMem *>(edit->pid.cache->mem_cache.first);

    for (; pm; pm = pm->next) {
      for (i = 0; i < BPHYS_TOT_DATA; i++) {
        pm->data[i] = MEM_dupallocN(pm->data[i]);
      }
      void *cur[BPHYS_TOT_DATA];
      BKE_ptcache_mem_pointers_init(pm, cur);

      LOOP_POINTS {
        LOOP_KEYS {
          if (int(key->ftime) == int(pm->frame)) {
            key->co = static_cast<float *>(cur[BPHYS_DATA_LOCATION]);
            key->vel = static_cast<float *>(cur[BPHYS_DATA_VELOCITY]);
            key->rot = static_cast<float *>(cur[BPHYS_DATA_ROTATION]);
            key->time = &key->ftime;
          }
        }
        BKE_ptcache_mem_pointers_incr(cur);
      }
    }
  }
}

static void undoptcache_free_data(PTCacheUndo *undo)
{
  PTCacheEditPoint *point;
  int i;

  for (i = 0, point = undo->points; i < undo->totpoint; i++, point++) {
    if (undo->particles && (undo->particles + i)->hair) {
      MEM_freeN((undo->particles + i)->hair);
    }
    if (point->keys) {
      MEM_freeN(point->keys);
    }
  }
  if (undo->points) {
    MEM_freeN(undo->points);
  }
  if (undo->particles) {
    MEM_freeN(undo->particles);
  }
  BKE_ptcache_free_mem(&undo->mem_cache);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

struct ParticleUndoStep {
  UndoStep step;
  UndoRefID_Scene scene_ref;
  UndoRefID_Object object_ref;
  PTCacheUndo data;
};

static bool particle_undosys_poll(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  return (edit != nullptr);
}

static bool particle_undosys_step_encode(bContext *C, Main * /*bmain*/, UndoStep *us_p)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ParticleUndoStep *us = (ParticleUndoStep *)us_p;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  us->scene_ref.ptr = CTX_data_scene(C);
  BKE_view_layer_synced_ensure(us->scene_ref.ptr, view_layer);
  us->object_ref.ptr = BKE_view_layer_active_object_get(view_layer);
  PTCacheEdit *edit = PE_get_current(depsgraph, us->scene_ref.ptr, us->object_ref.ptr);
  undoptcache_from_editcache(&us->data, edit);
  return true;
}

static void particle_undosys_step_decode(
    bContext *C, Main * /*bmain*/, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  ParticleUndoStep *us = (ParticleUndoStep *)us_p;
  Scene *scene = us->scene_ref.ptr;
  Object *ob = us->object_ref.ptr;

  ED_object_particle_edit_mode_enter_ex(depsgraph, scene, ob);

  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  /* While this shouldn't happen, entering particle edit-mode uses a more complex
   * setup compared to most other modes which we can't ensure succeeds. */
  if (UNLIKELY(edit == nullptr)) {
    BLI_assert(0);
    return;
  }

  undoptcache_to_editcache(&us->data, edit);
  ParticleEditSettings *pset = &scene->toolsettings->particle;
  if ((pset->flag & PE_DRAW_PART) != 0) {
    psys_free_path_cache(nullptr, edit);
    BKE_particle_batch_cache_dirty_tag(edit->psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
  }
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  ED_undo_object_set_active_or_warn(scene, CTX_data_view_layer(C), ob, us_p->name, &LOG);

  BLI_assert(particle_undosys_poll(C));
}

static void particle_undosys_step_free(UndoStep *us_p)
{
  ParticleUndoStep *us = (ParticleUndoStep *)us_p;
  undoptcache_free_data(&us->data);
}

static void particle_undosys_foreach_ID_ref(UndoStep *us_p,
                                            UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                            void *user_data)
{
  ParticleUndoStep *us = (ParticleUndoStep *)us_p;
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->scene_ref));
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->object_ref));
}

void ED_particle_undosys_type(UndoType *ut)
{
  ut->name = "Edit Particle";
  ut->poll = particle_undosys_poll;
  ut->step_encode = particle_undosys_step_encode;
  ut->step_decode = particle_undosys_step_decode;
  ut->step_free = particle_undosys_step_free;

  ut->step_foreach_ID_ref = particle_undosys_foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(ParticleUndoStep);
}

/** \} */
