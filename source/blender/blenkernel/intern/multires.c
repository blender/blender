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
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

/* for reading old multires */
#define DNA_DEPRECATED_ALLOW

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_ccg.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "BKE_object.h"

#include "CCGSubSurf.h"

#include "DEG_depsgraph_query.h"

#include "multires_reshape.h"

#include <math.h>
#include <string.h>

/* MULTIRES MODIFIER */
static const int multires_grid_tot[] = {
    0, 4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_side_tot[] = {
    0, 2, 3, 5, 9, 17, 33, 65, 129, 257, 513, 1025, 2049, 4097};

/* See multiresModifier_disp_run for description of each operation */
typedef enum {
  APPLY_DISPLACEMENTS,
  CALC_DISPLACEMENTS,
  ADD_DISPLACEMENTS,
} DispOp;

static void multiresModifier_disp_run(
    DerivedMesh *dm, Mesh *me, DerivedMesh *dm2, DispOp op, CCGElem **oldGridData, int totlvl);

/** Customdata */

void multires_customdata_delete(Mesh *me)
{
  if (me->edit_mesh) {
    BMEditMesh *em = me->edit_mesh;
    /* CustomData_external_remove is used here only to mark layer
     * as non-external for further free-ing, so zero element count
     * looks safer than em->totface */
    CustomData_external_remove(&em->bm->ldata, &me->id, CD_MDISPS, 0);

    if (CustomData_has_layer(&em->bm->ldata, CD_MDISPS)) {
      BM_data_layer_free(em->bm, &em->bm->ldata, CD_MDISPS);
    }

    if (CustomData_has_layer(&em->bm->ldata, CD_GRID_PAINT_MASK)) {
      BM_data_layer_free(em->bm, &em->bm->ldata, CD_GRID_PAINT_MASK);
    }
  }
  else {
    CustomData_external_remove(&me->ldata, &me->id, CD_MDISPS, me->totloop);
    CustomData_free_layer_active(&me->ldata, CD_MDISPS, me->totloop);

    CustomData_free_layer_active(&me->ldata, CD_GRID_PAINT_MASK, me->totloop);
  }
}

/** Grid hiding */
static BLI_bitmap *multires_mdisps_upsample_hidden(BLI_bitmap *lo_hidden,
                                                   int lo_level,
                                                   int hi_level,

                                                   /* assumed to be at hi_level (or null) */
                                                   const BLI_bitmap *prev_hidden)
{
  BLI_bitmap *subd;
  int hi_gridsize = BKE_ccg_gridsize(hi_level);
  int lo_gridsize = BKE_ccg_gridsize(lo_level);
  int yh, xh, xl, yl, xo, yo, hi_ndx;
  int offset, factor;

  BLI_assert(lo_level <= hi_level);

  /* fast case */
  if (lo_level == hi_level) {
    return MEM_dupallocN(lo_hidden);
  }

  subd = BLI_BITMAP_NEW(square_i(hi_gridsize), "MDisps.hidden upsample");

  factor = BKE_ccg_factor(lo_level, hi_level);
  offset = 1 << (hi_level - lo_level - 1);

  /* low-res blocks */
  for (yl = 0; yl < lo_gridsize; yl++) {
    for (xl = 0; xl < lo_gridsize; xl++) {
      int lo_val = BLI_BITMAP_TEST(lo_hidden, yl * lo_gridsize + xl);

      /* high-res blocks */
      for (yo = -offset; yo <= offset; yo++) {
        yh = yl * factor + yo;
        if (yh < 0 || yh >= hi_gridsize) {
          continue;
        }

        for (xo = -offset; xo <= offset; xo++) {
          xh = xl * factor + xo;
          if (xh < 0 || xh >= hi_gridsize) {
            continue;
          }

          hi_ndx = yh * hi_gridsize + xh;

          if (prev_hidden) {
            /* If prev_hidden is available, copy it to
             * subd, except when the equivalent element in
             * lo_hidden is different */
            if (lo_val != prev_hidden[hi_ndx]) {
              BLI_BITMAP_SET(subd, hi_ndx, lo_val);
            }
            else {
              BLI_BITMAP_SET(subd, hi_ndx, prev_hidden[hi_ndx]);
            }
          }
          else {
            BLI_BITMAP_SET(subd, hi_ndx, lo_val);
          }
        }
      }
    }
  }

  return subd;
}

static BLI_bitmap *multires_mdisps_downsample_hidden(const BLI_bitmap *old_hidden,
                                                     int old_level,
                                                     int new_level)
{
  BLI_bitmap *new_hidden;
  int new_gridsize = BKE_ccg_gridsize(new_level);
  int old_gridsize = BKE_ccg_gridsize(old_level);
  int x, y, factor, old_value;

  BLI_assert(new_level <= old_level);
  factor = BKE_ccg_factor(new_level, old_level);
  new_hidden = BLI_BITMAP_NEW(square_i(new_gridsize), "downsample hidden");

  for (y = 0; y < new_gridsize; y++) {
    for (x = 0; x < new_gridsize; x++) {
      old_value = BLI_BITMAP_TEST(old_hidden, factor * y * old_gridsize + x * factor);

      BLI_BITMAP_SET(new_hidden, y * new_gridsize + x, old_value);
    }
  }

  return new_hidden;
}

static void multires_output_hidden_to_ccgdm(CCGDerivedMesh *ccgdm, Mesh *me, int level)
{
  const MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
  BLI_bitmap **grid_hidden = ccgdm->gridHidden;
  int *gridOffset;
  int i, j;

  gridOffset = ccgdm->dm.getGridOffset(&ccgdm->dm);

  for (i = 0; i < me->totpoly; i++) {
    for (j = 0; j < me->mpoly[i].totloop; j++) {
      int g = gridOffset[i] + j;
      const MDisps *md = &mdisps[g];
      BLI_bitmap *gh = md->hidden;

      if (gh) {
        grid_hidden[g] = multires_mdisps_downsample_hidden(gh, md->level, level);
      }
    }
  }
}

/* subdivide mdisps.hidden if needed (assumes that md.level reflects
 * the current level of md.hidden) */
static void multires_mdisps_subdivide_hidden(MDisps *md, int new_level)
{
  BLI_bitmap *subd;

  BLI_assert(md->hidden);

  /* nothing to do if already subdivided enough */
  if (md->level >= new_level) {
    return;
  }

  subd = multires_mdisps_upsample_hidden(md->hidden, md->level, new_level, NULL);

  /* swap in the subdivided data */
  MEM_freeN(md->hidden);
  md->hidden = subd;
}

Mesh *BKE_multires_create_mesh(struct Depsgraph *depsgraph,
                               Object *object,
                               MultiresModifierData *mmd)
{
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Mesh *deformed_mesh = mesh_get_eval_deform(
      depsgraph, scene_eval, object_eval, &CD_MASK_BAREMESH);
  ModifierEvalContext modifier_ctx = {
      .depsgraph = depsgraph,
      .object = object_eval,
      .flag = MOD_APPLY_USECACHE | MOD_APPLY_IGNORE_SIMPLIFY,
  };

  const ModifierTypeInfo *mti = BKE_modifier_get_info(mmd->modifier.type);
  Mesh *result = mti->modifyMesh(&mmd->modifier, &modifier_ctx, deformed_mesh);

  if (result == deformed_mesh) {
    result = BKE_mesh_copy_for_eval(deformed_mesh, true);
  }
  return result;
}

float (*BKE_multires_create_deformed_base_mesh_vert_coords(struct Depsgraph *depsgraph,
                                                           struct Object *object,
                                                           struct MultiresModifierData *mmd,
                                                           int *r_num_deformed_verts))[3]
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);

  Object object_for_eval = *object_eval;
  object_for_eval.data = object->data;
  object_for_eval.sculpt = NULL;

  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  ModifierEvalContext mesh_eval_context = {depsgraph, &object_for_eval, 0};
  if (use_render) {
    mesh_eval_context.flag |= MOD_APPLY_RENDER;
  }
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;

  VirtualModifierData virtual_modifier_data;
  ModifierData *first_md = BKE_modifiers_get_virtual_modifierlist(&object_for_eval,
                                                                  &virtual_modifier_data);

  Mesh *base_mesh = object->data;

  int num_deformed_verts;
  float(*deformed_verts)[3] = BKE_mesh_vert_coords_alloc(base_mesh, &num_deformed_verts);

  for (ModifierData *md = first_md; md != NULL; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);

    if (md == &mmd->modifier) {
      break;
    }

    if (!BKE_modifier_is_enabled(scene_eval, md, required_mode)) {
      continue;
    }

    if (mti->type != eModifierTypeType_OnlyDeform) {
      break;
    }

    BKE_modifier_deform_verts(
        md, &mesh_eval_context, base_mesh, deformed_verts, num_deformed_verts);
  }

  if (r_num_deformed_verts != NULL) {
    *r_num_deformed_verts = num_deformed_verts;
  }
  return deformed_verts;
}

