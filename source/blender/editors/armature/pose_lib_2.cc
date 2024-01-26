/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include <cmath>
#include <cstring>

#include "AS_asset_representation.hh"

#include "MEM_guardedalloc.h"

#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"
#include "BKE_pose_backup.h"
#include "BKE_report.h"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "ED_asset.hh"
#include "ED_keyframing.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "ANIM_bone_collections.hh"
#include "ANIM_keyframing.hh"

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

  Object *ob;           /* Object to work on. */
  bAction *act;         /* Pose to blend into the current pose. */
  bAction *act_flipped; /* Flipped copy of `act`. */

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
  const bAction *action = poselib_action_to_blend(pbd);
  pbd->pose_backup = BKE_pose_backup_create_selected_bones(pbd->ob, action);

  if (pbd->state == POSE_BLEND_INIT) {
    /* Ready for blending now. */
    pbd->state = POSE_BLEND_BLENDING;
  }
}

/* ---------------------------- */

/* Auto-key/tag bones affected by the pose Action. */
static void poselib_keytag_pose(bContext *C, Scene *scene, PoseBlendData *pbd)
{
  if (!blender::animrig::autokeyframe_cfra_can_key(scene, &pbd->ob->id)) {
    return;
  }

  AnimData *adt = BKE_animdata_from_id(&pbd->ob->id);
  if (adt != nullptr && adt->action != nullptr &&
      !BKE_id_is_editable(CTX_data_main(C), &adt->action->id))
  {
    /* Changes to linked-in Actions are not allowed. */
    return;
  }

  bPose *pose = pbd->ob->pose;
  bAction *act = poselib_action_to_blend(pbd);

  KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_WHOLE_CHARACTER_ID);
  blender::Vector<PointerRNA> sources;

  /* start tagging/keying */
  const bArmature *armature = static_cast<const bArmature *>(pbd->ob->data);
  LISTBASE_FOREACH (bActionGroup *, agrp, &act->groups) {
    /* Only for selected bones unless there aren't any selected, in which case all are included. */
    bPoseChannel *pchan = BKE_pose_channel_find_name(pose, agrp->name);
    if (pchan == nullptr) {
      continue;
    }

    if (BKE_pose_backup_is_selection_relevant(pbd->pose_backup) &&
        !PBONE_SELECTED(armature, pchan->bone))
    {
      continue;
    }

    /* Add data-source override for the PoseChannel, to be used later. */
    ANIM_relative_keyingset_add_source(sources, &pbd->ob->id, &RNA_PoseBone, pchan);
  }

  /* Perform actual auto-keying. */
  ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, float(scene->r.cfra));

  /* send notifiers for this */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

/* Apply the relevant changes to the pose */
static void poselib_blend_apply(bContext *C, wmOperator *op)
{
  PoseBlendData *pbd = (PoseBlendData *)op->customdata;

  if (!pbd->needs_redraw) {
    return;
  }
  pbd->needs_redraw = false;

  BKE_pose_backup_restore(pbd->pose_backup);

  /* The pose needs updating, whether it's for restoring the original pose or for showing the
   * result of the blend. */
  DEG_id_tag_update(&pbd->ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, pbd->ob);

  if (pbd->state != POSE_BLEND_BLENDING) {
    return;
  }

  /* Perform the actual blending. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph, 0.0f);
  bAction *to_blend = poselib_action_to_blend(pbd);
  BKE_pose_apply_action_blend(pbd->ob, to_blend, &anim_eval_context, pbd->blend_factor);
}

/* ---------------------------- */

static void poselib_blend_set_factor(PoseBlendData *pbd, const float new_factor)
{
  pbd->blend_factor = new_factor;
  pbd->needs_redraw = true;
}

static void poselib_set_flipped(PoseBlendData *pbd, const bool new_flipped)
{
  if (pbd->is_flipped == new_flipped) {
    return;
  }

  /* The pose will toggle between flipped and normal. This means the pose
   * backup has to change, as it only contains the bones for one side. */
  BKE_pose_backup_restore(pbd->pose_backup);
  BKE_pose_backup_free(pbd->pose_backup);

  pbd->is_flipped = new_flipped;
  pbd->needs_redraw = true;

  poselib_backup_posecopy(pbd);
}

/* Return operator return value. */
static int poselib_blend_handle_event(bContext * /*C*/, wmOperator *op, const wmEvent *event)
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

  /* Ctrl manages the 'flipped' state. */
  poselib_set_flipped(pbd, event->modifier & KM_CTRL);

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
  }

  return OPERATOR_RUNNING_MODAL;
}

/* ---------------------------- */

static Object *get_poselib_object(bContext *C)
{
  if (C == nullptr) {
    return nullptr;
  }
  return BKE_object_pose_armature_get(CTX_data_active_object(C));
}

static void poselib_tempload_exit(PoseBlendData *pbd)
{
  using namespace blender::ed;
  asset::temp_id_consumer_free(&pbd->temp_id_consumer);
}

static bAction *poselib_blend_init_get_action(bContext *C, wmOperator *op)
{
  using namespace blender::ed;
  const AssetRepresentationHandle *asset = CTX_wm_asset(C);

  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);

  pbd->temp_id_consumer = asset::temp_id_consumer_create(asset);
  return (bAction *)asset::temp_id_consumer_ensure_local_id(
      pbd->temp_id_consumer, ID_AC, CTX_data_main(C), op->reports);
}

