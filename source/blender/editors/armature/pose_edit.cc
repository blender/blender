/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 * Pose Mode API's and Operators for Pose Mode armatures.
 */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim_visualization.h"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_anim_api.hh"
#include "ED_armature.hh"
#include "ED_keyframing.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_keyframing.hh"

#include "armature_intern.hh"

#undef DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time_utildefines.h"
#endif

using blender::Vector;

Object *ED_pose_object_from_context(bContext *C)
{
  /* NOTE: matches logic with #ED_operator_posemode_context(). */

  ScrArea *area = CTX_wm_area(C);
  Object *ob;

  /* Since this call may also be used from the buttons window,
   * we need to check for where to get the object. */
  if (area && area->spacetype == SPACE_PROPERTIES) {
    ob = blender::ed::object::context_active_object(C);
  }
  else {
    ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  }

  return ob;
}

bool ED_object_posemode_enter_ex(Main *bmain, Object *ob)
{
  BLI_assert(BKE_id_is_editable(bmain, &ob->id));
  bool ok = false;

  switch (ob->type) {
    case OB_ARMATURE:
      ob->restore_mode = ob->mode;
      ob->mode |= OB_MODE_POSE;

      /* Inform all evaluated versions that we changed the mode. */
      DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_SYNC_TO_EVAL);
      ok = true;

      break;
    default:
      break;
  }

  return ok;
}
bool ED_object_posemode_enter(bContext *C, Object *ob)
{
  ReportList *reports = CTX_wm_reports(C);
  Main *bmain = CTX_data_main(C);
  if (!BKE_id_is_editable(bmain, &ob->id)) {
    BKE_report(reports, RPT_WARNING, "Cannot pose libdata");
    return false;
  }
  bool ok = ED_object_posemode_enter_ex(bmain, ob);
  if (ok) {
    WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_POSE, nullptr);
  }
  return ok;
}

bool ED_object_posemode_exit_ex(Main *bmain, Object *ob)
{
  bool ok = false;
  if (ob) {
    ob->restore_mode = ob->mode;
    ob->mode &= ~OB_MODE_POSE;

    /* Inform all evaluated versions that we changed the mode. */
    DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_SYNC_TO_EVAL);
    ok = true;
  }
  return ok;
}
bool ED_object_posemode_exit(bContext *C, Object *ob)
{
  Main *bmain = CTX_data_main(C);
  bool ok = ED_object_posemode_exit_ex(bmain, ob);
  if (ok) {
    WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, nullptr);
  }
  return ok;
}

/* ********************************************** */
/* Motion Paths */

static eAnimvizCalcRange pose_path_convert_range(ePosePathCalcRange range)
{
  switch (range) {
    case POSE_PATH_CALC_RANGE_CURRENT_FRAME:
      return ANIMVIZ_CALC_RANGE_CURRENT_FRAME;
    case POSE_PATH_CALC_RANGE_CHANGED:
      return ANIMVIZ_CALC_RANGE_CHANGED;
    case POSE_PATH_CALC_RANGE_FULL:
      return ANIMVIZ_CALC_RANGE_FULL;
  }
  return ANIMVIZ_CALC_RANGE_FULL;
}