MultiresModifierData *find_multires_modifier_before(Scene *scene, ModifierData *lastmd)
{
  ModifierData *md;

  for (md = lastmd; md; md = md->prev) {
    if (md->type == eModifierType_Multires) {
      if (BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        return (MultiresModifierData *)md;
      }
    }
  }

  return NULL;
}

/* used for applying scale on mdisps layer and syncing subdivide levels when joining objects
 * use_first - return first multires modifier if all multires'es are disabled
 */
MultiresModifierData *get_multires_modifier(Scene *scene, Object *ob, bool use_first)
{
  ModifierData *md;
  MultiresModifierData *mmd = NULL, *firstmmd = NULL;

  /* find first active multires modifier */
  for (md = ob->modifiers.first; md; md = md->next) {
    if (md->type == eModifierType_Multires) {
      if (!firstmmd) {
        firstmmd = (MultiresModifierData *)md;
      }

      if (BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        mmd = (MultiresModifierData *)md;
        break;
      }
    }
  }

  if (!mmd && use_first) {
    /* active multires have not been found
     * try to use first one */
    return firstmmd;
  }

  return mmd;
}

int multires_get_level(const Scene *scene,
                       const Object *ob,
                       const MultiresModifierData *mmd,
                       bool render,
                       bool ignore_simplify)
{
  if (render) {
    return (scene != NULL) ? get_render_subsurf_level(&scene->r, mmd->renderlvl, true) :
                             mmd->renderlvl;
  }
  if (ob->mode == OB_MODE_SCULPT) {
    return mmd->sculptlvl;
  }
  if (ignore_simplify) {
    return mmd->lvl;
  }

  return (scene != NULL) ? get_render_subsurf_level(&scene->r, mmd->lvl, false) : mmd->lvl;
}

void multires_set_tot_level(Object *ob, MultiresModifierData *mmd, int lvl)
{
  mmd->totlvl = lvl;

  if (ob->mode != OB_MODE_SCULPT) {
    mmd->lvl = CLAMPIS(MAX2(mmd->lvl, lvl), 0, mmd->totlvl);
  }

  mmd->sculptlvl = CLAMPIS(MAX2(mmd->sculptlvl, lvl), 0, mmd->totlvl);
  mmd->renderlvl = CLAMPIS(MAX2(mmd->renderlvl, lvl), 0, mmd->totlvl);
}

static void multires_ccg_mark_as_modified(SubdivCCG *subdiv_ccg, MultiresModifiedFlags flags)
{
  if (flags & MULTIRES_COORDS_MODIFIED) {
    subdiv_ccg->dirty.coords = true;
  }
  if (flags & MULTIRES_HIDDEN_MODIFIED) {
    subdiv_ccg->dirty.hidden = true;
  }
}

void multires_mark_as_modified(Depsgraph *depsgraph, Object *object, MultiresModifiedFlags flags)
{
  if (object == NULL) {
    return;
  }
  /* NOTE: CCG live inside of evaluated object.
   *
   * While this is a bit weird to tag the only one, this is how other areas were built
   * historically: they are tagging multires for update and then rely on object re-evaluation to
   * do an actual update.
   *
   * In a longer term maybe special dependency graph tag can help sanitizing this a bit. */
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  Mesh *mesh = object_eval->data;
  SubdivCCG *subdiv_ccg = mesh->runtime.subdiv_ccg;
  if (subdiv_ccg == NULL) {
    return;
  }
  multires_ccg_mark_as_modified(subdiv_ccg, flags);
}

void multires_flush_sculpt_updates(Object *object)
{
  if (object == NULL || object->sculpt == NULL || object->sculpt->pbvh == NULL) {
    return;
  }

  SculptSession *sculpt_session = object->sculpt;
  if (BKE_pbvh_type(sculpt_session->pbvh) != PBVH_GRIDS || !sculpt_session->multires.active ||
      sculpt_session->multires.modifier == NULL) {
    return;
  }

  SubdivCCG *subdiv_ccg = sculpt_session->subdiv_ccg;
  if (subdiv_ccg == NULL) {
    return;
  }

  if (!subdiv_ccg->dirty.coords && !subdiv_ccg->dirty.hidden) {
    return;
  }

  Mesh *mesh = object->data;
  multiresModifier_reshapeFromCCG(
      sculpt_session->multires.modifier->totlvl, mesh, sculpt_session->subdiv_ccg);

  subdiv_ccg->dirty.coords = false;
  subdiv_ccg->dirty.hidden = false;
}

