/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_curves.hh"
#include "BKE_editmesh.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_idtype.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_tracking.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ANIM_keyframing.hh"

#include "ED_armature.hh"
#include "ED_keyframing.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "MEM_guardedalloc.h"

#include "object_intern.h"

using blender::Array;
using blender::float2;
using blender::float3;
using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Clear Transformation Utilities
 * \{ */

/* clear location of object */
static void object_clear_loc(Object *ob, const bool clear_delta)
{
  /* clear location if not locked */
  if ((ob->protectflag & OB_LOCK_LOCX) == 0) {
    ob->loc[0] = 0.0f;
    if (clear_delta) {
      ob->dloc[0] = 0.0f;
    }
  }
  if ((ob->protectflag & OB_LOCK_LOCY) == 0) {
    ob->loc[1] = 0.0f;
    if (clear_delta) {
      ob->dloc[1] = 0.0f;
    }
  }
  if ((ob->protectflag & OB_LOCK_LOCZ) == 0) {
    ob->loc[2] = 0.0f;
    if (clear_delta) {
      ob->dloc[2] = 0.0f;
    }
  }
}

/* clear rotation of object */
static void object_clear_rot(Object *ob, const bool clear_delta)
{
  /* clear rotations that aren't locked */
  if (ob->protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) {
    if (ob->protectflag & OB_LOCK_ROT4D) {
      /* perform clamping on a component by component basis */
      if (ob->rotmode == ROT_MODE_AXISANGLE) {
        if ((ob->protectflag & OB_LOCK_ROTW) == 0) {
          ob->rotAngle = 0.0f;
          if (clear_delta) {
            ob->drotAngle = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTX) == 0) {
          ob->rotAxis[0] = 0.0f;
          if (clear_delta) {
            ob->drotAxis[0] = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTY) == 0) {
          ob->rotAxis[1] = 0.0f;
          if (clear_delta) {
            ob->drotAxis[1] = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTZ) == 0) {
          ob->rotAxis[2] = 0.0f;
          if (clear_delta) {
            ob->drotAxis[2] = 0.0f;
          }
        }

        /* Check validity of axis - axis should never be 0,0,0
         * (if so, then we make it rotate about y). */
        if (IS_EQF(ob->rotAxis[0], ob->rotAxis[1]) && IS_EQF(ob->rotAxis[1], ob->rotAxis[2])) {
          ob->rotAxis[1] = 1.0f;
        }
        if (IS_EQF(ob->drotAxis[0], ob->drotAxis[1]) && IS_EQF(ob->drotAxis[1], ob->drotAxis[2]) &&
            clear_delta)
        {
          ob->drotAxis[1] = 1.0f;
        }
      }
      else if (ob->rotmode == ROT_MODE_QUAT) {
        if ((ob->protectflag & OB_LOCK_ROTW) == 0) {
          ob->quat[0] = 1.0f;
          if (clear_delta) {
            ob->dquat[0] = 1.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTX) == 0) {
          ob->quat[1] = 0.0f;
          if (clear_delta) {
            ob->dquat[1] = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTY) == 0) {
          ob->quat[2] = 0.0f;
          if (clear_delta) {
            ob->dquat[2] = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTZ) == 0) {
          ob->quat[3] = 0.0f;
          if (clear_delta) {
            ob->dquat[3] = 0.0f;
          }
        }
        /* TODO: does this quat need normalizing now? */
      }
      else {
        /* the flag may have been set for the other modes, so just ignore the extra flag... */
        if ((ob->protectflag & OB_LOCK_ROTX) == 0) {
          ob->rot[0] = 0.0f;
          if (clear_delta) {
            ob->drot[0] = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTY) == 0) {
          ob->rot[1] = 0.0f;
          if (clear_delta) {
            ob->drot[1] = 0.0f;
          }
        }
        if ((ob->protectflag & OB_LOCK_ROTZ) == 0) {
          ob->rot[2] = 0.0f;
          if (clear_delta) {
            ob->drot[2] = 0.0f;
          }
        }
      }
    }
    else {
      /* perform clamping using euler form (3-components) */
      /* FIXME: deltas are not handled for these cases yet... */
      float eul[3], oldeul[3], quat1[4] = {0};

      if (ob->rotmode == ROT_MODE_QUAT) {
        copy_qt_qt(quat1, ob->quat);
        quat_to_eul(oldeul, ob->quat);
      }
      else if (ob->rotmode == ROT_MODE_AXISANGLE) {
        axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, ob->rotAxis, ob->rotAngle);
      }
      else {
        copy_v3_v3(oldeul, ob->rot);
      }

      eul[0] = eul[1] = eul[2] = 0.0f;

      if (ob->protectflag & OB_LOCK_ROTX) {
        eul[0] = oldeul[0];
      }
      if (ob->protectflag & OB_LOCK_ROTY) {
        eul[1] = oldeul[1];
      }
      if (ob->protectflag & OB_LOCK_ROTZ) {
        eul[2] = oldeul[2];
      }

      if (ob->rotmode == ROT_MODE_QUAT) {
        eul_to_quat(ob->quat, eul);
        /* quaternions flip w sign to accumulate rotations correctly */
        if ((quat1[0] < 0.0f && ob->quat[0] > 0.0f) || (quat1[0] > 0.0f && ob->quat[0] < 0.0f)) {
          mul_qt_fl(ob->quat, -1.0f);
        }
      }
      else if (ob->rotmode == ROT_MODE_AXISANGLE) {
        eulO_to_axis_angle(ob->rotAxis, &ob->rotAngle, eul, EULER_ORDER_DEFAULT);
      }
      else {
        copy_v3_v3(ob->rot, eul);
      }
    }
  } /* Duplicated in source/blender/editors/armature/editarmature.c */
  else {
    if (ob->rotmode == ROT_MODE_QUAT) {
      unit_qt(ob->quat);
      if (clear_delta) {
        unit_qt(ob->dquat);
      }
    }
    else if (ob->rotmode == ROT_MODE_AXISANGLE) {
      unit_axis_angle(ob->rotAxis, &ob->rotAngle);
      if (clear_delta) {
        unit_axis_angle(ob->drotAxis, &ob->drotAngle);
      }
    }
    else {
      zero_v3(ob->rot);
      if (clear_delta) {
        zero_v3(ob->drot);
      }
    }
  }
}

/* clear scale of object */
static void object_clear_scale(Object *ob, const bool clear_delta)
{
  /* clear scale factors which are not locked */
  if ((ob->protectflag & OB_LOCK_SCALEX) == 0) {
    ob->scale[0] = 1.0f;
    if (clear_delta) {
      ob->dscale[0] = 1.0f;
    }
  }
  if ((ob->protectflag & OB_LOCK_SCALEY) == 0) {
    ob->scale[1] = 1.0f;
    if (clear_delta) {
      ob->dscale[1] = 1.0f;
    }
  }
  if ((ob->protectflag & OB_LOCK_SCALEZ) == 0) {
    ob->scale[2] = 1.0f;
    if (clear_delta) {
      ob->dscale[2] = 1.0f;
    }
  }
}