void ED_pose_recalculate_paths(bContext *C, Scene *scene, Object *ob, ePosePathCalcRange range)
{
  /* Transform doesn't always have context available to do update. */
  if (C == nullptr) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Depsgraph *depsgraph;
  bool free_depsgraph = false;

  blender::Vector<MPathTarget *> targets;
  /* set flag to force recalc, then grab the relevant bones to target */
  ob->pose->avs.recalc |= ANIMVIZ_RECALC_PATHS;
  animviz_build_motionpath_targets(ob, targets);

/* recalculate paths, then free */
#ifdef DEBUG_TIME
  TIMEIT_START(pose_path_calc);
#endif

  /* For a single frame update it's faster to re-use existing dependency graph and avoid overhead
   * of building all the relations and so on for a temporary one. */
  if (range == POSE_PATH_CALC_RANGE_CURRENT_FRAME) {
    /* NOTE: Dependency graph will be evaluated at all the frames, but we first need to access some
     * nested pointers, like animation data. */
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    free_depsgraph = false;
  }
  else {
    depsgraph = animviz_depsgraph_build(bmain, scene, view_layer, targets);
    free_depsgraph = true;
  }

  animviz_calc_motionpaths(
      depsgraph, bmain, scene, targets, pose_path_convert_range(range), !free_depsgraph);

#ifdef DEBUG_TIME
  TIMEIT_END(pose_path_calc);
#endif

  animviz_free_motionpath_targets(targets);

  if (range != POSE_PATH_CALC_RANGE_CURRENT_FRAME) {
    /* Tag armature object for copy-on-eval - so paths will draw/redraw.
     * For currently frame only we update evaluated object directly. */
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }

  /* Free temporary depsgraph. */
  if (free_depsgraph) {
    DEG_graph_free(depsgraph);
  }
}

/* show popup to determine settings */
static wmOperatorStatus pose_calculate_paths_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));

  if (ELEM(nullptr, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* set default settings from existing/stored settings */
  {
    bAnimVizSettings *avs = &ob->pose->avs;

    PointerRNA avs_ptr = RNA_pointer_create_discrete(nullptr, &RNA_AnimVizMotionPaths, avs);
    RNA_enum_set(op->ptr, "display_type", RNA_enum_get(&avs_ptr, "type"));
    RNA_enum_set(op->ptr, "range", RNA_enum_get(&avs_ptr, "range"));
    RNA_enum_set(op->ptr, "bake_location", RNA_enum_get(&avs_ptr, "bake_location"));
  }

  /* show popup dialog to allow editing of range... */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(
      C, op, 270, IFACE_("Calculate Paths for the Selected Bones"), IFACE_("Calculate"));
}

/**
 * For the object with pose/action: create path curves for selected bones
 * This recalculates the WHOLE path within the `pchan->pathsf` and `pchan->pathef` range.
 */
static wmOperatorStatus pose_calculate_paths_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  Scene *scene = CTX_data_scene(C);

  if (ELEM(nullptr, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* grab baking settings from operator settings */
  {
    bAnimVizSettings *avs = &ob->pose->avs;

    avs->path_type = RNA_enum_get(op->ptr, "display_type");
    avs->path_range = RNA_enum_get(op->ptr, "range");
    animviz_motionpath_compute_range(ob, scene);

    PointerRNA avs_ptr = RNA_pointer_create_discrete(nullptr, &RNA_AnimVizMotionPaths, avs);
    RNA_enum_set(&avs_ptr, "bake_location", RNA_enum_get(op->ptr, "bake_location"));
  }

  /* set up path data for bones being calculated */
  CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones_from_active_object) {
    /* verify makes sure that the selected bone has a bone with the appropriate settings */
    animviz_verify_motionpaths(op->reports, scene, ob, pchan);
  }
  CTX_DATA_END;

#ifdef DEBUG_TIME
  TIMEIT_START(recalc_pose_paths);
#endif

  /* Calculate the bones that now have motion-paths. */
  /* TODO: only make for the selected bones? */
  ED_pose_recalculate_paths(C, scene, ob, POSE_PATH_CALC_RANGE_FULL);

#ifdef DEBUG_TIME
  TIMEIT_END(recalc_pose_paths);
#endif

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  return OPERATOR_FINISHED;
}

void POSE_OT_paths_calculate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Calculate Bone Paths";
  ot->idname = "POSE_OT_paths_calculate";
  ot->description = "Calculate paths for the selected bones";

  /* API callbacks. */
  ot->invoke = pose_calculate_paths_invoke;
  ot->exec = pose_calculate_paths_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "display_type",
               rna_enum_motionpath_display_type_items,
               MOTIONPATH_TYPE_RANGE,
               "Display Type",
               "");
  RNA_def_enum(ot->srna,
               "range",
               rna_enum_motionpath_range_items,
               MOTIONPATH_RANGE_SCENE,
               "Computation Range",
               "");

  RNA_def_enum(ot->srna,
               "bake_location",
               rna_enum_motionpath_bake_location_items,
               MOTIONPATH_BAKE_HEADS,
               "Bake Location",
               "Which point on the bones is used when calculating paths");
}

