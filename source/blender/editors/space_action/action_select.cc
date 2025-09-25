/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_lasso_2d.hh"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_nla.hh"

#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_keyframes_edit.hh"
#include "ED_keyframes_keylist.hh"
#include "ED_markers.hh"
#include "ED_mask.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "action_intern.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Keyframes Stuff
 * \{ */

static bAnimListElem *actkeys_find_list_element_at_position(bAnimContext *ac,
                                                            eAnimFilter_Flags filter,
                                                            float region_x,
                                                            float region_y)
{
  View2D *v2d = &ac->region->v2d;

  float view_x, view_y;
  int channel_index;
  UI_view2d_region_to_view(v2d, region_x, region_y, &view_x, &view_y);
  UI_view2d_listview_view_to_cell(0,
                                  ANIM_UI_get_channel_step(),
                                  0,
                                  ANIM_UI_get_first_channel_top(v2d),
                                  view_x,
                                  view_y,
                                  nullptr,
                                  &channel_index);

  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  bAnimListElem *ale = static_cast<bAnimListElem *>(BLI_findlink(&anim_data, channel_index));
  if (ale != nullptr) {
    BLI_remlink(&anim_data, ale);
    ale->next = ale->prev = nullptr;
  }
  ANIM_animdata_freelist(&anim_data);

  return ale;
}

static void actkeys_list_element_to_keylist(bAnimContext *ac,
                                            AnimKeylist *keylist,
                                            bAnimListElem *ale)
{
  bDopeSheet *ads = nullptr;
  if (ELEM(ac->datatype, ANIMCONT_DOPESHEET, ANIMCONT_TIMELINE)) {
    ads = static_cast<bDopeSheet *>(ac->data);
  }

  blender::float2 range = {ac->region->v2d.cur.xmin, ac->region->v2d.cur.xmax};

  if (ale->key_data) {
    switch (ale->datatype) {
      case ALE_SCE: {
        Scene *scene = (Scene *)ale->key_data;
        scene_to_keylist(ads, scene, keylist, 0, range);
        break;
      }
      case ALE_OB: {
        Object *ob = (Object *)ale->key_data;
        ob_to_keylist(ads, ob, keylist, 0, range);
        break;
      }
      case ALE_ACTION_LAYERED: {
        /* This is only called for action summaries in the Dope-sheet, *not* the
         * Action Editor. Therefore despite the name `ALE_ACTION_LAYERED`, this
         * is only used to show a *single slot* of the action: the slot used by
         * the ID the action is listed under.
         *
         * Thus we use the same function as the `ALE_ACTION_SLOT` case below
         * because in practice the only distinction between these cases is where
         * they get the slot from. In this case, we get it from `elem`'s ADT. */
        animrig::Action *action = static_cast<animrig::Action *>(ale->key_data);
        BLI_assert(action);
        BLI_assert(ale->adt);
        action_slot_summary_to_keylist(
            ac, ale->id, *action, ale->adt->slot_handle, keylist, 0, range);
        break;
      }
      case ALE_ACTION_SLOT: {
        animrig::Action *action = static_cast<animrig::Action *>(ale->key_data);
        animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
        BLI_assert(action);
        BLI_assert(slot);
        action_slot_summary_to_keylist(ac, ale->id, *action, slot->handle, keylist, 0, range);
        break;
      }
      case ALE_ACT: {
        /* Legacy action. */
        bAction *act = (bAction *)ale->key_data;
        action_to_keylist(ale->adt, act, keylist, 0, range);
        break;
      }
      case ALE_FCURVE: {
        FCurve *fcu = (FCurve *)ale->key_data;
        fcurve_to_keylist(ale->adt, fcu, keylist, 0, range, ANIM_nla_mapping_allowed(ale));
        break;
      }
      case ALE_NONE:
      case ALE_GPFRAME:
      case ALE_MASKLAY:
      case ALE_NLASTRIP:
      case ALE_ALL:
      case ALE_GROUP:
      case ALE_GREASE_PENCIL_CEL:
      case ALE_GREASE_PENCIL_DATA:
      case ALE_GREASE_PENCIL_GROUP:
        break;
    }
  }
  else if (ale->type == ANIMTYPE_SUMMARY) {
    /* Dope-sheet summary covers everything. */
    summary_to_keylist(ac, keylist, 0, range);
  }
  else if (ale->type == ANIMTYPE_GROUP) {
    /* TODO: why don't we just give groups key_data too? */
    bActionGroup *agrp = (bActionGroup *)ale->data;
    action_group_to_keylist(ale->adt, agrp, keylist, 0, range);
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
    /* TODO: why don't we just give grease pencil layers key_data too? */
    grease_pencil_cels_to_keylist(
        ale->adt, static_cast<const GreasePencilLayer *>(ale->data), keylist, 0);
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER_GROUP) {
    /* TODO: why don't we just give grease pencil layers key_data too? */
    grease_pencil_layer_group_to_keylist(
        ale->adt, static_cast<const GreasePencilLayerTreeGroup *>(ale->data), keylist, 0);
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_DATABLOCK) {
    /* TODO: why don't we just give grease pencil layers key_data too? */
    grease_pencil_data_block_to_keylist(
        ale->adt, static_cast<const GreasePencil *>(ale->data), keylist, 0, false);
  }
  else if (ale->type == ANIMTYPE_GPLAYER) {
    /* TODO: why don't we just give gplayers key_data too? */
    bGPDlayer *gpl = (bGPDlayer *)ale->data;
    gpl_to_keylist(ads, gpl, keylist);
  }
  else if (ale->type == ANIMTYPE_MASKLAYER) {
    /* TODO: why don't we just give masklayers key_data too? */
    MaskLayer *masklay = (MaskLayer *)ale->data;
    mask_to_keylist(ads, masklay, keylist);
  }
}

static void actkeys_find_key_in_list_element(bAnimContext *ac,
                                             bAnimListElem *ale,
                                             float region_x,
                                             float *r_selx,
                                             float *r_frame,
                                             bool *r_found,
                                             bool *r_is_selected)
{
  *r_found = false;

  View2D *v2d = &ac->region->v2d;

  AnimKeylist *keylist = ED_keylist_create();
  actkeys_list_element_to_keylist(ac, keylist, ale);
  ED_keylist_prepare_for_direct_access(keylist);

  /* standard channel height (to allow for some slop) */
  float key_hsize = ANIM_UI_get_channel_height() * 0.8f;
  /* half-size (for either side), but rounded up to nearest int (for easier targeting) */
  key_hsize = roundf(key_hsize / 2.0f);

  const Bounds<float> range = {
      UI_view2d_region_to_view_x(v2d, region_x - int(key_hsize)),
      UI_view2d_region_to_view_x(v2d, region_x + int(key_hsize)),
  };
  const ActKeyColumn *ak = ED_keylist_find_any_between(keylist, range);
  if (ak) {

    /* set the frame to use, and apply inverse-correction for NLA-mapping
     * so that the frame will get selected by the selection functions without
     * requiring to map each frame once again...
     */
    *r_selx = ANIM_nla_tweakedit_remap(ale, ak->cfra, NLATIME_CONVERT_UNMAP);
    *r_frame = ak->cfra;
    *r_found = true;
    *r_is_selected = (ak->sel & SELECT) != 0;
  }

  /* cleanup temporary lists */
  ED_keylist_free(keylist);
}

static void actkeys_find_key_at_position(bAnimContext *ac,
                                         eAnimFilter_Flags filter,
                                         float region_x,
                                         float region_y,
                                         bAnimListElem **r_ale,
                                         float *r_selx,
                                         float *r_frame,
                                         bool *r_found,
                                         bool *r_is_selected)

{
  *r_found = false;
  *r_ale = actkeys_find_list_element_at_position(ac, filter, region_x, region_y);

  if (*r_ale != nullptr) {
    actkeys_find_key_in_list_element(
        ac, *r_ale, region_x, r_selx, r_frame, r_found, r_is_selected);
  }
}

static bool actkeys_is_key_at_position(bAnimContext *ac, float region_x, float region_y)
{
  bAnimListElem *ale;
  float selx, frame;
  bool found;
  bool is_selected;

  eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                             ANIMFILTER_LIST_CHANNELS;
  actkeys_find_key_at_position(
      ac, filter, region_x, region_y, &ale, &selx, &frame, &found, &is_selected);

  if (ale != nullptr) {
    MEM_freeN(ale);
  }
  return found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deselect All Operator
 *
 * This operator works in one of three ways:
 * 1) (de)select all (AKEY) - test if select all or deselect all.
 * 2) invert all (CTRL-IKEY) - invert selection of all keyframes.
 * 3) (de)select all - no testing is done; only for use internal tools as normal function.
 * \{ */

/* Deselects keyframes in the action editor
 * - This is called by the deselect all operator, as well as other ones!
 *
 * - test: check if select or deselect all
 * - sel: how to select keyframes (SELECT_*)
 */
static void deselect_action_keys(bAnimContext *ac, short test, eEditKeyframes_Select sel)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditData ked = {{nullptr}};
  KeyframeEditFunc test_cb, sel_cb;

  /* determine type-based settings */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);

  /* filter data */
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* init BezTriple looping data */
  test_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);

  /* See if we should be selecting or deselecting */
  if (test) {
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ale->type == ANIMTYPE_GPLAYER) {
        if (ED_gpencil_layer_frame_select_check(static_cast<bGPDlayer *>(ale->data))) {
          sel = SELECT_SUBTRACT;
          break;
        }
      }
      else if (ale->type == ANIMTYPE_MASKLAYER) {
        if (ED_masklayer_frame_select_check(static_cast<MaskLayer *>(ale->data))) {
          sel = SELECT_SUBTRACT;
          break;
        }
      }
      else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
        if (blender::ed::greasepencil::has_any_frame_selected(
                static_cast<GreasePencilLayer *>(ale->data)->wrap()))
        {
          sel = SELECT_SUBTRACT;
        }
        break;
      }
      else {
        if (ANIM_fcurve_keyframes_loop(
                &ked, static_cast<FCurve *>(ale->key_data), nullptr, test_cb, nullptr))
        {
          sel = SELECT_SUBTRACT;
          break;
        }
      }
    }
  }

  /* convert sel to selectmode, and use that to get editor */
  sel_cb = ANIM_editkeyframes_select(sel);

  /* Now set the flags */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ale->type == ANIMTYPE_GPLAYER) {
      ED_gpencil_layer_frame_select_set(static_cast<bGPDlayer *>(ale->data), sel);
      ale->update |= ANIM_UPDATE_DEPS;
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      ED_masklayer_frame_select_set(static_cast<MaskLayer *>(ale->data), sel);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      blender::ed::greasepencil::select_all_frames(
          static_cast<GreasePencilLayer *>(ale->data)->wrap(), sel);
      ale->update |= ANIM_UPDATE_DEPS;
    }
    else {
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), nullptr, sel_cb, nullptr);
    }
  }

  /* Cleanup */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_deselectall_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* 'standard' behavior - check if selected, then apply relevant selection */
  const int action = RNA_enum_get(op->ptr, "action");
  switch (action) {
    case SEL_TOGGLE:
      deselect_action_keys(&ac, 1, SELECT_ADD);
      break;
    case SEL_SELECT:
      deselect_action_keys(&ac, 0, SELECT_ADD);
      break;
    case SEL_DESELECT:
      deselect_action_keys(&ac, 0, SELECT_SUBTRACT);
      break;
    case SEL_INVERT:
      deselect_action_keys(&ac, 0, SELECT_INVERT);
      break;
    default:
      BLI_assert(0);
      break;
  }

  /* set notifier that keyframe selection have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(eAnimCont_Types(ac.datatype)))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select All";
  ot->idname = "ACTION_OT_select_all";
  ot->description = "Toggle selection of all keyframes";

  /* API callbacks. */
  ot->exec = actkeys_deselectall_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 *
 * This operator currently works in one of three ways:
 * - BKEY     - 1) all keyframes within region are selected #ACTKEYS_BORDERSEL_ALLKEYS.
 * - ALT-BKEY - depending on which axis of the region was larger...
 *   - 2) x-axis, so select all frames within frame range #ACTKEYS_BORDERSEL_FRAMERANGE.
 *   - 3) y-axis, so select all frames within channels that region included
 *     #ACTKEYS_BORDERSEL_CHANNELS.
 * \{ */

/* defines for box_select mode */
enum {
  ACTKEYS_BORDERSEL_ALLKEYS = 0,
  ACTKEYS_BORDERSEL_FRAMERANGE,
  ACTKEYS_BORDERSEL_CHANNELS,
} /*eActKeys_BoxSelect_Mode*/;

struct BoxSelectData {
  bAnimContext *ac;
  short selectmode;

  KeyframeEditData ked;
  KeyframeEditFunc ok_cb, select_cb;
};

static void box_select_elem(
    BoxSelectData *sel_data, bAnimListElem *ale, float xmin, float xmax, bool summary)
{
  bAnimContext *ac = sel_data->ac;

  switch (ale->type) {
    case ANIMTYPE_GREASE_PENCIL_DATABLOCK: {
      GreasePencil *grease_pencil = static_cast<GreasePencil *>(ale->data);
      for (blender::bke::greasepencil::Layer *layer : grease_pencil->layers_for_write()) {
        blender::ed::greasepencil::select_frames_range(
            layer->wrap().as_node(), xmin, xmax, sel_data->selectmode);
      }
      ale->update |= ANIM_UPDATE_DEPS;
      break;
    }
    case ANIMTYPE_GREASE_PENCIL_LAYER_GROUP:
    case ANIMTYPE_GREASE_PENCIL_LAYER:
      blender::ed::greasepencil::select_frames_range(
          static_cast<GreasePencilLayerTreeNode *>(ale->data)->wrap(),
          xmin,
          xmax,
          sel_data->selectmode);
      ale->update |= ANIM_UPDATE_DEPS;
      break;
    case ANIMTYPE_GPLAYER: {
      ED_gpencil_layer_frames_select_box(
          static_cast<bGPDlayer *>(ale->data), xmin, xmax, sel_data->selectmode);
      ale->update |= ANIM_UPDATE_DEPS;
      break;
    }
    case ANIMTYPE_MASKDATABLOCK: {
      Mask *mask = static_cast<Mask *>(ale->data);
      MaskLayer *masklay;
      for (masklay = static_cast<MaskLayer *>(mask->masklayers.first); masklay;
           masklay = masklay->next)
      {
        ED_masklayer_frames_select_box(masklay, xmin, xmax, sel_data->selectmode);
      }
      break;
    }
    case ANIMTYPE_MASKLAYER: {
      ED_masklayer_frames_select_box(
          static_cast<MaskLayer *>(ale->data), xmin, xmax, sel_data->selectmode);
      break;
    }
    default: {
      if (summary) {
        break;
      }

      if (ale->type == ANIMTYPE_SUMMARY) {
        ListBase anim_data = {nullptr, nullptr};
        ANIM_animdata_filter(
            ac, &anim_data, ANIMFILTER_DATA_VISIBLE, ac->data, eAnimCont_Types(ac->datatype));

        LISTBASE_FOREACH (bAnimListElem *, ale2, &anim_data) {
          box_select_elem(sel_data, ale2, xmin, xmax, true);
        }

        ANIM_animdata_update(ac, &anim_data);
        ANIM_animdata_freelist(&anim_data);
      }

      if (!ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
        ANIM_animchannel_keyframes_loop(
            &sel_data->ked, ac->ads, ale, sel_data->ok_cb, sel_data->select_cb, nullptr);
      }
    }
  }
}

static void box_select_action(bAnimContext *ac,
                              const rcti rect,
                              short mode,
                              const eEditKeyframes_Select selectmode)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  eAnimFilter_Flags filter;

  BoxSelectData sel_data{};
  sel_data.ac = ac;
  sel_data.selectmode = selectmode;

  View2D *v2d = &ac->region->v2d;
  rctf rectf;

  /* Convert mouse coordinates to frame ranges and channel
   * coordinates corrected for view pan/zoom. */
  UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin + 2, &rectf.xmin, &rectf.ymin);
  UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax - 2, &rectf.xmax, &rectf.ymax);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Get beztriple editing/validation functions. */
  sel_data.select_cb = ANIM_editkeyframes_select(selectmode);

  if (ELEM(mode, ACTKEYS_BORDERSEL_FRAMERANGE, ACTKEYS_BORDERSEL_ALLKEYS)) {
    sel_data.ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
  }
  else {
    sel_data.ok_cb = nullptr;
  }

  /* init editing data */
  memset(&sel_data.ked, 0, sizeof(KeyframeEditData));

  float ymax = ANIM_UI_get_first_channel_top(v2d);
  const float channel_step = ANIM_UI_get_channel_step();

  /* loop over data, doing box select */
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
       ale = ale->next, ymax -= channel_step)
  {
    /* get new vertical minimum extent of channel */
    float ymin = ymax - channel_step;

    /* set horizontal range (if applicable) */
    if (ELEM(mode, ACTKEYS_BORDERSEL_FRAMERANGE, ACTKEYS_BORDERSEL_ALLKEYS)) {
      /* if channel is mapped in NLA, apply correction */
      if (ANIM_nla_mapping_allowed(ale)) {
        sel_data.ked.iterflags &= ~(KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP);
        sel_data.ked.f1 = ANIM_nla_tweakedit_remap(ale, rectf.xmin, NLATIME_CONVERT_UNMAP);
        sel_data.ked.f2 = ANIM_nla_tweakedit_remap(ale, rectf.xmax, NLATIME_CONVERT_UNMAP);
      }
      else {
        sel_data.ked.iterflags |= (KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP); /* for summary tracks */
        sel_data.ked.f1 = rectf.xmin;
        sel_data.ked.f2 = rectf.xmax;
      }
    }

    /* perform vertical suitability check (if applicable) */
    if ((mode == ACTKEYS_BORDERSEL_FRAMERANGE) || !((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
      box_select_elem(&sel_data, ale, rectf.xmin, rectf.xmax, false);
    }
  }

  /* cleanup */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_box_select_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  bool tweak = RNA_boolean_get(op->ptr, "tweak");
  if (tweak) {
    int mval[2];
    WM_event_drag_start_mval(event, ac.region, mval);
    if (actkeys_is_key_at_position(&ac, mval[0], mval[1])) {
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  return WM_gesture_box_invoke(C, op, event);
}

static wmOperatorStatus actkeys_box_select_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  rcti rect;
  short mode = 0;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const eEditKeyframes_Select selectmode = (sel_op != SEL_OP_SUB) ? SELECT_ADD : SELECT_SUBTRACT;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_action_keys(&ac, 1, SELECT_SUBTRACT);
  }

  /* get settings from operator */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* selection 'mode' depends on whether box_select region only matters on one axis */
  if (RNA_boolean_get(op->ptr, "axis_range")) {
    /* Mode depends on which axis of the range is larger to determine which axis to use:
     * - checking this in region-space is fine,
     *   as it's fundamentally still going to be a different rect size.
     * - the frame-range select option is favored over the channel one (x over y),
     *   as frame-range one is often used for tweaking timing when "blocking",
     *   while channels is not that useful...
     */
    if (BLI_rcti_size_x(&rect) >= BLI_rcti_size_y(&rect)) {
      mode = ACTKEYS_BORDERSEL_FRAMERANGE;
    }
    else {
      mode = ACTKEYS_BORDERSEL_CHANNELS;
    }
  }
  else {
    mode = ACTKEYS_BORDERSEL_ALLKEYS;
  }

  /* apply box_select action */
  box_select_action(&ac, rect, mode, selectmode);

  /* set notifier that keyframe selection have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->idname = "ACTION_OT_select_box";
  ot->description = "Select all keyframes within the specified region";

  /* API callbacks. */
  ot->invoke = actkeys_box_select_invoke;
  ot->exec = actkeys_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* rna */
  ot->prop = RNA_def_boolean(ot->srna, "axis_range", false, "Axis Range", "");

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "tweak", false, "Tweak", "Operator has been activated using a click-drag event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Select Operators
 *
 * "Region Select" operators include the Lasso and Circle Select operators.
 * These two ended up being lumped together, as it was easier in the
 * original Graph Editor implementation of these to do it this way.
 * \{ */

struct RegionSelectData {
  bAnimContext *ac;
  short mode;
  short selectmode;

  KeyframeEditData ked;
  KeyframeEditFunc ok_cb, select_cb;
};

static void region_select_elem(RegionSelectData *sel_data, bAnimListElem *ale, bool summary)
{
  bAnimContext *ac = sel_data->ac;

  switch (ale->type) {
    case ANIMTYPE_GPLAYER: {
      ED_gpencil_layer_frames_select_region(&sel_data->ked,
                                            static_cast<bGPDlayer *>(ale->data),
                                            sel_data->mode,
                                            sel_data->selectmode);
      ale->update |= ANIM_UPDATE_DEPS;
      break;
    }
    case ANIMTYPE_GREASE_PENCIL_LAYER_GROUP:
    case ANIMTYPE_GREASE_PENCIL_LAYER: {
      blender::ed::greasepencil::select_frames_region(
          &sel_data->ked,
          static_cast<GreasePencilLayerTreeNode *>(ale->data)->wrap(),
          sel_data->mode,
          sel_data->selectmode);
      ale->update |= ANIM_UPDATE_DEPS;
      break;
    }
    case ANIMTYPE_GREASE_PENCIL_DATABLOCK: {
      ListBase anim_data = {nullptr, nullptr};
      ANIM_animdata_filter(
          ac, &anim_data, ANIMFILTER_DATA_VISIBLE, ac->data, eAnimCont_Types(ac->datatype));

      LISTBASE_FOREACH (bAnimListElem *, ale2, &anim_data) {
        if ((ale2->type == ANIMTYPE_GREASE_PENCIL_LAYER) && (ale2->id == ale->data)) {
          region_select_elem(sel_data, ale2, true);
        }
      }

      ANIM_animdata_update(ac, &anim_data);
      ANIM_animdata_freelist(&anim_data);
      break;
    }
    case ANIMTYPE_MASKDATABLOCK: {
      Mask *mask = static_cast<Mask *>(ale->data);
      MaskLayer *masklay;
      for (masklay = static_cast<MaskLayer *>(mask->masklayers.first); masklay;
           masklay = masklay->next)
      {
        ED_masklayer_frames_select_region(
            &sel_data->ked, masklay, sel_data->mode, sel_data->selectmode);
      }
      break;
    }
    case ANIMTYPE_MASKLAYER: {
      ED_masklayer_frames_select_region(&sel_data->ked,
                                        static_cast<MaskLayer *>(ale->data),
                                        sel_data->mode,
                                        sel_data->selectmode);
      break;
    }
    default: {
      if (summary) {
        break;
      }

      if (ale->type == ANIMTYPE_SUMMARY) {
        ListBase anim_data = {nullptr, nullptr};
        ANIM_animdata_filter(
            ac, &anim_data, ANIMFILTER_DATA_VISIBLE, ac->data, eAnimCont_Types(ac->datatype));

        LISTBASE_FOREACH (bAnimListElem *, ale2, &anim_data) {
          region_select_elem(sel_data, ale2, true);
        }

        ANIM_animdata_update(ac, &anim_data);
        ANIM_animdata_freelist(&anim_data);
      }

      if (!ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
        ANIM_animchannel_keyframes_loop(
            &sel_data->ked, ac->ads, ale, sel_data->ok_cb, sel_data->select_cb, nullptr);
      }
    }
  }
}

static void region_select_action_keys(bAnimContext *ac,
                                      const rctf *rectf_view,
                                      short mode,
                                      eEditKeyframes_Select selectmode,
                                      void *data)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  eAnimFilter_Flags filter;

  RegionSelectData sel_data{};
  sel_data.ac = ac;
  sel_data.mode = mode;
  sel_data.selectmode = selectmode;
  View2D *v2d = &ac->region->v2d;
  rctf rectf, scaled_rectf;

  /* Convert mouse coordinates to frame ranges and channel
   * coordinates corrected for view pan/zoom. */
  UI_view2d_region_to_view_rctf(v2d, rectf_view, &rectf);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Get beztriple editing/validation functions. */
  sel_data.select_cb = ANIM_editkeyframes_select(selectmode);
  sel_data.ok_cb = ANIM_editkeyframes_ok(mode);

  /* init editing data */
  memset(&sel_data.ked, 0, sizeof(KeyframeEditData));
  if (mode == BEZT_OK_CHANNEL_LASSO) {
    KeyframeEdit_LassoData *data_lasso = static_cast<KeyframeEdit_LassoData *>(data);
    data_lasso->rectf_scaled = &scaled_rectf;
    sel_data.ked.data = data_lasso;
  }
  else if (mode == BEZT_OK_CHANNEL_CIRCLE) {
    KeyframeEdit_CircleData *data_circle = static_cast<KeyframeEdit_CircleData *>(data);
    data_circle->rectf_scaled = &scaled_rectf;
    sel_data.ked.data = data;
  }
  else {
    sel_data.ked.data = &scaled_rectf;
  }

  float ymax = ANIM_UI_get_first_channel_top(v2d);
  const float channel_step = ANIM_UI_get_channel_step();

  /* loop over data, doing region select */
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
       ale = ale->next, ymax -= channel_step)
  {
    /* get new vertical minimum extent of channel */
    const float ymin = ymax - channel_step;

    /* compute midpoint of channel (used for testing if the key is in the region or not) */
    sel_data.ked.channel_y = (ymin + ymax) / 2.0f;

    /* if channel is mapped in NLA, apply correction
     * - Apply to the bounds being checked, not all the keyframe points,
     *   to avoid having scaling everything
     * - Save result to the scaled_rect, which is all that these operators
     *   will read from
     */
    if (ANIM_nla_mapping_allowed(ale)) {
      sel_data.ked.iterflags &= ~(KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP);
      sel_data.ked.f1 = ANIM_nla_tweakedit_remap(ale, rectf.xmin, NLATIME_CONVERT_UNMAP);
      sel_data.ked.f2 = ANIM_nla_tweakedit_remap(ale, rectf.xmax, NLATIME_CONVERT_UNMAP);
    }
    else {
      sel_data.ked.iterflags |= (KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP); /* for summary tracks */
      sel_data.ked.f1 = rectf.xmin;
      sel_data.ked.f2 = rectf.xmax;
    }

    /* Update values for scaled_rectf - which is used to compute the mapping in the callbacks
     * NOTE: Since summary tracks need late-binding remapping, the callbacks may overwrite these
     *       with the properly remapped ked.f1/f2 values, when needed
     */
    scaled_rectf.xmin = sel_data.ked.f1;
    scaled_rectf.xmax = sel_data.ked.f2;
    scaled_rectf.ymin = ymin;
    scaled_rectf.ymax = ymax;

    /* perform vertical suitability check (if applicable) */
    if ((mode == ACTKEYS_BORDERSEL_FRAMERANGE) || !((ymax < rectf.ymin) || (ymin > rectf.ymax))) {
      region_select_elem(&sel_data, ale, false);
    }
  }

  /* cleanup */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ----------------------------------- */

static wmOperatorStatus actkeys_lassoselect_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  KeyframeEdit_LassoData data_lasso;
  rcti rect;
  rctf rect_fl;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  data_lasso.rectf_view = &rect_fl;
  data_lasso.mcoords = WM_gesture_lasso_path_to_array(C, op);
  if (data_lasso.mcoords.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const eEditKeyframes_Select selectmode = (sel_op != SEL_OP_SUB) ? SELECT_ADD : SELECT_SUBTRACT;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_action_keys(&ac, 1, SELECT_SUBTRACT);
  }

  /* get settings from operator */
  BLI_lasso_boundbox(&rect, data_lasso.mcoords);
  BLI_rctf_rcti_copy(&rect_fl, &rect);

  /* apply box_select action */
  region_select_action_keys(&ac, &rect_fl, BEZT_OK_CHANNEL_LASSO, selectmode, &data_lasso);

  /* send notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_lasso(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lasso Select";
  ot->description = "Select keyframe points using lasso selection";
  ot->idname = "ACTION_OT_select_lasso";

  /* API callbacks. */
  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = actkeys_lassoselect_exec;
  ot->poll = ED_operator_action_active;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/* ------------------- */

static wmOperatorStatus action_circle_select_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  KeyframeEdit_CircleData data = {nullptr};
  rctf rect_fl;

  float x = RNA_int_get(op->ptr, "x");
  float y = RNA_int_get(op->ptr, "y");
  float radius = RNA_int_get(op->ptr, "radius");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = ED_select_op_modal(
      eSelectOp(RNA_enum_get(op->ptr, "mode")),
      WM_gesture_is_modal_first(static_cast<wmGesture *>(op->customdata)));
  const eEditKeyframes_Select selectmode = (sel_op != SEL_OP_SUB) ? SELECT_ADD : SELECT_SUBTRACT;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    deselect_action_keys(&ac, 0, SELECT_SUBTRACT);
  }

  data.mval[0] = x;
  data.mval[1] = y;
  data.radius_squared = radius * radius;
  data.rectf_view = &rect_fl;

  rect_fl.xmin = x - radius;
  rect_fl.xmax = x + radius;
  rect_fl.ymin = y - radius;
  rect_fl.ymax = y + radius;

  /* apply region select action */
  region_select_action_keys(&ac, &rect_fl, BEZT_OK_CHANNEL_CIRCLE, selectmode, &data);

  /* send notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_circle(wmOperatorType *ot)
{
  ot->name = "Circle Select";
  ot->description = "Select keyframe points using circle selection";
  ot->idname = "ACTION_OT_select_circle";

  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = action_circle_select_exec;
  ot->poll = ED_operator_action_active;
  ot->cancel = WM_gesture_circle_cancel;
  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Column Select Operator
 *
 * This operator works in one of four ways:
 * - 1) select all keyframes in the same frame as a selected one  (KKEY)
 * - 2) select all keyframes in the same frame as the current frame marker (CTRL-KKEY)
 * - 3) select all keyframes in the same frame as a selected markers (SHIFT-KKEY)
 * - 4) select all keyframes that occur between selected markers (ALT-KKEY)
 * \{ */

/* defines for column-select mode */
static const EnumPropertyItem prop_column_select_types[] = {
    {ACTKEYS_COLUMNSEL_KEYS, "KEYS", 0, "On Selected Keyframes", ""},
    {ACTKEYS_COLUMNSEL_CFRA, "CFRA", 0, "On Current Frame", ""},
    {ACTKEYS_COLUMNSEL_MARKERS_COLUMN, "MARKERS_COLUMN", 0, "On Selected Markers", ""},
    {ACTKEYS_COLUMNSEL_MARKERS_BETWEEN,
     "MARKERS_BETWEEN",
     0,
     "Between Min/Max Selected Markers",
     ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* ------------------- */

/* Selects all visible keyframes between the specified markers */
/* TODO(@ideasman42): this is almost an _exact_ duplicate of a function of the same name in
 * `graph_select.cc` should de-duplicate. */
static void markers_selectkeys_between(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditFunc ok_cb, select_cb;
  KeyframeEditData ked = {{nullptr}};
  float min, max;

  /* get extreme markers */
  ED_markers_get_minmax(ac->markers, 1, &min, &max);
  min -= 0.5f;
  max += 0.5f;

  /* Get editing functions + data. */
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
  select_cb = ANIM_editkeyframes_select(SELECT_ADD);

  ked.f1 = min;
  ked.f2 = max;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* select keys in-between */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->type) {
      case ANIMTYPE_GREASE_PENCIL_LAYER:
        blender::ed::greasepencil::select_frames_range(
            static_cast<GreasePencilLayerTreeNode *>(ale->data)->wrap(), min, max, SELECT_ADD);
        ale->update |= ANIM_UPDATE_DEPS;
        break;
      case ANIMTYPE_GPLAYER:
        ED_gpencil_layer_frames_select_box(
            static_cast<bGPDlayer *>(ale->data), min, max, SELECT_ADD);
        ale->update |= ANIM_UPDATE_DEPS;
        break;

      case ANIMTYPE_MASKLAYER:
        ED_masklayer_frames_select_box(static_cast<MaskLayer *>(ale->data), min, max, SELECT_ADD);
        break;

      case ANIMTYPE_FCURVE: {
        FCurve *fcurve = static_cast<FCurve *>(ale->key_data);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, false, true);
        ANIM_fcurve_keyframes_loop(&ked, fcurve, ok_cb, select_cb, nullptr);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, true, true);
        break;
      }

      default:
        BLI_assert_msg(false, "Keys cannot be selected into this animation type.");
    }
  }

  /* Cleanup */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* Selects all visible keyframes in the same frames as the specified elements */