void multires_force_sculpt_rebuild(Object *object)
{
  multires_flush_sculpt_updates(object);

  if (object == NULL || object->sculpt == NULL) {
    return;
  }

  SculptSession *ss = object->sculpt;

  if (ss->pbvh != NULL) {
    BKE_pbvh_free(ss->pbvh);
    object->sculpt->pbvh = NULL;
  }

  if (ss->pmap != NULL) {
    MEM_freeN(ss->pmap);
    ss->pmap = NULL;
  }

  if (ss->pmap_mem != NULL) {
    MEM_freeN(ss->pmap_mem);
    ss->pmap_mem = NULL;
  }
}

void multires_force_external_reload(Object *object)
{
  Mesh *mesh = BKE_mesh_from_object(object);

  CustomData_external_reload(&mesh->ldata, &mesh->id, CD_MASK_MDISPS, mesh->totloop);
  multires_force_sculpt_rebuild(object);
}

/* reset the multires levels to match the number of mdisps */
static int get_levels_from_disps(Object *ob)
{
  Mesh *me = ob->data;
  MDisps *mdisp, *md;
  int i, j, totlvl = 0;

  mdisp = CustomData_get_layer(&me->ldata, CD_MDISPS);

  for (i = 0; i < me->totpoly; i++) {
    md = mdisp + me->mpoly[i].loopstart;

    for (j = 0; j < me->mpoly[i].totloop; j++, md++) {
      if (md->totdisp == 0) {
        continue;
      }

      while (1) {
        int side = (1 << (totlvl - 1)) + 1;
        int lvl_totdisp = side * side;
        if (md->totdisp == lvl_totdisp) {
          break;
        }
        if (md->totdisp < lvl_totdisp) {
          totlvl--;
        }
        else {
          totlvl++;
        }
      }

      break;
    }
  }

  return totlvl;
}

/* reset the multires levels to match the number of mdisps */
void multiresModifier_set_levels_from_disps(MultiresModifierData *mmd, Object *ob)
{
  Mesh *me = ob->data;
  MDisps *mdisp;

  if (me->edit_mesh) {
    mdisp = CustomData_get_layer(&me->edit_mesh->bm->ldata, CD_MDISPS);
  }
  else {
    mdisp = CustomData_get_layer(&me->ldata, CD_MDISPS);
  }

  if (mdisp) {
    mmd->totlvl = get_levels_from_disps(ob);
    mmd->lvl = MIN2(mmd->sculptlvl, mmd->totlvl);
    mmd->sculptlvl = MIN2(mmd->sculptlvl, mmd->totlvl);
    mmd->renderlvl = MIN2(mmd->renderlvl, mmd->totlvl);
  }
}

static void multires_set_tot_mdisps(Mesh *me, int lvl)
{
  MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
  int i;

  if (mdisps) {
    for (i = 0; i < me->totloop; i++, mdisps++) {
      mdisps->totdisp = multires_grid_tot[lvl];
      mdisps->level = lvl;
    }
  }
}

static void multires_reallocate_mdisps(int totloop, MDisps *mdisps, int lvl)
{
  int i;

  /* reallocate displacements to be filled in */
  for (i = 0; i < totloop; i++) {
    int totdisp = multires_grid_tot[lvl];
    float(*disps)[3] = MEM_calloc_arrayN(totdisp, sizeof(float[3]), "multires disps");

    if (mdisps[i].disps) {
      MEM_freeN(mdisps[i].disps);
    }

    if (mdisps[i].level && mdisps[i].hidden) {
      multires_mdisps_subdivide_hidden(&mdisps[i], lvl);
    }

    mdisps[i].disps = disps;
    mdisps[i].totdisp = totdisp;
    mdisps[i].level = lvl;
  }
}

static void multires_copy_grid(float (*gridA)[3], float (*gridB)[3], int sizeA, int sizeB)
{
  int x, y, j, skip;

  if (sizeA > sizeB) {
    skip = (sizeA - 1) / (sizeB - 1);

    for (j = 0, y = 0; y < sizeB; y++) {
      for (x = 0; x < sizeB; x++, j++) {
        copy_v3_v3(gridA[y * skip * sizeA + x * skip], gridB[j]);
      }
    }
  }
  else {
    skip = (sizeB - 1) / (sizeA - 1);

    for (j = 0, y = 0; y < sizeA; y++) {
      for (x = 0; x < sizeA; x++, j++) {
        copy_v3_v3(gridA[j], gridB[y * skip * sizeB + x * skip]);
      }
    }
  }
}

static void multires_copy_dm_grid(CCGElem *gridA, CCGElem *gridB, CCGKey *keyA, CCGKey *keyB)
{
  int x, y, j, skip;

  if (keyA->grid_size > keyB->grid_size) {
    skip = (keyA->grid_size - 1) / (keyB->grid_size - 1);

    for (j = 0, y = 0; y < keyB->grid_size; y++) {
      for (x = 0; x < keyB->grid_size; x++, j++) {
        memcpy(CCG_elem_offset_co(keyA, gridA, y * skip * keyA->grid_size + x * skip),
               CCG_elem_offset_co(keyB, gridB, j),
               keyA->elem_size);
      }
    }
  }
  else {
    skip = (keyB->grid_size - 1) / (keyA->grid_size - 1);

    for (j = 0, y = 0; y < keyA->grid_size; y++) {
      for (x = 0; x < keyA->grid_size; x++, j++) {
        memcpy(CCG_elem_offset_co(keyA, gridA, j),
               CCG_elem_offset_co(keyB, gridB, y * skip * keyB->grid_size + x * skip),
               keyA->elem_size);
      }
    }
  }
}

/* Reallocate gpm->data at a lower resolution and copy values over
 * from the original high-resolution data */
static void multires_grid_paint_mask_downsample(GridPaintMask *gpm, int level)
{
  if (level < gpm->level) {
    int gridsize = BKE_ccg_gridsize(level);
    float *data = MEM_calloc_arrayN(
        square_i(gridsize), sizeof(float), "multires_grid_paint_mask_downsample");
    int x, y;

    for (y = 0; y < gridsize; y++) {
      for (x = 0; x < gridsize; x++) {
        data[y * gridsize + x] = paint_grid_paint_mask(gpm, level, x, y);
      }
    }

    MEM_freeN(gpm->data);
    gpm->data = data;
    gpm->level = level;
  }
}