/* --------- */

static bool pose_update_paths_poll(bContext *C)
{
  if (ED_operator_posemode_exclusive(C)) {
    Object *ob = CTX_data_active_object(C);
    return (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

static wmOperatorStatus pose_update_paths_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  Scene *scene = CTX_data_scene(C);

  if (ELEM(nullptr, ob, scene)) {
    return OPERATOR_CANCELLED;
  }
  animviz_motionpath_compute_range(ob, scene);

  /* set up path data for bones being calculated */
  CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones_from_active_object) {
    animviz_verify_motionpaths(op->reports, scene, ob, pchan);
  }
  CTX_DATA_END;

  /* Calculate the bones that now have motion-paths. */
  /* TODO: only make for the selected bones? */
  ED_pose_recalculate_paths(C, scene, ob, POSE_PATH_CALC_RANGE_FULL);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  return OPERATOR_FINISHED;
}

void POSE_OT_paths_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Bone Paths";
  ot->idname = "POSE_OT_paths_update";
  ot->description = "Recalculate paths for bones that already have them";

  /* API callbacks. */
  ot->exec = pose_update_paths_exec;
  ot->poll = pose_update_paths_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --------- */

/* for the object with pose/action: clear path curves for selected bones only */
static void pose_clear_paths(Object *ob, bool only_selected)
{
  bool skipped = false;

  if (ELEM(nullptr, ob, ob->pose)) {
    return;
  }

  /* free the motionpath blocks for all bones - This is easier for users to quickly clear all */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
    if (pchan->mpath) {
      if ((only_selected == false) || (pchan->flag & POSE_SELECTED)) {
        animviz_free_motionpath(pchan->mpath);
        pchan->mpath = nullptr;
      }
      else {
        skipped = true;
      }
    }
  }

  /* if nothing was skipped, there should be no paths left! */
  if (skipped == false) {
    ob->pose->avs.path_bakeflag &= ~MOTIONPATH_BAKE_HAS_PATHS;
  }

  /* tag armature object for copy-on-eval - so removed paths don't still show */
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
}

/* Operator callback - wrapper for the back-end function. */
static wmOperatorStatus pose_clear_paths_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  bool only_selected = RNA_boolean_get(op->ptr, "only_selected");

  /* only continue if there's an object */
  if (ELEM(nullptr, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* use the backend function for this */
  pose_clear_paths(ob, only_selected);

  /* notifiers for updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  return OPERATOR_FINISHED;
}

static std::string pose_clear_paths_get_description(bContext * /*C*/,
                                                    wmOperatorType * /*ot*/,
                                                    PointerRNA *ptr)
{
  const bool only_selected = RNA_boolean_get(ptr, "only_selected");
  if (only_selected) {
    return TIP_("Clear motion paths of selected bones");
  }
  return TIP_("Clear motion paths of all bones");
}

void POSE_OT_paths_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Bone Paths";
  ot->idname = "POSE_OT_paths_clear";

  /* API callbacks. */
  ot->exec = pose_clear_paths_exec;
  ot->poll = ED_operator_posemode_exclusive;
  ot->get_description = pose_clear_paths_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(ot->srna,
                             "only_selected",
                             false,
                             "Only Selected",
                             "Only clear motion paths of selected bones");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/* --------- */

static wmOperatorStatus pose_update_paths_range_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));

  if (ELEM(nullptr, scene, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* use Preview Range or Full Frame Range - whichever is in use */
  ob->pose->avs.path_sf = PSFRA;
  ob->pose->avs.path_ef = PEFRA;

  /* tag for updates */
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

  return OPERATOR_FINISHED;
}

void POSE_OT_paths_range_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Range from Scene";
  ot->idname = "POSE_OT_paths_range_update";
  ot->description = "Update frame range for motion paths from the Scene's current frame range";

  /* callbacks */
  ot->exec = pose_update_paths_range_exec;
  ot->poll = ED_operator_posemode_exclusive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */

static wmOperatorStatus pose_flip_names_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  const bool do_strip_numbers = RNA_boolean_get(op->ptr, "do_strip_numbers");

  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    ListBase bones_names = {nullptr};

    FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob, pchan) {
      BLI_addtail(&bones_names, BLI_genericNodeN(pchan->name));
    }
    FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

    ED_armature_bones_flip_names(bmain, arm, &bones_names, do_strip_numbers);

    BLI_freelistN(&bones_names);

    /* since we renamed stuff... */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

    /* NOTE: notifier might evolve. */
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  }
  FOREACH_OBJECT_IN_MODE_END;

  return OPERATOR_FINISHED;
}

