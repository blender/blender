/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_duplilist.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.hh"

#include "ANIM_action.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_rna.hh"

#include "ED_anim_api.hh"
#include "ED_object.hh"

#include "DEG_depsgraph_query.hh"

#include "transform.hh"
#include "transform_orientations.hh"
#include "transform_snap.hh"

/* Own include. */
#include "transform_convert.hh"

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name Object Mode Custom Data
 * \{ */

struct TransDataObject {

  /**
   * Object to object data transform table.
   * Don't add these to transform data because we may want to include child objects
   * which aren't being transformed.
   */
  object::XFormObjectData_Container *xds;

  /**
   * Transform
   * - The key is object data #Object.
   * - The value is #XFormObjectSkipChild.
   */
  object::XFormObjectSkipChild_Container *xcs;
};

static void freeTransObjectCustomData(TransInfo *t,
                                      TransDataContainer * /*tc*/,
                                      TransCustomData *custom_data)
{
  TransDataObject *tdo = static_cast<TransDataObject *>(custom_data->data);
  custom_data->data = nullptr;

  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    object::data_xform_container_destroy(tdo->xds);
  }

  if (t->options & CTX_OBMODE_XFORM_SKIP_CHILDREN) {
    object::object_xform_skip_child_container_destroy(tdo->xcs);
  }
  MEM_freeN(tdo);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Data in Object Mode
 *
 * Use to implement 'Affect Only Origins' feature.
 * We need this to be detached from transform data because,
 * unlike transforming regular objects, we need to transform the children.
 *
 * Nearly all of the logic here is in the 'object::data_xform_container_*' API.
 * \{ */

static void trans_obdata_in_obmode_update_all(TransInfo *t)
{
  TransDataObject *tdo = static_cast<TransDataObject *>(t->custom.type.data);
  if (tdo->xds == nullptr) {
    return;
  }

  Main *bmain = CTX_data_main(t->context);
  object::data_xform_container_update_all(tdo->xds, bmain, t->depsgraph);
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

static void trans_obchild_in_obmode_update_all(TransInfo *t)
{
  TransDataObject *tdo = static_cast<TransDataObject *>(t->custom.type.data);
  if (tdo->xcs == nullptr) {
    return;
  }

  Main *bmain = CTX_data_main(t->context);
  object::object_xform_skip_child_container_update_all(tdo->xcs, bmain, t->depsgraph);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Transform Creation
 *
 * Instead of transforming the selection, move the 2D/3D cursor.
 *
 * \{ */

/* *********************** Object Transform data ******************* */

/**
 * Transcribe given object into TransData for Transforming.
 */
static void ObjectToTransData(TransInfo *t, TransData *td, TransDataExtension *td_ext, Object *ob)
{
  Scene *scene = t->scene;
  bool constinv;
  bool skip_invert = false;

  if (t->mode != TFM_DUMMY && ob->rigidbody_object) {
    float rot[3][3], scale[3];
    float ctime = BKE_scene_ctime_get(scene);

    /* Only use rigid body transform if simulation is running,
     * avoids problems with initial setup of rigid bodies. */
    if (BKE_rigidbody_check_sim_running(scene->rigidbody_world, ctime)) {

      /* Save original object transform. */
      copy_v3_v3(td_ext->oloc, ob->loc);

      if (ob->rotmode > 0) {
        copy_v3_v3(td_ext->orot, ob->rot);
      }
      else if (ob->rotmode == ROT_MODE_AXISANGLE) {
        td_ext->orotAngle = ob->rotAngle;
        copy_v3_v3(td_ext->orotAxis, ob->rotAxis);
      }
      else {
        copy_qt_qt(td_ext->oquat, ob->quat);
      }
      /* Update object's loc/rot to get current rigid body transform. */
      mat4_to_loc_rot_size(ob->loc, rot, scale, ob->object_to_world().ptr());
      sub_v3_v3(ob->loc, ob->dloc);
      BKE_object_mat3_to_rot(ob, rot, false); /* `drot` is already corrected here. */
    }
  }

  /* `axismtx` has the real orientation. */
  transform_orientations_create_from_axis(td->axismtx, UNPACK3(ob->object_to_world().ptr()));
  if (t->orient_type_mask & (1 << V3D_ORIENT_GIMBAL)) {
    if (!gimbal_axis_object(ob, td_ext->axismtx_gimbal)) {
      copy_m3_m3(td_ext->axismtx_gimbal, td->axismtx);
    }
  }

  td->con = static_cast<bConstraint *>(ob->constraints.first);

  /* HACK: temporarily disable tracking and/or constraints when getting
   * object matrix, if tracking is on, or if constraints don't need
   * inverse correction to stop it from screwing up space conversion
   * matrix later. */
  constinv = constraints_list_needinv(t, &ob->constraints);

  /* Disable constraints inversion for dummy pass. */
  if (t->mode == TFM_DUMMY) {
    skip_invert = true;
  }

  /* NOTE: This is not really following copy-on-evaluation design and we should not
   * be re-evaluating the evaluated object. But as the comment above mentioned
   * this is part of a hack.
   * More proper solution would be to make a shallow copy of the object and
   * evaluate that, and access matrix of that evaluated copy of the object.
   * Might be more tricky than it sounds, if some logic later on accesses the
   * object matrix via td->ob->object_to_world().ptr(). */
  Object *object_eval = DEG_get_evaluated(t->depsgraph, ob);
  if (skip_invert == false && constinv == false) {
    object_eval->transflag |= OB_NO_CONSTRAINTS; /* #BKE_object_where_is_calc checks this. */
    /* It is possible to have transform data initialization prior to a
     * complete dependency graph evaluated. Happens, for example, when
     * changing transformation mode. */
    BKE_object_tfm_copy(object_eval, ob);
    BKE_object_where_is_calc(t->depsgraph, t->scene, object_eval);
    object_eval->transflag &= ~OB_NO_CONSTRAINTS;
  }
  else {
    BKE_object_where_is_calc(t->depsgraph, t->scene, object_eval);
  }
  /* Copy newly evaluated fields to the original object, similar to how
   * active dependency graph will do it. */
  copy_m4_m4(ob->runtime->object_to_world.ptr(), object_eval->object_to_world().ptr());
  /* Only copy negative scale flag, this is the only flag which is modified by
   * the BKE_object_where_is_calc(). The rest of the flags we need to keep,
   * otherwise we might lose dupli flags  (see #61787). */
  ob->transflag &= ~OB_NEG_SCALE;
  ob->transflag |= (object_eval->transflag & OB_NEG_SCALE);

  td->extra = ob;
  td->loc = ob->loc;
  copy_v3_v3(td->iloc, td->loc);

  if (ob->rotmode > 0) {
    td_ext->rot = ob->rot;
    td_ext->rotAxis = nullptr;
    td_ext->rotAngle = nullptr;
    td_ext->quat = nullptr;

    copy_v3_v3(td_ext->irot, ob->rot);
    copy_v3_v3(td_ext->drot, ob->drot);
  }
  else if (ob->rotmode == ROT_MODE_AXISANGLE) {
    td_ext->rot = nullptr;
    td_ext->rotAxis = ob->rotAxis;
    td_ext->rotAngle = &ob->rotAngle;
    td_ext->quat = nullptr;

    td_ext->irotAngle = ob->rotAngle;
    copy_v3_v3(td_ext->irotAxis, ob->rotAxis);
/* XXX, not implemented. */
#if 0
    td_ext->drotAngle = ob->drotAngle;
    copy_v3_v3(td_ext->drotAxis, ob->drotAxis);
#endif
  }
  else {
    td_ext->rot = nullptr;
    td_ext->rotAxis = nullptr;
    td_ext->rotAngle = nullptr;
    td_ext->quat = ob->quat;

    copy_qt_qt(td_ext->iquat, ob->quat);
    copy_qt_qt(td_ext->dquat, ob->dquat);
  }
  td_ext->rotOrder = ob->rotmode;

  td_ext->scale = ob->scale;
  copy_v3_v3(td_ext->iscale, ob->scale);
  copy_v3_v3(td_ext->dscale, ob->dscale);

  copy_v3_v3(td->center, ob->object_to_world().location());

  copy_m4_m4(td_ext->obmat, ob->object_to_world().ptr());

  /* Is there a need to set the global<->data space conversion matrices? */
  if (ob->parent || constinv) {
    float obmtx[3][3], totmat[3][3], obinv[3][3];

    /* Get the effect of parenting, and/or certain constraints.
     * NOTE: some Constraints, and also Tracking should never get this
     *       done, as it doesn't work well.
     */
    BKE_object_to_mat3(ob, obmtx);
    copy_m3_m4(totmat, ob->object_to_world().ptr());

    /* If the object scale is zero on any axis, this might result in a zero matrix.
     * In this case, the transformation would not do anything, see: #50103. */
    orthogonalize_m3_zero_axes(obmtx, 1.0f);
    orthogonalize_m3_zero_axes(totmat, 1.0f);

    /* Use safe invert even though the input matrices have had zero axes set to unit length,
     * in the unlikely case of failure (float precision for eg) this uses unit matrix fallback. */
    invert_m3_m3_safe_ortho(obinv, totmat);
    mul_m3_m3m3(td->smtx, obmtx, obinv);
    invert_m3_m3_safe_ortho(td->mtx, td->smtx);
  }
  else {
    /* No conversion to/from data-space. */
    unit_m3(td->smtx);
    unit_m3(td->mtx);
  }
}

static void trans_object_base_deps_flag_prepare(const TransInfo *t,
                                                const Scene *scene,
                                                ViewLayer *view_layer)
{
  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    return;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    base->object->id.tag &= ~ID_TAG_DOIT;
  }
}

static void tag_trans_objects_with_geometry_dep_only_fn(ID *id, eDepsObjectComponentType component)
{
  /* Here we only handle object IDs. */
  if (GS(id->name) != ID_OB) {
    return;
  }
  if (component == DEG_OB_COMP_GEOMETRY) {
    id->tag |= ID_TAG_DOIT;
  }
}

static void tag_trans_objects_dep_fn(ID *id, eDepsObjectComponentType component)
{
  /* Here we only handle object IDs. */
  if (GS(id->name) != ID_OB) {
    return;
  }
  if (!ELEM(component, DEG_OB_COMP_TRANSFORM, DEG_OB_COMP_GEOMETRY)) {
    return;
  }
  id->tag |= ID_TAG_DOIT;
}

static void flush_trans_object_base_deps_flag(const TransInfo *t, Object *object)
{
  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    return;
  }
  object->id.tag |= ID_TAG_DOIT;

  DEG_foreach_dependent_ID_component(
      t->depsgraph,
      &object->id,
      DEG_OB_COMP_TRANSFORM,
      DEG_FOREACH_COMPONENT_IGNORE_TRANSFORM_SOLVERS,

      /* When we transform parents while skipping children, we only traverse the GEOMETRY-dependent
       * components. This avoids marking children as not participating in snapping but still marks
       * objects with modifier dependencies.
       * Unfortunately, some transform-dependent objects that are not children may also be skipped,
       * such as constrained ones.
       * See #121378 for details. */
      (t->options & CTX_OBMODE_XFORM_SKIP_CHILDREN) ? tag_trans_objects_with_geometry_dep_only_fn :
                                                      tag_trans_objects_dep_fn);
}

static void trans_object_base_deps_flag_finish(const TransInfo *t,
                                               const Scene *scene,
                                               ViewLayer *view_layer)
{
  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    return;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->object->id.tag & ID_TAG_DOIT) {
      base->flag_legacy |= BA_SNAP_FIX_DEPS_FIASCO;
    }
  }
}

/**
 * Sets flags in Bases to define whether they take part in transform.
 * It deselects Bases, so we have to call the clear function always after.
 */
static void set_trans_object_base_flags(TransInfo *t)
{
  Main *bmain = CTX_data_main(t->context);
  ViewLayer *view_layer = t->view_layer;
  View3D *v3d = static_cast<View3D *>(t->view);
  Scene *scene = t->scene;
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
  /* NOTE: if Base selected and has parent selected:
   *   base->flag_legacy = BA_WAS_SEL
   */
  /* Don't do it if we're not actually going to recalculate anything. */
  if (t->mode == TFM_DUMMY) {
    return;
  }
  /* Makes sure base flags and object flags are identical. */
  BKE_scene_base_flag_to_objects(t->scene, t->view_layer);
  /* Make sure depsgraph is here. */
  DEG_graph_relations_update(depsgraph);
  /* Clear all flags we need. It will be used to detect dependencies. */
  trans_object_base_deps_flag_prepare(t, scene, view_layer);
  /* Traverse all bases and set all possible flags. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    base->flag_legacy &= ~(BA_WAS_SEL | BA_TRANSFORM_LOCKED_IN_PLACE);
    if (BASE_SELECTED_EDITABLE(v3d, base)) {
      Object *ob = base->object;
      Object *parsel = ob->parent;
      /* If parent selected, deselect. */
      while (parsel != nullptr) {
        if (parsel->base_flag & BASE_SELECTED) {
          Base *parbase = BKE_view_layer_base_find(view_layer, parsel);
          if (parbase != nullptr) { /* In rare cases this can fail. */
            if (BASE_SELECTED_EDITABLE(v3d, parbase)) {
              break;
            }
          }
        }
        parsel = parsel->parent;
      }
      if (parsel != nullptr) {
        /* Rotation around local centers are allowed to propagate. */
        if ((t->around == V3D_AROUND_LOCAL_ORIGINS) && ELEM(t->mode, TFM_ROTATION, TFM_TRACKBALL))
        {
          base->flag_legacy |= BA_TRANSFORM_CHILD;
        }
        else {
          base->flag &= ~BASE_SELECTED;
          base->flag_legacy |= BA_WAS_SEL;
        }
      }
      flush_trans_object_base_deps_flag(t, ob);
    }
  }
  /* Store temporary bits in base indicating that base is being modified
   * (directly or indirectly) by transforming objects.
   */
  trans_object_base_deps_flag_finish(t, scene, view_layer);
}

