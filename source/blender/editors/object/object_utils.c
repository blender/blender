/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "WM_types.h"

#include "ED_curve.h"
#include "ED_object.h" /* own include */

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Active Element Center
 * \{ */

bool ED_object_calc_active_center_for_editmode(Object *obedit,
                                               const bool select_only,
                                               float r_center[3])
{
  switch (obedit->type) {
    case OB_MESH: {
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      BMEditSelection ese;

      if (BM_select_history_active_get(em->bm, &ese)) {
        BM_editselection_center(&ese, r_center);
        return true;
      }
      break;
    }
    case OB_ARMATURE: {
      bArmature *arm = obedit->data;
      EditBone *ebo = arm->act_edbone;

      if (ebo && (!select_only || (ebo->flag & (BONE_SELECTED | BONE_ROOTSEL)))) {
        copy_v3_v3(r_center, ebo->head);
        return true;
      }

      break;
    }
    case OB_CURVES_LEGACY:
    case OB_SURF: {
      Curve *cu = obedit->data;

      if (ED_curve_active_center(cu, r_center)) {
        return true;
      }
      break;
    }
    case OB_MBALL: {
      MetaBall *mb = obedit->data;
      MetaElem *ml_act = mb->lastelem;

      if (ml_act && (!select_only || (ml_act->flag & SELECT))) {
        copy_v3_v3(r_center, &ml_act->x);
        return true;
      }
      break;
    }
    case OB_LATTICE: {
      BPoint *actbp = BKE_lattice_active_point_get(obedit->data);

      if (actbp) {
        copy_v3_v3(r_center, actbp->vec);
        return true;
      }
      break;
    }
  }

  return false;
}

bool ED_object_calc_active_center_for_posemode(Object *ob,
                                               const bool select_only,
                                               float r_center[3])
{
  bPoseChannel *pchan = BKE_pose_channel_active_if_layer_visible(ob);
  if (pchan && (!select_only || (pchan->bone->flag & BONE_SELECTED))) {
    copy_v3_v3(r_center, pchan->pose_head);
    return true;
  }
  return false;
}

bool ED_object_calc_active_center(Object *ob, const bool select_only, float r_center[3])
{
  if (ob->mode & OB_MODE_EDIT) {
    if (ED_object_calc_active_center_for_editmode(ob, select_only, r_center)) {
      mul_m4_v3(ob->object_to_world, r_center);
      return true;
    }
    return false;
  }
  if (ob->mode & OB_MODE_POSE) {
    if (ED_object_calc_active_center_for_posemode(ob, select_only, r_center)) {
      mul_m4_v3(ob->object_to_world, r_center);
      return true;
    }
    return false;
  }
  if (!select_only || (ob->base_flag & BASE_SELECTED)) {
    copy_v3_v3(r_center, ob->object_to_world[3]);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Child Skip
 *
 * Don't transform unselected children, this is done using the parent inverse matrix.
 *
 * \note The complex logic here is caused by mixed selection within a single selection chain,
 * otherwise we only need #XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM for single objects.
 *
 * \{ */

struct XFormObjectSkipChild_Container {
  GHash *obchild_in_obmode_map;
};

struct XFormObjectSkipChild {
  float obmat_orig[4][4];
  float parent_obmat_orig[4][4];
  float parent_obmat_inv_orig[4][4];
  float parent_recurse_obmat_orig[4][4];
  float parentinv_orig[4][4];
  Object *ob_parent_recurse;
  int mode;
};

struct XFormObjectSkipChild_Container *ED_object_xform_skip_child_container_create(void)
{
  struct XFormObjectSkipChild_Container *xcs = MEM_callocN(sizeof(*xcs), __func__);
  if (xcs->obchild_in_obmode_map == NULL) {
    xcs->obchild_in_obmode_map = BLI_ghash_ptr_new(__func__);
  }
  return xcs;
}

void ED_object_xform_skip_child_container_item_ensure_from_array(
    struct XFormObjectSkipChild_Container *xcs,
    const Scene *scene,
    ViewLayer *view_layer,
    Object **objects,
    uint objects_len)
{
  GSet *objects_in_transdata = BLI_gset_ptr_new_ex(__func__, objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BLI_gset_add(objects_in_transdata, ob);
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer);
  LISTBASE_FOREACH (Base *, base, object_bases) {
    Object *ob = base->object;
    if (ob->parent != NULL) {
      if (!BLI_gset_haskey(objects_in_transdata, ob)) {
        if (BLI_gset_haskey(objects_in_transdata, ob->parent)) {
          ED_object_xform_skip_child_container_item_ensure(
              xcs, ob, NULL, XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM);
        }
      }
      else {
        if (!BLI_gset_haskey(objects_in_transdata, ob->parent)) {
          Object *ob_parent_recurse = ob->parent;
          if (ob_parent_recurse != NULL) {
            while (ob_parent_recurse != NULL) {
              if (BLI_gset_haskey(objects_in_transdata, ob_parent_recurse)) {
                break;
              }
              ob_parent_recurse = ob_parent_recurse->parent;
            }

            if (ob_parent_recurse) {
              ED_object_xform_skip_child_container_item_ensure(
                  xcs, ob, ob_parent_recurse, XFORM_OB_SKIP_CHILD_PARENT_APPLY);
            }
          }
        }
      }
    }
  }

  LISTBASE_FOREACH (Base *, base, object_bases) {
    Object *ob = base->object;

    if (BLI_gset_haskey(objects_in_transdata, ob)) {
      /* pass. */
    }
    else if (ob->parent != NULL) {
      if (BLI_gset_haskey(objects_in_transdata, ob->parent)) {
        if (!BLI_gset_haskey(objects_in_transdata, ob)) {
          ED_object_xform_skip_child_container_item_ensure(
              xcs, ob, NULL, XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM);
        }
      }
    }
  }
  BLI_gset_free(objects_in_transdata, NULL);
}

void ED_object_xform_skip_child_container_destroy(struct XFormObjectSkipChild_Container *xcs)
{
  BLI_ghash_free(xcs->obchild_in_obmode_map, NULL, MEM_freeN);
  MEM_freeN(xcs);
}

void ED_object_xform_skip_child_container_item_ensure(struct XFormObjectSkipChild_Container *xcs,
                                                      Object *ob,
                                                      Object *ob_parent_recurse,
                                                      int mode)
{
  void **xf_p;
  if (!BLI_ghash_ensure_p(xcs->obchild_in_obmode_map, ob, &xf_p)) {
    struct XFormObjectSkipChild *xf = MEM_mallocN(sizeof(*xf), __func__);
    copy_m4_m4(xf->parentinv_orig, ob->parentinv);
    copy_m4_m4(xf->obmat_orig, ob->object_to_world);
    copy_m4_m4(xf->parent_obmat_orig, ob->parent->object_to_world);
    invert_m4_m4(xf->parent_obmat_inv_orig, ob->parent->object_to_world);
    if (ob_parent_recurse) {
      copy_m4_m4(xf->parent_recurse_obmat_orig, ob_parent_recurse->object_to_world);
    }
    xf->mode = mode;
    xf->ob_parent_recurse = ob_parent_recurse;
    *xf_p = xf;
  }
}

void ED_object_xform_skip_child_container_update_all(struct XFormObjectSkipChild_Container *xcs,
                                                     struct Main *bmain,
                                                     Depsgraph *depsgraph)
{
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, xcs->obchild_in_obmode_map) {
    Object *ob = BLI_ghashIterator_getKey(&gh_iter);
    struct XFormObjectSkipChild *xf = BLI_ghashIterator_getValue(&gh_iter);

    /* The following blocks below assign 'dmat'. */
    float dmat[4][4];

    if (xf->mode == XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM) {
      /* Parent is transformed, this isn't so compensate. */
      Object *ob_parent_eval = DEG_get_evaluated_object(depsgraph, ob->parent);
      mul_m4_m4m4(dmat, xf->parent_obmat_inv_orig, ob_parent_eval->object_to_world);
      invert_m4(dmat);
    }
    else if (xf->mode == XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM_INDIRECT) {
      /* Calculate parent matrix (from the root transform). */
      Object *ob_parent_recurse_eval = DEG_get_evaluated_object(depsgraph, xf->ob_parent_recurse);
      float parent_recurse_obmat_inv[4][4];
      invert_m4_m4(parent_recurse_obmat_inv, ob_parent_recurse_eval->object_to_world);
      mul_m4_m4m4(dmat, xf->parent_recurse_obmat_orig, parent_recurse_obmat_inv);
      invert_m4(dmat);
      float parent_obmat_calc[4][4];
      mul_m4_m4m4(parent_obmat_calc, dmat, xf->parent_obmat_orig);

      /* Apply to the parent inverse matrix. */
      mul_m4_m4m4(dmat, xf->parent_obmat_inv_orig, parent_obmat_calc);
      invert_m4(dmat);
    }
    else {
      BLI_assert(xf->mode == XFORM_OB_SKIP_CHILD_PARENT_APPLY);
      /* Transform this - without transform data. */
      Object *ob_parent_recurse_eval = DEG_get_evaluated_object(depsgraph, xf->ob_parent_recurse);
      float parent_recurse_obmat_inv[4][4];
      invert_m4_m4(parent_recurse_obmat_inv, ob_parent_recurse_eval->object_to_world);
      mul_m4_m4m4(dmat, xf->parent_recurse_obmat_orig, parent_recurse_obmat_inv);
      invert_m4(dmat);
      float obmat_calc[4][4];
      mul_m4_m4m4(obmat_calc, dmat, xf->obmat_orig);
      /* obmat_calc is just obmat. */

      /* Get the matrices relative to the parent. */
      float obmat_parent_relative_orig[4][4];
      float obmat_parent_relative_calc[4][4];
      float obmat_parent_relative_inv_orig[4][4];

      mul_m4_m4m4(obmat_parent_relative_orig, xf->parent_obmat_inv_orig, xf->obmat_orig);
      mul_m4_m4m4(obmat_parent_relative_calc, xf->parent_obmat_inv_orig, obmat_calc);
      invert_m4_m4(obmat_parent_relative_inv_orig, obmat_parent_relative_orig);

      /* Apply to the parent inverse matrix. */
      mul_m4_m4m4(dmat, obmat_parent_relative_calc, obmat_parent_relative_inv_orig);
    }

    mul_m4_m4m4(ob->parentinv, dmat, xf->parentinv_orig);

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Data Transform Container
 *
 * Use to implement 'Affect Only Origins' feature.
 *
 * \{ */

struct XFormObjectData_Container {
  GHash *obdata_in_obmode_map;
};

struct XFormObjectData_Extra {
  Object *ob;
  float obmat_orig[4][4];
  struct XFormObjectData *xod;
};

void ED_object_data_xform_container_item_ensure(struct XFormObjectData_Container *xds, Object *ob)
{
  if (xds->obdata_in_obmode_map == NULL) {
    xds->obdata_in_obmode_map = BLI_ghash_ptr_new(__func__);
  }

  void **xf_p;
  if (!BLI_ghash_ensure_p(xds->obdata_in_obmode_map, ob->data, &xf_p)) {
    struct XFormObjectData_Extra *xf = MEM_mallocN(sizeof(*xf), __func__);
    copy_m4_m4(xf->obmat_orig, ob->object_to_world);
    xf->ob = ob;
    /* Result may be NULL, that's OK. */
    xf->xod = ED_object_data_xform_create(ob->data);
    *xf_p = xf;
  }
}

void ED_object_data_xform_container_update_all(struct XFormObjectData_Container *xds,
                                               struct Main *bmain,
                                               Depsgraph *depsgraph)
{
  if (xds->obdata_in_obmode_map == NULL) {
    return;
  }
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, xds->obdata_in_obmode_map) {
    ID *id = BLI_ghashIterator_getKey(&gh_iter);
    struct XFormObjectData_Extra *xf = BLI_ghashIterator_getValue(&gh_iter);
    if (xf->xod == NULL) {
      continue;
    }

    Object *ob_eval = DEG_get_evaluated_object(depsgraph, xf->ob);
    float imat[4][4], dmat[4][4];
    invert_m4_m4(imat, xf->obmat_orig);
    mul_m4_m4m4(dmat, imat, ob_eval->object_to_world);
    invert_m4(dmat);

    ED_object_data_xform_by_mat4(xf->xod, dmat);
    if (xf->ob->type == OB_ARMATURE) {
      /* TODO: none of the current flags properly update armatures, needs investigation. */
      DEG_id_tag_update(id, 0);
    }
    else {
      DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    }
  }
}

/** Callback for #GHash free. */
static void trans_obdata_in_obmode_free_elem(void *xf_p)
{
  struct XFormObjectData_Extra *xf = xf_p;
  if (xf->xod) {
    ED_object_data_xform_destroy(xf->xod);
  }
  MEM_freeN(xf);
}

struct XFormObjectData_Container *ED_object_data_xform_container_create(void)
{
  struct XFormObjectData_Container *xds = MEM_callocN(sizeof(*xds), __func__);
  xds->obdata_in_obmode_map = BLI_ghash_ptr_new(__func__);
  return xds;
}

void ED_object_data_xform_container_destroy(struct XFormObjectData_Container *xds)
{
  BLI_ghash_free(xds->obdata_in_obmode_map, NULL, trans_obdata_in_obmode_free_elem);
  MEM_freeN(xds);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Object Array
 *
 * Low level object transform function, transforming objects by `matrix`.
 * Simple alternative to full transform logic.
 * \{ */

static bool object_parent_in_set(GSet *objects_set, Object *ob)
{
  for (Object *parent = ob->parent; parent; parent = parent->parent) {
    if (BLI_gset_lookup(objects_set, parent)) {
      return true;
    }
  }
  return false;
}

void ED_object_xform_array_m4(Object **objects, uint objects_len, const float matrix[4][4])
{
  /* Filter out objects that have parents in `objects_set`. */
  {
    GSet *objects_set = BLI_gset_ptr_new_ex(__func__, objects_len);
    for (uint i = 0; i < objects_len; i++) {
      BLI_gset_add(objects_set, objects[i]);
    }
    for (uint i = 0; i < objects_len;) {
      if (object_parent_in_set(objects_set, objects[i])) {
        objects[i] = objects[--objects_len];
      }
      else {
        i++;
      }
    }
    BLI_gset_free(objects_set, NULL);
  }

  /* Detect translation only matrix, prevent rotation/scale channels from being touched at all. */
  bool is_translation_only;
  {
    float test_m4_a[4][4], test_m4_b[4][4];
    unit_m4(test_m4_a);
    copy_m4_m4(test_m4_b, matrix);
    zero_v3(test_m4_b[3]);
    is_translation_only = equals_m4m4(test_m4_a, test_m4_b);
  }

  if (is_translation_only) {
    for (uint i = 0; i < objects_len; i++) {
      Object *ob = objects[i];
      add_v3_v3(ob->loc, matrix[3]);
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }
  }
  else {
    for (uint i = 0; i < objects_len; i++) {
      float m4[4][4];
      Object *ob = objects[i];
      BKE_object_to_mat4(ob, m4);
      mul_m4_m4m4(m4, matrix, m4);
      BKE_object_apply_mat4(ob, m4, true, true);
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }
  }
}

/** \} */
