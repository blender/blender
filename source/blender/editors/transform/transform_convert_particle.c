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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "ED_particle.h"

#include "transform.h"
#include "transform_snap.h"

/* Own include. */
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Particle Edit Transform Creation
 *
 * \{ */

void createTransParticleVerts(bContext *C, TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = NULL;
    TransDataExtension *tx;
    Object *ob = CTX_data_active_object(C);
    ParticleEditSettings *pset = PE_settings(t->scene);
    PTCacheEdit *edit = PE_get_current(t->depsgraph, t->scene, ob);
    ParticleSystem *psys = NULL;
    PTCacheEditPoint *point;
    PTCacheEditKey *key;
    float mat[4][4];
    int i, k, transformparticle;
    int count = 0, hasselected = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

    if (edit == NULL || t->settings->particle.selectmode == SCE_SELECT_PATH) {
      return;
    }

    psys = edit->psys;

    for (i = 0, point = edit->points; i < edit->totpoint; i++, point++) {
      point->flag &= ~PEP_TRANSFORM;
      transformparticle = 0;

      if ((point->flag & PEP_HIDE) == 0) {
        for (k = 0, key = point->keys; k < point->totkey; k++, key++) {
          if ((key->flag & PEK_HIDE) == 0) {
            if (key->flag & PEK_SELECT) {
              hasselected = 1;
              transformparticle = 1;
            }
            else if (is_prop_edit) {
              transformparticle = 1;
            }
          }
        }
      }

      if (transformparticle) {
        count += point->totkey;
        point->flag |= PEP_TRANSFORM;
      }
    }

    /* note: in prop mode we need at least 1 selected */
    if (hasselected == 0) {
      return;
    }

    tc->data_len = count;
    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Particle Mode)");

    if (t->mode == TFM_BAKE_TIME) {
      tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                      "Particle_TransExtension");
    }
    else {
      tx = tc->data_ext = NULL;
    }

    unit_m4(mat);

    invert_m4_m4(ob->imat, ob->obmat);

    for (i = 0, point = edit->points; i < edit->totpoint; i++, point++) {
      TransData *head, *tail;
      head = tail = td;

      if (!(point->flag & PEP_TRANSFORM)) {
        continue;
      }

      if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
        ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
        psys_mat_hair_to_global(
            ob, psmd_eval->mesh_final, psys->part->from, psys->particles + i, mat);
      }

      for (k = 0, key = point->keys; k < point->totkey; k++, key++) {
        if (key->flag & PEK_USE_WCO) {
          copy_v3_v3(key->world_co, key->co);
          mul_m4_v3(mat, key->world_co);
          td->loc = key->world_co;
        }
        else {
          td->loc = key->co;
        }

        copy_v3_v3(td->iloc, td->loc);
        copy_v3_v3(td->center, td->loc);

        if (key->flag & PEK_SELECT) {
          td->flag |= TD_SELECTED;
        }
        else if (!is_prop_edit) {
          td->flag |= TD_SKIP;
        }

        unit_m3(td->mtx);
        unit_m3(td->smtx);

        /* don't allow moving roots */
        if (k == 0 && pset->flag & PE_LOCK_FIRST && (!psys || !(psys->flag & PSYS_GLOBAL_HAIR))) {
          td->protectflag |= OB_LOCK_LOC;
        }

        td->ob = ob;
        td->ext = tx;
        if (t->mode == TFM_BAKE_TIME) {
          td->val = key->time;
          td->ival = *(key->time);
          /* abuse size and quat for min/max values */
          td->flag |= TD_NO_EXT;
          if (k == 0) {
            tx->size = NULL;
          }
          else {
            tx->size = (key - 1)->time;
          }

          if (k == point->totkey - 1) {
            tx->quat = NULL;
          }
          else {
            tx->quat = (key + 1)->time;
          }
        }

        td++;
        if (tx) {
          tx++;
        }
        tail++;
      }
      if (is_prop_edit && head != tail) {
        calc_distanceCurveVerts(head, tail - 1);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Transform Creation
 *
 * \{ */

static void flushTransParticles(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    PTCacheEdit *edit = PE_get_current(t->depsgraph, scene, ob);
    ParticleSystem *psys = edit->psys;
    PTCacheEditPoint *point;
    PTCacheEditKey *key;
    TransData *td;
    float mat[4][4], imat[4][4], co[3];
    int i, k;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

    /* we do transform in world space, so flush world space position
     * back to particle local space (only for hair particles) */
    td = tc->data;
    for (i = 0, point = edit->points; i < edit->totpoint; i++, point++, td++) {
      if (!(point->flag & PEP_TRANSFORM)) {
        continue;
      }

      if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
        ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
        psys_mat_hair_to_global(
            ob, psmd_eval->mesh_final, psys->part->from, psys->particles + i, mat);
        invert_m4_m4(imat, mat);

        for (k = 0, key = point->keys; k < point->totkey; k++, key++) {
          copy_v3_v3(co, key->world_co);
          mul_m4_v3(imat, co);

          /* optimization for proportional edit */
          if (!is_prop_edit || !compare_v3v3(key->co, co, 0.0001f)) {
            copy_v3_v3(key->co, co);
            point->flag |= PEP_EDIT_RECALC;
          }
        }
      }
      else {
        point->flag |= PEP_EDIT_RECALC;
      }
    }

    PE_update_object(t->depsgraph, scene, OBACT(view_layer), 1);
    BKE_particle_batch_cache_dirty_tag(psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
    DEG_id_tag_update(&ob->id, ID_RECALC_PSYS_REDO);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Transform Particles Data
 *
 * \{ */

void recalcData_particles(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    applyProject(t);
  }
  flushTransParticles(t);
}

/** \} */
