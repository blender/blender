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
 * The Original Code is Copyright (C) 20014 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_effect.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_light.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_gpencil.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/**
 * Restore the object->data to a non-modifier evaluated state.
 *
 * Some changes done directly in evaluated object require them to be reset
 * before being re-evaluated.
 * For example, we need to call this before #BKE_mesh_new_from_object(),
 * in case we removed/added modifiers in the evaluated object.
 */
void BKE_object_eval_reset(Object *ob_eval)
{
  BKE_object_free_derived_caches(ob_eval);
}

void BKE_object_eval_local_transform(Depsgraph *depsgraph, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* calculate local matrix */
  BKE_object_to_mat4(ob, ob->obmat);
}

/* Evaluate parent */
/* NOTE: based on solve_parenting(), but with the cruft stripped out */
void BKE_object_eval_parent(Depsgraph *depsgraph, Object *ob)
{
  Object *par = ob->parent;

  float totmat[4][4];
  float tmat[4][4];
  float locmat[4][4];

  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* get local matrix (but don't calculate it, as that was done already!) */
  // XXX: redundant?
  copy_m4_m4(locmat, ob->obmat);

  /* get parent effect matrix */
  BKE_object_get_parent_matrix(ob, par, totmat);

  /* total */
  mul_m4_m4m4(tmat, totmat, ob->parentinv);
  mul_m4_m4m4(ob->obmat, tmat, locmat);

  /* origin, for help line */
  if ((ob->partype & PARTYPE) == PARSKEL) {
    copy_v3_v3(ob->runtime.parent_display_origin, par->obmat[3]);
  }
  else {
    copy_v3_v3(ob->runtime.parent_display_origin, totmat[3]);
  }
}

void BKE_object_eval_constraints(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bConstraintOb *cob;
  float ctime = BKE_scene_frame_get(scene);

  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* evaluate constraints stack */
  /* TODO: split this into:
   * - pre (i.e. BKE_constraints_make_evalob, per-constraint (i.e.
   * - inner body of BKE_constraints_solve),
   * - post (i.e. BKE_constraints_clear_evalob)
   *
   * Not sure why, this is from Joshua - sergey
   *
   */
  cob = BKE_constraints_make_evalob(depsgraph, scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
  BKE_constraints_solve(depsgraph, &ob->constraints, cob, ctime);
  BKE_constraints_clear_evalob(cob);
}

void BKE_object_eval_transform_final(Depsgraph *depsgraph, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);
  /* Make sure inverse matrix is always up to date. This way users of it
   * do not need to worry about recalculating it. */
  invert_m4_m4(ob->imat, ob->obmat);
  /* Set negative scale flag in object. */
  if (is_negative_m4(ob->obmat)) {
    ob->transflag |= OB_NEG_SCALE;
  }
  else {
    ob->transflag &= ~OB_NEG_SCALE;
  }
}

void BKE_object_handle_data_update(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* includes all keys and modifiers */
  switch (ob->type) {
    case OB_MESH: {
#if 0
      BMEditMesh *em = (ob->mode & OB_MODE_EDIT) ? BKE_editmesh_from_object(ob) : NULL;
#else
      BMEditMesh *em = (ob->mode & OB_MODE_EDIT) ? ((Mesh *)ob->data)->edit_mesh : NULL;
      if (em && em->ob != ob) {
        em = NULL;
      }
#endif

      CustomData_MeshMasks cddata_masks = scene->customdata_mask;
      CustomData_MeshMasks_update(&cddata_masks, &CD_MASK_BAREMESH);
#ifdef WITH_FREESTYLE
      /* make sure Freestyle edge/face marks appear in DM for render (see T40315) */
      if (DEG_get_mode(depsgraph) != DAG_EVAL_VIEWPORT) {
        cddata_masks.emask |= CD_MASK_FREESTYLE_EDGE;
        cddata_masks.pmask |= CD_MASK_FREESTYLE_FACE;
      }
#endif
      if (em) {
        makeDerivedMesh(depsgraph, scene, ob, em, &cddata_masks); /* was CD_MASK_BAREMESH */
      }
      else {
        makeDerivedMesh(depsgraph, scene, ob, NULL, &cddata_masks);
      }
      break;
    }
    case OB_ARMATURE:
      if (ID_IS_LINKED(ob) && ob->proxy_from) {
        if (BKE_pose_copy_result(ob->pose, ob->proxy_from->pose) == false) {
          printf("Proxy copy error, lib Object: %s proxy Object: %s\n",
                 ob->id.name + 2,
                 ob->proxy_from->id.name + 2);
        }
      }
      else {
        BKE_pose_where_is(depsgraph, scene, ob);
      }
      break;

    case OB_MBALL:
      BKE_displist_make_mball(depsgraph, scene, ob);
      break;

    case OB_CURVE:
    case OB_SURF:
    case OB_FONT: {
      bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
      BKE_displist_make_curveTypes(depsgraph, scene, ob, for_render, false);
      break;
    }

    case OB_LATTICE:
      BKE_lattice_modifiers_calc(depsgraph, scene, ob);
      break;
  }

  /* particles */
  if (!(ob->mode & OB_MODE_EDIT) && ob->particlesystem.first) {
    const bool use_render_params = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
    ParticleSystem *tpsys, *psys;
    ob->transflag &= ~OB_DUPLIPARTS;
    psys = ob->particlesystem.first;
    while (psys) {
      if (psys_check_enabled(ob, psys, use_render_params)) {
        /* check use of dupli objects here */
        if (psys->part && (psys->part->draw_as == PART_DRAW_REND || use_render_params) &&
            ((psys->part->ren_as == PART_DRAW_OB && psys->part->instance_object) ||
             (psys->part->ren_as == PART_DRAW_GR && psys->part->instance_collection))) {
          ob->transflag |= OB_DUPLIPARTS;
        }

        particle_system_update(depsgraph, scene, ob, psys, use_render_params);
        psys = psys->next;
      }
      else if (psys->flag & PSYS_DELETE) {
        tpsys = psys->next;
        BLI_remlink(&ob->particlesystem, psys);
        psys_free(ob, psys);
        psys = tpsys;
      }
      else {
        psys = psys->next;
      }
    }
  }
  BKE_object_eval_boundbox(depsgraph, ob);
}

/**
 * TODO(sergey): Ensure that bounding box is already calculated, and move this
 * into #BKE_object_synchronize_to_original().
 */
void BKE_object_eval_boundbox(Depsgraph *depsgraph, Object *object)
{
  if (!DEG_is_active(depsgraph)) {
    return;
  }
  Object *ob_orig = DEG_get_original_object(object);
  BoundBox *bb = BKE_object_boundbox_get(object);
  if (bb != NULL) {
    if (ob_orig->runtime.bb == NULL) {
      ob_orig->runtime.bb = MEM_mallocN(sizeof(*ob_orig->runtime.bb), __func__);
    }
    *ob_orig->runtime.bb = *bb;
  }
}

void BKE_object_synchronize_to_original(Depsgraph *depsgraph, Object *object)
{
  if (!DEG_is_active(depsgraph)) {
    return;
  }
  Object *object_orig = DEG_get_original_object(object);
  /* Base flags. */
  object_orig->base_flag = object->base_flag;
  /* Transformation flags. */
  copy_m4_m4(object_orig->obmat, object->obmat);
  copy_m4_m4(object_orig->imat, object->imat);
  copy_m4_m4(object_orig->constinv, object->constinv);
  object_orig->transflag = object->transflag;
  object_orig->flag = object->flag;

  /* Copy back error messages from modifiers. */
  for (ModifierData *md = object->modifiers.first, *md_orig = object_orig->modifiers.first;
       md != NULL && md_orig != NULL;
       md = md->next, md_orig = md_orig->next) {
    BLI_assert(md->type == md_orig->type && STREQ(md->name, md_orig->name));
    MEM_SAFE_FREE(md_orig->error);
    if (md->error != NULL) {
      md_orig->error = BLI_strdup(md->error);
    }
  }
}

bool BKE_object_eval_proxy_copy(Depsgraph *depsgraph, Object *object)
{
  /* Handle proxy copy for target, */
  if (ID_IS_LINKED(object) && object->proxy_from) {
    DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
    if (object->proxy_from->proxy_group) {
      /* Transform proxy into group space. */
      Object *obg = object->proxy_from->proxy_group;
      float imat[4][4];
      invert_m4_m4(imat, obg->obmat);
      mul_m4_m4m4(object->obmat, imat, object->proxy_from->obmat);
      /* Should always be true. */
      if (obg->instance_collection) {
        add_v3_v3(object->obmat[3], obg->instance_collection->instance_offset);
      }
    }
    else {
      copy_m4_m4(object->obmat, object->proxy_from->obmat);
    }
    return true;
  }
  return false;
}

void BKE_object_eval_uber_transform(Depsgraph *depsgraph, Object *object)
{
  BKE_object_eval_proxy_copy(depsgraph, object);
}

void BKE_object_batch_cache_dirty_tag(Object *ob)
{
  switch (ob->type) {
    case OB_MESH:
      BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
      break;
    case OB_LATTICE:
      BKE_lattice_batch_cache_dirty_tag(ob->data, BKE_LATTICE_BATCH_DIRTY_ALL);
      break;
    case OB_CURVE:
    case OB_FONT:
    case OB_SURF:
      BKE_curve_batch_cache_dirty_tag(ob->data, BKE_CURVE_BATCH_DIRTY_ALL);
      break;
    case OB_MBALL:
      BKE_mball_batch_cache_dirty_tag(ob->data, BKE_MBALL_BATCH_DIRTY_ALL);
      break;
    case OB_GPENCIL:
      BKE_gpencil_batch_cache_dirty_tag(ob->data);
      break;
  }
}

void BKE_object_eval_uber_data(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);
  BLI_assert(ob->type != OB_ARMATURE);
  BKE_object_handle_data_update(depsgraph, scene, ob);
  BKE_object_batch_cache_dirty_tag(ob);
}