/* generic exec for clear-transform operators */
static int object_clear_transform_generic_exec(bContext *C,
                                               wmOperator *op,
                                               void (*clear_func)(Object *, const bool),
                                               const char default_ksName[])
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  /* May be null. */
  View3D *v3d = CTX_wm_view3d(C);
  KeyingSet *ks;
  const bool clear_delta = RNA_boolean_get(op->ptr, "clear_delta");

  BLI_assert(!ELEM(nullptr, clear_func, default_ksName));

  Vector<Object *> objects;
  FOREACH_SELECTED_EDITABLE_OBJECT_BEGIN (view_layer, v3d, ob) {
    objects.append(ob);
  }
  FOREACH_SELECTED_EDITABLE_OBJECT_END;

  if (objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Support transforming the object data. */
  const bool use_transform_skip_children = (scene->toolsettings->transform_flag &
                                            SCE_XFORM_SKIP_CHILDREN);
  const bool use_transform_data_origin = (scene->toolsettings->transform_flag &
                                          SCE_XFORM_DATA_ORIGIN);
  XFormObjectSkipChild_Container *xcs = nullptr;
  XFormObjectData_Container *xds = nullptr;

  if (use_transform_skip_children) {
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
    xcs = ED_object_xform_skip_child_container_create();
    ED_object_xform_skip_child_container_item_ensure_from_array(
        xcs, scene, view_layer, objects.data(), objects.size());
  }
  if (use_transform_data_origin) {
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
    xds = ED_object_data_xform_container_create();
  }

  /* get KeyingSet to use */
  ks = ANIM_get_keyingset_for_autokeying(scene, default_ksName);

  for (Object *ob : objects) {
    if (use_transform_data_origin) {
      ED_object_data_xform_container_item_ensure(xds, ob);
    }

    /* run provided clearing function */
    clear_func(ob, clear_delta);

    blender::animrig::autokeyframe_object(C, scene, ob, ks);

    /* tag for updates */
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }

  if (use_transform_skip_children) {
    ED_object_xform_skip_child_container_update_all(xcs, bmain, depsgraph);
    ED_object_xform_skip_child_container_destroy(xcs);
  }

  if (use_transform_data_origin) {
    ED_object_data_xform_container_update_all(xds, bmain, depsgraph);
    ED_object_data_xform_container_destroy(xds);
  }

  /* this is needed so children are also updated */
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Location Operator
 * \{ */

static int object_location_clear_exec(bContext *C, wmOperator *op)
{
  return object_clear_transform_generic_exec(C, op, object_clear_loc, ANIM_KS_LOCATION_ID);
}

void OBJECT_OT_location_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Location";
  ot->description = "Clear the object's location";
  ot->idname = "OBJECT_OT_location_clear";

  /* api callbacks */
  ot->exec = object_location_clear_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(
      ot->srna,
      "clear_delta",
      false,
      "Clear Delta",
      "Clear delta location in addition to clearing the normal location transform");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Rotation Operator
 * \{ */

static int object_rotation_clear_exec(bContext *C, wmOperator *op)
{
  return object_clear_transform_generic_exec(C, op, object_clear_rot, ANIM_KS_ROTATION_ID);
}

void OBJECT_OT_rotation_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Rotation";
  ot->description = "Clear the object's rotation";
  ot->idname = "OBJECT_OT_rotation_clear";

  /* api callbacks */
  ot->exec = object_rotation_clear_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(
      ot->srna,
      "clear_delta",
      false,
      "Clear Delta",
      "Clear delta rotation in addition to clearing the normal rotation transform");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Scale Operator
 * \{ */

static int object_scale_clear_exec(bContext *C, wmOperator *op)
{
  return object_clear_transform_generic_exec(C, op, object_clear_scale, ANIM_KS_SCALING_ID);
}

void OBJECT_OT_scale_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Scale";
  ot->description = "Clear the object's scale";
  ot->idname = "OBJECT_OT_scale_clear";

  /* api callbacks */
  ot->exec = object_scale_clear_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(
      ot->srna,
      "clear_delta",
      false,
      "Clear Delta",
      "Clear delta scale in addition to clearing the normal scale transform");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Origin Operator
 * \{ */

static int object_origin_clear_exec(bContext *C, wmOperator * /*op*/)
{
  float *v1, *v3;
  float mat[3][3];

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob->parent) {
      /* vectors pointed to by v1 and v3 will get modified */
      v1 = ob->loc;
      v3 = ob->parentinv[3];

      copy_m3_m4(mat, ob->parentinv);
      negate_v3_v3(v3, v1);
      mul_m3_v3(mat, v3);
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
  CTX_DATA_END;

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Origin";
  ot->description = "Clear the object's origin";
  ot->idname = "OBJECT_OT_origin_clear";

  /* api callbacks */
  ot->exec = object_origin_clear_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Transformation Operator
 * \{ */

/* use this when the loc/size/rot of the parent has changed but the children
 * should stay in the same place, e.g. for apply-size-rot or object center */
static void ignore_parent_tx(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  Object workob;

  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);

  /* a change was made, adjust the children to compensate */
  LISTBASE_FOREACH (Object *, ob_child, &bmain->objects) {
    if (ob_child->parent == ob) {
      Object *ob_child_eval = DEG_get_evaluated_object(depsgraph, ob_child);
      BKE_object_apply_mat4(ob_child_eval, ob_child_eval->object_to_world().ptr(), true, false);
      BKE_object_workob_calc_parent(depsgraph, scene, ob_child_eval, &workob);
      invert_m4_m4(ob_child->parentinv, workob.object_to_world().ptr());
      /* Copy result of BKE_object_apply_mat4(). */
      BKE_object_transform_copy(ob_child, ob_child_eval);
      /* Make sure evaluated object is in a consistent state with the original one.
       * It might be needed for applying transform on its children. */
      copy_m4_m4(ob_child_eval->parentinv, ob_child->parentinv);
      BKE_object_eval_transform_all(depsgraph, scene_eval, ob_child_eval);
      /* Tag for update.
       * This is because parent matrix did change, so in theory the child object might now be
       * evaluated to a different location in another editing context. */
      DEG_id_tag_update(&ob_child->id, ID_RECALC_TRANSFORM);
    }
  }
}

static void append_sorted_object_parent_hierarchy(Object *root_object,
                                                  Object *object,
                                                  Object **sorted_objects,
                                                  int *object_index)
{
  if (!ELEM(object->parent, nullptr, root_object)) {
    append_sorted_object_parent_hierarchy(
        root_object, object->parent, sorted_objects, object_index);
  }
  if (object->id.tag & LIB_TAG_DOIT) {
    sorted_objects[*object_index] = object;
    (*object_index)++;
    object->id.tag &= ~LIB_TAG_DOIT;
  }
}

static Array<Object *> sorted_selected_editable_objects(bContext *C)
{
  Main *bmain = CTX_data_main(C);

  /* Count all objects, but also tag all the selected ones. */
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  int num_objects = 0;
  CTX_DATA_BEGIN (C, Object *, object, selected_editable_objects) {
    object->id.tag |= LIB_TAG_DOIT;
    num_objects++;
  }
  CTX_DATA_END;
  if (num_objects == 0) {
    return {};
  }

  /* Append all the objects. */
  Array<Object *> sorted_objects(num_objects);
  int object_index = 0;
  CTX_DATA_BEGIN (C, Object *, object, selected_editable_objects) {
    if ((object->id.tag & LIB_TAG_DOIT) == 0) {
      continue;
    }
    append_sorted_object_parent_hierarchy(object, object, sorted_objects.data(), &object_index);
  }
  CTX_DATA_END;

  return sorted_objects;
}

/**
 * Check if we need and can handle the special multiuser case.
 */
static bool apply_objects_internal_can_multiuser(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  if (ELEM(nullptr, obact, obact->data)) {
    return false;
  }

  if (ID_REAL_USERS(obact->data) == 1) {
    return false;
  }

  bool all_objects_same_data = true;
  bool obact_selected = false;

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob->data != obact->data) {
      all_objects_same_data = false;
      break;
    }

    if (ob == obact) {
      obact_selected = true;
    }
  }
  CTX_DATA_END;

  return all_objects_same_data && obact_selected;
}

/**
 * Check if the current selection need to be made into single user.
 *
 * It assumes that all selected objects share the same object data.
 */
static bool apply_objects_internal_need_single_user(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  BLI_assert(apply_objects_internal_can_multiuser(C));

  /* Counting the number of objects is valid since it's known the
   * selection is only made up of users of the active objects data. */
  return (ID_REAL_USERS(ob->data) > CTX_DATA_COUNT(C, selected_editable_objects));
}

static void transform_positions(blender::MutableSpan<blender::float3> positions,
                                const blender::float4x4 &matrix)
{
  using namespace blender;
  threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position = math::transform_point(matrix, position);
    }
  });
}

static int apply_objects_internal(bContext *C,
                                  ReportList *reports,
                                  bool apply_loc,
                                  bool apply_rot,
                                  bool apply_scale,
                                  bool do_props,
                                  bool do_single_user)
{
  using namespace blender;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  float rsmat[3][3], obmat[3][3], iobmat[3][3], mat[4][4], scale;
  bool changed = true;
  bool const do_multi_user = apply_objects_internal_can_multiuser(C);
  float obact_invmat[4][4], obact_parent[4][4], obact_parentinv[4][4];

  /* Only used when do_multi_user is set. */
  Object *obact = nullptr;
  bool make_single_user = false;

  if (do_multi_user) {
    obact = CTX_data_active_object(C);
    invert_m4_m4(obact_invmat, obact->object_to_world().ptr());

    Object workob;
    BKE_object_workob_calc_parent(depsgraph, scene, obact, &workob);
    copy_m4_m4(obact_parent, workob.object_to_world().ptr());
    copy_m4_m4(obact_parentinv, obact->parentinv);

    if (apply_objects_internal_need_single_user(C)) {
      if (do_single_user) {
        make_single_user = true;
      }
      else {
        ID *obact_data = static_cast<ID *>(obact->data);
        BKE_reportf(reports,
                    RPT_ERROR,
                    R"(Cannot apply to a multi user: Object "%s", %s "%s", aborting)",
                    obact->id.name + 2,
                    BKE_idtype_idcode_to_name(GS(obact_data->name)),
                    obact_data->name + 2);
        return OPERATOR_CANCELLED;
      }
    }
  }

  /* first check if we can execute */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ELEM(ob->type,
             OB_MESH,
             OB_ARMATURE,
             OB_LATTICE,
             OB_MBALL,
             OB_CURVES_LEGACY,
             OB_SURF,
             OB_FONT,
             OB_GPENCIL_LEGACY,
             OB_CURVES,
             OB_POINTCLOUD))
    {
      ID *obdata = static_cast<ID *>(ob->data);
      if (!do_multi_user && ID_REAL_USERS(obdata) > 1) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    R"(Cannot apply to a multi user: Object "%s", %s "%s", aborting)",
                    ob->id.name + 2,
                    BKE_idtype_idcode_to_name(GS(obdata->name)),
                    obdata->name + 2);
        changed = false;
      }

      if (ID_IS_LINKED(obdata) || ID_IS_OVERRIDE_LIBRARY(obdata)) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    R"(Cannot apply to library or override data: Object "%s", %s "%s", aborting)",
                    ob->id.name + 2,
                    BKE_idtype_idcode_to_name(GS(obdata->name)),
                    obdata->name + 2);
        changed = false;
      }
    }

    if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      ID *obdata = static_cast<ID *>(ob->data);
      Curve *cu = static_cast<Curve *>(ob->data);

      if (((ob->type == OB_CURVES_LEGACY) && !(cu->flag & CU_3D)) && (apply_rot || apply_loc)) {
        BKE_reportf(
            reports,
            RPT_ERROR,
            R"(Rotation/Location can't apply to a 2D curve: Object "%s", %s "%s", aborting)",
            ob->id.name + 2,
            BKE_idtype_idcode_to_name(GS(obdata->name)),
            obdata->name + 2);
        changed = false;
      }
      if (cu->key) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    R"(Can't apply to a curve with shape-keys: Object "%s", %s "%s", aborting)",
                    ob->id.name + 2,
                    BKE_idtype_idcode_to_name(GS(obdata->name)),
                    obdata->name + 2);
        changed = false;
      }
    }

    if (ob->type == OB_FONT) {
      if (apply_rot || apply_loc) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Text objects can only have their scale applied: \"%s\"",
                    ob->id.name + 2);
        changed = false;
      }
    }

    if (ob->type == OB_GPENCIL_LEGACY) {
      bGPdata *gpd = static_cast<bGPdata *>(ob->data);
      if (gpd) {
        if (gpd->layers.first) {
          /* Unsupported configuration */
          bool has_unparented_layers = false;

          LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
            /* Parented layers aren't supported as we can't easily re-evaluate
             * the scene to sample parent movement */
            if (gpl->parent == nullptr) {
              has_unparented_layers = true;
              break;
            }
          }

          if (has_unparented_layers == false) {
            BKE_reportf(reports,
                        RPT_ERROR,
                        "Can't apply to a GP data-block where all layers are parented: Object "
                        "\"%s\", %s \"%s\", aborting",
                        ob->id.name + 2,
                        BKE_idtype_idcode_to_name(ID_GD_LEGACY),
                        gpd->id.name + 2);
            changed = false;
          }
        }
        else {
          /* No layers/data */
          BKE_reportf(
              reports,
              RPT_ERROR,
              R"(Can't apply to GP data-block with no layers: Object "%s", %s "%s", aborting)",
              ob->id.name + 2,
              BKE_idtype_idcode_to_name(ID_GD_LEGACY),
              gpd->id.name + 2);
        }
      }
    }

    if (ob->type == OB_LAMP) {
      Light *la = static_cast<Light *>(ob->data);
      if (la->type == LA_AREA) {
        if (apply_rot || apply_loc) {
          BKE_reportf(reports,
                      RPT_ERROR,
                      "Area Lights can only have scale applied: \"%s\"",
                      ob->id.name + 2);
          changed = false;
        }
      }
    }
  }
  CTX_DATA_END;

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  changed = false;

  /* now execute */

  if (make_single_user) {
    /* Make single user. */
    ED_object_single_obdata_user(bmain, scene, obact);
    BKE_main_id_newptr_and_tag_clear(bmain);
    WM_event_add_notifier(C, NC_WINDOW, nullptr);
    DEG_relations_tag_update(bmain);
  }

  Array<Object *> objects = sorted_selected_editable_objects(C);
  if (objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  for (Object *ob : objects) {
    /* calculate rotation/scale matrix */
    if (apply_scale && apply_rot) {
      BKE_object_to_mat3(ob, rsmat);
    }
    else if (apply_scale) {
      BKE_object_scale_to_mat3(ob, rsmat);
    }
    else if (apply_rot) {
      float tmat[3][3], timat[3][3];

      /* simple rotation matrix */
      BKE_object_rot_to_mat3(ob, rsmat, true);

      /* correct for scale, note mul_m3_m3m3 has swapped args! */
      BKE_object_scale_to_mat3(ob, tmat);
      invert_m3_m3(timat, tmat);
      mul_m3_m3m3(rsmat, timat, rsmat);
      mul_m3_m3m3(rsmat, rsmat, tmat);
    }
    else {
      unit_m3(rsmat);
    }

    copy_m4_m3(mat, rsmat);

    /* calculate translation */
    if (apply_loc) {
      add_v3_v3v3(mat[3], ob->loc, ob->dloc);

      if (!(apply_scale && apply_rot)) {
        float tmat[3][3];
        /* correct for scale and rotation that is still applied */
        BKE_object_to_mat3(ob, obmat);
        invert_m3_m3(iobmat, obmat);
        mul_m3_m3m3(tmat, rsmat, iobmat);
        mul_m3_v3(tmat, mat[3]);
      }
    }

    /* apply to object data */
    if (do_multi_user && ob != obact) {
      /* Don't apply, just set the new object data, the correct
       * transformations will happen later. */
      id_us_min((ID *)ob->data);
      ob->data = obact->data;
      id_us_plus((ID *)ob->data);
    }
    else if (ob->type == OB_MESH) {
      Mesh *mesh = static_cast<Mesh *>(ob->data);

      if (apply_scale) {
        multiresModifier_scale_disp(depsgraph, scene, ob);
      }

      /* adjust data */
      BKE_mesh_transform(mesh, mat, true);
    }
    else if (ob->type == OB_ARMATURE) {
      bArmature *arm = static_cast<bArmature *>(ob->data);
      BKE_armature_transform(arm, mat, do_props);
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = static_cast<Lattice *>(ob->data);

      BKE_lattice_transform(lt, mat, true);
    }
    else if (ob->type == OB_MBALL) {
      MetaBall *mb = static_cast<MetaBall *>(ob->data);
      BKE_mball_transform(mb, mat, do_props);
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      Curve *cu = static_cast<Curve *>(ob->data);
      scale = mat3_to_scale(rsmat);
      BKE_curve_transform_ex(cu, mat, true, do_props, scale);
    }
    else if (ob->type == OB_FONT) {
      Curve *cu = static_cast<Curve *>(ob->data);

      scale = mat3_to_scale(rsmat);

      for (int i = 0; i < cu->totbox; i++) {
        TextBox *tb = &cu->tb[i];
        tb->x *= scale;
        tb->y *= scale;
        tb->w *= scale;
        tb->h *= scale;
      }

      if (do_props) {
        cu->fsize *= scale;
      }
    }
    else if (ob->type == OB_GPENCIL_LEGACY) {
      bGPdata *gpd = static_cast<bGPdata *>(ob->data);
      BKE_gpencil_transform(gpd, mat);
    }
    else if (ob->type == OB_CURVES) {
      Curves &curves = *static_cast<Curves *>(ob->data);
      curves.geometry.wrap().transform(float4x4(mat));
      curves.geometry.wrap().calculate_bezier_auto_handles();
    }
    else if (ob->type == OB_POINTCLOUD) {
      PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
      transform_positions(pointcloud.positions_for_write(), float4x4(mat));
      pointcloud.tag_positions_changed();
    }
    else if (ob->type == OB_CAMERA) {
      MovieClip *clip = BKE_object_movieclip_get(scene, ob, false);

      /* applying scale on camera actually scales clip's reconstruction.
       * of there's clip assigned to camera nothing to do actually.
       */
      if (!clip) {
        continue;
      }

      if (apply_scale) {
        BKE_tracking_reconstruction_scale(&clip->tracking, ob->scale);
      }
    }
    else if (ob->type == OB_EMPTY) {
      /* It's possible for empties too, even though they don't
       * really have obdata, since we can simply apply the maximum
       * scaling to the empty's drawsize.
       *
       * Core Assumptions:
       * 1) Most scaled empties have uniform scaling
       *    (i.e. for visibility reasons), AND/OR
       * 2) Preserving non-uniform scaling is not that important,
       *    and is something that many users would be willing to
       *    sacrifice for having an easy way to do this.
       */

      if ((apply_loc == false) && (apply_rot == false) && (apply_scale == true)) {
        float max_scale = max_fff(fabsf(ob->scale[0]), fabsf(ob->scale[1]), fabsf(ob->scale[2]));
        ob->empty_drawsize *= max_scale;
      }
    }
    else if (ob->type == OB_LAMP) {
      Light *la = static_cast<Light *>(ob->data);
      if (la->type != LA_AREA) {
        continue;
      }

      bool keeps_aspect_ratio = compare_ff_relative(rsmat[0][0], rsmat[1][1], FLT_EPSILON, 64);
      if ((la->area_shape == LA_AREA_SQUARE) && !keeps_aspect_ratio) {
        la->area_shape = LA_AREA_RECT;
        la->area_sizey = la->area_size;
      }
      else if ((la->area_shape == LA_AREA_DISK) && !keeps_aspect_ratio) {
        la->area_shape = LA_AREA_ELLIPSE;
        la->area_sizey = la->area_size;
      }

      la->area_size *= rsmat[0][0];
      la->area_sizey *= rsmat[1][1];
      la->area_sizez *= rsmat[2][2];
    }
    else {
      continue;
    }

    if (do_multi_user && ob != obact) {
      float _obmat[4][4], _iobmat[4][4];
      float _mat[4][4];

      copy_m4_m4(_obmat, ob->object_to_world().ptr());
      invert_m4_m4(_iobmat, _obmat);

      copy_m4_m4(_mat, _obmat);
      mul_m4_m4_post(_mat, obact_invmat);
      mul_m4_m4_post(_mat, obact_parent);
      mul_m4_m4_post(_mat, obact_parentinv);

      if (apply_loc && apply_scale && apply_rot) {
        BKE_object_apply_mat4(ob, _mat, false, true);
      }
      else {
        Object ob_temp = blender::dna::shallow_copy(*ob);
        BKE_object_apply_mat4(&ob_temp, _mat, false, true);

        if (apply_loc) {
          copy_v3_v3(ob->loc, ob_temp.loc);
        }

        if (apply_scale) {
          copy_v3_v3(ob->scale, ob_temp.scale);
        }

        if (apply_rot) {
          copy_v4_v4(ob->quat, ob_temp.quat);
          copy_v3_v3(ob->rot, ob_temp.rot);
          copy_v3_v3(ob->rotAxis, ob_temp.rotAxis);
          ob->rotAngle = ob_temp.rotAngle;
        }
      }
    }
    else {
      if (apply_loc) {
        zero_v3(ob->loc);
        zero_v3(ob->dloc);
      }
      if (apply_scale) {
        copy_v3_fl(ob->scale, 1.0f);
        copy_v3_fl(ob->dscale, 1.0f);
      }
      if (apply_rot) {
        zero_v3(ob->rot);
        zero_v3(ob->drot);
        unit_qt(ob->quat);
        unit_qt(ob->dquat);
        unit_axis_angle(ob->rotAxis, &ob->rotAngle);
        unit_axis_angle(ob->drotAxis, &ob->drotAngle);
      }
    }

    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    BKE_object_transform_copy(ob_eval, ob);

    BKE_object_where_is_calc(depsgraph, scene, ob_eval);
    if (ob->type == OB_ARMATURE) {
      /* needed for bone parents */
      BKE_armature_copy_bone_transforms(static_cast<bArmature *>(ob_eval->data),
                                        static_cast<bArmature *>(ob->data));
      BKE_pose_where_is(depsgraph, scene, ob_eval);
    }

    ignore_parent_tx(bmain, depsgraph, scene, ob);

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

    changed = true;
  }

  if (!changed) {
    BKE_report(reports, RPT_WARNING, "Objects have no data to transform");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  return OPERATOR_FINISHED;
}

static int visual_transform_apply_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  bool changed = false;

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    BKE_object_where_is_calc(depsgraph, scene, ob_eval);
    BKE_object_apply_mat4(ob_eval, ob_eval->object_to_world().ptr(), true, true);
    BKE_object_transform_copy(ob, ob_eval);

    /* update for any children that may get moved */
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

    changed = true;
  }
  CTX_DATA_END;

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_visual_transform_apply(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Visual Transform";
  ot->description = "Apply the object's visual transformation to its data";
  ot->idname = "OBJECT_OT_visual_transform_apply";

  /* api callbacks */
  ot->exec = visual_transform_apply_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_transform_apply_exec(bContext *C, wmOperator *op)
{
  const bool loc = RNA_boolean_get(op->ptr, "location");
  const bool rot = RNA_boolean_get(op->ptr, "rotation");
  const bool sca = RNA_boolean_get(op->ptr, "scale");
  const bool do_props = RNA_boolean_get(op->ptr, "properties");
  const bool do_single_user = RNA_boolean_get(op->ptr, "isolate_users");

  if (loc || rot || sca) {
    return apply_objects_internal(C, op->reports, loc, rot, sca, do_props, do_single_user);
  }
  /* allow for redo */
  return OPERATOR_FINISHED;
}

static int object_transform_apply_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Object *ob = ED_object_active_context(C);

  bool can_handle_multiuser = apply_objects_internal_can_multiuser(C);
  bool need_single_user = can_handle_multiuser && apply_objects_internal_need_single_user(C);

  if ((ob != nullptr) && (ob->data != nullptr) && need_single_user) {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "isolate_users");
    if (!RNA_property_is_set(op->ptr, prop)) {
      RNA_property_boolean_set(op->ptr, prop, true);
    }
    if (RNA_property_boolean_get(op->ptr, prop)) {
      return WM_operator_confirm_ex(C,
                                    op,
                                    IFACE_("Apply Object Transformations"),
                                    IFACE_("Warning: Multiple objects share the same data.\nMake "
                                           "single user and then apply transformations?"),
                                    IFACE_("Apply"),
                                    ALERT_ICON_WARNING,
                                    false);
    }
  }
  return object_transform_apply_exec(C, op);
}