static bool mark_children(Object *ob)
{
  if (ob->flag & (SELECT | BA_TRANSFORM_CHILD)) {
    return true;
  }

  if (ob->parent) {
    if (mark_children(ob->parent)) {
      ob->flag |= BA_TRANSFORM_CHILD;
      return true;
    }
  }

  return false;
}

static int count_proportional_objects(TransInfo *t)
{
  int total = 0;
  ViewLayer *view_layer = t->view_layer;
  View3D *v3d = static_cast<View3D *>(t->view);
  Scene *scene = t->scene;
  /* Clear all flags we need. It will be used to detect dependencies. */
  trans_object_base_deps_flag_prepare(t, scene, view_layer);
  /* Rotations around local centers are allowed to propagate, so we take all objects. */
  if (!((t->around == V3D_AROUND_LOCAL_ORIGINS) && ELEM(t->mode, TFM_ROTATION, TFM_TRACKBALL))) {
    /* Mark all parents. */
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      if (BASE_SELECTED_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base)) {
        Object *parent = base->object->parent;
        /* Flag all parents. */
        while (parent != nullptr) {
          parent->flag |= BA_TRANSFORM_PARENT;
          parent = parent->parent;
        }
      }
    }
    /* Mark all children. */
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      /* All base not already selected or marked that is editable. */
      if ((base->object->flag & (BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT)) == 0 &&
          (base->flag & BASE_SELECTED) == 0 &&
          (BASE_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base)))
      {
        mark_children(base->object);
      }
    }
  }
  /* Flush changed flags to all dependencies. */
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *ob = base->object;
    /* If base is not selected, not a parent of selection or not a child of
     * selection and it is editable and selectable.
     */
    if ((ob->flag & (BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT)) == 0 &&
        (base->flag & BASE_SELECTED) == 0 &&
        (BASE_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base)))
    {
      flush_trans_object_base_deps_flag(t, ob);
      total += 1;
    }
  }
  /* Store temporary bits in base indicating that base is being modified
   * (directly or indirectly) by transforming objects.
   */
  trans_object_base_deps_flag_finish(t, scene, view_layer);
  return total;
}

