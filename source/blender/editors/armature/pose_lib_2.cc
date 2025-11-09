/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include <cstring>

#include "AS_asset_representation.hh"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"
#include "BKE_pose_backup.h"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_keyframing.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_armature.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"
#include "ANIM_pose.hh"
#include "ANIM_rna.hh"

#include "armature_intern.hh"

enum ePoseBlendState {
  POSE_BLEND_INIT,
  POSE_BLEND_BLENDING,
  POSE_BLEND_ORIGINAL,
  POSE_BLEND_CONFIRM,
  POSE_BLEND_CANCEL,
};

struct PoseBlendData {
  ePoseBlendState state;
  bool needs_redraw;

  struct {
    bool use_release_confirm;
    int init_event_type;

  } release_confirm_info;

  /* For temp-loading the Action from the pose library. */
  AssetTempIDConsumer *temp_id_consumer;

  /* Blend factor for interpolating between current and given pose.
   * 1.0 means "100% pose asset". Negative values and values > 1.0 will be used as-is, and can
   * cause interesting effects. */
  float blend_factor;
  bool is_flipped;
  PoseBackup *pose_backup;

  blender::Vector<Object *> objects; /* Objects to work on. */
  bAction *act;                      /* Pose to blend into the current pose. */
  bAction *act_flipped;              /* Flipped copy of `act`. */

  Scene *scene;  /* For auto-keying. */
  ScrArea *area; /* For drawing status text. */

  tSlider *slider; /* Slider UI and event handling. */

  /** Info-text to print in header. */
  char headerstr[UI_MAX_DRAW_STR];
};

/**
 * Return the bAction that should be blended.
 * This is either `pbd->act` or `pbd->act_flipped`, depending on `is_flipped`.
 */
static bAction *poselib_action_to_blend(PoseBlendData *pbd)
{
  return pbd->is_flipped ? pbd->act_flipped : pbd->act;
}

/* Makes a copy of the current pose for restoration purposes - doesn't do constraints currently */
static void poselib_backup_posecopy(PoseBlendData *pbd)
{
  bAction *action = poselib_action_to_blend(pbd);
  pbd->pose_backup = BKE_pose_backup_create_selected_bones(pbd->objects, action);

  if (pbd->state == POSE_BLEND_INIT) {
    /* Ready for blending now. */
    pbd->state = POSE_BLEND_BLENDING;
  }
}

/* ---------------------------- */

/* Auto-key/tag bones affected by the pose Action. */
static void poselib_keytag_pose(bContext *C, Scene *scene, PoseBlendData *pbd)
{
  for (Object *ob : pbd->objects) {
    if (!blender::animrig::autokeyframe_cfra_can_key(scene, &ob->id)) {
      return;
    }

    AnimData *adt = BKE_animdata_from_id(&ob->id);
    if (adt != nullptr && adt->action != nullptr &&
        !BKE_id_is_editable(CTX_data_main(C), &adt->action->id))
    {
      /* Changes to linked-in Actions are not allowed. */
      return;
    }

    bPose *pose = ob->pose;
    bAction *act = poselib_action_to_blend(pbd);
    const bArmature *armature = static_cast<const bArmature *>(ob->data);

    blender::animrig::Slot &slot = blender::animrig::get_best_pose_slot_for_id(ob->id,
                                                                               act->wrap());

    /* Storing which pose bones were already keyed since multiple FCurves will probably exist per
     * pose bone. */
    blender::Set<bPoseChannel *> keyed_pose_bones;
    auto autokey_pose_bones = [&](FCurve * /* fcu */, const char *bone_name) {
      bPoseChannel *pchan = BKE_pose_channel_find_name(pose, bone_name);
      if (!pchan) {
        /* This bone cannot be found any more. This is fine, this can happen
         * when F-Curves for a bone are included in a pose asset, and later the
         * bone itself was renamed or removed. */
        return;
      }
      if (BKE_pose_backup_is_selection_relevant(pbd->pose_backup) &&
          !blender::animrig::bone_is_selected(armature, pchan))
      {
        return;
      }
      if (keyed_pose_bones.contains(pchan)) {
        return;
      }
      /* This mimics the Whole Character Keying Set that was used here previously. In the future we
       * could only key rna paths of FCurves that are actually in the applied pose. */
      PointerRNA pose_bone_pointer = RNA_pointer_create_discrete(&ob->id, &RNA_PoseBone, pchan);
      blender::Vector<RNAPath> rna_paths = blender::animrig::get_keyable_id_property_paths(
          pose_bone_pointer);
      rna_paths.append({"location"});
      const blender::StringRef rotation_mode_path = blender::animrig::get_rotation_mode_path(
          eRotationModes(pchan->rotmode));
      rna_paths.append({rotation_mode_path});
      rna_paths.append({"scale"});
      blender::animrig::autokeyframe_pose_channel(C, scene, ob, pchan, rna_paths, 0);
      keyed_pose_bones.add(pchan);
    };
    blender::bke::BKE_action_find_fcurves_with_bones(act, slot.handle, autokey_pose_bones);
  }

  /* send notifiers for this */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

/* Apply the relevant changes to the pose */
static void poselib_blend_apply(bContext *C, wmOperator *op)
{
  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);

  if (!pbd->needs_redraw) {
    return;
  }
  pbd->needs_redraw = false;

  BKE_pose_backup_restore(pbd->pose_backup);

  /* The pose needs updating, whether it's for restoring the original pose or for showing the
   * result of the blend. */
  for (Object *ob : pbd->objects) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  }

  if (pbd->state != POSE_BLEND_BLENDING) {
    return;
  }

  /* Perform the actual blending. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph, 0.0f);
  blender::animrig::Action &pose_action = poselib_action_to_blend(pbd)->wrap();
  if (pose_action.slot_array_num == 0) {
    return;
  }

  blender::animrig::pose_apply_action(
      pbd->objects, pose_action, &anim_eval_context, pbd->blend_factor);
}

/* ---------------------------- */

