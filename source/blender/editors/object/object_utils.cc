/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstdlib>
#include <cstring>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_editmesh.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_curve.hh"
#include "ED_object.hh" /* own include */

#include "WM_api.hh"

#include "MEM_guardedalloc.h"

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name Material Functions
 * \{ */

bool material_active_index_set(Object *ob, const int index)
{
  if (ob->totcol > 0) {
    const short actcol_test = std::clamp(index + 1, 1, ob->totcol);
    if (ob->actcol != actcol_test) {
      ob->actcol = actcol_test;
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, nullptr);
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Active Element Center
 * \{ */

bool calc_active_center_for_editmode(Object *obedit, const bool select_only, float r_center[3])
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
      bArmature *arm = static_cast<bArmature *>(obedit->data);
      EditBone *ebo = arm->act_edbone;

      if (ebo && (!select_only || (ebo->flag & (BONE_SELECTED | BONE_ROOTSEL)))) {
        copy_v3_v3(r_center, ebo->head);
        return true;
      }

      break;
    }
    case OB_CURVES_LEGACY:
    case OB_SURF: {
      Curve *cu = static_cast<Curve *>(obedit->data);

      if (ED_curve_active_center(cu, r_center)) {
        return true;
      }
      break;
    }
    case OB_MBALL: {
      MetaBall *mb = static_cast<MetaBall *>(obedit->data);
      MetaElem *ml_act = mb->lastelem;

      if (ml_act && (!select_only || (ml_act->flag & SELECT))) {
        copy_v3_v3(r_center, &ml_act->x);
        return true;
      }
      break;
    }
    case OB_LATTICE: {
      BPoint *actbp = BKE_lattice_active_point_get(static_cast<Lattice *>(obedit->data));

      if (actbp) {
        copy_v3_v3(r_center, actbp->vec);
        return true;
      }
      break;
    }
    case OB_GREASE_PENCIL: {
      copy_v3_v3(r_center, obedit->loc);
      mul_m4_v3(obedit->world_to_object().ptr(), r_center);
      return true;
    }
  }

  return false;
}

bool calc_active_center_for_posemode(Object *ob, const bool select_only, float r_center[3])
{
  bPoseChannel *pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);
  if (pchan && (!select_only || (pchan->flag & POSE_SELECTED))) {
    const bArmature *arm = static_cast<bArmature *>(ob->data);
    BKE_pose_channel_transform_location(arm, pchan, r_center);
    return true;
  }
  return false;
}