static void clear_trans_object_base_flags(TransInfo *t)
{
  Scene *scene = t->scene;
  ViewLayer *view_layer = t->view_layer;

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->flag_legacy & BA_WAS_SEL) {
      object::base_select(base, object::BA_SELECT);
    }

    base->flag_legacy &= ~(BA_WAS_SEL | BA_SNAP_FIX_DEPS_FIASCO | BA_TEMP_TAG |
                           BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT |
                           BA_TRANSFORM_LOCKED_IN_PLACE);
  }
}

static void createTransObject(bContext *C, TransInfo *t)
{
  Main *bmain = CTX_data_main(C);
  TransData *td = nullptr;
  TransDataExtension *tx;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

  set_trans_object_base_flags(t);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* Count. */
  tc->data_len = CTX_DATA_COUNT(C, selected_bases);

  if (!tc->data_len) {
    /* Clear here, main transform function escapes too. */
    clear_trans_object_base_flags(t);
    return;
  }

  if (is_prop_edit) {
    tc->data_len += count_proportional_objects(t);
  }

  td = tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, "TransOb");
  tx = tc->data_ext = MEM_calloc_arrayN<TransDataExtension>(tc->data_len, "TransObExtension");

  TransDataObject *tdo = MEM_callocN<TransDataObject>(__func__);
  t->custom.type.data = tdo;
  t->custom.type.free_cb = freeTransObjectCustomData;

  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    tdo->xds = object::data_xform_container_create();
  }

  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    Object *ob = base->object;

    td->flag = TD_SELECTED;
    td->protectflag = ob->protectflag;
    tx->rotOrder = ob->rotmode;

    if (base->flag & BA_TRANSFORM_CHILD) {
      td->flag |= TD_NOCENTER;
      td->flag |= TD_NO_LOC;
    }

    /* Select linked objects, but skip them later. */
    if (!BKE_id_is_editable(bmain, &ob->id)) {
      td->flag |= TD_SKIP;
    }

    if (t->options & CTX_OBMODE_XFORM_OBDATA) {
      ID *id = static_cast<ID *>(ob->data);
      if (!id || id->lib) {
        td->flag |= TD_SKIP;
      }
      else if (BKE_object_is_in_editmode(ob)) {
        /* NOTE(@ideasman42): The object could have edit-mode data from another view-layer,
         * it's such a corner-case it can be skipped for now. */
        td->flag |= TD_SKIP;
      }
    }

    if (t->options & CTX_OBMODE_XFORM_OBDATA) {
      if ((td->flag & TD_SKIP) == 0) {
        object::data_xform_container_item_ensure(tdo->xds, ob);
      }
    }

    ObjectToTransData(t, td, tx, ob);
    td->val = nullptr;
    td++;
    tx++;
  }
  CTX_DATA_END;

  if (is_prop_edit) {
    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;
    View3D *v3d = static_cast<View3D *>(t->view);

    BKE_view_layer_synced_ensure(scene, view_layer);
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      Object *ob = base->object;

      /* If base is not selected, not a parent of selection
       * or not a child of selection and it is editable and selectable. */
      if ((ob->flag & (BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT)) == 0 &&
          (base->flag & BASE_SELECTED) == 0 && BASE_EDITABLE(v3d, base) &&
          BASE_SELECTABLE(v3d, base))
      {
        td->protectflag = ob->protectflag;
        tx->rotOrder = ob->rotmode;

        ObjectToTransData(t, td, tx, ob);
        td->val = nullptr;
        td++;
        tx++;
      }
    }
  }

  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    Set<Object *> objects_in_transdata;
    td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      if ((td->flag & TD_SKIP) == 0) {
        objects_in_transdata.add(static_cast<Object *>(td->extra));
      }
    }

    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;
    View3D *v3d = static_cast<View3D *>(t->view);

    BKE_view_layer_synced_ensure(scene, view_layer);
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      Object *ob = base->object;

      /* If base is not selected, not a parent of selection
       * or not a child of selection and it is editable and selectable. */
      if ((base->flag_legacy & BA_WAS_SEL) && (base->flag & BASE_SELECTED) == 0 &&
          BASE_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base))
      {

        Object *ob_parent = ob->parent;
        if (ob_parent != nullptr) {
          if (!objects_in_transdata.contains(ob)) {
            bool parent_in_transdata = false;
            while (ob_parent != nullptr) {
              if (objects_in_transdata.contains(ob_parent)) {
                parent_in_transdata = true;
                break;
              }
              ob_parent = ob_parent->parent;
            }
            if (parent_in_transdata) {
              object::data_xform_container_item_ensure(tdo->xds, ob);
            }
          }
        }
      }
    }
  }

  if (t->options & CTX_OBMODE_XFORM_SKIP_CHILDREN) {

    tdo->xcs = object::xform_skip_child_container_create();

#define BASE_XFORM_INDIRECT(base) \
\
  ((base->flag_legacy & BA_WAS_SEL) && (base->flag & BASE_SELECTED) == 0)

    Set<Object *> objects_in_transdata;
    GHash *objects_parent_root = BLI_ghash_ptr_new_ex(__func__, tc->data_len);
    td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      if ((td->flag & TD_SKIP) == 0) {
        objects_in_transdata.add(static_cast<Object *>(td->extra));
      }
    }

    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;

    BKE_view_layer_synced_ensure(scene, view_layer);
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      Object *ob = base->object;
      if (ob->parent != nullptr) {
        if (ob->parent && !objects_in_transdata.contains(ob->parent) &&
            !objects_in_transdata.contains(ob))
        {
          if ((base->flag_legacy & BA_WAS_SEL) && (base->flag & BASE_SELECTED) == 0) {
            Base *base_parent = BKE_view_layer_base_find(view_layer, ob->parent);
            if (base_parent && !BASE_XFORM_INDIRECT(base_parent)) {
              Object *ob_parent_recurse = ob->parent;
              if (ob_parent_recurse != nullptr) {
                while (ob_parent_recurse != nullptr) {
                  if (objects_in_transdata.contains(ob_parent_recurse)) {
                    break;
                  }
                  ob_parent_recurse = ob_parent_recurse->parent;
                }

                if (ob_parent_recurse) {
                  object::object_xform_skip_child_container_item_ensure(
                      tdo->xcs, ob, ob_parent_recurse, object::XFORM_OB_SKIP_CHILD_PARENT_APPLY);
                  BLI_ghash_insert(objects_parent_root, ob, ob_parent_recurse);
                  base->flag_legacy |= BA_TRANSFORM_LOCKED_IN_PLACE;
                  base->flag_legacy &= ~BA_SNAP_FIX_DEPS_FIASCO;
                }
              }
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      Object *ob = base->object;

      if (BASE_XFORM_INDIRECT(base) || objects_in_transdata.contains(ob)) {
        /* Pass. */
      }
      else if (ob->parent != nullptr) {
        Base *base_parent = BKE_view_layer_base_find(view_layer, ob->parent);
        if (base_parent) {
          if (BASE_XFORM_INDIRECT(base_parent) || objects_in_transdata.contains(ob->parent)) {
            object::object_xform_skip_child_container_item_ensure(
                tdo->xcs, ob, nullptr, object::XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM);
            base->flag_legacy |= BA_TRANSFORM_LOCKED_IN_PLACE;
            base->flag_legacy &= ~BA_SNAP_FIX_DEPS_FIASCO;
          }
          else {
            Object *ob_parent_recurse = static_cast<Object *>(
                BLI_ghash_lookup(objects_parent_root, ob->parent));
            if (ob_parent_recurse) {
              object::object_xform_skip_child_container_item_ensure(
                  tdo->xcs,
                  ob,
                  ob_parent_recurse,
                  object::XFORM_OB_SKIP_CHILD_PARENT_IS_XFORM_INDIRECT);
            }
          }
        }
      }
    }
    BLI_ghash_free(objects_parent_root, nullptr, nullptr);

#undef BASE_XFORM_INDIRECT
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Auto-Keyframing)
 * \{ */

/* Return if we need to update motion paths, only if they already exist,
 * and we will insert a keyframe at the end of transform. */
static bool motionpath_need_update_object(Scene *scene, Object *ob)
{
  /* XXX: there's potential here for problems with unkeyed rotations/scale,
   *      but for now (until proper data-locality for baking operations),
   *      this should be a better fix for #24451 and #37755
   */

  if (animrig::autokeyframe_cfra_can_key(scene, &ob->id)) {
    return (ob->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Data object
 * \{ */

/* Given the transform mode `tmode` return a Vector of RNA paths that were possibly modified during
 * that transformation. */
static Vector<RNAPath> get_affected_rna_paths_from_transform_mode(
    const eTfmMode tmode,
    Scene *scene,
    ViewLayer *view_layer,
    Object *ob,
    const StringRef rotation_path,
    const bool transforming_more_than_one_object)
{
  Vector<RNAPath> rna_paths;

  /* Handle the cases where we always need to key location, regardless of
   * transform mode. */
  if (scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    if (ob != BKE_view_layer_active_object_get(view_layer)) {
      rna_paths.append({"location"});
    }
  }
  else if (transforming_more_than_one_object &&
           scene->toolsettings->transform_pivot_point != V3D_AROUND_LOCAL_ORIGINS)
  {
    rna_paths.append({"location"});
  }
  else if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CURSOR) {
    rna_paths.append({"location"});
  }

  /* Handle the transform-mode-specific cases. */
  switch (tmode) {
    case TFM_TRANSLATION:
      rna_paths.append_non_duplicates({"location"});
      break;

    case TFM_ROTATION:
    case TFM_TRACKBALL:
      if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
        rna_paths.append({rotation_path});
      }
      break;

    case TFM_RESIZE:
      if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
        rna_paths.append({"scale"});
      }
      break;

    default:
      rna_paths.append_non_duplicates({"location"});
      rna_paths.append({rotation_path});
      rna_paths.append({"scale"});
  }

  return rna_paths;
}

static void autokeyframe_object(bContext *C,
                                Scene *scene,
                                Object *ob,
                                const eTfmMode tmode,
                                const bool transforming_more_than_one_object)
{
  Vector<RNAPath> rna_paths;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const StringRef rotation_path = animrig::get_rotation_mode_path(eRotationModes(ob->rotmode));

  if (animrig::is_keying_flag(scene, AUTOKEY_FLAG_INSERTNEEDED)) {
    rna_paths = get_affected_rna_paths_from_transform_mode(
        tmode, scene, view_layer, ob, rotation_path, transforming_more_than_one_object);
  }
  else {
    rna_paths = {{"location"}, {rotation_path}, {"scale"}};
  }
  animrig::autokeyframe_object(C, scene, ob, rna_paths.as_span());
}

static void recalcData_objects(TransInfo *t)
{
  bool motionpath_update = false;

  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;

    for (int i = 0; i < tc->data_len; i++, td++) {
      Object *ob = static_cast<Object *>(td->extra);
      if (td->flag & TD_SKIP) {
        continue;
      }

      /* If animtimer is running, and the object already has animation data,
       * check if the auto-record feature means that we should record 'samples'
       * (i.e. uneditable animation values). */

      /* TODO: auto-keyframe calls need some setting to specify to add samples
       * (FPoints) instead of keyframes? */
      if ((t->animtimer) && animrig::is_autokey_on(t->scene)) {
        animrecord_check_state(t, &ob->id);
        autokeyframe_object(t->context, t->scene, ob, t->mode, t->data_len_all > 1);
      }

      motionpath_update |= motionpath_need_update_object(t->scene, ob);

      /* Sets recalc flags fully, instead of flushing existing ones
       * otherwise proxies don't function correctly. */
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }
  }

  if (motionpath_update) {
    /* Update motion paths once for all transformed objects. */
    object::motion_paths_recalc_selected(
        t->context, t->scene, object::OBJECT_PATH_CALC_RANGE_CURRENT_FRAME);
  }

  if (t->options & CTX_OBMODE_XFORM_SKIP_CHILDREN) {
    trans_obchild_in_obmode_update_all(t);
  }

  if (t->options & CTX_OBMODE_XFORM_OBDATA) {
    trans_obdata_in_obmode_update_all(t);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Object
 * \{ */

static void special_aftertrans_update__object(bContext *C, TransInfo *t)
{
  BLI_assert(t->options & CTX_OBJECT);

  Object *ob;
  const bool canceled = (t->state == TRANS_CANCEL);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  bool motionpath_update = false;

  if (animrig::is_autokey_on(t->scene) && !canceled) {
    ANIM_deselect_keys_in_animation_editors(C);
  }

  for (int i = 0; i < tc->data_len; i++) {
    TransData *td = tc->data + i;
    TransDataExtension *td_ext = tc->data_ext + i;
    ListBase pidlist;
    ob = static_cast<Object *>(td->extra);

    if (td->flag & TD_SKIP) {
      continue;
    }

    /* Flag object caches as outdated. */
    BKE_ptcache_ids_from_object(&pidlist, ob, t->scene, MAX_DUPLI_RECUR);
    LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
      if (pid->type != PTCACHE_TYPE_PARTICLES) {
        /* Particles don't need reset on geometry change. */
        pid->cache->flag |= PTCACHE_OUTDATED;
      }
    }
    BLI_freelistN(&pidlist);

    /* Point-cache refresh. */
    if (BKE_ptcache_object_reset(t->scene, ob, PTCACHE_RESET_OUTDATED)) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }

    /* Needed for proper updating of "quick cached" dynamics.
     * Creates troubles for moving animated objects without
     * auto-key though, probably needed is an animation-system override?
     * NOTE(@jahka): Please remove if some other solution is found. */
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

    /* Set auto-key if necessary. */
    if (!canceled) {
      autokeyframe_object(C, t->scene, ob, t->mode, tc->data_len > 1);
    }

    motionpath_update |= motionpath_need_update_object(t->scene, ob);

    /* Restore rigid body transform. */
    if (ob->rigidbody_object && canceled) {
      float ctime = BKE_scene_ctime_get(t->scene);
      if (BKE_rigidbody_check_sim_running(t->scene->rigidbody_world, ctime)) {
        BKE_rigidbody_aftertrans_update(
            ob, td_ext->oloc, td_ext->orot, td_ext->oquat, td_ext->orotAxis, td_ext->orotAngle);
      }
    }
  }

  if (motionpath_update) {
    /* Update motion paths once for all transformed objects. */
    const object::eObjectPathCalcRange range = canceled ?
                                                   object::OBJECT_PATH_CALC_RANGE_CURRENT_FRAME :
                                                   object::OBJECT_PATH_CALC_RANGE_CHANGED;
    object::motion_paths_recalc_selected(C, t->scene, range);
  }

  clear_trans_object_base_flags(t);
}

/** \} */

TransConvertTypeInfo TransConvertType_Object = {
    /*flags*/ 0,
    /*create_trans_data*/ createTransObject,
    /*recalc_data*/ recalcData_objects,
    /*special_aftertrans_update*/ special_aftertrans_update__object,
};

}  // namespace blender::ed::transform