static void poselib_blend_set_factor(PoseBlendData *pbd, const float new_factor)
{
  pbd->blend_factor = new_factor;
  pbd->needs_redraw = true;
}

static void poselib_toggle_flipped(PoseBlendData *pbd)
{
  /* The pose will toggle between flipped and normal. This means the pose
   * backup has to change, as it only contains the bones for one side. */
  BKE_pose_backup_restore(pbd->pose_backup);
  BKE_pose_backup_free(pbd->pose_backup);

  pbd->is_flipped = !pbd->is_flipped;
  pbd->needs_redraw = true;

  poselib_backup_posecopy(pbd);
}

/* Return operator return value. */
static wmOperatorStatus poselib_blend_handle_event(bContext * /*C*/,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);

  ED_slider_modal(pbd->slider, event);
  const float factor = ED_slider_factor_get(pbd->slider);
  poselib_blend_set_factor(pbd, factor);

  if (event->type == MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* Handle the release confirm event directly, it has priority over others. */
  if (pbd->release_confirm_info.use_release_confirm &&
      (event->type == pbd->release_confirm_info.init_event_type) && (event->val == KM_RELEASE))
  {
    pbd->state = POSE_BLEND_CONFIRM;
    return OPERATOR_RUNNING_MODAL;
  }

  /* Ctrl manages the 'flipped' state. It works as a toggle so if the operator started in flipped
   * mode, pressing it will unflip the pose. */
  if (ELEM(event->val, KM_PRESS, KM_RELEASE) &&
      ELEM(event->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY))
  {
    poselib_toggle_flipped(pbd);
  }

  /* only accept 'press' event, and ignore 'release', so that we don't get double actions */
  if (ELEM(event->val, KM_PRESS, KM_NOTHING) == 0) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* NORMAL EVENT HANDLING... */
  /* searching takes priority over normal activity */
  switch (event->type) {
    /* Exit - cancel. */
    case EVT_ESCKEY:
    case RIGHTMOUSE:
      pbd->state = POSE_BLEND_CANCEL;
      break;

    /* Exit - confirm. */
    case LEFTMOUSE:
    case EVT_RETKEY:
    case EVT_PADENTER:
    case EVT_SPACEKEY:
      pbd->state = POSE_BLEND_CONFIRM;
      break;

    /* TODO(Sybren): toggle between original pose and poselib pose. */
    case EVT_TABKEY:
      pbd->state = pbd->state == POSE_BLEND_BLENDING ? POSE_BLEND_ORIGINAL : POSE_BLEND_BLENDING;
      pbd->needs_redraw = true;
      break;
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* ---------------------------- */

static blender::Vector<Object *> get_poselib_objects(bContext &C)
{
  blender::Vector<PointerRNA> selected_objects;
  CTX_data_selected_objects(&C, &selected_objects);

  blender::Vector<Object *> selected_pose_objects;
  for (const PointerRNA &ptr : selected_objects) {
    Object *object = reinterpret_cast<Object *>(ptr.owner_id);
    if (!object || !object->pose) {
      continue;
    }
    selected_pose_objects.append(object);
  }

  Object *active_object = CTX_data_active_object(&C);
  /* The active object may not be selected, it should be added because you can still switch to pose
   * mode. */
  if (active_object && active_object->pose && !selected_pose_objects.contains(active_object)) {
    selected_pose_objects.append(active_object);
  }
  return selected_pose_objects;
}

static void poselib_tempload_exit(PoseBlendData *pbd)
{
  using namespace blender::ed;
  asset::temp_id_consumer_free(&pbd->temp_id_consumer);
}

static bAction *poselib_blend_init_get_action(bContext *C, wmOperator *op)
{
  using namespace blender::ed;

  const blender::asset_system::AssetRepresentation *asset = nullptr;

  if (asset::operator_asset_reference_props_is_set(*op->ptr)) {
    asset = asset::operator_asset_reference_props_get_asset_from_all_library(
        *C, *op->ptr, op->reports);
    if (!asset) {
      /* Explicit asset reference passed, but cannot be found. Error out. */
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Asset not found: '%s'",
                  RNA_string_get(op->ptr, "relative_asset_identifier").c_str());
      return nullptr;
    }
  }
  else {
    /* If no explicit asset reference was passed, get asset from context. */
    asset = CTX_wm_asset(C);
    if (!asset) {
      BKE_report(op->reports, RPT_ERROR, "No asset in context");
      return nullptr;
    }
  }

  if (asset->get_id_type() != ID_AC) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Asset ('%s') is not an action data-block",
                asset->get_name().c_str());
    return nullptr;
  }

  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);

  pbd->temp_id_consumer = asset::temp_id_consumer_create(asset);
  return reinterpret_cast<bAction *>(asset::temp_id_consumer_ensure_local_id(
      pbd->temp_id_consumer, ID_AC, CTX_data_main(C), op->reports));
}