static void columnselect_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  Scene *scene = ac->scene;
  CfraElem *ce;
  KeyframeEditFunc select_cb, ok_cb;
  KeyframeEditData ked = {{nullptr}};

  /* build list of columns */
  switch (mode) {
    case ACTKEYS_COLUMNSEL_KEYS: /* list of selected keys */
      if (ac->datatype == ANIMCONT_GPENCIL) {
        filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE);
        ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

        LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
          switch (ale->type) {
            case ANIMTYPE_GPLAYER:
              ED_gpencil_layer_make_cfra_list(
                  static_cast<bGPDlayer *>(ale->data), &ked.list, true);
              break;
            case ANIMTYPE_GREASE_PENCIL_LAYER:
              blender::ed::greasepencil ::create_keyframe_edit_data_selected_frames_list(
                  &ked, static_cast<GreasePencilLayer *>(ale->data)->wrap());
              break;
            default:
              /* Invalid channel type. */
              BLI_assert_unreachable();
          }
        }
      }
      else {
        filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE);
        ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

        LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
          if (ale->datatype == ALE_GPFRAME) {
            ED_gpencil_layer_make_cfra_list(static_cast<bGPDlayer *>(ale->data), &ked.list, true);
          }
          else {
            ked.data = ale;
            ANIM_fcurve_keyframes_loop(
                &ked, static_cast<FCurve *>(ale->key_data), nullptr, bezt_to_cfraelem, nullptr);
          }
        }
      }
      ANIM_animdata_freelist(&anim_data);
      break;

    case ACTKEYS_COLUMNSEL_CFRA: /* current frame */
      /* make a single CfraElem for storing this */
      ce = MEM_callocN<CfraElem>("cfraElem");
      BLI_addtail(&ked.list, ce);

      ce->cfra = float(scene->r.cfra);
      break;

    case ACTKEYS_COLUMNSEL_MARKERS_COLUMN: /* list of selected markers */
      ED_markers_make_cfra_list(ac->markers, &ked.list, true);
      break;

    default: /* invalid option */
      return;
  }

  /* set up BezTriple edit callbacks */
  select_cb = ANIM_editkeyframes_select(SELECT_ADD);
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);

  /* loop through all of the keys and select additional keyframes
   * based on the keys found to be selected above
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    /* loop over cfraelems (stored in the KeyframeEditData->list)
     * - we need to do this here, as we can apply fewer NLA-mapping conversions
     */
    LISTBASE_FOREACH (CfraElem *, ce, &ked.list) {
      /* set frame for validation callback to refer to */
      ked.f1 = ANIM_nla_tweakedit_remap(ale, ce->cfra, NLATIME_CONVERT_UNMAP);

      /* select elements with frame number matching cfraelem */
      if (ale->type == ANIMTYPE_GPLAYER) {
        ED_gpencil_select_frame(static_cast<bGPDlayer *>(ale->data), ce->cfra, SELECT_ADD);
        ale->update |= ANIM_UPDATE_DEPS;
      }
      else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
        blender::ed::greasepencil::select_frame_at(
            static_cast<GreasePencilLayer *>(ale->data)->wrap(), ce->cfra, SELECT_ADD);
        ale->update |= ANIM_UPDATE_DEPS;
      }
      else if (ale->type == ANIMTYPE_MASKLAYER) {
        ED_mask_select_frame(static_cast<MaskLayer *>(ale->data), ce->cfra, SELECT_ADD);
      }
      else {
        ANIM_fcurve_keyframes_loop(
            &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
      }
    }
  }

  /* free elements */
  BLI_freelistN(&ked.list);

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_columnselect_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* action to take depends on the mode */
  mode = RNA_enum_get(op->ptr, "mode");

  if (mode == ACTKEYS_COLUMNSEL_MARKERS_BETWEEN) {
    markers_selectkeys_between(&ac);
  }
  else {
    columnselect_action_keys(&ac, mode);
  }

  /* set notifier that keyframe selection have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_column(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select All";
  ot->idname = "ACTION_OT_select_column";
  ot->description = "Select all keyframes on the specified frame(s)";

  /* API callbacks. */
  ot->exec = actkeys_columnselect_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_enum(ot->srna, "mode", prop_column_select_types, 0, "Mode", "");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static wmOperatorStatus actkeys_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditFunc ok_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
  KeyframeEditFunc sel_cb = ANIM_editkeyframes_select(SELECT_ADD);

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* loop through all of the keys and select additional keyframes based on these */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* check if anything selected? */
    if (ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, ok_cb, nullptr)) {
      /* select every keyframe in this curve then */
      ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, sel_cb, nullptr);
    }
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->idname = "ACTION_OT_select_linked";
  ot->description = "Select keyframes occurring in the same F-Curves as selected ones";

  /* API callbacks. */
  ot->exec = actkeys_select_linked_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operators
 * \{ */

/* Common code to perform selection */
static void select_moreless_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditData ked = {{nullptr}};
  KeyframeEditFunc build_cb;

  /* init selmap building data */
  build_cb = ANIM_editkeyframes_buildselmap(mode);

  /* loop through all of the keys and select additional keyframes based on these */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {

    /* TODO: other types. */
    if (ale->datatype != ALE_FCURVE) {
      continue;
    }

    /* only continue if F-Curve has keyframes */
    FCurve *fcu = (FCurve *)ale->key_data;
    if (fcu->bezt == nullptr) {
      continue;
    }

    /* build up map of whether F-Curve's keyframes should be selected or not */
    ked.data = MEM_callocN(fcu->totvert, "selmap actEdit more");
    ANIM_fcurve_keyframes_loop(&ked, fcu, nullptr, build_cb, nullptr);

    /* based on this map, adjust the selection status of the keyframes */
    ANIM_fcurve_keyframes_loop(&ked, fcu, nullptr, bezt_selmap_flush, nullptr);

    /* free the selmap used here */
    MEM_freeN(ked.data);
    ked.data = nullptr;
  }

  /* Cleanup */
  ANIM_animdata_freelist(&anim_data);
}