static void multires_del_higher(MultiresModifierData *mmd, Object *ob, int lvl)
{
  Mesh *me = (Mesh *)ob->data;
  int levels = mmd->totlvl - lvl;
  MDisps *mdisps;
  GridPaintMask *gpm;

  multires_set_tot_mdisps(me, mmd->totlvl);
  multiresModifier_ensure_external_read(me, mmd);
  mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
  gpm = CustomData_get_layer(&me->ldata, CD_GRID_PAINT_MASK);

  multires_force_sculpt_rebuild(ob);

  if (mdisps && levels > 0) {
    if (lvl > 0) {
      /* MLoop *ml = me->mloop; */ /*UNUSED*/
      int nsize = multires_side_tot[lvl];
      int hsize = multires_side_tot[mmd->totlvl];
      int i, j;

      for (i = 0; i < me->totpoly; i++) {
        for (j = 0; j < me->mpoly[i].totloop; j++) {
          int g = me->mpoly[i].loopstart + j;
          MDisps *mdisp = &mdisps[g];
          float(*disps)[3], (*ndisps)[3], (*hdisps)[3];
          int totdisp = multires_grid_tot[lvl];

          disps = MEM_calloc_arrayN(totdisp, sizeof(float[3]), "multires disps");

          if (mdisp->disps != NULL) {
            ndisps = disps;
            hdisps = mdisp->disps;

            multires_copy_grid(ndisps, hdisps, nsize, hsize);
            if (mdisp->hidden) {
              BLI_bitmap *gh = multires_mdisps_downsample_hidden(mdisp->hidden, mdisp->level, lvl);
              MEM_freeN(mdisp->hidden);
              mdisp->hidden = gh;
            }

            MEM_freeN(mdisp->disps);
          }

          mdisp->disps = disps;
          mdisp->totdisp = totdisp;
          mdisp->level = lvl;

          if (gpm) {
            multires_grid_paint_mask_downsample(&gpm[g], lvl);
          }
        }
      }
    }
    else {
      multires_customdata_delete(me);
    }
  }

  multires_set_tot_level(ob, mmd, lvl);
}

/* (direction = 1) for delete higher, (direction = 0) for lower (not implemented yet) */
void multiresModifier_del_levels(MultiresModifierData *mmd,
                                 Scene *scene,
                                 Object *ob,
                                 int direction)
{
  Mesh *me = BKE_mesh_from_object(ob);
  int lvl = multires_get_level(scene, ob, mmd, false, true);
  int levels = mmd->totlvl - lvl;
  MDisps *mdisps;

  multires_set_tot_mdisps(me, mmd->totlvl);
  multiresModifier_ensure_external_read(me, mmd);
  mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);

  multires_force_sculpt_rebuild(ob);

  if (mdisps && levels > 0 && direction == 1) {
    multires_del_higher(mmd, ob, lvl);
  }

  multires_set_tot_level(ob, mmd, lvl);
}

static DerivedMesh *multires_dm_create_local(Scene *scene,
                                             Object *ob,
                                             DerivedMesh *dm,
                                             int lvl,
                                             int totlvl,
                                             bool alloc_paint_mask,
                                             int flags)
{
  MultiresModifierData mmd = {{NULL}};

  mmd.lvl = lvl;
  mmd.sculptlvl = lvl;
  mmd.renderlvl = lvl;
  mmd.totlvl = totlvl;

  flags |= MULTIRES_USE_LOCAL_MMD;
  if (alloc_paint_mask) {
    flags |= MULTIRES_ALLOC_PAINT_MASK;
  }

  return multires_make_derived_from_derived(dm, &mmd, scene, ob, flags);
}

static DerivedMesh *subsurf_dm_create_local(Scene *scene,
                                            Object *ob,
                                            DerivedMesh *dm,
                                            int lvl,
                                            bool is_simple,
                                            bool is_optimal,
                                            bool is_plain_uv,
                                            bool alloc_paint_mask,
                                            bool for_render,
                                            SubsurfFlags flags)
{
  SubsurfModifierData smd = {{NULL}};

  smd.levels = smd.renderLevels = lvl;
  smd.quality = 3;
  if (!is_plain_uv) {
    smd.uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
  }
  else {
    smd.uv_smooth = SUBSURF_UV_SMOOTH_NONE;
  }
  if (is_simple) {
    smd.subdivType = ME_SIMPLE_SUBSURF;
  }
  if (is_optimal) {
    smd.flags |= eSubsurfModifierFlag_ControlEdges;
  }

  if (ob->mode & OB_MODE_EDIT) {
    flags |= SUBSURF_IN_EDIT_MODE;
  }
  if (alloc_paint_mask) {
    flags |= SUBSURF_ALLOC_PAINT_MASK;
  }
  if (for_render) {
    flags |= SUBSURF_USE_RENDER_PARAMS;
  }

  return subsurf_make_derived_from_derived(dm, &smd, scene, NULL, flags);
}

static void grid_tangent(const CCGKey *key, int x, int y, int axis, CCGElem *grid, float t[3])
{
  if (axis == 0) {
    if (x == key->grid_size - 1) {
      if (y == key->grid_size - 1) {
        sub_v3_v3v3(
            t, CCG_grid_elem_co(key, grid, x, y - 1), CCG_grid_elem_co(key, grid, x - 1, y - 1));
      }
      else {
        sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y), CCG_grid_elem_co(key, grid, x - 1, y));
      }
    }
    else {
      sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x + 1, y), CCG_grid_elem_co(key, grid, x, y));
    }
  }
  else if (axis == 1) {
    if (y == key->grid_size - 1) {
      if (x == key->grid_size - 1) {
        sub_v3_v3v3(
            t, CCG_grid_elem_co(key, grid, x - 1, y), CCG_grid_elem_co(key, grid, x - 1, (y - 1)));
      }
      else {
        sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y), CCG_grid_elem_co(key, grid, x, (y - 1)));
      }
    }
    else {
      sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, (y + 1)), CCG_grid_elem_co(key, grid, x, y));
    }
  }
}

/* Construct 3x3 tangent-space matrix in 'mat' */
static void grid_tangent_matrix(float mat[3][3], const CCGKey *key, int x, int y, CCGElem *grid)
{
  grid_tangent(key, x, y, 0, grid, mat[0]);
  normalize_v3(mat[0]);

  grid_tangent(key, x, y, 1, grid, mat[1]);
  normalize_v3(mat[1]);

  copy_v3_v3(mat[2], CCG_grid_elem_no(key, grid, x, y));
}

typedef struct MultiresThreadedData {
  DispOp op;
  CCGElem **gridData, **subGridData;
  CCGKey *key;
  CCGKey *sub_key;
  MPoly *mpoly;
  MDisps *mdisps;
  GridPaintMask *grid_paint_mask;
  int *gridOffset;
  int gridSize, dGridSize, dSkip;
  float (*smat)[3];
} MultiresThreadedData;