static bAction *flip_pose(bContext *C, blender::Span<Object *> objects, bAction *action)
{
  bAction *action_copy = reinterpret_cast<bAction *>(
      BKE_id_copy_ex(nullptr, &action->id, nullptr, LIB_ID_COPY_LOCALIZE));

  /* Lock the window manager while flipping the pose. Flipping requires temporarily modifying the
   * pose, which can cause unwanted visual glitches. */
  wmWindowManager *wm = CTX_wm_manager(C);
  const bool interface_was_locked = CTX_wm_interface_locked(C);
  WM_locked_interface_set(wm, true);

  BKE_action_flip_with_pose(action_copy, objects);

  WM_locked_interface_set(wm, interface_was_locked);
  return action_copy;
}

/* Return true on success, false if the context isn't suitable. */
static bool poselib_blend_init_data(bContext *C, wmOperator *op, const wmEvent *event)
{
  op->customdata = nullptr;

  /* check if valid poselib */
  blender::Vector<Object *> selected_pose_objects = get_poselib_objects(*C);
  if (selected_pose_objects.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "Pose lib is only for armatures in pose mode");
    return false;
  }

  /* Set up blend state info. */
  PoseBlendData *pbd;
  op->customdata = pbd = MEM_new<PoseBlendData>("PoseLib Preview Data");

  pbd->act = poselib_blend_init_get_action(C, op);
  if (pbd->act == nullptr) {
    /* No report here. The poll function cannot check if the operator properties have an asset
     * reference to determine the asset to operate on, in which case we fallback to getting the
     * asset from context. */
    return false;
  }
  if (pbd->act->wrap().slots().size() == 0) {
    BKE_report(op->reports, RPT_ERROR, "This pose asset is empty, and thus has no pose");
    return false;
  }

  pbd->is_flipped = RNA_struct_property_is_set(op->ptr, "flipped") ?
                        RNA_boolean_get(op->ptr, "flipped") :
                        (event && (event->modifier & KM_CTRL));
  pbd->blend_factor = RNA_float_get(op->ptr, "blend_factor");

  /* Only construct the flipped pose if there is a chance it's actually needed. */
  const bool is_interactive = (event != nullptr);
  if (is_interactive || pbd->is_flipped) {
    pbd->act_flipped = flip_pose(C, selected_pose_objects, pbd->act);
  }

  /* Get the basic data. */
  pbd->objects = selected_pose_objects;

  pbd->scene = CTX_data_scene(C);
  pbd->area = CTX_wm_area(C);

  pbd->state = POSE_BLEND_INIT;
  pbd->needs_redraw = true;

  /* Just to avoid a clang-analyzer warning (false positive), it's set properly below. */
  pbd->release_confirm_info.use_release_confirm = false;

  /* Release confirm data. Only available if there's an event to work with. */
  if (is_interactive) {
    PropertyRNA *release_confirm_prop = RNA_struct_find_property(op->ptr, "release_confirm");
    if (release_confirm_prop && RNA_property_is_set(op->ptr, release_confirm_prop)) {
      pbd->release_confirm_info.use_release_confirm = RNA_property_boolean_get(
          op->ptr, release_confirm_prop);
    }
    else {
      pbd->release_confirm_info.use_release_confirm = event->val != KM_RELEASE;
    }

    pbd->slider = ED_slider_create(C);
    ED_slider_init(pbd->slider, event);
    ED_slider_factor_set(pbd->slider, pbd->blend_factor);
    ED_slider_allow_overshoot_set(pbd->slider, true, true);
    ED_slider_allow_increments_set(pbd->slider, false);
    ED_slider_factor_bounds_set(pbd->slider, -1, 1);
  }

  if (pbd->release_confirm_info.use_release_confirm) {
    BLI_assert(is_interactive);
    pbd->release_confirm_info.init_event_type = WM_userdef_event_type_from_keymap_type(
        event->type);
  }

  /* Make backups for blending and restoring the pose. */
  poselib_backup_posecopy(pbd);

  /* Set pose flags to ensure the depsgraph evaluation doesn't overwrite it. */
  for (Object *ob : selected_pose_objects) {
    ob->pose->flag &= ~POSE_DO_UNLOCK;
    ob->pose->flag |= POSE_LOCKED;
  }

  return true;
}