/* ----------------- */

static wmOperatorStatus actkeys_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* perform select changes */
  select_moreless_action_keys(&ac, SELMAP_MORE);

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->idname = "ACTION_OT_select_more";
  ot->description = "Select keyframes beside already selected ones";

  /* API callbacks. */
  ot->exec = actkeys_select_more_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ----------------- */

static wmOperatorStatus actkeys_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* perform select changes */
  select_moreless_action_keys(&ac, SELMAP_LESS);

  /* set notifier that keyframe selection has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  if (ANIM_animdata_can_have_greasepencil(eAnimCont_Types(ac.datatype))) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);
  }
  return OPERATOR_FINISHED;
}

void ACTION_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->idname = "ACTION_OT_select_less";
  ot->description = "Deselect keyframes on ends of selection islands";

  /* API callbacks. */
  ot->exec = actkeys_select_less_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Left/Right Operator
 *
 * Select keyframes left/right of the current frame indicator.
 * \{ */

/* defines for left-right select tool */
static const EnumPropertyItem prop_actkeys_leftright_select_types[] = {
    {ACTKEYS_LRSEL_TEST, "CHECK", 0, "Check if Select Left or Right", ""},
    {ACTKEYS_LRSEL_LEFT, "LEFT", 0, "Before Current Frame", ""},
    {ACTKEYS_LRSEL_RIGHT, "RIGHT", 0, "After Current Frame", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* --------------------------------- */

static void actkeys_select_leftright(bAnimContext *ac,
                                     short leftright,
                                     eEditKeyframes_Select select_mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditFunc ok_cb, select_cb;
  KeyframeEditData ked = {{nullptr}};
  Scene *scene = ac->scene;

  /* if select mode is replace, deselect all keyframes (and channels) first */
  if (select_mode == SELECT_REPLACE) {
    select_mode = SELECT_ADD;

    /* - deselect all other keyframes, so that just the newly selected remain
     * - channels aren't deselected, since we don't re-select any as a consequence
     */
    deselect_action_keys(ac, 0, SELECT_SUBTRACT);
  }

  /* set callbacks and editing data */
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
  select_cb = ANIM_editkeyframes_select(select_mode);

  if (leftright == ACTKEYS_LRSEL_LEFT) {
    ked.f1 = MINAFRAMEF;
    ked.f2 = float(scene->r.cfra + 0.1f);
  }
  else {
    ked.f1 = float(scene->r.cfra - 0.1f);
    ked.f2 = MAXFRAMEF;
  }

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* select keys */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->type) {
      case ANIMTYPE_GREASE_PENCIL_LAYER:
        blender::ed::greasepencil::select_frames_range(
            static_cast<GreasePencilLayerTreeNode *>(ale->data)->wrap(),
            ked.f1,
            ked.f2,
            select_mode);
        ale->update |= ANIM_UPDATE_DEPS;
        break;
      case ANIMTYPE_GPLAYER:
        ED_gpencil_layer_frames_select_box(
            static_cast<bGPDlayer *>(ale->data), ked.f1, ked.f2, select_mode);
        ale->update |= ANIM_UPDATE_DEPS;
        break;

      case ANIMTYPE_MASKLAYER:
        ED_masklayer_frames_select_box(
            static_cast<MaskLayer *>(ale->data), ked.f1, ked.f2, select_mode);
        break;

      case ANIMTYPE_FCURVE: {
        FCurve *fcurve = static_cast<FCurve *>(ale->key_data);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, false, true);
        ANIM_fcurve_keyframes_loop(&ked, fcurve, ok_cb, select_cb, nullptr);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, true, true);
        break;
      }

      default:
        BLI_assert_msg(false, "Keys cannot be selected into this animation type.");
    }
  }

  /* Sync marker support */
  if (select_mode == SELECT_ADD) {
    SpaceAction *saction = (SpaceAction *)ac->sl;

    if (saction && ac->markers && (saction->flag & SACTION_MARKERS_MOVE)) {
      LISTBASE_FOREACH (TimeMarker *, marker, ac->markers) {
        if (((leftright == ACTKEYS_LRSEL_LEFT) && (marker->frame < scene->r.cfra)) ||
            ((leftright == ACTKEYS_LRSEL_RIGHT) && (marker->frame >= scene->r.cfra)))
        {
          marker->flag |= SELECT;
        }
        else {
          marker->flag &= ~SELECT;
        }
      }
    }
  }

  /* Cleanup */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ----------------- */

