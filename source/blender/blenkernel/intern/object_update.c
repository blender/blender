/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_curves.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_effect.h"
#include "BKE_geometry_cache.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_light.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.h"
#include "BKE_scene.h"
#include "BKE_volume.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

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

void BKE_object_eval_parent(Depsgraph *depsgraph, Object *ob)
{
  /* NOTE: based on `solve_parenting()`, but with the cruft stripped out. */

  Object *par = ob->parent;

  float totmat[4][4];
  float tmat[4][4];
  float locmat[4][4];

  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);

  /* get local matrix (but don't calculate it, as that was done already!) */
  /* XXX: redundant? */
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
  float ctime = BKE_scene_ctime_get(scene);

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
  invert_m4_m4_safe(ob->imat, ob->obmat);
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
      CustomData_MeshMasks cddata_masks = scene->customdata_mask;
      CustomData_MeshMasks_update(&cddata_masks, &CD_MASK_BAREMESH);
      /* Custom attributes should not be removed automatically. They might be used by the render
       * engine or scripts. They can still be removed explicitly using geometry nodes. */
      cddata_masks.vmask |= CD_MASK_PROP_ALL;
      cddata_masks.emask |= CD_MASK_PROP_ALL;
      cddata_masks.fmask |= CD_MASK_PROP_ALL;
      cddata_masks.pmask |= CD_MASK_PROP_ALL;
      cddata_masks.lmask |= CD_MASK_PROP_ALL;

      /* Also copy over normal layers to avoid recomputation. */
      cddata_masks.pmask |= CD_MASK_NORMAL;
      cddata_masks.vmask |= CD_MASK_NORMAL;

      /* Make sure Freestyle edge/face marks appear in DM for render (see T40315).
       * Due to Line Art implementation, edge marks should also be shown in viewport. */
#ifdef WITH_FREESTYLE
      cddata_masks.emask |= CD_MASK_FREESTYLE_EDGE;
      cddata_masks.pmask |= CD_MASK_FREESTYLE_FACE;
      cddata_masks.vmask |= CD_MASK_MDEFORMVERT;
#endif
      if (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER) {
        /* Always compute UVs, vertex colors as orcos for render. */
        cddata_masks.lmask |= CD_MASK_MLOOPUV | CD_MASK_PROP_BYTE_COLOR;
        cddata_masks.vmask |= CD_MASK_ORCO | CD_MASK_PROP_COLOR;
      }
      makeDerivedMesh(depsgraph, scene, ob, &cddata_masks); /* was CD_MASK_BAREMESH */
      break;
    }
    case OB_ARMATURE:
      BKE_pose_where_is(depsgraph, scene, ob);
      break;

    case OB_MBALL:
      BKE_displist_make_mball(depsgraph, scene, ob);
      break;

    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_FONT: {
      bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
      BKE_displist_make_curveTypes(depsgraph, scene, ob, for_render);
      break;
    }

    case OB_LATTICE:
      BKE_lattice_modifiers_calc(depsgraph, scene, ob);
      break;
    case OB_GPENCIL: {
      BKE_gpencil_prepare_eval_data(depsgraph, scene, ob);
      BKE_gpencil_modifiers_calc(depsgraph, scene, ob);
      BKE_gpencil_update_layer_transforms(depsgraph, ob);
      break;
    }
    case OB_CURVES:
      BKE_curves_data_update(depsgraph, scene, ob);
      break;
    case OB_POINTCLOUD:
      BKE_pointcloud_data_update(depsgraph, scene, ob);
      break;
    case OB_VOLUME:
      BKE_volume_data_update(depsgraph, scene, ob);
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
}

/** Bounding box from evaluated geometry. */
static void object_sync_boundbox_to_original(Object *object_orig, Object *object_eval)
{
  const BoundBox *bb = object_eval->runtime.bb;
  if (!bb || (bb->flag & BOUNDBOX_DIRTY)) {
    BKE_object_boundbox_calc_from_evaluated_geometry(object_eval);
  }

  bb = BKE_object_boundbox_get(object_eval);
  if (bb != NULL) {
    if (object_orig->runtime.bb == NULL) {
      object_orig->runtime.bb = MEM_mallocN(sizeof(*object_orig->runtime.bb), __func__);
    }
    *object_orig->runtime.bb = *bb;
  }
}