bool calc_active_center(Object *ob, const bool select_only, float r_center[3])
{
  if (ob->mode & OB_MODE_EDIT) {
    if (calc_active_center_for_editmode(ob, select_only, r_center)) {
      mul_m4_v3(ob->object_to_world().ptr(), r_center);
      return true;
    }
    return false;
  }
  if (ob->mode & OB_MODE_POSE) {
    if (calc_active_center_for_posemode(ob, select_only, r_center)) {
      mul_m4_v3(ob->object_to_world().ptr(), r_center);
      return true;
    }
    return false;
  }
  if (!select_only || (ob->base_flag & BASE_SELECTED)) {
    copy_v3_v3(r_center, ob->object_to_world().location());
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

struct XFormObjectSkipChild {
  float obmat_orig[4][4] = {};
  float parent_obmat_orig[4][4] = {};
  float parent_obmat_inv_orig[4][4] = {};
  float parent_recurse_obmat_orig[4][4] = {};
  float parentinv_orig[4][4] = {};
  Object *ob_parent_recurse = nullptr;
  int mode = OB_MODE_OBJECT;
};

struct XFormObjectSkipChild_Container {
  Map<Object *, std::unique_ptr<XFormObjectSkipChild>> obchild_in_obmode_map;
};

XFormObjectSkipChild_Container *xform_skip_child_container_create()
{
  XFormObjectSkipChild_Container *xcs = MEM_new<XFormObjectSkipChild_Container>(__func__);
  return xcs;
}

void xform_skip_child_container_item_ensure_from_array(XFormObjectSkipChild_Container *xcs,
                                                       const Scene *scene,
                                                       ViewLayer *view_layer,
                                                       Object **objects,
                                                       uint objects_len)
{
  Set<Object *> objects_in_transdata(Span(objects, objects_len));
  BKE_view_layer_synced_ensure(scene, view_layer);
  ListBase *object_bases = BKE_view_layer_object_bases_get(view_layer);
  LISTBASE_FOREACH (Base *, base, object_bases) {
    Object *ob = base->object;
    if (ob->parent != nullptr) {
      if (!objects_in_transdata.contains(ob)) {
        if (objects_in_transdata.contains(ob->parent)) {
          object_xform_skip_child_container_item_ensure(
              xcs, ob, nullptr, XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM);
        }
      }
      else {
        if (!objects_in_transdata.contains(ob->parent)) {
          Object *ob_parent_recurse = ob->parent;
          if (ob_parent_recurse != nullptr) {
            while (ob_parent_recurse != nullptr) {
              if (objects_in_transdata.contains(ob_parent_recurse)) {
                break;
              }
              ob_parent_recurse = ob_parent_recurse->parent;
            }

            if (ob_parent_recurse) {
              object_xform_skip_child_container_item_ensure(
                  xcs, ob, ob_parent_recurse, XFORM_OB_SKIP_CHILD_PARENT_APPLY);
            }
          }
        }
      }
    }
  }

  LISTBASE_FOREACH (Base *, base, object_bases) {
    Object *ob = base->object;

    if (objects_in_transdata.contains(ob)) {
      /* pass. */
    }
    else if (ob->parent != nullptr) {
      if (objects_in_transdata.contains(ob->parent)) {
        if (!objects_in_transdata.contains(ob)) {
          object_xform_skip_child_container_item_ensure(
              xcs, ob, nullptr, XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM);
        }
      }
    }
  }
}

void object_xform_skip_child_container_destroy(XFormObjectSkipChild_Container *xcs)
{
  MEM_delete(xcs);
}

void object_xform_skip_child_container_item_ensure(XFormObjectSkipChild_Container *xcs,
                                                   Object *ob,
                                                   Object *ob_parent_recurse,
                                                   int mode)
{
  xcs->obchild_in_obmode_map.lookup_or_add_cb(ob, [&]() {
    std::unique_ptr<XFormObjectSkipChild> xf = std::make_unique<XFormObjectSkipChild>();
    copy_m4_m4(xf->parentinv_orig, ob->parentinv);
    copy_m4_m4(xf->obmat_orig, ob->object_to_world().ptr());
    copy_m4_m4(xf->parent_obmat_orig, ob->parent->object_to_world().ptr());
    invert_m4_m4(xf->parent_obmat_inv_orig, ob->parent->object_to_world().ptr());
    if (ob_parent_recurse) {
      copy_m4_m4(xf->parent_recurse_obmat_orig, ob_parent_recurse->object_to_world().ptr());
    }
    xf->mode = mode;
    xf->ob_parent_recurse = ob_parent_recurse;
    return xf;
  });
}

void object_xform_skip_child_container_update_all(XFormObjectSkipChild_Container *xcs,
                                                  Main *bmain,
                                                  Depsgraph *depsgraph)
{
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  for (auto item : xcs->obchild_in_obmode_map.items()) {
    Object *ob = item.key;
    XFormObjectSkipChild *xf = item.value.get();

    /* The following blocks below assign 'dmat'. */
    float dmat[4][4];

    if (xf->mode == XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM) {
      /* Parent is transformed, this isn't so compensate. */
      Object *ob_parent_eval = DEG_get_evaluated(depsgraph, ob->parent);
      mul_m4_m4m4(dmat, xf->parent_obmat_inv_orig, ob_parent_eval->object_to_world().ptr());
      invert_m4(dmat);
    }
    else if (xf->mode == XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM_INDIRECT) {
      /* Calculate parent matrix (from the root transform). */
      Object *ob_parent_recurse_eval = DEG_get_evaluated(depsgraph, xf->ob_parent_recurse);
      float parent_recurse_obmat_inv[4][4];
      invert_m4_m4(parent_recurse_obmat_inv, ob_parent_recurse_eval->object_to_world().ptr());
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
      Object *ob_parent_recurse_eval = DEG_get_evaluated(depsgraph, xf->ob_parent_recurse);
      float parent_recurse_obmat_inv[4][4];
      invert_m4_m4(parent_recurse_obmat_inv, ob_parent_recurse_eval->object_to_world().ptr());
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
  GHash *obdata_in_obmode_map = nullptr;
};

struct XFormObjectData_Extra {
  Object *ob = nullptr;
  float obmat_orig[4][4] = {};
  std::unique_ptr<XFormObjectData> xod;
};

void data_xform_container_item_ensure(XFormObjectData_Container *xds, Object *ob)
{
  if (xds->obdata_in_obmode_map == nullptr) {
    xds->obdata_in_obmode_map = BLI_ghash_ptr_new(__func__);
  }

  void **xf_p;
  if (!BLI_ghash_ensure_p(xds->obdata_in_obmode_map, ob->data, &xf_p)) {
    XFormObjectData_Extra *xf = MEM_new<XFormObjectData_Extra>(__func__);
    copy_m4_m4(xf->obmat_orig, ob->object_to_world().ptr());
    xf->ob = ob;
    /* Result may be nullptr, that's OK. */
    xf->xod = data_xform_create(static_cast<ID *>(ob->data));
    *xf_p = xf;
  }
}

void data_xform_container_update_all(XFormObjectData_Container *xds,
                                     Main *bmain,
                                     Depsgraph *depsgraph)
{
  if (xds->obdata_in_obmode_map == nullptr) {
    return;
  }
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, xds->obdata_in_obmode_map) {
    ID *id = static_cast<ID *>(BLI_ghashIterator_getKey(&gh_iter));
    XFormObjectData_Extra *xf = static_cast<XFormObjectData_Extra *>(
        BLI_ghashIterator_getValue(&gh_iter));
    if (!xf->xod) {
      continue;
    }

    Object *ob_eval = DEG_get_evaluated(depsgraph, xf->ob);
    float4x4 imat, dmat;
    invert_m4_m4(imat.ptr(), xf->obmat_orig);
    mul_m4_m4m4(dmat.ptr(), imat.ptr(), ob_eval->object_to_world().ptr());
    invert_m4(dmat.ptr());

    data_xform_by_mat4(*xf->xod, dmat);
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
  XFormObjectData_Extra *xf = static_cast<XFormObjectData_Extra *>(xf_p);
  MEM_delete(xf);
}

XFormObjectData_Container *data_xform_container_create()
{
  XFormObjectData_Container *xds = MEM_new<XFormObjectData_Container>(__func__);
  xds->obdata_in_obmode_map = BLI_ghash_ptr_new(__func__);
  return xds;
}

void data_xform_container_destroy(XFormObjectData_Container *xds)
{
  BLI_ghash_free(xds->obdata_in_obmode_map, nullptr, trans_obdata_in_obmode_free_elem);
  MEM_delete(xds);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Object Array
 *
 * Low level object transform function, transforming objects by `matrix`.
 * Simple alternative to full transform logic.
 * \{ */

static bool object_parent_in_set(const Set<Object *> &objects_set, Object *ob)
{
  for (Object *parent = ob->parent; parent; parent = parent->parent) {
    if (objects_set.contains(parent)) {
      return true;
    }
  }
  return false;
}

void object_xform_array_m4(Object **objects, uint objects_len, const float matrix[4][4])
{
  /* Filter out objects that have parents in `objects_set`. */
  {
    Set<Object *> objects_set(Span(objects, objects_len));
    for (uint i = 0; i < objects_len;) {
      if (object_parent_in_set(objects_set, objects[i])) {
        objects[i] = objects[--objects_len];
      }
      else {
        i++;
      }
    }
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

}  // namespace blender::ed::object