static wmOperatorStatus actkeys_select_leftright_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short leftright = RNA_enum_get(op->ptr, "mode");
  eEditKeyframes_Select selectmode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* select mode is either replace (deselect all, then add) or add/extend */
  if (RNA_boolean_get(op->ptr, "extend")) {
    selectmode = SELECT_INVERT;
  }
  else {
    selectmode = SELECT_REPLACE;
  }

  /* if "test" mode is set, we don't have any info to set this with */
  if (leftright == ACTKEYS_LRSEL_TEST) {
    return OPERATOR_CANCELLED;
  }

  /* do the selecting now */
  actkeys_select_leftright(&ac, leftright, selectmode);

  /* set notifier that keyframe selection (and channels too) have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus actkeys_select_leftright_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  bAnimContext ac;
  short leftright = RNA_enum_get(op->ptr, "mode");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* handle mode-based testing */
  if (leftright == ACTKEYS_LRSEL_TEST) {
    Scene *scene = ac.scene;
    ARegion *region = ac.region;
    View2D *v2d = &region->v2d;
    float x;

    /* determine which side of the current frame mouse is on */
    x = UI_view2d_region_to_view_x(v2d, event->mval[0]);
    if (x < scene->r.cfra) {
      RNA_enum_set(op->ptr, "mode", ACTKEYS_LRSEL_LEFT);
    }
    else {
      RNA_enum_set(op->ptr, "mode", ACTKEYS_LRSEL_RIGHT);
    }
  }

  /* perform selection */
  return actkeys_select_leftright_exec(C, op);
}