static bAction *flip_pose(bContext *C, Object *ob, bAction *action)
{
  bAction *action_copy = (bAction *)BKE_id_copy_ex(
      nullptr, &action->id, nullptr, LIB_ID_COPY_LOCALIZE);

  /* Lock the window manager while flipping the pose. Flipping requires temporarily modifying the
   * pose, which can cause unwanted visual glitches. */
  wmWindowManager *wm = CTX_wm_manager(C);
  const bool interface_was_locked = CTX_wm_interface_locked(C);
  WM_set_locked_interface(wm, true);

  BKE_action_flip_with_pose(action_copy, ob);

  WM_set_locked_interface(wm, interface_was_locked);
  return action_copy;
}

/* Return true on success, false if the context isn't suitable. */
static bool poselib_blend_init_data(bContext *C, wmOperator *op, const wmEvent *event)
{
  op->customdata = nullptr;

  /* check if valid poselib */
  Object *ob = get_poselib_object(C);
  if (ELEM(nullptr, ob, ob->pose, ob->data)) {
    BKE_report(op->reports, RPT_ERROR, "Pose lib is only for armatures in pose mode");
    return false;
  }

  /* Set up blend state info. */
  PoseBlendData *pbd;
  op->customdata = pbd = static_cast<PoseBlendData *>(
      MEM_callocN(sizeof(PoseBlendData), "PoseLib Preview Data"));

  pbd->act = poselib_blend_init_get_action(C, op);
  if (pbd->act == nullptr) {
    return false;
  }

  pbd->is_flipped = RNA_boolean_get(op->ptr, "flipped");
  pbd->blend_factor = RNA_float_get(op->ptr, "blend_factor");

  /* Only construct the flipped pose if there is a chance it's actually needed. */
  const bool is_interactive = (event != nullptr);
  if (is_interactive || pbd->is_flipped) {
    pbd->act_flipped = flip_pose(C, ob, pbd->act);
  }

  /* Get the basic data. */
  pbd->ob = ob;
  pbd->ob->pose = ob->pose;

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
  pbd->ob->pose->flag &= ~POSE_DO_UNLOCK;
  pbd->ob->pose->flag |= POSE_LOCKED;

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
  bPose *pose = pbd->ob->pose;
  pose->flag |= POSE_DO_UNLOCK;

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

  DEG_id_tag_update(&pbd->ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, pbd->ob);
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

  MEM_SAFE_FREE(op->customdata);
}

static int poselib_blend_exit(bContext *C, wmOperator *op)
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
static int poselib_blend_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int operator_result = poselib_blend_handle_event(C, op, event);

  const PoseBlendData *pbd = static_cast<const PoseBlendData *>(op->customdata);
  if (ELEM(pbd->state, POSE_BLEND_CONFIRM, POSE_BLEND_CANCEL)) {
    return poselib_blend_exit(C, op);
  }

  if (pbd->needs_redraw) {
    char status_string[UI_MAX_DRAW_STR];
    char slider_string[UI_MAX_DRAW_STR];
    char tab_string[50];

    ED_slider_status_string_get(pbd->slider, slider_string, sizeof(slider_string));

    if (pbd->state == POSE_BLEND_BLENDING) {
      STRNCPY(tab_string, RPT_("[Tab] - Show original pose"));
    }
    else {
      STRNCPY(tab_string, RPT_("[Tab] - Show blended pose"));
    }

    SNPRINTF(status_string, "%s | %s | [Ctrl] - Flip Pose", tab_string, slider_string);
    ED_workspace_status_text(C, status_string);

    poselib_blend_apply(C, op);
  }

  return operator_result;
}

/* Modal Operator init. */
static int poselib_blend_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
static int poselib_blend_exec(bContext *C, wmOperator *op)
{
  if (!poselib_blend_init_data(C, op, nullptr)) {
    poselib_blend_free(op);
    return OPERATOR_CANCELLED;
  }

  poselib_blend_apply(C, op);

  PoseBlendData *pbd = static_cast<PoseBlendData *>(op->customdata);
  pbd->state = POSE_BLEND_CONFIRM;
  return poselib_blend_exit(C, op);
}

static bool poselib_asset_in_context(bContext *C)
{
  /* Check whether the context provides the asset data needed to add a pose. */
  const AssetRepresentationHandle *asset = CTX_wm_asset(C);
  return asset && (asset->get_id_type() == ID_AC);
}

/* Poll callback for operators that require existing PoseLib data (with poses) to work. */
static bool poselib_blend_poll(bContext *C)
{
  Object *ob = get_poselib_object(C);
  if (ELEM(nullptr, ob, ob->pose, ob->data)) {
    /* Pose lib is only for armatures in pose mode. */
    return false;
  }

  return poselib_asset_in_context(C);
}

void POSELIB_OT_apply_pose_asset(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers: */
  ot->name = "Apply Pose Asset";
  ot->idname = "POSELIB_OT_apply_pose_asset";
  ot->description = "Apply the given Pose Action to the rig";

  /* Callbacks: */
  ot->exec = poselib_blend_exec;
  ot->poll = poselib_blend_poll;

  /* Flags: */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties: */
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