static void poselib_blend_cleanup(bContext *C, wmOperator *op)
{
  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);
  wmWindow *win = CTX_wm_window(C);

  /* Redraw the header so that it doesn't show any of our stuff anymore. */
  ED_area_status_text(pbd->area, nullptr);
  ED_workspace_status_text(C, nullptr);

  if (pbd->slider) {
    ED_slider_destroy(C, pbd->slider);
  }

  /* This signals the depsgraph to unlock and reevaluate the pose on the next evaluation. */
  for (Object *ob : pbd->objects) {
    bPose *pose = ob->pose;
    pose->flag |= POSE_DO_UNLOCK;
  }

  switch (pbd->state) {
    case POSE_BLEND_CONFIRM: {
      Scene *scene = pbd->scene;
      poselib_keytag_pose(C, scene, pbd);

      /* Ensure the redo panel has the actually-used value, instead of the initial value. */
      RNA_float_set(op->ptr, "blend_factor", pbd->blend_factor);
      RNA_boolean_set(op->ptr, "flipped", pbd->is_flipped);
      break;
    }

    case POSE_BLEND_INIT:
    case POSE_BLEND_BLENDING:
    case POSE_BLEND_ORIGINAL:
      /* Cleanup should not be called directly from these states. */
      BLI_assert_msg(0, "poselib_blend_cleanup: unexpected pose blend state");
      BKE_report(op->reports, RPT_ERROR, "Internal pose library error, canceling operator");
      ATTR_FALLTHROUGH;
    case POSE_BLEND_CANCEL:
      BKE_pose_backup_restore(pbd->pose_backup);
      break;
  }

  for (Object *ob : pbd->objects) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  }
  /* Update mouse-hover highlights. */
  WM_event_add_mousemove(win);
}

static void poselib_blend_free(wmOperator *op)
{
  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);
  if (pbd == nullptr) {
    return;
  }

  if (pbd->act_flipped) {
    BKE_id_free(nullptr, pbd->act_flipped);
  }
  poselib_tempload_exit(pbd);

  /* Free temp data for operator */
  BKE_pose_backup_free(pbd->pose_backup);
  pbd->pose_backup = nullptr;

  MEM_delete(pbd);
}

static wmOperatorStatus poselib_blend_exit(bContext *C, wmOperator *op)
{
  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);
  const ePoseBlendState exit_state = pbd->state;

  poselib_blend_cleanup(C, op);
  poselib_blend_free(op);

  wmWindow *win = CTX_wm_window(C);
  WM_cursor_modal_restore(win);

  if (exit_state == POSE_BLEND_CANCEL) {
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

/* Cancel previewing operation (called when exiting Blender) */
static void poselib_blend_cancel(bContext *C, wmOperator *op)
{
  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);
  pbd->state = POSE_BLEND_CANCEL;
  poselib_blend_exit(C, op);
}