void OBJECT_OT_transform_apply(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Object Transform";
  ot->description = "Apply the object's transformation to its data";
  ot->idname = "OBJECT_OT_transform_apply";

  /* api callbacks */
  ot->exec = object_transform_apply_exec;
  ot->invoke = object_transform_apply_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "location", true, "Location", "");
  RNA_def_boolean(ot->srna, "rotation", true, "Rotation", "");
  RNA_def_boolean(ot->srna, "scale", true, "Scale", "");
  RNA_def_boolean(ot->srna,
                  "properties",
                  true,
                  "Apply Properties",
                  "Modify properties such as curve vertex radius, font size and bone envelope");
  PropertyRNA *prop = RNA_def_boolean(ot->srna,
                                      "isolate_users",
                                      false,
                                      "Isolate Multi User Data",
                                      "Create new object-data users if needed");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Parent Inverse Operator
 * \{ */

static int object_parent_inverse_apply_exec(bContext *C, wmOperator * /*op*/)
{
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob->parent == nullptr) {
      continue;
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    BKE_object_apply_parent_inverse(ob);
  }
  CTX_DATA_END;

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_inverse_apply(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Parent Inverse";
  ot->description = "Apply the object's parent inverse to its data";
  ot->idname = "OBJECT_OT_parent_inverse_apply";

  /* api callbacks */
  ot->exec = object_parent_inverse_apply_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Object Center Operator
 * \{ */

enum {
  GEOMETRY_TO_ORIGIN = 0,
  ORIGIN_TO_GEOMETRY,
  ORIGIN_TO_CURSOR,
  ORIGIN_TO_CENTER_OF_MASS_SURFACE,
  ORIGIN_TO_CENTER_OF_MASS_VOLUME,
};

static float3 arithmetic_mean(const blender::Span<blender::float3> values)
{
  if (values.is_empty()) {
    return float3(0);
  }
  /* TODO: Use a method that avoids overflow. */
  return std::accumulate(values.begin(), values.end(), float3(0)) / values.size();
}

static void translate_positions(blender::MutableSpan<blender::float3> positions,
                                const blender::float3 &translation)
{
  using namespace blender;
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

static int object_origin_set_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);
  Object *obedit = CTX_data_edit_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  float3 cent, cent_neg, centn;
  const float *cursor = scene->cursor.location;
  int centermode = RNA_enum_get(op->ptr, "type");

  /* keep track of what is changed */
  int tot_change = 0, tot_lib_error = 0, tot_multiuser_arm_error = 0;

  if (obedit && centermode != GEOMETRY_TO_ORIGIN) {
    BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in edit mode");
    return OPERATOR_CANCELLED;
  }

  int around;
  {
    PropertyRNA *prop_center = RNA_struct_find_property(op->ptr, "center");
    if (RNA_property_is_set(op->ptr, prop_center)) {
      around = RNA_property_enum_get(op->ptr, prop_center);
    }
    else {
      if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CENTER_BOUNDS) {
        around = V3D_AROUND_CENTER_BOUNDS;
      }
      else {
        around = V3D_AROUND_CENTER_MEDIAN;
      }
      RNA_property_enum_set(op->ptr, prop_center, around);
    }
  }

  zero_v3(cent);

  if (obedit) {
    if (obedit->type == OB_MESH) {
      Mesh *mesh = static_cast<Mesh *>(obedit->data);
      BMEditMesh *em = mesh->edit_mesh;
      BMVert *eve;
      BMIter iter;

      if (centermode == ORIGIN_TO_CURSOR) {
        copy_v3_v3(cent, cursor);
        invert_m4_m4(obedit->runtime->world_to_object.ptr(), obedit->object_to_world().ptr());
        mul_m4_v3(obedit->world_to_object().ptr(), cent);
      }
      else {
        if (around == V3D_AROUND_CENTER_BOUNDS) {
          float min[3], max[3];
          INIT_MINMAX(min, max);
          BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
            minmax_v3v3_v3(min, max, eve->co);
          }
          mid_v3_v3v3(cent, min, max);
        }
        else { /* #V3D_AROUND_CENTER_MEDIAN. */
          if (em->bm->totvert) {
            const float total_div = 1.0f / float(em->bm->totvert);
            BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
              madd_v3_v3fl(cent, eve->co, total_div);
            }
          }
        }
      }

      BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
        sub_v3_v3(eve->co, cent);
      }

      EDBM_mesh_normals_update(em);
      tot_change++;
      DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
    }
  }

  Array<Object *> objects = sorted_selected_editable_objects(C);
  if (objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* reset flags */
  for (const int object_index : objects.index_range()) {
    Object *ob = objects[object_index];
    ob->flag &= ~OB_DONE;

    /* move active first */
    if (ob == obact && objects.size() > 1) {
      memmove(&objects[1], objects.data(), object_index * sizeof(Object *));
      objects[0] = ob;
    }
  }

  LISTBASE_FOREACH (Object *, tob, &bmain->objects) {
    if (tob->data) {
      ((ID *)tob->data)->tag &= ~LIB_TAG_DOIT;
    }
    if (tob->instance_collection) {
      ((ID *)tob->instance_collection)->tag &= ~LIB_TAG_DOIT;
    }
  }

  for (Object *ob : objects) {
    if (ob->flag & OB_DONE) {
      continue;
    }

    bool do_inverse_offset = false;
    ob->flag |= OB_DONE;

    if (centermode == ORIGIN_TO_CURSOR) {
      copy_v3_v3(cent, cursor);
      invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      mul_m4_v3(ob->world_to_object().ptr(), cent);
    }

    if (ob->data == nullptr) {
      /* Special support for instanced collections. */
      if ((ob->transflag & OB_DUPLICOLLECTION) && ob->instance_collection &&
          (ob->instance_collection->id.tag & LIB_TAG_DOIT) == 0)
      {
        if (!BKE_id_is_editable(bmain, &ob->instance_collection->id)) {
          tot_lib_error++;
        }
        else {
          if (centermode == ORIGIN_TO_CURSOR) {
            /* done */
          }
          else {
            float min[3], max[3];
            /* only bounds support */
            INIT_MINMAX(min, max);
            BKE_object_minmax_dupli(depsgraph, scene, ob, min, max, true);
            mid_v3_v3v3(cent, min, max);
            invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
            mul_m4_v3(ob->world_to_object().ptr(), cent);
          }

          add_v3_v3(ob->instance_collection->instance_offset, cent);

          tot_change++;
          ob->instance_collection->id.tag |= LIB_TAG_DOIT;
          do_inverse_offset = true;
        }
      }
    }
    else if (ID_IS_LINKED(ob->data) || ID_IS_OVERRIDE_LIBRARY(ob->data)) {
      tot_lib_error++;
    }
    else if (ob->type == OB_MESH) {
      if (obedit == nullptr) {
        Mesh *mesh = static_cast<Mesh *>(ob->data);

        if (centermode == ORIGIN_TO_CURSOR) {
          /* done */
        }
        else if (centermode == ORIGIN_TO_CENTER_OF_MASS_SURFACE) {
          BKE_mesh_center_of_surface(mesh, cent);
        }
        else if (centermode == ORIGIN_TO_CENTER_OF_MASS_VOLUME) {
          BKE_mesh_center_of_volume(mesh, cent);
        }
        else if (around == V3D_AROUND_CENTER_BOUNDS) {
          if (const std::optional<Bounds<float3>> bounds = mesh->bounds_min_max()) {
            cent = math::midpoint(bounds->min, bounds->max);
          }
        }
        else { /* #V3D_AROUND_CENTER_MEDIAN. */
          BKE_mesh_center_median(mesh, cent);
        }

        negate_v3_v3(cent_neg, cent);
        BKE_mesh_translate(mesh, cent_neg, true);

        tot_change++;
        mesh->id.tag |= LIB_TAG_DOIT;
        do_inverse_offset = true;
      }
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      Curve *cu = static_cast<Curve *>(ob->data);

      if (centermode == ORIGIN_TO_CURSOR) {
        /* done */
      }
      else if (around == V3D_AROUND_CENTER_BOUNDS) {
        if (std::optional<blender::Bounds<blender::float3>> bounds = BKE_curve_minmax(cu, true)) {
          cent = math::midpoint(bounds->min, bounds->max);
        }
      }
      else { /* #V3D_AROUND_CENTER_MEDIAN. */
        BKE_curve_center_median(cu, cent);
      }

      /* don't allow Z change if curve is 2D */
      if ((ob->type == OB_CURVES_LEGACY) && !(cu->flag & CU_3D)) {
        cent[2] = 0.0;
      }

      negate_v3_v3(cent_neg, cent);
      BKE_curve_translate(cu, cent_neg, true);

      tot_change++;
      cu->id.tag |= LIB_TAG_DOIT;
      do_inverse_offset = true;

      if (obedit) {
        if (centermode == GEOMETRY_TO_ORIGIN) {
          DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
        }
        break;
      }
    }
    else if (ob->type == OB_FONT) {
      /* Get from bounding-box. */

      Curve *cu = static_cast<Curve *>(ob->data);
      std::optional<blender::Bounds<blender::float3>> bounds = BKE_curve_minmax(cu, true);

      if (!bounds && (centermode != ORIGIN_TO_CURSOR)) {
        /* Do nothing. */
      }
      else {
        if (centermode == ORIGIN_TO_CURSOR) {
          /* Done. */
        }
        else {
          /* extra 0.5 is the height o above line */
          cent = math::midpoint(bounds->min, bounds->max);
        }

        cent[2] = 0.0f;

        cu->xof = cu->xof - cent[0];
        cu->yof = cu->yof - cent[1];

        tot_change++;
        cu->id.tag |= LIB_TAG_DOIT;
        do_inverse_offset = true;
      }
    }
    else if (ob->type == OB_ARMATURE) {
      bArmature *arm = static_cast<bArmature *>(ob->data);

      if (ID_REAL_USERS(arm) > 1) {
#if 0
        BKE_report(op->reports, RPT_ERROR, "Cannot apply to a multi user armature");
        return;
#endif
        tot_multiuser_arm_error++;
      }
      else {
        /* Function to recenter armatures in editarmature.c
         * Bone + object locations are handled there.
         */
        ED_armature_origin_set(bmain, ob, cursor, centermode, around);

        tot_change++;
        arm->id.tag |= LIB_TAG_DOIT;
        // do_inverse_offset = true; /* docenter_armature() handles this. */

        Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
        BKE_object_transform_copy(ob_eval, ob);
        BKE_armature_copy_bone_transforms(static_cast<bArmature *>(ob_eval->data),
                                          static_cast<bArmature *>(ob->data));
        BKE_object_where_is_calc(depsgraph, scene, ob_eval);
        BKE_pose_where_is(depsgraph, scene, ob_eval); /* needed for bone parents */

        ignore_parent_tx(bmain, depsgraph, scene, ob);

        if (obedit) {
          break;
        }
      }
    }
    else if (ob->type == OB_MBALL) {
      MetaBall *mb = static_cast<MetaBall *>(ob->data);

      if (centermode == ORIGIN_TO_CURSOR) {
        /* done */
      }
      else if (around == V3D_AROUND_CENTER_BOUNDS) {
        BKE_mball_center_bounds(mb, cent);
      }
      else { /* #V3D_AROUND_CENTER_MEDIAN. */
        BKE_mball_center_median(mb, cent);
      }

      negate_v3_v3(cent_neg, cent);
      BKE_mball_translate(mb, cent_neg);

      tot_change++;
      mb->id.tag |= LIB_TAG_DOIT;
      do_inverse_offset = true;

      if (obedit) {
        if (centermode == GEOMETRY_TO_ORIGIN) {
          DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
        }
        break;
      }
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = static_cast<Lattice *>(ob->data);

      if (centermode == ORIGIN_TO_CURSOR) {
        /* done */
      }
      else if (around == V3D_AROUND_CENTER_BOUNDS) {
        if (std::optional<blender::Bounds<blender::float3>> bounds = BKE_lattice_minmax(lt)) {
          cent = math::midpoint(bounds->min, bounds->max);
        }
      }
      else { /* #V3D_AROUND_CENTER_MEDIAN. */
        BKE_lattice_center_median(lt, cent);
      }

      negate_v3_v3(cent_neg, cent);
      BKE_lattice_translate(lt, cent_neg, true);

      tot_change++;
      lt->id.tag |= LIB_TAG_DOIT;
      do_inverse_offset = true;
    }
    else if (ob->type == OB_GPENCIL_LEGACY) {
      bGPdata *gpd = static_cast<bGPdata *>(ob->data);
      float gpcenter[3];
      if (gpd) {
        if (centermode == ORIGIN_TO_GEOMETRY) {
          zero_v3(gpcenter);
          BKE_gpencil_centroid_3d(gpd, gpcenter);
          add_v3_v3(gpcenter, ob->object_to_world().location());
        }
        if (centermode == ORIGIN_TO_CURSOR) {
          copy_v3_v3(gpcenter, cursor);
        }
        if (ELEM(centermode, ORIGIN_TO_GEOMETRY, ORIGIN_TO_CURSOR)) {
          bGPDspoint *pt;
          float imat[3][3], bmat[3][3];
          float offset_global[3];
          float offset_local[3];
          int i;

          sub_v3_v3v3(offset_global, gpcenter, ob->object_to_world().location());
          copy_m3_m4(bmat, obact->object_to_world().ptr());
          invert_m3_m3(imat, bmat);
          mul_m3_v3(imat, offset_global);
          mul_v3_m3v3(offset_local, imat, offset_global);

          float diff_mat[4][4];
          float inverse_diff_mat[4][4];

          /* recalculate all strokes
           * (all layers are considered without evaluating lock attributes) */
          LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
            /* calculate difference matrix */
            BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);
            /* undo matrix */
            invert_m4_m4(inverse_diff_mat, diff_mat);
            LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
              LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
                for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                  float mpt[3];
                  mul_v3_m4v3(mpt, inverse_diff_mat, &pt->x);
                  sub_v3_v3(mpt, offset_local);
                  mul_v3_m4v3(&pt->x, diff_mat, mpt);
                }

                /* Apply transform to edit-curve. */
                if (gps->editcurve != nullptr) {
                  for (i = 0; i < gps->editcurve->tot_curve_points; i++) {
                    BezTriple *bezt = &gps->editcurve->curve_points[i].bezt;
                    for (int j = 0; j < 3; j++) {
                      float mpt[3];
                      mul_v3_m4v3(mpt, inverse_diff_mat, bezt->vec[j]);
                      sub_v3_v3(mpt, offset_local);
                      mul_v3_m4v3(bezt->vec[j], diff_mat, mpt);
                    }
                  }
                }
                BKE_gpencil_stroke_geometry_update(gpd, gps);
              }
            }
          }
          tot_change++;
          if (centermode == ORIGIN_TO_GEOMETRY) {
            copy_v3_v3(ob->loc, gpcenter);
          }
          DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
          DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

          ob->id.tag |= LIB_TAG_DOIT;
          do_inverse_offset = true;
        }
        else {
          BKE_report(op->reports,
                     RPT_WARNING,
                     "Grease Pencil Object does not support this set origin option");
        }
      }
    }
    else if (ob->type == OB_CURVES) {
      using namespace blender;
      Curves &curves_id = *static_cast<Curves *>(ob->data);
      bke::CurvesGeometry &curves = curves_id.geometry.wrap();
      if (ELEM(centermode, ORIGIN_TO_CENTER_OF_MASS_SURFACE, ORIGIN_TO_CENTER_OF_MASS_VOLUME) ||
          !ELEM(around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN))
      {
        BKE_report(
            op->reports, RPT_WARNING, "Curves Object does not support this set origin operation");
        continue;
      }

      if (curves.points_num() == 0) {
        continue;
      }

      if (centermode == ORIGIN_TO_CURSOR) {
        /* done */
      }
      else if (around == V3D_AROUND_CENTER_BOUNDS) {
        const Bounds<float3> bounds = *curves.bounds_min_max();
        cent = math::midpoint(bounds.min, bounds.max);
      }
      else if (around == V3D_AROUND_CENTER_MEDIAN) {
        cent = arithmetic_mean(curves.positions());
      }

      tot_change++;
      curves.translate(-cent);
      curves_id.id.tag |= LIB_TAG_DOIT;
      do_inverse_offset = true;
    }
    else if (ob->type == OB_POINTCLOUD) {
      PointCloud &pointcloud = *static_cast<PointCloud *>(ob->data);
      MutableSpan<float3> positions = pointcloud.positions_for_write();
      if (ELEM(centermode, ORIGIN_TO_CENTER_OF_MASS_SURFACE, ORIGIN_TO_CENTER_OF_MASS_VOLUME) ||
          !ELEM(around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN))
      {
        BKE_report(op->reports,
                   RPT_WARNING,
                   "Point cloud object does not support this set origin operation");
        continue;
      }

      if (centermode == ORIGIN_TO_CURSOR) {
        /* Done. */
      }
      else if (around == V3D_AROUND_CENTER_BOUNDS) {
        if (const std::optional<Bounds<float3>> bounds = pointcloud.bounds_min_max()) {
          cent = math::midpoint(bounds->min, bounds->max);
        }
      }
      else if (around == V3D_AROUND_CENTER_MEDIAN) {
        cent = arithmetic_mean(positions);
      }

      tot_change++;
      translate_positions(positions, -cent);
      pointcloud.tag_positions_changed();
      pointcloud.id.tag |= LIB_TAG_DOIT;
      do_inverse_offset = true;
    }

    /* offset other selected objects */
    if (do_inverse_offset && (centermode != GEOMETRY_TO_ORIGIN)) {
      float obmat[4][4];

      /* was the object data modified
       * NOTE: the functions above must set 'cent'. */

      /* convert the offset to parent space */
      BKE_object_to_mat4(ob, obmat);
      mul_v3_mat3_m4v3(centn, obmat, cent); /* omit translation part */

      add_v3_v3(ob->loc, centn);

      Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
      BKE_object_transform_copy(ob_eval, ob);
      BKE_object_where_is_calc(depsgraph, scene, ob_eval);
      if (ob->type == OB_ARMATURE) {
        /* needed for bone parents */
        BKE_armature_copy_bone_transforms(static_cast<bArmature *>(ob_eval->data),
                                          static_cast<bArmature *>(ob->data));
        BKE_pose_where_is(depsgraph, scene, ob_eval);
      }

      ignore_parent_tx(bmain, depsgraph, scene, ob);

      /* other users? */
      // CTX_DATA_BEGIN (C, Object *, ob_other, selected_editable_objects)
      //{

      /* use existing context looper */
      for (Object *ob_other : objects) {
        if ((ob_other->flag & OB_DONE) == 0 &&
            ((ob->data && (ob->data == ob_other->data)) ||
             (ob->instance_collection == ob_other->instance_collection &&
              (ob->transflag | ob_other->transflag) & OB_DUPLICOLLECTION)))
        {
          ob_other->flag |= OB_DONE;
          DEG_id_tag_update(&ob_other->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

          mul_v3_mat3_m4v3(
              centn, ob_other->object_to_world().ptr(), cent); /* omit translation part */
          add_v3_v3(ob_other->loc, centn);

          Object *ob_other_eval = DEG_get_evaluated_object(depsgraph, ob_other);
          BKE_object_transform_copy(ob_other_eval, ob_other);
          BKE_object_where_is_calc(depsgraph, scene, ob_other_eval);
          if (ob_other->type == OB_ARMATURE) {
            /* needed for bone parents */
            BKE_armature_copy_bone_transforms(static_cast<bArmature *>(ob_eval->data),
                                              static_cast<bArmature *>(ob->data));
            BKE_pose_where_is(depsgraph, scene, ob_other_eval);
          }
          ignore_parent_tx(bmain, depsgraph, scene, ob_other);
        }
      }
      // CTX_DATA_END;
    }
  }

  LISTBASE_FOREACH (Object *, tob, &bmain->objects) {
    if (tob->data && (((ID *)tob->data)->tag & LIB_TAG_DOIT)) {
      BKE_object_batch_cache_dirty_tag(tob);
      DEG_id_tag_update(&tob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }
    /* Special support for dupli-groups. */
    else if (tob->instance_collection && tob->instance_collection->id.tag & LIB_TAG_DOIT) {
      DEG_id_tag_update(&tob->id, ID_RECALC_TRANSFORM);
      DEG_id_tag_update(&tob->instance_collection->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }

  if (tot_change) {
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  }

  /* Warn if any errors occurred */
  if (tot_lib_error + tot_multiuser_arm_error) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "%i object(s) not centered, %i changed:",
                tot_lib_error + tot_multiuser_arm_error,
                tot_change);
    if (tot_lib_error) {
      BKE_reportf(op->reports, RPT_WARNING, "|%i linked library object(s)", tot_lib_error);
    }
    if (tot_multiuser_arm_error) {
      BKE_reportf(
          op->reports, RPT_WARNING, "|%i multiuser armature object(s)", tot_multiuser_arm_error);
    }
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_set(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_set_center_types[] = {
      {GEOMETRY_TO_ORIGIN,
       "GEOMETRY_ORIGIN",
       0,
       "Geometry to Origin",
       "Move object geometry to object origin"},
      {ORIGIN_TO_GEOMETRY,
       "ORIGIN_GEOMETRY",
       0,
       "Origin to Geometry",
       "Calculate the center of geometry based on the current pivot point (median, otherwise "
       "bounding box)"},
      {ORIGIN_TO_CURSOR,
       "ORIGIN_CURSOR",
       0,
       "Origin to 3D Cursor",
       "Move object origin to position of the 3D cursor"},
      /* Intentional naming mismatch since some scripts refer to this. */
      {ORIGIN_TO_CENTER_OF_MASS_SURFACE,
       "ORIGIN_CENTER_OF_MASS",
       0,
       "Origin to Center of Mass (Surface)",
       "Calculate the center of mass from the surface area"},
      {ORIGIN_TO_CENTER_OF_MASS_VOLUME,
       "ORIGIN_CENTER_OF_VOLUME",
       0,
       "Origin to Center of Mass (Volume)",
       "Calculate the center of mass from the volume (must be manifold geometry with consistent "
       "normals)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_set_bounds_types[] = {
      {V3D_AROUND_CENTER_MEDIAN, "MEDIAN", 0, "Median Center", ""},
      {V3D_AROUND_CENTER_BOUNDS, "BOUNDS", 0, "Bounds Center", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Set Origin";
  ot->description =
      "Set the object's origin, by either moving the data, or set to center of data, or use 3D "
      "cursor";
  ot->idname = "OBJECT_OT_origin_set";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_origin_set_exec;

  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_set_center_types, 0, "Type", "");
  RNA_def_enum(ot->srna, "center", prop_set_bounds_types, V3D_AROUND_CENTER_MEDIAN, "Center", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Axis Target
 *
 * Note this is an experimental operator to point lights/cameras at objects.
 * We may re-work how this behaves based on user feedback.
 * - campbell.
 * \{ */

/** When using multiple objects, apply their relative rotational offset to the active object. */
#define USE_RELATIVE_ROTATION
/** Disable overlays, ignoring user setting (light wire gets in the way). */
#define USE_RENDER_OVERRIDE
/**
 * Calculate a depth if the cursor isn't already over a depth
 * (not essential but feels buggy without).
 */
#define USE_FAKE_DEPTH_INIT

struct XFormAxisItem {
  Object *ob;
  float rot_mat[3][3];
  void *obtfm;
  float xform_dist;
  bool is_z_flip;

#ifdef USE_RELATIVE_ROTATION
  /* use when translating multiple */
  float xform_rot_offset[3][3];
#endif
};

struct XFormAxisData {
  ViewContext vc;
  ViewDepths *depths;
  struct {
    float depth;
    float normal[3];
    bool is_depth_valid;
    bool is_normal_valid;
  } prev;

  Vector<XFormAxisItem> object_data;
  bool is_translate;

  int init_event;
};

#ifdef USE_FAKE_DEPTH_INIT
static void object_transform_axis_target_calc_depth_init(XFormAxisData *xfd, const int mval[2])
{
  float view_co_a[3], view_co_b[3];
  const float2 mval_fl = {float(mval[0]), float(mval[1])};
  ED_view3d_win_to_ray(xfd->vc.region, mval_fl, view_co_a, view_co_b);
  add_v3_v3(view_co_b, view_co_a);
  float center[3] = {0.0f};
  int center_tot = 0;
  for (XFormAxisItem &item : xfd->object_data) {
    const Object *ob = item.ob;
    const float *ob_co_a = ob->object_to_world().location();
    float ob_co_b[3];
    add_v3_v3v3(ob_co_b, ob->object_to_world().location(), ob->object_to_world().ptr()[2]);
    float view_isect[3], ob_isect[3];
    if (isect_line_line_v3(view_co_a, view_co_b, ob_co_a, ob_co_b, view_isect, ob_isect)) {
      add_v3_v3(center, view_isect);
      center_tot += 1;
    }
  }
  if (center_tot) {
    mul_v3_fl(center, 1.0f / center_tot);
    float center_proj[3];
    ED_view3d_project_v3(xfd->vc.region, center, center_proj);
    xfd->prev.depth = center_proj[2];
    xfd->prev.is_depth_valid = true;
  }
}
#endif /* USE_FAKE_DEPTH_INIT */

static bool object_is_target_compat(const Object *ob)
{
  if (ob->type == OB_LAMP) {
    const Light *la = static_cast<Light *>(ob->data);
    if (ELEM(la->type, LA_SUN, LA_SPOT, LA_AREA)) {
      return true;
    }
  }
  /* We might want to enable this later, for now just lights. */
#if 0
  else if (ob->type == OB_CAMERA) {
    return true;
  }
#endif
  return false;
}

static void object_transform_axis_target_free_data(wmOperator *op)
{
  XFormAxisData *xfd = static_cast<XFormAxisData *>(op->customdata);

#ifdef USE_RENDER_OVERRIDE
  if (xfd->depths) {
    ED_view3d_depths_free(xfd->depths);
  }
#endif

  for (XFormAxisItem &item : xfd->object_data) {
    MEM_freeN(item.obtfm);
  }
  MEM_delete(xfd);
  op->customdata = nullptr;
}

/* We may want to expose as alternative to: BKE_object_apply_rotation */
static void object_apply_rotation(Object *ob, const float rmat[3][3])
{
  float size[3];
  float loc[3];
  float rmat4[4][4];
  copy_m4_m3(rmat4, rmat);

  copy_v3_v3(size, ob->scale);
  copy_v3_v3(loc, ob->loc);
  BKE_object_apply_mat4(ob, rmat4, true, true);
  copy_v3_v3(ob->scale, size);
  copy_v3_v3(ob->loc, loc);
}
/* We may want to extract this to: BKE_object_apply_location */
static void object_apply_location(Object *ob, const float loc[3])
{
  /* quick but weak */
  Object ob_prev = blender::dna::shallow_copy(*ob);
  float mat[4][4];
  copy_m4_m4(mat, ob->object_to_world().ptr());
  copy_v3_v3(mat[3], loc);
  BKE_object_apply_mat4(ob, mat, true, true);
  copy_v3_v3(mat[3], ob->loc);
  *ob = blender::dna::shallow_copy(ob_prev);
  copy_v3_v3(ob->loc, mat[3]);
}

static bool object_orient_to_location(Object *ob,
                                      const float rot_orig[3][3],
                                      const float axis[3],
                                      const float location[3],
                                      const bool z_flip)
{
  float delta[3];
  sub_v3_v3v3(delta, ob->object_to_world().location(), location);
  if (normalize_v3(delta) != 0.0f) {
    if (z_flip) {
      negate_v3(delta);
    }

    if (len_squared_v3v3(delta, axis) > FLT_EPSILON) {
      float delta_rot[3][3];
      float final_rot[3][3];
      rotation_between_vecs_to_mat3(delta_rot, axis, delta);

      mul_m3_m3m3(final_rot, delta_rot, rot_orig);

      object_apply_rotation(ob, final_rot);

      return true;
    }
  }
  return false;
}

static void object_transform_axis_target_cancel(bContext *C, wmOperator *op)
{
  XFormAxisData *xfd = static_cast<XFormAxisData *>(op->customdata);
  for (XFormAxisItem &item : xfd->object_data) {
    BKE_object_tfm_restore(item.ob, item.obtfm);
    DEG_id_tag_update(&item.ob->id, ID_RECALC_TRANSFORM);
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, item.ob);
  }

  object_transform_axis_target_free_data(op);
}

static int object_transform_axis_target_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  if (vc.obact == nullptr || !object_is_target_compat(vc.obact)) {
    /* Falls back to texture space transform. */
    return OPERATOR_PASS_THROUGH;
  }

#ifdef USE_RENDER_OVERRIDE
  int flag2_prev = vc.v3d->flag2;
  vc.v3d->flag2 |= V3D_HIDE_OVERLAYS;
#endif

  ViewDepths *depths = nullptr;
  ED_view3d_depth_override(
      vc.depsgraph, vc.region, vc.v3d, nullptr, V3D_DEPTH_NO_GPENCIL, &depths);

#ifdef USE_RENDER_OVERRIDE
  vc.v3d->flag2 = flag2_prev;
#endif

  if (depths == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Unable to access depth buffer, using view plane");
    return OPERATOR_CANCELLED;
  }

  ED_region_tag_redraw(vc.region);

  XFormAxisData *xfd = MEM_new<XFormAxisData>(__func__);
  op->customdata = xfd;

  /* Don't change this at runtime. */
  xfd->vc = vc;
  xfd->depths = depths;
  xfd->vc.mval[0] = event->mval[0];
  xfd->vc.mval[1] = event->mval[1];

  xfd->prev.depth = 1.0f;
  xfd->prev.is_depth_valid = false;
  xfd->prev.is_normal_valid = false;
  xfd->is_translate = false;

  xfd->init_event = WM_userdef_event_type_from_keymap_type(event->type);

  xfd->object_data.append({});
  xfd->object_data.last().ob = xfd->vc.obact;

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if ((ob != xfd->vc.obact) && object_is_target_compat(ob)) {
      xfd->object_data.append({});
      xfd->object_data.last().ob = ob;
    }
  }
  CTX_DATA_END;

  for (XFormAxisItem &item : xfd->object_data) {
    item.obtfm = BKE_object_tfm_backup(item.ob);
    BKE_object_rot_to_mat3(item.ob, item.rot_mat, true);

    /* Detect negative scale matrix. */
    float full_mat3[3][3];
    BKE_object_to_mat3(item.ob, full_mat3);
    item.is_z_flip = dot_v3v3(item.rot_mat[2], full_mat3[2]) < 0.0f;
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int object_transform_axis_target_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  XFormAxisData *xfd = static_cast<XFormAxisData *>(op->customdata);
  ARegion *region = xfd->vc.region;

  view3d_operator_needs_opengl(C);

  const bool is_translate = event->modifier & KM_CTRL;
  const bool is_translate_init = is_translate && (xfd->is_translate != is_translate);

  if (event->type == MOUSEMOVE || is_translate_init) {
    const ViewDepths *depths = xfd->depths;
    if (depths && (uint(event->mval[0]) < depths->w) && (uint(event->mval[1]) < depths->h)) {
      float depth_fl = 1.0f;
      ED_view3d_depth_read_cached(depths, event->mval, 0, &depth_fl);
      float location_world[3];
      if (depth_fl == 1.0f) {
        if (xfd->prev.is_depth_valid) {
          depth_fl = xfd->prev.depth;
        }
      }

#ifdef USE_FAKE_DEPTH_INIT
      /* First time only. */
      if (depth_fl == 1.0f) {
        if (xfd->prev.is_depth_valid == false) {
          object_transform_axis_target_calc_depth_init(xfd, event->mval);
          if (xfd->prev.is_depth_valid) {
            depth_fl = xfd->prev.depth;
          }
        }
      }
#endif

      double depth = double(depth_fl);
      if ((depth > depths->depth_range[0]) && (depth < depths->depth_range[1])) {
        xfd->prev.depth = depth_fl;
        xfd->prev.is_depth_valid = true;
        if (ED_view3d_depth_unproject_v3(region, event->mval, depth, location_world)) {
          if (is_translate) {

            float normal[3];
            bool normal_found = false;
            if (ED_view3d_depth_read_cached_normal(region, depths, event->mval, normal)) {
              normal_found = true;

              /* cheap attempt to smooth normals out a bit! */
              const int ofs = 2;
              for (int x = -ofs; x <= ofs; x += ofs / 2) {
                for (int y = -ofs; y <= ofs; y += ofs / 2) {
                  if (x != 0 && y != 0) {
                    const int mval_ofs[2] = {event->mval[0] + x, event->mval[1] + y};
                    float n[3];
                    if (ED_view3d_depth_read_cached_normal(region, depths, mval_ofs, n)) {
                      add_v3_v3(normal, n);
                    }
                  }
                }
              }
              normalize_v3(normal);
            }
            else if (xfd->prev.is_normal_valid) {
              copy_v3_v3(normal, xfd->prev.normal);
              normal_found = true;
            }

            {
#ifdef USE_RELATIVE_ROTATION
              if (is_translate_init && xfd->object_data.size() > 1) {
                float xform_rot_offset_inv_first[3][3];
                for (const int i : xfd->object_data.index_range()) {
                  XFormAxisItem &item = xfd->object_data[i];
                  copy_m3_m4(item.xform_rot_offset, item.ob->object_to_world().ptr());
                  normalize_m3(item.xform_rot_offset);

                  if (i == 0) {
                    invert_m3_m3(xform_rot_offset_inv_first, xfd->object_data[0].xform_rot_offset);
                  }
                  else {
                    mul_m3_m3m3(
                        item.xform_rot_offset, item.xform_rot_offset, xform_rot_offset_inv_first);
                  }
                }
              }

#endif

              for (const int i : xfd->object_data.index_range()) {
                XFormAxisItem &item = xfd->object_data[i];
                if (is_translate_init) {
                  float ob_axis[3];
                  item.xform_dist = len_v3v3(item.ob->object_to_world().location(),
                                             location_world);
                  normalize_v3_v3(ob_axis, item.ob->object_to_world().ptr()[2]);
                  /* Scale to avoid adding distance when moving between surfaces. */
                  if (normal_found) {
                    float scale = fabsf(dot_v3v3(ob_axis, normal));
                    item.xform_dist *= scale;
                  }
                }

                float target_normal[3];

                if (normal_found) {
                  copy_v3_v3(target_normal, normal);
                }
                else {
                  normalize_v3_v3(target_normal, item.ob->object_to_world().ptr()[2]);
                }

#ifdef USE_RELATIVE_ROTATION
                if (normal_found) {
                  if (i != 0) {
                    mul_m3_v3(item.xform_rot_offset, target_normal);
                  }
                }
#endif
                {
                  float loc[3];

                  copy_v3_v3(loc, location_world);
                  madd_v3_v3fl(loc, target_normal, item.xform_dist);
                  object_apply_location(item.ob, loc);
                  /* so orient behaves as expected */
                  copy_v3_v3(item.ob->runtime->object_to_world.location(), loc);
                }

                object_orient_to_location(
                    item.ob, item.rot_mat, item.rot_mat[2], location_world, item.is_z_flip);

                DEG_id_tag_update(&item.ob->id, ID_RECALC_TRANSFORM);
                WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, item.ob);
              }
              if (normal_found) {
                copy_v3_v3(xfd->prev.normal, normal);
                xfd->prev.is_normal_valid = true;
              }
            }
          }
          else {
            for (XFormAxisItem &item : xfd->object_data) {
              if (object_orient_to_location(
                      item.ob, item.rot_mat, item.rot_mat[2], location_world, item.is_z_flip))
              {
                DEG_id_tag_update(&item.ob->id, ID_RECALC_TRANSFORM);
                WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, item.ob);
              }
            }
            xfd->prev.is_normal_valid = false;
          }
        }
      }
    }
    xfd->is_translate = is_translate;

    ED_region_tag_redraw(xfd->vc.region);
  }

  bool is_finished = false;

  if (ISMOUSE_BUTTON(xfd->init_event)) {
    if ((event->type == xfd->init_event) && (event->val == KM_RELEASE)) {
      is_finished = true;
    }
  }
  else {
    if (ELEM(event->type, LEFTMOUSE, EVT_RETKEY, EVT_PADENTER)) {
      is_finished = true;
    }
  }

  if (is_finished) {
    object_transform_axis_target_free_data(op);
    return OPERATOR_FINISHED;
  }
  if (ELEM(event->type, EVT_ESCKEY, RIGHTMOUSE)) {
    object_transform_axis_target_cancel(C, op);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

void OBJECT_OT_transform_axis_target(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Interactive Light Track to Cursor";
  ot->description = "Interactively point cameras and lights to a location (Ctrl translates)";
  ot->idname = "OBJECT_OT_transform_axis_target";

  /* api callbacks */
  ot->invoke = object_transform_axis_target_invoke;
  ot->cancel = object_transform_axis_target_cancel;
  ot->modal = object_transform_axis_target_modal;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;
}

#undef USE_RELATIVE_ROTATION

/** \} */