void POSE_OT_flip_names(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Names";
  ot->idname = "POSE_OT_flip_names";
  ot->description = "Flips (and corrects) the axis suffixes of the names of selected bones";

  /* API callbacks. */
  ot->exec = pose_flip_names_exec;
  ot->poll = ED_operator_posemode_local;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "do_strip_numbers",
                  false,
                  "Strip Numbers",
                  "Try to remove right-most dot-number from flipped names.\n"
                  "Warning: May result in incoherent naming in some cases");
}

/* ------------------ */

static wmOperatorStatus pose_autoside_names_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  char newname[MAXBONENAME];
  short axis = RNA_enum_get(op->ptr, "axis");
  Object *ob_prev = nullptr;

  /* loop through selected bones, auto-naming them */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    STRNCPY_UTF8(newname, pchan->name);
    if (bone_autoside_name(newname, 1, axis, pchan->bone->head[axis], pchan->bone->tail[axis])) {
      ED_armature_bone_rename(bmain, arm, pchan->name, newname);
    }

    if (ob_prev != ob) {
      /* since we renamed stuff... */
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

      /* NOTE: notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
      ob_prev = ob;
    }
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

void POSE_OT_autoside_names(wmOperatorType *ot)
{
  static const EnumPropertyItem axis_items[] = {
      {0, "XAXIS", 0, "X-Axis", "Left/Right"},
      {1, "YAXIS", 0, "Y-Axis", "Front/Back"},
      {2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Auto-Name by Axis";
  ot->idname = "POSE_OT_autoside_names";
  ot->description =
      "Automatically renames the selected bones according to which side of the target axis they "
      "fall on";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_autoside_names_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* settings */
  ot->prop = RNA_def_enum(ot->srna, "axis", axis_items, 0, "Axis", "Axis to tag names with");
}

/* ********************************************** */