/* Main modal status check. */
static wmOperatorStatus poselib_blend_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  const wmOperatorStatus operator_result = poselib_blend_handle_event(C, op, event);

  const PoseBlendData *pbd = static_cast<const PoseBlendData *>(op->customdata);
  if (ELEM(pbd->state, POSE_BLEND_CONFIRM, POSE_BLEND_CANCEL)) {
    return poselib_blend_exit(C, op);
  }

  if (pbd->needs_redraw) {

    WorkspaceStatus status(C);

    if (pbd->state == POSE_BLEND_BLENDING) {
      status.item(IFACE_("Show Original Pose"), ICON_EVENT_TAB);
    }
    else {
      status.item(IFACE_("Show Blended Pose"), ICON_EVENT_TAB);
    }

    ED_slider_status_get(pbd->slider, status);

    status.item_bool(IFACE_("Flip Pose"), pbd->is_flipped, ICON_EVENT_CTRL);

    poselib_blend_apply(C, op);
  }

  return operator_result;
}

static wmOperatorStatus poselib_apply_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!poselib_blend_init_data(C, op, event)) {
    poselib_blend_free(op);
    return OPERATOR_CANCELLED;
  }

  poselib_blend_apply(C, op);

  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);
  pbd->state = POSE_BLEND_CONFIRM;
  return poselib_blend_exit(C, op);
}

static wmOperatorStatus poselib_apply_exec(bContext *C, wmOperator *op)
{
  return poselib_apply_invoke(C, op, nullptr);
}

/* Modal Operator init. */
static wmOperatorStatus poselib_blend_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!poselib_blend_init_data(C, op, event)) {
    poselib_blend_free(op);
    return OPERATOR_CANCELLED;
  }

  wmWindow *win = CTX_wm_window(C);
  WM_cursor_modal_set(win, WM_CURSOR_EW_SCROLL);

  /* Do initial apply to have something to look at. */
  poselib_blend_apply(C, op);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/* Single-shot apply. */
static wmOperatorStatus poselib_blend_exec(bContext *C, wmOperator *op)
{
  return poselib_apply_invoke(C, op, nullptr);
}

/* Poll callback for operators that require existing PoseLib data (with poses) to work. */
static bool poselib_blend_poll(bContext *C)
{
  blender::Span<Object *> selected_pose_objects = get_poselib_objects(*C);
  if (selected_pose_objects.is_empty()) {
    /* Pose lib is only for armatures in pose mode. */
    return false;
  }

  return true;
}

/* Operator properties can set an asset reference to determine the asset to operate on (the pose
 * can then be applied via shortcut too, for example). If this isn't set, an active asset from
 * context is queried. */
void POSELIB_OT_apply_pose_asset(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers: */
  ot->name = "Apply Pose Asset";
  ot->idname = "POSELIB_OT_apply_pose_asset";
  ot->description = "Apply the given Pose Action to the rig";

  /* Callbacks: */
  ot->invoke = poselib_apply_invoke;
  ot->exec = poselib_apply_exec;
  ot->poll = poselib_blend_poll;

  /* Flags: */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties: */
  blender::ed::asset::operator_asset_reference_props_register(*ot->srna);
  RNA_def_float_factor(ot->srna,
                       "blend_factor",
                       1.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Blend Factor",
                       "Amount that the pose is applied on top of the existing poses. A negative "
                       "value will subtract the pose instead of adding it",
                       -1.0f,
                       1.0f);
  prop = RNA_def_boolean(ot->srna,
                         "flipped",
                         false,
                         "Apply Flipped",
                         "When enabled, applies the pose flipped over the X-axis");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* See comment on #POSELIB_OT_apply_pose_asset. */
void POSELIB_OT_blend_pose_asset(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers: */
  ot->name = "Blend Pose Asset";
  ot->idname = "POSELIB_OT_blend_pose_asset";
  ot->description = "Blend the given Pose Action to the rig";

  /* Callbacks: */
  ot->invoke = poselib_blend_invoke;
  ot->modal = poselib_blend_modal;
  ot->cancel = poselib_blend_cancel;
  ot->exec = poselib_blend_exec;
  ot->poll = poselib_blend_poll;

  /* Flags: */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  /* Properties: */
  blender::ed::asset::operator_asset_reference_props_register(*ot->srna);
  prop = RNA_def_float_factor(ot->srna,
                              "blend_factor",
                              0.0f,
                              -FLT_MAX,
                              FLT_MAX,
                              "Blend Factor",
                              "Amount that the pose is applied on top of the existing poses. A "
                              "negative value will subtract the pose instead of adding it",
                              -1.0f,
                              1.0f);
  /* Blending should always start at 0%, and not at whatever percentage was last used. This RNA
   * property just exists for symmetry with the Apply operator (and thus simplicity of the rest of
   * the code, which can assume this property exists). */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "flipped",
                         false,
                         "Apply Flipped",
                         "When enabled, applies the pose flipped over the X-axis");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "release_confirm",
                         false,
                         "Confirm on Release",
                         "Always confirm operation when releasing button");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