static void multires_disp_run_cb(void *__restrict userdata,
                                 const int pidx,
                                 const TaskParallelTLS *__restrict UNUSED(tls))
{
  MultiresThreadedData *tdata = userdata;

  DispOp op = tdata->op;
  CCGElem **gridData = tdata->gridData;
  CCGElem **subGridData = tdata->subGridData;
  CCGKey *key = tdata->key;
  MPoly *mpoly = tdata->mpoly;
  MDisps *mdisps = tdata->mdisps;
  GridPaintMask *grid_paint_mask = tdata->grid_paint_mask;
  int *gridOffset = tdata->gridOffset;
  int gridSize = tdata->gridSize;
  int dGridSize = tdata->dGridSize;
  int dSkip = tdata->dSkip;

  const int numVerts = mpoly[pidx].totloop;
  int S, x, y, gIndex = gridOffset[pidx];

  for (S = 0; S < numVerts; S++, gIndex++) {
    GridPaintMask *gpm = grid_paint_mask ? &grid_paint_mask[gIndex] : NULL;
    MDisps *mdisp = &mdisps[mpoly[pidx].loopstart + S];
    CCGElem *grid = gridData[gIndex];
    CCGElem *subgrid = subGridData[gIndex];
    float(*dispgrid)[3] = NULL;

    dispgrid = mdisp->disps;

    /* if needed, reallocate multires paint mask */
    if (gpm && gpm->level < key->level) {
      gpm->level = key->level;
      if (gpm->data) {
        MEM_freeN(gpm->data);
      }
      gpm->data = MEM_calloc_arrayN(key->grid_area, sizeof(float), "gpm.data");
    }

    for (y = 0; y < gridSize; y++) {
      for (x = 0; x < gridSize; x++) {
        float *co = CCG_grid_elem_co(key, grid, x, y);
        float *sco = CCG_grid_elem_co(key, subgrid, x, y);
        float *data = dispgrid[dGridSize * y * dSkip + x * dSkip];
        float mat[3][3], disp[3], d[3], mask;

        /* construct tangent space matrix */
        grid_tangent_matrix(mat, key, x, y, subgrid);

        switch (op) {
          case APPLY_DISPLACEMENTS:
            /* Convert displacement to object space
             * and add to grid points */
            mul_v3_m3v3(disp, mat, data);
            add_v3_v3v3(co, sco, disp);
            break;
          case CALC_DISPLACEMENTS:
            /* Calculate displacement between new and old
             * grid points and convert to tangent space */
            sub_v3_v3v3(disp, co, sco);
            invert_m3(mat);
            mul_v3_m3v3(data, mat, disp);
            break;
          case ADD_DISPLACEMENTS:
            /* Convert subdivided displacements to tangent
             * space and add to the original displacements */
            invert_m3(mat);
            mul_v3_m3v3(d, mat, co);
            add_v3_v3(data, d);
            break;
        }

        if (gpm) {
          switch (op) {
            case APPLY_DISPLACEMENTS:
              /* Copy mask from gpm to DM */
              *CCG_grid_elem_mask(key, grid, x, y) = paint_grid_paint_mask(gpm, key->level, x, y);
              break;
            case CALC_DISPLACEMENTS:
              /* Copy mask from DM to gpm */
              mask = *CCG_grid_elem_mask(key, grid, x, y);
              gpm->data[y * gridSize + x] = CLAMPIS(mask, 0, 1);
              break;
            case ADD_DISPLACEMENTS:
              /* Add mask displacement to gpm */
              gpm->data[y * gridSize + x] += *CCG_grid_elem_mask(key, grid, x, y);
              break;
          }
        }
      }
    }
  }
}

/* XXX WARNING: subsurf elements from dm and oldGridData *must* be of the same format (size),
 *              because this code uses CCGKey's info from dm to access oldGridData's normals
 *              (through the call to grid_tangent_matrix())! */
static void multiresModifier_disp_run(
    DerivedMesh *dm, Mesh *me, DerivedMesh *dm2, DispOp op, CCGElem **oldGridData, int totlvl)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  CCGElem **gridData, **subGridData;
  CCGKey key;
  MPoly *mpoly = me->mpoly;
  MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
  GridPaintMask *grid_paint_mask = NULL;
  int *gridOffset;
  int i, gridSize, dGridSize, dSkip;
  int totloop, totpoly;

  /* this happens in the dm made by bmesh_mdisps_space_set */
  if (dm2 && CustomData_has_layer(&dm2->loopData, CD_MDISPS)) {
    mpoly = CustomData_get_layer(&dm2->polyData, CD_MPOLY);
    mdisps = CustomData_get_layer(&dm2->loopData, CD_MDISPS);
    totloop = dm2->numLoopData;
    totpoly = dm2->numPolyData;
  }
  else {
    totloop = me->totloop;
    totpoly = me->totpoly;
  }

  if (!mdisps) {
    if (op == CALC_DISPLACEMENTS) {
      mdisps = CustomData_add_layer(&me->ldata, CD_MDISPS, CD_DEFAULT, NULL, me->totloop);
    }
    else {
      return;
    }
  }

  /*numGrids = dm->getNumGrids(dm);*/ /*UNUSED*/
  gridSize = dm->getGridSize(dm);
  gridData = dm->getGridData(dm);
  gridOffset = dm->getGridOffset(dm);
  dm->getGridKey(dm, &key);
  subGridData = (oldGridData) ? oldGridData : gridData;

  dGridSize = multires_side_tot[totlvl];
  dSkip = (dGridSize - 1) / (gridSize - 1);

  /* multires paint masks */
  if (key.has_mask) {
    grid_paint_mask = CustomData_get_layer(&me->ldata, CD_GRID_PAINT_MASK);
  }

  /* when adding new faces in edit mode, need to allocate disps */
  for (i = 0; i < totloop; i++) {
    if (mdisps[i].disps == NULL) {
      multires_reallocate_mdisps(totloop, mdisps, totlvl);
      break;
    }
  }

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = CCG_TASK_LIMIT;

  MultiresThreadedData data = {
      .op = op,
      .gridData = gridData,
      .subGridData = subGridData,
      .key = &key,
      .mpoly = mpoly,
      .mdisps = mdisps,
      .grid_paint_mask = grid_paint_mask,
      .gridOffset = gridOffset,
      .gridSize = gridSize,
      .dGridSize = dGridSize,
      .dSkip = dSkip,
  };

  BLI_task_parallel_range(0, totpoly, &data, multires_disp_run_cb, &settings);

  if (op == APPLY_DISPLACEMENTS) {
    ccgSubSurf_stitchFaces(ccgdm->ss, 0, NULL, 0);
    ccgSubSurf_updateNormals(ccgdm->ss, NULL, 0);
  }
}