static wmOperatorStatus pose_bone_rotmode_exec(bContext *C, wmOperator *op)
{
  const int mode = RNA_enum_get(op->ptr, "type");
  Object *prev_ob = nullptr;

  /* Set rotation mode of selected bones. */
  CTX_DATA_BEGIN_WITH_ID (C, bPoseChannel *, pchan, selected_pose_bones, Object *, ob) {
    /* use API Method for conversions... */
    BKE_rotMode_change_values(
        pchan->quat, pchan->eul, pchan->rotAxis, &pchan->rotAngle, pchan->rotmode, short(mode));

    /* finally, set the new rotation type */
    pchan->rotmode = mode;

    if (prev_ob != ob) {
      /* Notifiers and updates. */
      DEG_id_tag_update(reinterpret_cast<ID *>(ob), ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
      prev_ob = ob;
    }
  }
  CTX_DATA_END;

  return OPERATOR_FINISHED;
}

void POSE_OT_rotation_mode_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Rotation Mode";
  ot->idname = "POSE_OT_rotation_mode_set";
  ot->description = "Set the rotation representation used by selected bones";

  /* callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = pose_bone_rotmode_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_object_rotation_mode_items, 0, "Rotation Mode", "");
}

/* ********************************************** */
/* Show/Hide Bones */

/* active object is armature in posemode, poll checked */
static wmOperatorStatus pose_hide_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_object_pose_array_get_unique(scene, view_layer, CTX_wm_view3d(C));
  bool changed_multi = false;

  const int hide_select = !RNA_boolean_get(op->ptr, "unselected");

  for (Object *ob_iter : objects) {
    bool changed = false;
    bArmature *arm = static_cast<bArmature *>(ob_iter->data);
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob_iter->pose->chanbase) {
      if (!ANIM_bone_in_visible_collection(arm, pchan->bone)) {
        continue;
      }
      if (((pchan->flag & POSE_SELECTED) != 0) != hide_select) {
        continue;
      }
      pchan->drawflag |= PCHAN_DRAW_HIDDEN;
      pchan->flag &= ~POSE_SELECTED;
      changed = true;
    }

    if (changed) {
      changed_multi = true;
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob_iter);
      DEG_id_tag_update(&arm->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void POSE_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->idname = "POSE_OT_hide";
  ot->description = "Tag selected bones to not be visible in Pose Mode";

  /* API callbacks. */
  ot->exec = pose_hide_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "unselected", false, "Unselected", "");
}

/* active object is armature in posemode, poll checked */
static wmOperatorStatus pose_reveal_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_object_pose_array_get_unique(scene, view_layer, CTX_wm_view3d(C));
  bool changed_multi = false;
  const bool select = RNA_boolean_get(op->ptr, "select");

  for (Object *ob_iter : objects) {
    bArmature *arm = static_cast<bArmature *>(ob_iter->data);

    bool changed = false;
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob_iter->pose->chanbase) {
      if (!ANIM_bone_in_visible_collection(arm, pchan->bone)) {
        continue;
      }
      if ((pchan->drawflag & PCHAN_DRAW_HIDDEN) == 0) {
        continue;
      }
      if (!(pchan->bone->flag & BONE_UNSELECTABLE)) {
        SET_FLAG_FROM_TEST(pchan->flag, select, POSE_SELECTED);
      }
      pchan->drawflag &= ~PCHAN_DRAW_HIDDEN;
      changed = true;
    }

    if (changed) {
      changed_multi = true;
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob_iter);
      DEG_id_tag_update(&arm->id, ID_RECALC_PARAMETERS);
    }
  }

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void POSE_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Selected";
  ot->idname = "POSE_OT_reveal";
  ot->description = "Reveal all bones hidden in Pose Mode";

  /* API callbacks. */
  ot->exec = pose_reveal_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/* -------------------------------------------------------------------- */
/** \name Flip Quaternions
 * \{ */

static wmOperatorStatus pose_flip_quats_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  bool changed_multi = false;

  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter) {
    bool changed = false;
    /* loop through all selected pchans, flipping and keying (as needed) */
    FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob_iter, pchan) {
      /* only if bone is using quaternion rotation */
      if (pchan->rotmode == ROT_MODE_QUAT) {
        changed = true;
        /* quaternions have 720 degree range */
        negate_v4(pchan->quat);

        blender::animrig::autokeyframe_pose_channel(
            C, scene, ob_iter, pchan, {{"rotation_quaternion"}}, false);
      }
    }
    FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

    if (changed) {
      changed_multi = true;
      /* notifiers and updates */
      DEG_id_tag_update(&ob_iter->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob_iter);
    }
  }
  FOREACH_OBJECT_IN_MODE_END;

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void POSE_OT_quaternions_flip(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Flip Quaternions";
  ot->idname = "POSE_OT_quaternions_flip";
  ot->description =
      "Flip quaternion values to achieve desired rotations, while maintaining the same "
      "orientations";

  /* callbacks */
  ot->exec = pose_flip_quats_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
