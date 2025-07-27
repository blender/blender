/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Eyedropper (bones)
 */

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "BLI_assert.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_armature.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_view2d.hh"

#include "eyedropper_intern.hh"
#include "interface_intern.hh"

namespace blender::ui {

enum class SampleResult {
  UNKNOWN_FAILURE,
  SUCCESS,
  NO_BONE_3DVIEW,
  NO_BONE_OUTLINER,
  WRONG_AREA,
  WRONG_ARMATURE,
};

struct BoneDropper {
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  PointerRNA search_ptr = {};
  PropertyRNA *search_prop = nullptr;

  bool is_undo = false;

  ScrArea *cursor_area = nullptr; /* Area under the cursor. */
  ARegionType *area_region_type = nullptr;
  void *draw_handle_pixel = nullptr;
  int name_pos[2] = {};
  char name[64] = {};
};

struct BoneSampleData {
  SampleResult sample_result = SampleResult::UNKNOWN_FAILURE;
  /* Either EditBone, bPoseChannel or Bone. */
  PointerRNA bone_rna;
  char *name = nullptr;
};

static void datadropper_draw_cb(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  BoneDropper *ddr = static_cast<BoneDropper *>(arg);
  eyedropper_draw_cursor_text_region(ddr->name_pos, ddr->name);
}

static bool is_bone_dropper_valid(BoneDropper *bone_dropper)
{
  if ((bone_dropper->ptr.data == nullptr) || (bone_dropper->prop == nullptr)) {
    return false;
  }
  if (!RNA_property_editable(&bone_dropper->ptr, bone_dropper->prop)) {
    return false;
  }

  PointerRNA owner_ptr = RNA_id_pointer_create(bone_dropper->search_ptr.owner_id);
  if (RNA_type_to_ID_code(owner_ptr.type) != ID_AR) {
    return false;
  }

  return true;
}

static int bonedropper_init(bContext *C, wmOperator *op)
{
  int index_dummy;
  PointerRNA button_ptr;
  PropertyRNA *button_prop;
  uiBut *button = UI_context_active_but_prop_get(C, &button_ptr, &button_prop, &index_dummy);

  if (!button || button->type != ButType::SearchMenu) {
    return false;
  }

  BoneDropper *bone_dropper = MEM_new<BoneDropper>(__func__);
  uiButSearch *search_button = (uiButSearch *)button;
  bone_dropper->ptr = button_ptr;
  bone_dropper->prop = button_prop;
  bone_dropper->search_ptr = search_button->rnasearchpoin;
  bone_dropper->search_prop = search_button->rnasearchprop;
  if (!is_bone_dropper_valid(bone_dropper)) {
    MEM_delete(bone_dropper);
    return false;
  }

  op->customdata = bone_dropper;

  bone_dropper->is_undo = UI_but_flag_is_set(button, UI_BUT_UNDO);

  SpaceType *space_type = BKE_spacetype_from_id(SPACE_VIEW3D);
  ARegionType *area_region_type = BKE_regiontype_from_id(space_type, RGN_TYPE_WINDOW);
  bone_dropper->cursor_area = CTX_wm_area(C);
  bone_dropper->area_region_type = area_region_type;
  bone_dropper->draw_handle_pixel = ED_region_draw_cb_activate(
      area_region_type, datadropper_draw_cb, bone_dropper, REGION_DRAW_POST_PIXEL);

  return true;
}

static void bonedropper_exit(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  WM_cursor_modal_restore(win);

  if (op->customdata) {
    BoneDropper *bdr = (BoneDropper *)op->customdata;
    op->customdata = nullptr;

    if (bdr->area_region_type) {
      ED_region_draw_cb_exit(bdr->area_region_type, bdr->draw_handle_pixel);
    }
    ED_area_tag_redraw(bdr->cursor_area);

    MEM_delete(bdr);
  }
  WM_event_add_mousemove(win);
}

static void bonedropper_cancel(bContext *C, wmOperator *op)
{
  bonedropper_exit(C, op);
}

/* To switch the draw callback when region under mouse event changes */
static void bonedropper_set_draw_callback_region(ScrArea &area, BoneDropper &bdr)
{
  if (area.spacetype == bdr.cursor_area->spacetype) {
    return;
  }

  /* If the spacetype changed remove the old callback. */
  ED_region_draw_cb_exit(bdr.area_region_type, bdr.draw_handle_pixel);

  ARegion *region = BKE_area_find_region_type(bdr.cursor_area, RGN_TYPE_WINDOW);
  ED_region_tag_redraw(region);

  /* Set draw callback in new region. */
  ARegionType *art = BKE_regiontype_from_id(area.type, RGN_TYPE_WINDOW);

  bdr.cursor_area = &area;
  bdr.area_region_type = art;
  bdr.draw_handle_pixel = ED_region_draw_cb_activate(
      art, datadropper_draw_cb, &bdr, REGION_DRAW_POST_PIXEL);
}

static BoneSampleData sample_data_from_3d_view(bContext *C,
                                               const int mval[2],
                                               const BoneDropper &bdr)
{
  Base *base = nullptr;

  switch (CTX_data_mode_enum(C)) {
    case CTX_MODE_POSE: {
      bPoseChannel *bone = ED_armature_pick_pchan(C, mval, true, &base);
      if (!bone || !base) {
        return {SampleResult::NO_BONE_3DVIEW};
      }
      Object *ob = base->object;
      bArmature *armature = (bArmature *)ob->data;
      if (!armature || &armature->id != bdr.search_ptr.owner_id) {
        return {SampleResult::WRONG_ARMATURE};
      }

      BoneSampleData sample_data;
      sample_data.name = bone->name;
      /* Not using the search pointer owner ID because pose bones are part of the object. */
      sample_data.bone_rna = RNA_pointer_create_discrete(&base->object->id, &RNA_PoseBone, bone);
      sample_data.sample_result = SampleResult::SUCCESS;
      return sample_data;
    }

    case CTX_MODE_EDIT_ARMATURE: {
      EditBone *ebone = ED_armature_pick_ebone(C, mval, true, &base);
      if (!ebone || !base) {
        return {SampleResult::NO_BONE_3DVIEW};
      }
      Object *ob = base->object;
      bArmature *armature = (bArmature *)ob->data;
      if (!armature || &armature->id != bdr.search_ptr.owner_id) {
        return {SampleResult::WRONG_ARMATURE};
      }

      BoneSampleData sample_data;
      sample_data.name = ebone->name;
      sample_data.bone_rna = RNA_pointer_create_discrete(&armature->id, &RNA_EditBone, ebone);
      sample_data.sample_result = SampleResult::SUCCESS;
      return sample_data;
    }

    default:
      return {SampleResult::NO_BONE_3DVIEW};
  }
}

static BoneSampleData sample_data_from_outliner(bContext *C,
                                                const int mval[2],
                                                const BoneDropper &bdr)
{
  BoneSampleData sample_data;

  const bool success = ED_outliner_give_rna_under_cursor(C, mval, &sample_data.bone_rna);
  if (!success) {
    sample_data.sample_result = SampleResult::NO_BONE_OUTLINER;
    return sample_data;
  }
  ID *bone_id = sample_data.bone_rna.owner_id;
  ID *search_id = bdr.search_ptr.owner_id;

  /* By comparing the ID of the RNA returned by the outliner with the ID we are searching in, we
   * can determine if the Bone is for the correct armature. */
  if (sample_data.bone_rna.type == &RNA_Bone) {
    if (bone_id != search_id) {
      sample_data.sample_result = SampleResult::WRONG_ARMATURE;
      return sample_data;
    }
    Bone *bone = (Bone *)sample_data.bone_rna.data;
    sample_data.name = bone->name;
    sample_data.sample_result = SampleResult::SUCCESS;
    return sample_data;
  }

  if (sample_data.bone_rna.type == &RNA_EditBone) {
    if (bone_id != search_id) {
      sample_data.sample_result = SampleResult::WRONG_ARMATURE;
      return sample_data;
    }
    EditBone *bone = (EditBone *)sample_data.bone_rna.data;
    sample_data.name = bone->name;
    sample_data.sample_result = SampleResult::SUCCESS;
    return sample_data;
  }

  if (sample_data.bone_rna.type == &RNA_PoseBone) {
    bPoseChannel *pose_bone = (bPoseChannel *)sample_data.bone_rna.data;
    /* Special case for pose bones. Because they are not stored in the Armature, the IDs of the
     * search property and the picked result might not match since the comparison would be between
     * armature and object. */
    if (bdr.search_ptr.type == &RNA_Object) {
      if (bone_id != search_id) {
        sample_data.sample_result = SampleResult::WRONG_ARMATURE;
        return sample_data;
      }
    }
    /* If looking for an armature, get the Armature object and follow the data pointer. */
    if (bdr.search_ptr.type == &RNA_Armature) {
      /* Expecting Pose Bones to be stored on the object. */
      BLI_assert(GS(sample_data.bone_rna.owner_id->name) == ID_OB);
      Object *armature_object = (Object *)sample_data.bone_rna.owner_id;
      if (armature_object->data != bdr.search_ptr.owner_id) {
        sample_data.sample_result = SampleResult::WRONG_ARMATURE;
        return sample_data;
      }
    }
    sample_data.name = pose_bone->name;
    sample_data.sample_result = SampleResult::SUCCESS;
    return sample_data;
  }

  sample_data.sample_result = SampleResult::NO_BONE_OUTLINER;
  return sample_data;
}

static BoneSampleData bonedropper_sample_pt(
    bContext *C, wmWindow &win, ScrArea &area, BoneDropper &bdr, const int event_xy[2])
{
  if (!ELEM(area.spacetype, SPACE_VIEW3D, SPACE_OUTLINER)) {
    return {};
  }

  ARegion *region = BKE_area_find_region_xy(&area, RGN_TYPE_WINDOW, event_xy);

  if (!region) {
    return {};
  }

  wmWindow *win_prev = CTX_wm_window(C);
  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *region_prev = CTX_wm_region(C);

  const int mval[2] = {event_xy[0] - region->winrct.xmin, event_xy[1] - region->winrct.ymin};

  CTX_wm_window_set(C, &win);
  CTX_wm_area_set(C, &area);
  CTX_wm_region_set(C, region);

  /* Unfortunately it's necessary to always draw else we leave stale text. */
  ED_region_tag_redraw(region);

  BoneSampleData sample_data;
  switch (area.spacetype) {
    case SPACE_VIEW3D: {
      sample_data = sample_data_from_3d_view(C, mval, bdr);
      break;
    }
    case SPACE_OUTLINER: {
      sample_data = sample_data_from_outliner(C, mval, bdr);
      break;
    }

    default:
      BLI_assert_unreachable();
      break;
  }

  if (sample_data.name) {
    STRNCPY_UTF8(bdr.name, sample_data.name);
    copy_v2_v2_int(bdr.name_pos, mval);
  }

  CTX_wm_window_set(C, win_prev);
  CTX_wm_area_set(C, area_prev);
  CTX_wm_region_set(C, region_prev);

  return sample_data;
}

static SampleResult bonedropper_sample(bContext *C, BoneDropper &bdr, const int event_xy[2])
{
  int event_xy_win[2];
  wmWindow *win = nullptr;
  ScrArea *area = nullptr;
  eyedropper_win_area_find(C, event_xy, event_xy_win, &win, &area);

  if (!win || !area) {
    return SampleResult::WRONG_AREA;
  }
  if (!ELEM(area->spacetype, SPACE_VIEW3D, SPACE_OUTLINER)) {
    return SampleResult::WRONG_AREA;
  }

  BoneSampleData sample_data = bonedropper_sample_pt(C, *win, *area, bdr, event_xy_win);
  if (!sample_data.name) {
    return sample_data.sample_result;
  }

  StructRNA *search_type = RNA_property_pointer_type(&bdr.search_ptr, bdr.search_prop);
  /* In case we are searching for a bone, convert the pointer from bPoseChannel. */
  if (search_type == &RNA_Bone && sample_data.bone_rna.type == &RNA_PoseBone &&
      bdr.search_ptr.type == &RNA_Armature)
  {
    /* We are searching for something in the armature but got a pose bone on the object, so we
     * need to do a conversion. We will just assume the ID under the cursor is the one we are
     * searching for since there is no way to get the armature ID from the object ID that we
     * have. */
    bPoseChannel *pose_bone = (bPoseChannel *)sample_data.bone_rna.data;
    sample_data.bone_rna = RNA_pointer_create_discrete(
        bdr.search_ptr.owner_id, &RNA_Bone, pose_bone->bone);
  }

  PropertyType type = RNA_property_type(bdr.prop);
  switch (type) {
    case PROP_STRING:
      RNA_property_string_set(&bdr.ptr, bdr.prop, sample_data.name);
      break;
    case PROP_POINTER:
      RNA_property_pointer_set(&bdr.ptr, bdr.prop, sample_data.bone_rna, CTX_wm_reports(C));
      break;

    default:
      BLI_assert_unreachable();
      break;
  }

  RNA_property_update(C, &bdr.ptr, bdr.prop);

  return SampleResult::SUCCESS;
}

static void generate_sample_warning(SampleResult result, wmOperator *op)
{
  switch (result) {
    case SampleResult::UNKNOWN_FAILURE:
      BKE_report(op->reports, RPT_WARNING, "Picking a bone failed");
      break;
    case SampleResult::WRONG_ARMATURE:
      BKE_report(
          op->reports, RPT_WARNING, "Picked bone does not belong to the already chosen armature");
      break;

    case SampleResult::NO_BONE_3DVIEW:
      BKE_report(op->reports,
                 RPT_WARNING,
                 "Selection is not a bone. Armature needs to be in Pose Mode or Edit Mode "
                 "to pick in the 3D Viewport");
      break;

    case SampleResult::NO_BONE_OUTLINER:
      BKE_report(op->reports, RPT_WARNING, "Selection is not a bone");
      break;

    case SampleResult::WRONG_AREA:
      BKE_report(op->reports, RPT_WARNING, "Can only pick from the 3D viewport or the outliner");
      break;

    case SampleResult::SUCCESS:
      break;
  }
}

static wmOperatorStatus bonedropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  BoneDropper *bdr = (BoneDropper *)op->customdata;
  if (!bdr) {
    return OPERATOR_CANCELLED;
  }

  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        bonedropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = bdr->is_undo;
        const SampleResult result = bonedropper_sample(C, *bdr, event->xy);
        bonedropper_exit(C, op);
        if (result == SampleResult::SUCCESS) {
          /* Could support finished & undo-skip. */
          return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
        }
        generate_sample_warning(result, op);
      }
    }
  }
  else if (event->type == MOUSEMOVE) {
    bdr->name[0] = '\0';
    int event_xy_win[2];
    wmWindow *win = nullptr;
    ScrArea *area = nullptr;
    eyedropper_win_area_find(C, event->xy, event_xy_win, &win, &area);

    if (win && area) {
      /* Set the region for eyedropper cursor text drawing */
      bonedropper_set_draw_callback_region(*area, *bdr);
      bonedropper_sample_pt(C, *win, *area, *bdr, event->xy);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}
static wmOperatorStatus bonedropper_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* This is needed to ensure viewport picking works. */
  BKE_object_update_select_id(CTX_data_main(C));

  if (bonedropper_init(C, op)) {
    wmWindow *win = CTX_wm_window(C);
    /* Workaround for de-activating the button clearing the cursor, see #76794 */
    UI_context_active_but_clear(C, win, CTX_wm_region(C));
    WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus bonedropper_exec(bContext *C, wmOperator *op)
{
  if (bonedropper_init(C, op)) {
    bonedropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool bonedropper_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index_dummy;

  if (CTX_wm_window(C) == nullptr) {
    return false;
  }

  const Object *active_object = CTX_data_active_object(C);

  if (!active_object || active_object->type != OB_ARMATURE) {
    CTX_wm_operator_poll_msg_set(C, "The active object needs to be an armature");
    return false;
  }

  if (!ELEM(active_object->mode, OB_MODE_POSE, OB_MODE_EDIT)) {
    CTX_wm_operator_poll_msg_set(C, "The armature needs to be in Pose mode or Edit mode");
    return false;
  }

  uiBut *but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy);

  if (!but) {
    return false;
  }

  if (but->type != ButType::SearchMenu || !(but->flag & UI_BUT_VALUE_CLEAR)) {
    return false;
  }

  uiButSearch *search_but = (uiButSearch *)but;

  if (!ELEM(RNA_property_type(prop), PROP_STRING, PROP_POINTER)) {
    return false;
  }

  const StructRNA *type = RNA_property_pointer_type(&search_but->rnasearchpoin,
                                                    search_but->rnasearchprop);

  return type == &RNA_Bone || type == &RNA_EditBone;
}

void UI_OT_eyedropper_bone(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Eyedropper Bone";
  ot->idname = "UI_OT_eyedropper_bone";
  ot->description = "Sample a bone from the 3D View or the Outliner to store in a property";

  /* API callbacks. */
  ot->invoke = bonedropper_invoke;
  ot->modal = bonedropper_modal;
  ot->cancel = bonedropper_cancel;
  ot->exec = bonedropper_exec;
  ot->poll = bonedropper_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;
}

}  // namespace blender::ui