void ACTION_OT_select_leftright(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Left/Right";
  ot->idname = "ACTION_OT_select_leftright";
  ot->description = "Select keyframes to the left or the right of the current frame";

  /* API callbacks. */
  ot->invoke = actkeys_select_leftright_invoke;
  ot->exec = actkeys_select_leftright_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", prop_actkeys_leftright_select_types, ACTKEYS_LRSEL_TEST, "Mode", "");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend Select", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse-Click Select Operator
 *
 * This operator works in one of three ways:
 * - 1) keyframe under mouse - no special modifiers
 * - 2) all keyframes on the same side of current frame indicator as mouse - ALT modifier
 * - 3) column select all keyframes in frame under mouse - CTRL modifier
 * - 4) all keyframes in channel under mouse - CTRL+ALT modifiers
 *
 * In addition to these basic options, the SHIFT modifier can be used to toggle the
 * selection mode between replacing the selection (without) and inverting the selection (with).
 * \{ */

/* option 1) select keyframe directly under mouse */
static void actkeys_mselect_single(bAnimContext *ac,
                                   bAnimListElem *ale,
                                   const eEditKeyframes_Select select_mode,
                                   float selx)
{
  KeyframeEditData ked = {{nullptr}};
  KeyframeEditFunc select_cb, ok_cb;

  /* get functions for selecting keyframes */
  select_cb = ANIM_editkeyframes_select(select_mode);
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);
  ked.f1 = selx;
  ked.iterflags |= KED_F1_NLA_UNMAP;

  /* select the nominated keyframe on the given frame */
  if (ale->type == ANIMTYPE_GPLAYER) {
    ED_gpencil_select_frame(static_cast<bGPDlayer *>(ale->data), selx, select_mode);
    ale->update |= ANIM_UPDATE_DEPS;
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
    blender::ed::greasepencil::select_frame_at(
        static_cast<GreasePencilLayer *>(ale->data)->wrap(), selx, select_mode);
    ale->update |= ANIM_UPDATE_DEPS;
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER_GROUP) {
    blender::ed::greasepencil::select_frames_at(
        static_cast<GreasePencilLayerTreeGroup *>(ale->data)->wrap(), selx, select_mode);
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_DATABLOCK) {
    ListBase anim_data = {nullptr, nullptr};
    eAnimFilter_Flags filter;

    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
    ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

    /* Loop over all keys that are represented by this data-block key. */
    LISTBASE_FOREACH (bAnimListElem *, ale2, &anim_data) {
      if ((ale2->type != ANIMTYPE_GREASE_PENCIL_LAYER) || (ale2->id != ale->data)) {
        continue;
      }
      blender::ed::greasepencil::select_frame_at(
          static_cast<GreasePencilLayer *>(ale2->data)->wrap(), selx, select_mode);
      ale2->update |= ANIM_UPDATE_DEPS;
    }
  }
  else if (ale->type == ANIMTYPE_MASKLAYER) {
    ED_mask_select_frame(static_cast<MaskLayer *>(ale->data), selx, select_mode);
  }
  else {
    if (ale->type == ANIMTYPE_SUMMARY && ale->datatype == ALE_ALL) {
      ListBase anim_data = {nullptr, nullptr};
      eAnimFilter_Flags filter;

      filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
      ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

      /* Loop over all keys that are represented by this summary key. */
      LISTBASE_FOREACH (bAnimListElem *, ale2, &anim_data) {
        switch (ale2->type) {
          case ANIMTYPE_GPLAYER:
            ED_gpencil_select_frame(static_cast<bGPDlayer *>(ale2->data), selx, select_mode);
            ale2->update |= ANIM_UPDATE_DEPS;
            break;

          case ANIMTYPE_MASKLAYER:
            ED_mask_select_frame(static_cast<MaskLayer *>(ale2->data), selx, select_mode);
            break;

          case ANIMTYPE_GREASE_PENCIL_LAYER:
            blender::ed::greasepencil::select_frame_at(
                static_cast<GreasePencilLayer *>(ale2->data)->wrap(), selx, select_mode);
            ale2->update |= ANIM_UPDATE_DEPS;
            break;

          default:
            break;
        }
      }

      ANIM_animdata_update(ac, &anim_data);
      ANIM_animdata_freelist(&anim_data);
    }

    if (!ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
      ANIM_animchannel_keyframes_loop(&ked, ac->ads, ale, ok_cb, select_cb, nullptr);
    }
  }
}