void BKE_object_eval_ptcache_reset(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BKE_ptcache_object_reset(scene, object, PTCACHE_RESET_DEPSGRAPH);
}

void BKE_object_eval_transform_all(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  /* This mimics full transform update chain from new depsgraph. */
  BKE_object_eval_local_transform(depsgraph, object);
  if (object->parent != NULL) {
    BKE_object_eval_parent(depsgraph, object);
  }
  if (!BLI_listbase_is_empty(&object->constraints)) {
    BKE_object_eval_constraints(depsgraph, scene, object);
  }
  BKE_object_eval_uber_transform(depsgraph, object);
  BKE_object_eval_transform_final(depsgraph, object);
}

void BKE_object_data_select_update(Depsgraph *depsgraph, ID *object_data)
{
  DEG_debug_print_eval(depsgraph, __func__, object_data->name, object_data);
  switch (GS(object_data->name)) {
    case ID_ME:
      BKE_mesh_batch_cache_dirty_tag((Mesh *)object_data, BKE_MESH_BATCH_DIRTY_SELECT);
      break;
    case ID_CU:
      BKE_curve_batch_cache_dirty_tag((Curve *)object_data, BKE_CURVE_BATCH_DIRTY_SELECT);
      break;
    case ID_LT:
      BKE_lattice_batch_cache_dirty_tag((struct Lattice *)object_data,
                                        BKE_LATTICE_BATCH_DIRTY_SELECT);
      break;
    default:
      break;
  }
}