void multires_modifier_update_mdisps(struct DerivedMesh *dm, Scene *scene)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  Object *ob;
  Mesh *me;
  MDisps *mdisps;
  MultiresModifierData *mmd;

  ob = ccgdm->multires.ob;
  me = ccgdm->multires.ob->data;
  mmd = ccgdm->multires.mmd;
  multires_set_tot_mdisps(me, mmd->totlvl);
  multiresModifier_ensure_external_read(me, mmd);
  mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);

  if (mdisps) {
    int lvl = ccgdm->multires.lvl;
    int totlvl = ccgdm->multires.totlvl;

    if (lvl < totlvl) {
      DerivedMesh *lowdm, *cddm, *highdm;
      CCGElem **highGridData, **lowGridData, **subGridData, **gridData, *diffGrid;
      CCGKey highGridKey, lowGridKey;
      CCGSubSurf *ss;
      int i, j, numGrids, highGridSize, lowGridSize;
      const bool has_mask = CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK);

      /* Create subsurf DM from original mesh at high level. */
      /* TODO: use mesh_deform_eval when sculpting on deformed mesh. */
      cddm = CDDM_from_mesh(me);
      DM_set_only_copy(cddm, &CD_MASK_BAREMESH);

      highdm = subsurf_dm_create_local(scene,
                                       ob,
                                       cddm,
                                       totlvl,
                                       false,
                                       0,
                                       mmd->uv_smooth == SUBSURF_UV_SMOOTH_NONE,
                                       has_mask,
                                       false,
                                       SUBSURF_IGNORE_SIMPLIFY);
      ss = ((CCGDerivedMesh *)highdm)->ss;

      /* create multires DM from original mesh and displacements */
      lowdm = multires_dm_create_local(
          scene, ob, cddm, lvl, totlvl, has_mask, MULTIRES_IGNORE_SIMPLIFY);
      cddm->release(cddm);

      /* gather grid data */
      numGrids = highdm->getNumGrids(highdm);
      highGridSize = highdm->getGridSize(highdm);
      highGridData = highdm->getGridData(highdm);
      highdm->getGridKey(highdm, &highGridKey);
      lowGridSize = lowdm->getGridSize(lowdm);
      lowGridData = lowdm->getGridData(lowdm);
      lowdm->getGridKey(lowdm, &lowGridKey);
      gridData = dm->getGridData(dm);

      BLI_assert(highGridKey.elem_size == lowGridKey.elem_size);

      subGridData = MEM_calloc_arrayN(numGrids, sizeof(CCGElem *), "subGridData*");
      diffGrid = MEM_calloc_arrayN(lowGridKey.elem_size, lowGridSize * lowGridSize, "diff");

      for (i = 0; i < numGrids; i++) {
        /* backup subsurf grids */
        subGridData[i] = MEM_calloc_arrayN(
            highGridKey.elem_size, highGridSize * highGridSize, "subGridData");
        memcpy(
            subGridData[i], highGridData[i], highGridKey.elem_size * highGridSize * highGridSize);

        /* write difference of subsurf and displaced low level into high subsurf */
        for (j = 0; j < lowGridSize * lowGridSize; j++) {
          sub_v4_v4v4(CCG_elem_offset_co(&lowGridKey, diffGrid, j),
                      CCG_elem_offset_co(&lowGridKey, gridData[i], j),
                      CCG_elem_offset_co(&lowGridKey, lowGridData[i], j));
        }

        multires_copy_dm_grid(highGridData[i], diffGrid, &highGridKey, &lowGridKey);
      }

      /* lower level dm no longer needed at this point */
      MEM_freeN(diffGrid);
      lowdm->release(lowdm);

      /* subsurf higher levels again with difference of coordinates */
      ccgSubSurf_updateFromFaces(ss, lvl, NULL, 0);
      ccgSubSurf_updateLevels(ss, lvl, NULL, 0);

      /* add to displacements */
      multiresModifier_disp_run(highdm, me, NULL, ADD_DISPLACEMENTS, subGridData, mmd->totlvl);

      /* free */
      highdm->release(highdm);
      for (i = 0; i < numGrids; i++) {
        MEM_freeN(subGridData[i]);
      }
      MEM_freeN(subGridData);
    }
    else {
      DerivedMesh *cddm, *subdm;
      const bool has_mask = CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK);

      /* TODO: use mesh_deform_eval when sculpting on deformed mesh. */
      cddm = CDDM_from_mesh(me);
      DM_set_only_copy(cddm, &CD_MASK_BAREMESH);

      subdm = subsurf_dm_create_local(scene,
                                      ob,
                                      cddm,
                                      mmd->totlvl,
                                      false,
                                      0,
                                      mmd->uv_smooth == SUBSURF_UV_SMOOTH_NONE,
                                      has_mask,
                                      false,
                                      SUBSURF_IGNORE_SIMPLIFY);
      cddm->release(cddm);

      multiresModifier_disp_run(
          dm, me, NULL, CALC_DISPLACEMENTS, subdm->getGridData(subdm), mmd->totlvl);

      subdm->release(subdm);
    }
  }
}

void multires_modifier_update_hidden(DerivedMesh *dm)
{
  CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
  BLI_bitmap **grid_hidden = ccgdm->gridHidden;
  Mesh *me = ccgdm->multires.ob->data;
  MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
  int totlvl = ccgdm->multires.totlvl;
  int lvl = ccgdm->multires.lvl;

  if (mdisps) {
    int i;

    for (i = 0; i < me->totloop; i++) {
      MDisps *md = &mdisps[i];
      BLI_bitmap *gh = grid_hidden[i];

      if (!gh && md->hidden) {
        MEM_freeN(md->hidden);
        md->hidden = NULL;
      }
      else if (gh) {
        gh = multires_mdisps_upsample_hidden(gh, lvl, totlvl, md->hidden);
        if (md->hidden) {
          MEM_freeN(md->hidden);
        }

        md->hidden = gh;
      }
    }
  }
}

void multires_stitch_grids(Object *ob)
{
  if (ob == NULL) {
    return;
  }
  SculptSession *sculpt_session = ob->sculpt;
  if (sculpt_session == NULL) {
    return;
  }
  PBVH *pbvh = sculpt_session->pbvh;
  SubdivCCG *subdiv_ccg = sculpt_session->subdiv_ccg;
  if (pbvh == NULL || subdiv_ccg == NULL) {
    return;
  }
  BLI_assert(BKE_pbvh_type(pbvh) == PBVH_GRIDS);
  /* NOTE: Currently CCG does not keep track of faces, making it impossible
   * to use BKE_pbvh_get_grid_updates().
   */
  CCGFace **faces;
  int num_faces;
  BKE_pbvh_get_grid_updates(pbvh, false, (void ***)&faces, &num_faces);
  if (num_faces) {
    BKE_subdiv_ccg_average_stitch_faces(subdiv_ccg, faces, num_faces);
    MEM_freeN(faces);
  }
}