void BKE_object_sync_to_original(Depsgraph *depsgraph, Object *object)
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

  object_sync_boundbox_to_original(object_orig, object);
}

void BKE_object_eval_uber_transform(Depsgraph *UNUSED(depsgraph), Object *UNUSED(object))
{
}

void BKE_object_data_batch_cache_dirty_tag(ID *object_data)
{
  switch (GS(object_data->name)) {
    case ID_ME:
      BKE_mesh_batch_cache_dirty_tag((struct Mesh *)object_data, BKE_MESH_BATCH_DIRTY_ALL);
      break;
    case ID_LT:
      BKE_lattice_batch_cache_dirty_tag((struct Lattice *)object_data,
                                        BKE_LATTICE_BATCH_DIRTY_ALL);
      break;
    case ID_CU_LEGACY:
      BKE_curve_batch_cache_dirty_tag((struct Curve *)object_data, BKE_CURVE_BATCH_DIRTY_ALL);
      break;
    case ID_MB:
      BKE_mball_batch_cache_dirty_tag((struct MetaBall *)object_data, BKE_MBALL_BATCH_DIRTY_ALL);
      break;
    case ID_GD:
      BKE_gpencil_batch_cache_dirty_tag((struct bGPdata *)object_data);
      break;
    case ID_CV:
      BKE_curves_batch_cache_dirty_tag((struct Curves *)object_data, BKE_CURVES_BATCH_DIRTY_ALL);
      break;
    case ID_PT:
      BKE_pointcloud_batch_cache_dirty_tag((struct PointCloud *)object_data,
                                           BKE_POINTCLOUD_BATCH_DIRTY_ALL);
      break;
    case ID_VO:
      BKE_volume_batch_cache_dirty_tag((struct Volume *)object_data, BKE_VOLUME_BATCH_DIRTY_ALL);
      break;
    default:
      break;
  }
}

void BKE_object_batch_cache_dirty_tag(Object *ob)
{
  BKE_object_data_batch_cache_dirty_tag(ob->data);
}

void BKE_object_eval_uber_data(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);
  BLI_assert(ob->type != OB_ARMATURE);
  BKE_object_handle_data_update(depsgraph, scene, ob);
  BKE_object_batch_cache_dirty_tag(ob);
}

void BKE_object_write_geometry_cache(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  DEG_debug_print_eval(depsgraph, __func__, ob->id.name, ob);
  BLI_assert(ob->id.orig_id);
  Object *orig_ob = (Object *)ob->id.orig_id;
  if (orig_ob->runtime.geometry_cache && ob->runtime.geometry_set_eval) {
    BKE_geometry_cache_insert_and_continue_from(
        orig_ob->runtime.geometry_cache, scene->r.cfra, ob->runtime.geometry_set_eval);
  }
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
    case ID_CU_LEGACY:
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

void BKE_object_select_update(Depsgraph *depsgraph, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  if (object->type == OB_MESH && !object->runtime.is_data_eval_owned) {
    Mesh *mesh_input = (Mesh *)object->runtime.data_orig;
    Mesh_Runtime *mesh_runtime = &mesh_input->runtime;
    BLI_mutex_lock(mesh_runtime->eval_mutex);
    BKE_object_data_select_update(depsgraph, object->data);
    BLI_mutex_unlock(mesh_runtime->eval_mutex);
  }
  else {
    BKE_object_data_select_update(depsgraph, object->data);
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
   * assumed viewport visibility. Select-ability does not matter here. */
  if (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER) {
    if (base->flag & BASE_ENABLED_RENDER) {
      base->flag |= BASE_VISIBLE_DEPSGRAPH;
    }
    else {
      base->flag &= ~BASE_VISIBLE_DEPSGRAPH;
    }
  }

  /* Copy flags and settings from base. */
  object->base_flag = base->flag;
  if (is_from_set) {
    object->base_flag |= BASE_FROM_SET;
    object->base_flag &= ~(BASE_SELECTED | BASE_SELECTABLE);
  }
  object->base_local_view_bits = base->local_view_bits;
  object->runtime.local_collections_bits = base->local_collections_bits;

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