/* Option 2) Selects all the keyframes on either side of the current frame
 * (depends on which side the mouse is on) */
/* (see actkeys_select_leftright) */

/* Option 3) Selects all visible keyframes in the same frame as the mouse click */
static void actkeys_mselect_column(bAnimContext *ac, eEditKeyframes_Select select_mode, float selx)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditFunc select_cb, ok_cb;
  KeyframeEditData ked = {{nullptr}};

  /* set up BezTriple edit callbacks */
  select_cb = ANIM_editkeyframes_select(select_mode);
  ok_cb = ANIM_editkeyframes_ok(BEZT_OK_FRAME);

  /* loop through all of the keys and select additional keyframes
   * based on the keys found to be selected above
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    /* select elements with frame number matching cfra */
    if (ale->type == ANIMTYPE_GPLAYER) {
      ED_gpencil_select_frame(static_cast<bGPDlayer *>(ale->data), selx, select_mode);
      ale->update |= ANIM_UPDATE_DEPS;
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      ED_mask_select_frame(static_cast<MaskLayer *>(ale->data), selx, select_mode);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      blender::ed::greasepencil::select_frame_at(
          static_cast<GreasePencilLayer *>(ale->data)->wrap(), selx, select_mode);
      ale->update |= ANIM_UPDATE_DEPS;
    }
    else {
      /* set frame for validation callback to refer to */
      ked.f1 = ANIM_nla_tweakedit_remap(ale, selx, NLATIME_CONVERT_UNMAP);

      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), ok_cb, select_cb, nullptr);
    }
  }

  /* free elements */
  BLI_freelistN(&ked.list);

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* option 4) select all keyframes in same channel */
static void actkeys_mselect_channel_only(bAnimContext *ac,
                                         bAnimListElem *ale,
                                         eEditKeyframes_Select select_mode)
{
  KeyframeEditFunc select_cb;

  /* get functions for selecting keyframes */
  select_cb = ANIM_editkeyframes_select(select_mode);

  /* select all keyframes in this channel */
  if (ale->type == ANIMTYPE_GPLAYER) {
    ED_gpencil_select_frames(static_cast<bGPDlayer *>(ale->data), select_mode);
    ale->update = ANIM_UPDATE_DEPS;
  }
  else if (ale->type == ANIMTYPE_MASKLAYER) {
    ED_mask_select_frames(static_cast<MaskLayer *>(ale->data), select_mode);
  }
  else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
    blender::ed::greasepencil::select_all_frames(
        static_cast<GreasePencilLayer *>(ale->data)->wrap(), select_mode);
    ale->update |= ANIM_UPDATE_DEPS;
  }
  else {
    if (ale->type == ANIMTYPE_SUMMARY && ale->datatype == ALE_ALL) {
      ListBase anim_data = {nullptr, nullptr};
      eAnimFilter_Flags filter;

      filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
      ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

      LISTBASE_FOREACH (bAnimListElem *, ale2, &anim_data) {
        if (ale2->type == ANIMTYPE_GPLAYER) {
          ED_gpencil_select_frames(static_cast<bGPDlayer *>(ale2->data), select_mode);
          ale2->update |= ANIM_UPDATE_DEPS;
        }
        else if (ale2->type == ANIMTYPE_MASKLAYER) {
          ED_mask_select_frames(static_cast<MaskLayer *>(ale2->data), select_mode);
        }
      }

      ANIM_animdata_update(ac, &anim_data);
      ANIM_animdata_freelist(&anim_data);
    }

    if (!ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
      ANIM_animchannel_keyframes_loop(nullptr, ac->ads, ale, nullptr, select_cb, nullptr);
    }
  }
}