DerivedMesh *multires_make_derived_from_derived(
    DerivedMesh *dm, MultiresModifierData *mmd, Scene *scene, Object *ob, MultiresFlags flags)
{
  Mesh *me = ob->data;
  DerivedMesh *result;
  CCGDerivedMesh *ccgdm = NULL;
  CCGElem **gridData, **subGridData;
  CCGKey key;
  const bool render = (flags & MULTIRES_USE_RENDER_PARAMS) != 0;
  const bool ignore_simplify = (flags & MULTIRES_IGNORE_SIMPLIFY) != 0;
  int lvl = multires_get_level(scene, ob, mmd, render, ignore_simplify);
  int i, gridSize, numGrids;

  if (lvl == 0) {
    return dm;
  }

  const int subsurf_flags = ignore_simplify ? SUBSURF_IGNORE_SIMPLIFY : 0;

  result = subsurf_dm_create_local(scene,
                                   ob,
                                   dm,
                                   lvl,
                                   false,
                                   mmd->flags & eMultiresModifierFlag_ControlEdges,
                                   mmd->uv_smooth == SUBSURF_UV_SMOOTH_NONE,
                                   flags & MULTIRES_ALLOC_PAINT_MASK,
                                   render,
                                   subsurf_flags);

  if (!(flags & MULTIRES_USE_LOCAL_MMD)) {
    ccgdm = (CCGDerivedMesh *)result;

    ccgdm->multires.ob = ob;
    ccgdm->multires.mmd = mmd;
    ccgdm->multires.local_mmd = 0;
    ccgdm->multires.lvl = lvl;
    ccgdm->multires.totlvl = mmd->totlvl;
    ccgdm->multires.modified_flags = 0;
  }

  numGrids = result->getNumGrids(result);
  gridSize = result->getGridSize(result);
  gridData = result->getGridData(result);
  result->getGridKey(result, &key);

  subGridData = MEM_malloc_arrayN(numGrids, sizeof(CCGElem *), "subGridData*");

  for (i = 0; i < numGrids; i++) {
    subGridData[i] = MEM_malloc_arrayN(key.elem_size, gridSize * gridSize, "subGridData");
    memcpy(subGridData[i], gridData[i], key.elem_size * gridSize * gridSize);
  }

  multires_set_tot_mdisps(me, mmd->totlvl);
  multiresModifier_ensure_external_read(me, mmd);

  /*run displacement*/
  multiresModifier_disp_run(result, ob->data, dm, APPLY_DISPLACEMENTS, subGridData, mmd->totlvl);

  /* copy hidden elements for this level */
  if (ccgdm) {
    multires_output_hidden_to_ccgdm(ccgdm, me, lvl);
  }

  for (i = 0; i < numGrids; i++) {
    MEM_freeN(subGridData[i]);
  }
  MEM_freeN(subGridData);

  return result;
}

/* Adapted from sculptmode.c */
void old_mdisps_bilinear(float out[3], float (*disps)[3], const int st, float u, float v)
{
  int x, y, x2, y2;
  const int st_max = st - 1;
  float urat, vrat, uopp;
  float d[4][3], d2[2][3];

  if (!disps || isnan(u) || isnan(v)) {
    return;
  }

  if (u < 0) {
    u = 0;
  }
  else if (u >= st) {
    u = st_max;
  }
  if (v < 0) {
    v = 0;
  }
  else if (v >= st) {
    v = st_max;
  }

  x = floor(u);
  y = floor(v);
  x2 = x + 1;
  y2 = y + 1;

  if (x2 >= st) {
    x2 = st_max;
  }
  if (y2 >= st) {
    y2 = st_max;
  }

  urat = u - x;
  vrat = v - y;
  uopp = 1 - urat;

  mul_v3_v3fl(d[0], disps[y * st + x], uopp);
  mul_v3_v3fl(d[1], disps[y * st + x2], urat);
  mul_v3_v3fl(d[2], disps[y2 * st + x], uopp);
  mul_v3_v3fl(d[3], disps[y2 * st + x2], urat);

  add_v3_v3v3(d2[0], d[0], d[1]);
  add_v3_v3v3(d2[1], d[2], d[3]);
  mul_v3_fl(d2[0], 1 - vrat);
  mul_v3_fl(d2[1], vrat);

  add_v3_v3v3(out, d2[0], d2[1]);
}

/* If 'ob_src' and 'ob_dst' both have multires modifiers, synchronize them
 * such that 'ob_dst' has the same total number of levels as 'ob_src'. */
void multiresModifier_sync_levels_ex(Object *ob_dst,
                                     MultiresModifierData *mmd_src,
                                     MultiresModifierData *mmd_dst)
{
  if (mmd_src->totlvl == mmd_dst->totlvl) {
    return;
  }

  if (mmd_src->totlvl > mmd_dst->totlvl) {
    multiresModifier_subdivide_to_level(
        ob_dst, mmd_dst, mmd_src->totlvl, MULTIRES_SUBDIVIDE_CATMULL_CLARK);
  }
  else {
    multires_del_higher(mmd_dst, ob_dst, mmd_src->totlvl);
  }
}

static void multires_sync_levels(Scene *scene, Object *ob_src, Object *ob_dst)
{
  MultiresModifierData *mmd_src = get_multires_modifier(scene, ob_src, true);
  MultiresModifierData *mmd_dst = get_multires_modifier(scene, ob_dst, true);

  if (!mmd_src) {
    /* object could have MDISP even when there is no multires modifier
     * this could lead to troubles due to i've got no idea how mdisp could be
     * up-sampled correct without modifier data.
     * just remove mdisps if no multires present (nazgul) */

    multires_customdata_delete(ob_src->data);
  }

  if (mmd_src && mmd_dst) {
    multiresModifier_sync_levels_ex(ob_dst, mmd_src, mmd_dst);
  }
}

static void multires_apply_uniform_scale(Object *object, const float scale)
{
  Mesh *mesh = (Mesh *)object->data;
  MDisps *mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
  for (int i = 0; i < mesh->totloop; i++) {
    MDisps *grid = &mdisps[i];
    for (int j = 0; j < grid->totdisp; j++) {
      mul_v3_fl(grid->disps[j], scale);
    }
  }
}