void BKE_object_eval_eval_base_flags(Depsgraph *depsgraph,
                                     Scene *scene,
                                     const int view_layer_index,
                                     Object *object,
                                     int base_index,
                                     const bool is_from_set)
{
  /* TODO(sergey): Avoid list lookup. */
  BLI_assert(view_layer_index >= 0);
  ViewLayer *view_layer = BLI_findlink(&scene->view_layers, view_layer_index);
  BLI_assert(view_layer != NULL);
  BLI_assert(view_layer->object_bases_array != NULL);
  BLI_assert(base_index >= 0);
  BLI_assert(base_index < MEM_allocN_len(view_layer->object_bases_array) / sizeof(Base *));
  Base *base = view_layer->object_bases_array[base_index];
  BLI_assert(base->object == object);

  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);

  /* Set base flags based on collection and object restriction. */
  BKE_base_eval_flags(base);

  /* For render, compute base visibility again since BKE_base_eval_flags
   * assumed viewport visibility. Selectability does not matter here. */
  if (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER) {
    if (base->flag & BASE_ENABLED_RENDER) {
      base->flag |= BASE_VISIBLE;
    }
    else {
      base->flag &= ~BASE_VISIBLE;
    }
  }

  /* Copy flags and settings from base. */
  object->base_flag = base->flag;
  if (is_from_set) {
    object->base_flag |= BASE_FROM_SET;
    object->base_flag &= ~(BASE_SELECTED | BASE_SELECTABLE);
  }
  object->base_local_view_bits = base->local_view_bits;

  if (object->mode == OB_MODE_PARTICLE_EDIT) {
    for (ParticleSystem *psys = object->particlesystem.first; psys != NULL; psys = psys->next) {
      BKE_particle_batch_cache_dirty_tag(psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
    }
  }

  /* Copy base flag back to the original view layer for editing. */
  if (DEG_is_active(depsgraph) && (view_layer == DEG_get_evaluated_view_layer(depsgraph))) {
    Base *base_orig = base->base_orig;
    BLI_assert(base_orig != NULL);
    BLI_assert(base_orig->object != NULL);
    base_orig->flag = base->flag;
  }
}