/* ------------------- */

static wmOperatorStatus mouse_action_keys(bAnimContext *ac,
                                          const int mval[2],
                                          eEditKeyframes_Select select_mode,
                                          const bool deselect_all,
                                          const bool column,
                                          const bool same_channel,
                                          bool wait_to_deselect_others)
{
  /* NOTE: keep this functionality in sync with #MARKER_OT_select.
   * The logic here closely matches its internals.
   * From a user perspective the functions should also behave in much the same way. */

  eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                             ANIMFILTER_LIST_CHANNELS;

  bAnimListElem *ale = nullptr;
  bool found = false;
  bool is_selected = false;
  float frame = 0.0f; /* frame of keyframe under mouse - NLA corrections not applied/included */
  float selx = 0.0f;  /* frame of keyframe under mouse */
  wmOperatorStatus ret_value = OPERATOR_FINISHED;

  actkeys_find_key_at_position(
      ac, filter, mval[0], mval[1], &ale, &selx, &frame, &found, &is_selected);

  if (select_mode != SELECT_REPLACE) {
    wait_to_deselect_others = false;
  }

  /* For replacing selection, if we have something to select, we have to clear existing selection.
   * The same goes if we found nothing to select, and deselect_all is true
   * (deselect on nothing behavior). */
  if ((select_mode == SELECT_REPLACE && found) || (!found && deselect_all)) {
    /* reset selection mode for next steps */
    select_mode = SELECT_ADD;

    /* Rather than deselecting others, users may want to drag to box-select (drag from empty space)
     * or tweak-translate an already selected item. If these cases may apply, delay deselection. */
    if (wait_to_deselect_others && (!found || is_selected)) {
      ret_value = OPERATOR_RUNNING_MODAL;
    }
    else {
      /* deselect all keyframes */
      deselect_action_keys(ac, 0, SELECT_SUBTRACT);

      /* highlight channel clicked on */
      if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET, ANIMCONT_TIMELINE)) {
        /* deselect all other channels first */
        ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);

        /* Highlight Action-Group or F-Curve? */
        if (ale != nullptr && ale->data) {
          if (ale->type == ANIMTYPE_GROUP) {
            bActionGroup *agrp = static_cast<bActionGroup *>(ale->data);

            agrp->flag |= AGRP_SELECTED;
            ANIM_set_active_channel(
                ac, ac->data, eAnimCont_Types(ac->datatype), filter, agrp, ANIMTYPE_GROUP);
          }
          else if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
            FCurve *fcu = static_cast<FCurve *>(ale->data);

            fcu->flag |= FCURVE_SELECTED;
            ANIM_set_active_channel(ac,
                                    ac->data,
                                    eAnimCont_Types(ac->datatype),
                                    filter,
                                    fcu,
                                    eAnim_ChannelType(ale->type));
          }
          else if (ale->type == ANIMTYPE_GPLAYER) {
            bGPdata *gpd = (bGPdata *)ale->id;
            bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

            ED_gpencil_set_active_channel(gpd, gpl);
          }
          else if (ale->type == ANIMTYPE_ACTION_SLOT) {
            BLI_assert_msg(GS(ale->fcurve_owner_id->name) == ID_AC,
                           "fcurve_owner_id of an Action Slot should be an Action");
            animrig::Action *action = reinterpret_cast<animrig::Action *>(ale->fcurve_owner_id);
            animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
            slot->set_selected(true);
            action->slot_active_set(slot->handle);
          }
        }
      }
      else if (ac->datatype == ANIMCONT_GPENCIL) {
        /* Deselect all other channels first. */
        ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);

        /* Highlight the grease pencil channel, and set the corresponding layer as active. */
        if (ale != nullptr && ale->data != nullptr && ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
          blender::ed::greasepencil::select_layer_channel(
              *reinterpret_cast<GreasePencil *>(ale->id),
              static_cast<blender::bke::greasepencil::Layer *>(ale->data));
        }

        /* Highlight GPencil Layer (Legacy). */
        if (ale != nullptr && ale->data != nullptr && ale->type == ANIMTYPE_GPLAYER) {
          bGPdata *gpd = (bGPdata *)ale->id;
          bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

          ED_gpencil_set_active_channel(gpd, gpl);
        }
      }
      else if (ac->datatype == ANIMCONT_MASK) {
        /* deselect all other channels first */
        ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);

        if (ale != nullptr && ale->data != nullptr && ale->type == ANIMTYPE_MASKLAYER) {
          MaskLayer *masklay = static_cast<MaskLayer *>(ale->data);

          masklay->flag |= MASK_LAYERFLAG_SELECT;
        }
      }
    }
  }

  /* only select keyframes if we clicked on a valid channel and hit something */
  if (ale != nullptr) {
    if (found) {
      /* apply selection to keyframes */
      if (column) {
        /* select all keyframes in the same frame as the one we hit on the active channel
         * [#41077]: "frame" not "selx" here (i.e. no NLA corrections yet) as the code here
         *            does that itself again as it needs to work on multiple data-blocks.
         */
        actkeys_mselect_column(ac, select_mode, frame);
      }
      else if (same_channel) {
        /* select all keyframes in the active channel */
        actkeys_mselect_channel_only(ac, ale, select_mode);
      }
      else {
        /* select the nominated keyframe on the given frame */
        actkeys_mselect_single(ac, ale, select_mode, selx);
      }
    }

    /* flush tagged updates
     * NOTE: We temporarily add this channel back to the list so that this can happen
     */
    ListBase anim_data = {ale, ale};
    ANIM_animdata_update(ac, &anim_data);

    /* free this channel */
    MEM_freeN(ale);
  }

  return ret_value;
}