static void multires_apply_smat(struct Depsgraph *UNUSED(depsgraph),
                                Scene *scene,
                                Object *object,
                                const float smat[3][3])
{
  const MultiresModifierData *mmd = get_multires_modifier(scene, object, true);
  if (mmd == NULL || mmd->totlvl == 0) {
    return;
  }
  /* Make sure layer present. */
  Mesh *mesh = (Mesh *)object->data;
  multiresModifier_ensure_external_read(mesh, mmd);
  if (!CustomData_get_layer(&mesh->ldata, CD_MDISPS)) {
    return;
  }
  if (is_uniform_scaled_m3(smat)) {
    const float scale = mat3_to_scale(smat);
    multires_apply_uniform_scale(object, scale);
  }
  else {
    /* TODO(sergey): This branch of code actually requires more work to
     * preserve all the details.
     */
    const float scale = mat3_to_scale(smat);
    multires_apply_uniform_scale(object, scale);
  }
}

int multires_mdisp_corners(MDisps *s)
{
  int lvl = 13;

  while (lvl > 0) {
    int side = (1 << (lvl - 1)) + 1;
    if ((s->totdisp % (side * side)) == 0) {
      return s->totdisp / (side * side);
    }
    lvl--;
  }

  return 0;
}

void multiresModifier_scale_disp(struct Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  float smat[3][3];

  /* object's scale matrix */
  BKE_object_scale_to_mat3(ob, smat);

  multires_apply_smat(depsgraph, scene, ob, smat);
}

void multiresModifier_prepare_join(struct Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   Object *to_ob)
{
  float smat[3][3], tmat[3][3], mat[3][3];
  multires_sync_levels(scene, to_ob, ob);

  /* construct scale matrix for displacement */
  BKE_object_scale_to_mat3(to_ob, tmat);
  invert_m3(tmat);
  BKE_object_scale_to_mat3(ob, smat);
  mul_m3_m3m3(mat, smat, tmat);

  multires_apply_smat(depsgraph, scene, ob, mat);
}

/* update multires data after topology changing */
void multires_topology_changed(Mesh *me)
{
  MDisps *mdisp = NULL, *cur = NULL;
  int i, grid = 0;

  CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
  mdisp = CustomData_get_layer(&me->ldata, CD_MDISPS);

  if (!mdisp) {
    return;
  }

  cur = mdisp;
  for (i = 0; i < me->totloop; i++, cur++) {
    if (cur->totdisp) {
      grid = mdisp->totdisp;

      break;
    }
  }

  for (i = 0; i < me->totloop; i++, mdisp++) {
    /* allocate memory for mdisp, the whole disp layer would be erased otherwise */
    if (!mdisp->totdisp || !mdisp->disps) {
      if (grid) {
        mdisp->totdisp = grid;
        mdisp->disps = MEM_calloc_arrayN(sizeof(float[3]), mdisp->totdisp, "mdisp topology");
      }

      continue;
    }
  }
}

/* Makes sure data from an external file is fully read.
 *
 * Since the multires data files only contain displacement vectors without knowledge about
 * subdivision level some extra work is needed. Namely make is to all displacement grids have
 * proper level and number of displacement vectors set.  */
void multires_ensure_external_read(struct Mesh *mesh, int top_level)
{
  if (!CustomData_external_test(&mesh->ldata, CD_MDISPS)) {
    return;
  }

  MDisps *mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
  if (mdisps == NULL) {
    mdisps = CustomData_add_layer(&mesh->ldata, CD_MDISPS, CD_DEFAULT, NULL, mesh->totloop);
  }

  const int totloop = mesh->totloop;

  for (int i = 0; i < totloop; ++i) {
    if (mdisps[i].level != top_level) {
      MEM_SAFE_FREE(mdisps[i].disps);
    }

    /* NOTE: CustomData_external_read will take care of allocation of displacement vectors if
     * they are missing. */

    const int totdisp = multires_grid_tot[top_level];
    mdisps[i].totdisp = totdisp;
    mdisps[i].level = top_level;
  }

  CustomData_external_read(&mesh->ldata, &mesh->id, CD_MASK_MDISPS, mesh->totloop);
}
void multiresModifier_ensure_external_read(struct Mesh *mesh, const MultiresModifierData *mmd)
{
  multires_ensure_external_read(mesh, mmd->totlvl);
}

/***************** Multires interpolation stuff *****************/

/* Find per-corner coordinate with given per-face UV coord */
int mdisp_rot_face_to_crn(struct MVert *UNUSED(mvert),
                          struct MPoly *mpoly,
                          struct MLoop *UNUSED(mloop),
                          const struct MLoopTri *UNUSED(lt),
                          const int face_side,
                          const float u,
                          const float v,
                          float *x,
                          float *y)
{
  const float offset = face_side * 0.5f - 0.5f;
  int S = 0;

  if (mpoly->totloop == 4) {
    if (u <= offset && v <= offset) {
      S = 0;
    }
    else if (u > offset && v <= offset) {
      S = 1;
    }
    else if (u > offset && v > offset) {
      S = 2;
    }
    else if (u <= offset && v >= offset) {
      S = 3;
    }

    if (S == 0) {
      *y = offset - u;
      *x = offset - v;
    }
    else if (S == 1) {
      *x = u - offset;
      *y = offset - v;
    }
    else if (S == 2) {
      *y = u - offset;
      *x = v - offset;
    }
    else if (S == 3) {
      *x = offset - u;
      *y = v - offset;
    }
  }
  else if (mpoly->totloop == 3) {
    int grid_size = offset;
    float w = (face_side - 1) - u - v;
    float W1, W2;

    if (u >= v && u >= w) {
      S = 0;
      W1 = w;
      W2 = v;
    }
    else if (v >= u && v >= w) {
      S = 1;
      W1 = u;
      W2 = w;
    }
    else {
      S = 2;
      W1 = v;
      W2 = u;
    }

    W1 /= (face_side - 1);
    W2 /= (face_side - 1);

    *x = (1 - (2 * W1) / (1 - W2)) * grid_size;
    *y = (1 - (2 * W2) / (1 - W1)) * grid_size;
  }
  else {
    /* the complicated ngon case: find the actual coordinate from
     * the barycentric coordinates and finally find the closest vertex
     * should work reliably for convex cases only but better than nothing */

#if 0
    int minS, i;
    float mindist = FLT_MAX;

    for (i = 0; i < mpoly->totloop; i++) {
      float len = len_v3v3(NULL, mvert[mloop[mpoly->loopstart + i].v].co);
      if (len < mindist) {
        mindist = len;
        minS = i;
      }
    }
    S = minS;
#endif
    /* temp not implemented yet and also not working properly in current master.
     * (was worked around by subdividing once) */
    S = 0;
    *x = 0;
    *y = 0;
  }

  return S;
}