/* handle clicking */
static wmOperatorStatus actkeys_clickselect_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  wmOperatorStatus ret_value;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get useful pointers from animation context data */
  // region = ac.region; /* UNUSED. */

  /* select mode is either replace (deselect all, then add) or add/extend */
  const eEditKeyframes_Select selectmode = RNA_boolean_get(op->ptr, "extend") ? SELECT_INVERT :
                                                                                SELECT_REPLACE;
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  int mval[2];

  /* column selection */
  const bool column = RNA_boolean_get(op->ptr, "column");
  const bool channel = RNA_boolean_get(op->ptr, "channel");

  mval[0] = RNA_int_get(op->ptr, "mouse_x");
  mval[1] = RNA_int_get(op->ptr, "mouse_y");

  /* Select keyframe(s) based upon mouse position. */
  ret_value = mouse_action_keys(
      &ac, mval, selectmode, deselect_all, column, channel, wait_to_deselect_others);

  /* set notifier that keyframe selection (and channels too) have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_SELECTED, nullptr);

  /* for tweak grab to work */
  return ret_value | OPERATOR_PASS_THROUGH;
}

void ACTION_OT_clickselect(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Keyframes";
  ot->idname = "ACTION_OT_clickselect";
  ot->description = "Select keyframes by clicking on them";

  /* callbacks */
  ot->poll = ED_operator_action_active;
  ot->exec = actkeys_clickselect_exec;
  ot->invoke = WM_generic_select_invoke;
  ot->modal = WM_generic_select_modal;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_generic_select(ot);
  /* Key-map: Enable with `Shift`. */
  prop = RNA_def_boolean(
      ot->srna,
      "extend",
      false,
      "Extend Select",
      "Toggle keyframe selection instead of leaving newly selected keyframes only");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Key-map: Enable with `Alt`. */
  prop = RNA_def_boolean(
      ot->srna,
      "column",
      false,
      "Column Select",
      "Select all keyframes that occur on the same frame as the one under the mouse");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Key-map: Enable with `Ctrl-Alt`. */
  prop = RNA_def_boolean(ot->srna,
                         "channel",
                         false,
                         "Only Channel",
                         "Select all the keyframes in the channel under the mouse");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */
