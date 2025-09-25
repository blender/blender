/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"

#include "DNA_anim_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_nla.hh"
#include "BKE_report.hh"

#include "UI_interface_icons.hh"
#include "UI_view2d.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"
#include "ED_anim_api.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_keyframes_edit.hh"
#include "ED_keyframing.hh"
#include "ED_markers.hh"
#include "ED_mask.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "action_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Pose Markers: Localize Markers
 * \{ */

/* ensure that there is:
 * 1) an active action editor
 * 2) that the set of markers being shown are the scene markers, not the list we're merging
 * 3) that the mode will have an active action available
 * 4) that there are some selected markers
 */
static bool act_markers_make_local_poll(bContext *C)
{
  SpaceAction *sact = CTX_wm_space_action(C);

  /* 1) */
  if (sact == nullptr) {
    return false;
  }

  /* 2) */
  if (sact->flag & SACTION_POSEMARKERS_SHOW) {
    return false;
  }

  /* 3) */
  bAction *active_action = ANIM_active_action_from_area(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_area(C));
  if (!active_action) {
    return false;
  }

  /* 4) */
  return ED_markers_get_first_selected(ED_context_get_markers(C)) != nullptr;
}

static wmOperatorStatus act_markers_make_local_exec(bContext *C, wmOperator * /*op*/)
{
  ListBase *markers = ED_context_get_markers(C);
  bAction *act = ANIM_active_action_from_area(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_area(C));

  TimeMarker *marker, *markern = nullptr;

  /* sanity checks */
  if (ELEM(nullptr, markers, act)) {
    return OPERATOR_CANCELLED;
  }

  /* migrate markers */
  for (marker = static_cast<TimeMarker *>(markers->first); marker; marker = markern) {
    markern = marker->next;

    /* move if marker is selected */
    if (marker->flag & SELECT) {
      BLI_remlink(markers, marker);
      BLI_addtail(&act->markers, marker);
    }
  }

  /* Now enable the "show pose-markers only" setting,
   * so that we can see that something did happen. */
  SpaceAction *sact = CTX_wm_space_action(C);
  sact->flag |= SACTION_POSEMARKERS_SHOW;

  /* notifiers - both sets, as this change affects both */
  WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_markers_make_local(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Markers Local";
  ot->idname = "ACTION_OT_markers_make_local";
  ot->description = "Move selected scene markers to the active Action as local 'pose' markers";

  /* callbacks */
  ot->exec = act_markers_make_local_exec;
  ot->poll = act_markers_make_local_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Range
 * \{ */

/* Get the min/max keyframes. */
static bool get_keyframe_extents(bAnimContext *ac, float *min, float *max, const short onlySel)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;
  bool found = false;

  /* Get data to filter, from Action or Dope-sheet. */
  /* XXX: what is sel doing here?!
   *      Commented it, was breaking things (eg. the "auto preview range" tool). */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_SEL */ |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* set large values to try to override */
  *min = 999999999.0f;
  *max = -999999999.0f;

  /* check if any channels to set range with */
  if (anim_data.first) {
    /* go through channels, finding max extents */
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      if (ale->datatype == ALE_GPFRAME) {
        bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

        /* Find GP-frame which is less than or equal to current-frame. */
        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          if (!onlySel || (gpf->flag & GP_FRAME_SELECT)) {
            const float framenum = float(gpf->framenum);
            *min = min_ff(*min, framenum);
            *max = max_ff(*max, framenum);
            found = true;
          }
        }
      }
      else if (ale->datatype == ALE_MASKLAY) {
        MaskLayer *masklay = static_cast<MaskLayer *>(ale->data);
        /* Find mask layer which is less than or equal to current-frame. */
        LISTBASE_FOREACH (MaskLayerShape *, masklay_shape, &masklay->splines_shapes) {
          const float framenum = float(masklay_shape->frame);
          *min = min_ff(*min, framenum);
          *max = max_ff(*max, framenum);
          found = true;
        }
      }
      else if (ale->datatype == ALE_GREASE_PENCIL_CEL) {
        const blender::bke::greasepencil::Layer &layer =
            static_cast<GreasePencilLayer *>(ale->data)->wrap();

        for (const auto [key, frame] : layer.frames().items()) {
          if (onlySel && !frame.is_selected()) {
            continue;
          }
          *min = min_ff(*min, float(key));
          *max = max_ff(*max, float(key));
          found = true;
        }
      }
      else {
        FCurve *fcu = (FCurve *)ale->key_data;
        float tmin, tmax;

        /* get range and apply necessary scaling before processing */
        if (BKE_fcurve_calc_range(fcu, &tmin, &tmax, onlySel)) {
          tmin = ANIM_nla_tweakedit_remap(ale, tmin, NLATIME_CONVERT_MAP);
          tmax = ANIM_nla_tweakedit_remap(ale, tmax, NLATIME_CONVERT_MAP);

          /* Try to set cur using these values,
           * if they're more extreme than previously set values. */
          *min = min_ff(*min, tmin);
          *max = max_ff(*max, tmax);
          found = true;
        }
      }
    }

    if (fabsf(*max - *min) < 0.001f) {
      *min -= 0.0005f;
      *max += 0.0005f;
    }

    /* free memory */
    ANIM_animdata_freelist(&anim_data);
  }
  else {
    /* set default range */
    if (ac->scene) {
      *min = float(ac->scene->r.sfra);
      *max = float(ac->scene->r.efra);
    }
    else {
      *min = -5;
      *max = 100;
    }
  }

  return found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View: Automatic Preview-Range Operator
 * \{ */

static wmOperatorStatus actkeys_previewrange_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  Scene *scene;
  float min, max;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  if (ac.scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* set the range directly */
  if (!get_keyframe_extents(&ac, &min, &max, true)) {
    return OPERATOR_CANCELLED;
  }
  scene = ac.scene;
  scene->r.flag |= SCER_PRV_RANGE;
  scene->r.psfra = floorf(min);
  scene->r.pefra = ceilf(max);

  if (scene->r.psfra == scene->r.pefra) {
    scene->r.pefra = scene->r.psfra + 1;
  }

  /* set notifier that things have changed */
  /* XXX err... there's nothing for frame ranges yet, but this should do fine too */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);

  return OPERATOR_FINISHED;
}

void ACTION_OT_previewrange_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Preview Range to Selected";
  ot->idname = "ACTION_OT_previewrange_set";
  ot->description = "Set Preview Range based on extents of selected Keyframes";

  /* API callbacks. */
  ot->exec = actkeys_previewrange_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View: All Operator
 * \{ */

/**
 * Find the extents of the active channel
 *
 * \param r_min: Bottom y-extent of channel.
 * \param r_max: Top y-extent of channel.
 * \return Success of finding a selected channel.
 */
static bool actkeys_channels_get_selected_extents(bAnimContext *ac, float *r_min, float *r_max)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  eAnimFilter_Flags filter;

  /* NOTE: not bool, since we want prioritize individual channels over expanders. */
  short found = 0;

  /* get all items - we need to do it this way */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* loop through all channels, finding the first one that's selected */
  float ymax = ANIM_UI_get_first_channel_top(&ac->region->v2d);
  const float channel_step = ANIM_UI_get_channel_step();
  for (ale = static_cast<bAnimListElem *>(anim_data.first); ale;
       ale = ale->next, ymax -= channel_step)
  {
    const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

    /* must be selected... */
    if (acf && acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT) &&
        ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT))
    {
      /* update best estimate */
      *r_min = ymax - ANIM_UI_get_channel_height();
      *r_max = ymax;

      /* is this high enough priority yet? */
      found = acf->channel_role;

      /* only stop our search when we've found an actual channel
       * - data-block expanders get less priority so that we don't abort prematurely
       */
      if (found == ACHANNEL_ROLE_CHANNEL) {
        break;
      }
    }
  }

  /* free all temp data */
  ANIM_animdata_freelist(&anim_data);

  return (found != 0);
}

static wmOperatorStatus actkeys_viewall(bContext *C, const bool only_sel)
{
  bAnimContext ac;
  View2D *v2d;
  float min, max;
  bool found;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  v2d = &ac.region->v2d;

  /* set the horizontal range, with an extra offset so that the extreme keys will be in view */
  found = get_keyframe_extents(&ac, &min, &max, only_sel);

  if (only_sel && (found == false)) {
    return OPERATOR_CANCELLED;
  }

  if (fabsf(max - min) < 1.0f) {
    /* Exception - center the single keyframe. */
    float xwidth = BLI_rctf_size_x(&v2d->cur);

    v2d->cur.xmin = min - xwidth / 2.0f;
    v2d->cur.xmax = max + xwidth / 2.0f;
  }
  else {
    /* Normal case - stretch the two keyframes out to fill the space, with extra spacing */
    v2d->cur.xmin = min;
    v2d->cur.xmax = max;

    v2d->cur = ANIM_frame_range_view2d_add_xmargin(*v2d, v2d->cur);
  }

  /* set vertical range */
  if (only_sel == false) {
    /* view all -> the summary channel is usually the shows everything,
     * and resides right at the top... */
    v2d->cur.ymax = 0.0f;
    v2d->cur.ymin = float(-BLI_rcti_size_y(&v2d->mask));
  }
  else {
    /* locate first selected channel (or the active one), and frame those */
    float ymin = v2d->cur.ymin;
    float ymax = v2d->cur.ymax;

    if (actkeys_channels_get_selected_extents(&ac, &ymin, &ymax)) {
      /* recenter the view so that this range is in the middle */
      float ymid = (ymax - ymin) / 2.0f + ymin;
      float x_center;

      UI_view2d_center_get(v2d, &x_center, nullptr);
      UI_view2d_center_set(v2d, x_center, ymid);
    }
  }

  /* do View2D syncing */
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);

  /* just redraw this view */
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

/* ......... */

static wmOperatorStatus actkeys_viewall_exec(bContext *C, wmOperator * /*op*/)
{
  /* whole range */
  return actkeys_viewall(C, false);
}

static wmOperatorStatus actkeys_viewsel_exec(bContext *C, wmOperator * /*op*/)
{
  /* only selected */
  return actkeys_viewall(C, true);
}

/* ......... */

void ACTION_OT_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame All";
  ot->idname = "ACTION_OT_view_all";
  ot->description = "Reset viewable area to show full keyframe range";

  /* API callbacks. */
  ot->exec = actkeys_viewall_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = 0;
}

void ACTION_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Selected";
  ot->idname = "ACTION_OT_view_selected";
  ot->description = "Reset viewable area to show selected keyframes range";

  /* API callbacks. */
  ot->exec = actkeys_viewsel_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View: Frame Operator
 * \{ */

static wmOperatorStatus actkeys_view_frame_exec(bContext *C, wmOperator *op)
{
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  ANIM_center_frame(C, smooth_viewtx);

  return OPERATOR_FINISHED;
}

void ACTION_OT_view_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Go to Current Frame";
  ot->idname = "ACTION_OT_view_frame";
  ot->description = "Move the view to the current frame";

  /* API callbacks. */
  ot->exec = actkeys_view_frame_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes: Copy/Paste Operator
 * \{ */

/* NOTE: the backend code for this is shared with the graph editor */

static bool copy_action_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* copy keyframes */
  const bool ok = copy_animedit_keys(ac, &anim_data);

  /* clean up */
  ANIM_animdata_freelist(&anim_data);

  return ok;
}

static eKeyPasteError paste_action_keys(bAnimContext *ac,
                                        const eKeyPasteOffset offset_mode,
                                        const eKeyMergeMode merge_mode,
                                        bool flip)
{
  /* TODO: deduplicate this function and `paste_graph_keys()` in `graph_edit.cc`, */

  /* Determine paste context. */
  KeyframePasteContext paste_context{};
  paste_context.offset_mode = offset_mode;
  /* Value offset is always None because the user cannot see the effect of it. */
  paste_context.value_offset_mode = KEYFRAME_PASTE_VALUE_OFFSET_NONE;
  paste_context.merge_mode = merge_mode;
  paste_context.flip = flip;

  /* See how many slots are selected to paste into. This determines how slot matching is done:
   * - Copied from one slot, paste into multiple: duplicate into each slot.
   * - Otherwise: match slots by name. */
  {
    ListBase anim_data = {nullptr, nullptr};
    const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                     ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS |
                                     ANIMFILTER_SEL;
    ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
    LISTBASE_FOREACH (const bAnimListElem *, ale, &anim_data) {
      if (ale->datatype == ALE_ACTION_SLOT) {
        paste_context.num_slots_selected++;
      }
    }
    ANIM_animdata_freelist(&anim_data);
  }

  /* Find F-Curves to paste into, in two stages.
   * - First time we try to filter more strictly, allowing only selected channels
   *   to allow copying animation between channels
   * - Second time, we loosen things up if nothing was found the first time, allowing
   *   users to just paste keyframes back into the original curve again #31670.
   */
  ListBase anim_data = {nullptr, nullptr};
  {
    const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                     ANIMFILTER_FOREDIT | ANIMFILTER_FCURVESONLY |
                                     ANIMFILTER_NODUPLIS;
    paste_context.num_fcurves_selected = ANIM_animdata_filter(
        ac, &anim_data, filter | ANIMFILTER_SEL, ac->data, ac->datatype);
    if (paste_context.num_fcurves_selected == 0) {
      ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
    }
  }

  const eKeyPasteError ok = paste_animedit_keys(ac, &anim_data, paste_context);

  /* clean up */
  ANIM_animdata_freelist(&anim_data);

  return ok;
}

/* ------------------- */

static blender::ed::greasepencil::KeyframeClipboard &get_grease_pencil_keyframe_clipboard()
{
  static blender::ed::greasepencil::KeyframeClipboard clipboard;
  return clipboard;
}

static wmOperatorStatus actkeys_copy_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* copy keyframes */
  if (ac.datatype == ANIMCONT_GPENCIL) {
    if (ED_gpencil_anim_copybuf_copy(&ac) == false &&
        blender::ed::greasepencil::grease_pencil_copy_keyframes(
            &ac, get_grease_pencil_keyframe_clipboard()) == false)
    {
      /* check if anything ended up in the buffer */
      BKE_report(op->reports, RPT_ERROR, "No keyframes copied to the internal clipboard");
      return OPERATOR_CANCELLED;
    }
  }
  else if (ac.datatype == ANIMCONT_MASK) {
    /* FIXME: support this case. */
    BKE_report(op->reports, RPT_ERROR, "Keyframe pasting is not available for mask mode");
    return OPERATOR_CANCELLED;
  }
  else {
    /* Both copy function needs to be evaluated to account for mixed selection */
    const bool kf_ok = copy_action_keys(&ac);
    const bool gpf_ok = ED_gpencil_anim_copybuf_copy(&ac) ||
                        blender::ed::greasepencil::grease_pencil_copy_keyframes(
                            &ac, get_grease_pencil_keyframe_clipboard());

    if (!kf_ok && !gpf_ok) {
      BKE_report(op->reports, RPT_ERROR, "No keyframes copied to the internal clipboard");
      return OPERATOR_CANCELLED;
    }
  }

  return OPERATOR_FINISHED;
}

void ACTION_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Keyframes";
  ot->idname = "ACTION_OT_copy";
  ot->description = "Copy selected keyframes to the internal clipboard";

  /* API callbacks. */
  ot->exec = actkeys_copy_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus actkeys_paste_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  const eKeyPasteOffset offset_mode = eKeyPasteOffset(RNA_enum_get(op->ptr, "offset"));
  const eKeyMergeMode merge_mode = eKeyMergeMode(RNA_enum_get(op->ptr, "merge"));
  const bool flipped = RNA_boolean_get(op->ptr, "flipped");

  bool gpframes_inbuf = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* ac.reports by default will be the global reports list, which won't show warnings */
  ac.reports = op->reports;

  /* paste keyframes */
  if (ac.datatype == ANIMCONT_GPENCIL) {
    if (ED_gpencil_anim_copybuf_paste(&ac, offset_mode) == false &&
        blender::ed::greasepencil::grease_pencil_paste_keyframes(
            &ac, offset_mode, merge_mode, get_grease_pencil_keyframe_clipboard()) == false)
    {
      BKE_report(op->reports, RPT_ERROR, "No data in the internal clipboard to paste");
      return OPERATOR_CANCELLED;
    }
  }
  else if (ac.datatype == ANIMCONT_MASK) {
    /* FIXME: support this case. */
    BKE_report(op->reports,
               RPT_ERROR,
               "Keyframe pasting is not available for Grease Pencil or mask mode");
    return OPERATOR_CANCELLED;
  }
  else {
    /* Both paste function needs to be evaluated to account for mixed selection */
    const eKeyPasteError kf_empty = paste_action_keys(&ac, offset_mode, merge_mode, flipped);
    /* non-zero return means an error occurred while trying to paste */
    gpframes_inbuf = ED_gpencil_anim_copybuf_paste(&ac, offset_mode) ||
                     blender::ed::greasepencil::grease_pencil_paste_keyframes(
                         &ac, offset_mode, merge_mode, get_grease_pencil_keyframe_clipboard());

    /* Only report an error if nothing was pasted, i.e. when both FCurve and GPencil failed. */
    if (!gpframes_inbuf) {
      switch (kf_empty) {
        case KEYFRAME_PASTE_OK:
          /* FCurve paste was ok, so it's all good. */
          break;

        case KEYFRAME_PASTE_NOWHERE_TO_PASTE:
          BKE_report(op->reports, RPT_ERROR, "No selected F-Curves to paste into");
          return OPERATOR_CANCELLED;

        case KEYFRAME_PASTE_NOTHING_TO_PASTE:
          BKE_report(op->reports, RPT_ERROR, "No data in the internal clipboard to paste");
          return OPERATOR_CANCELLED;
      }
    }
  }

  /* Grease Pencil needs extra update to refresh the added keyframes. */
  if (ac.datatype == ANIMCONT_GPENCIL || gpframes_inbuf) {
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, nullptr);
  }
  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static std::string actkeys_paste_get_description(bContext * /*C*/,
                                                 wmOperatorType * /*ot*/,
                                                 PointerRNA *ptr)
{
  /* Custom description if the 'flipped' option is used. */
  if (RNA_boolean_get(ptr, "flipped")) {
    return BLI_strdup(TIP_("Paste keyframes from mirrored bones if they exist"));
  }

  /* Use the default description in the other cases. */
  return "";
}

void ACTION_OT_paste(wmOperatorType *ot)
{
  PropertyRNA *prop;
  /* identifiers */
  ot->name = "Paste Keyframes";
  ot->idname = "ACTION_OT_paste";
  ot->description =
      "Paste keyframes from the internal clipboard for the selected channels, starting on the "
      "current "
      "frame";

  /* API callbacks. */
  //  ot->invoke = WM_operator_props_popup; /* Better wait for action redo panel. */
  ot->get_description = actkeys_paste_get_description;
  ot->exec = actkeys_paste_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_enum(ot->srna,
               "offset",
               rna_enum_keyframe_paste_offset_items,
               KEYFRAME_PASTE_OFFSET_CFRA_START,
               "Offset",
               "Paste time offset of keys");
  RNA_def_enum(ot->srna,
               "merge",
               rna_enum_keyframe_paste_merge_items,
               KEYFRAME_PASTE_MERGE_MIX,
               "Type",
               "Method of merging pasted keys and existing");
  prop = RNA_def_boolean(
      ot->srna, "flipped", false, "Flipped", "Paste keyframes from mirrored bones if they exist");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes: Insert Operator
 * \{ */

/* defines for insert keyframes tool */
static const EnumPropertyItem prop_actkeys_insertkey_types[] = {
    {1, "ALL", 0, "All Channels", ""},
    {2, "SEL", 0, "Only Selected Channels", ""},
    /* XXX not in all cases. */
    {3, "GROUP", 0, "In Active Group", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void insert_gpencil_key(bAnimContext *ac,
                               bAnimListElem *ale,
                               const eGP_GetFrame_Mode add_frame_mode,
                               bGPdata **gpd_old)
{
  Scene *scene = ac->scene;
  bGPdata *gpd = (bGPdata *)ale->id;
  bGPDlayer *gpl = (bGPDlayer *)ale->data;
  BKE_gpencil_layer_frame_get(gpl, scene->r.cfra, add_frame_mode);
  /* Check if the gpd changes to tag only once. */
  if (gpd != *gpd_old) {
    BKE_gpencil_tag(gpd);
    *gpd_old = gpd;
  }
}

static void insert_grease_pencil_key(bAnimContext *ac,
                                     bAnimListElem *ale,
                                     const bool hold_previous)
{
  using namespace blender::bke::greasepencil;
  Layer *layer = static_cast<Layer *>(ale->data);
  GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
  const int current_frame_number = ac->scene->r.cfra;

  if (layer->frames().contains(current_frame_number)) {
    return;
  }

  bool changed = false;
  if (hold_previous) {
    const std::optional<int> active_frame_number = layer->start_frame_at(current_frame_number);
    if (!active_frame_number) {
      /* There is no active frame to hold to, or it's an end frame. Therefore just insert a blank
       * frame. */
      changed |= grease_pencil->insert_frame(*layer, current_frame_number) != nullptr;
    }
    else {
      /* Duplicate the active frame. */
      changed = grease_pencil->insert_duplicate_frame(
          *layer, *active_frame_number, current_frame_number, false);
    }
  }
  else {
    /* Insert a blank frame. */
    changed |= grease_pencil->insert_frame(*layer, current_frame_number) != nullptr;
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
  }
}

static void insert_fcurve_key(bAnimContext *ac,
                              bAnimListElem *ale,
                              const AnimationEvalContext anim_eval_context,
                              eInsertKeyFlags flag)
{
  using namespace blender::animrig;
  FCurve *fcu = (FCurve *)ale->key_data;

  ReportList *reports = ac->reports;
  Scene *scene = ac->scene;
  ToolSettings *ts = scene->toolsettings;

  /* These asserts are ensuring that the fcurve we're keying lives on an Action,
   * rather than being e.g. an fcurve a driver or NLA Strip. This should
   * always hold true for this function, since all the other cases take
   * different code paths before getting here. */
  BLI_assert(ale->owner == nullptr);
  BLI_assert(ale->fcurve_owner_id != nullptr);
  BLI_assert(GS(ale->fcurve_owner_id->name) == ID_AC);

  bAction *action = reinterpret_cast<bAction *>(ale->fcurve_owner_id);
  ID *id = action_slot_get_id_for_keying(*ale->bmain, action->wrap(), ale->slot_handle, ale->id);

  /* If we found an unambiguous ID to use for keying the channel, go through the
   * normal keyframing code path.  Otherwise, just directly key the fcurve
   * itself. */
  if (id) {
    const std::optional<blender::StringRefNull> channel_group = fcu->grp ?
                                                                    std::optional(fcu->grp->name) :
                                                                    std::nullopt;
    PointerRNA id_rna_pointer = RNA_id_pointer_create(id);
    CombinedKeyingResult result = insert_keyframes(ac->bmain,
                                                   &id_rna_pointer,
                                                   channel_group,
                                                   {{fcu->rna_path, {}, fcu->array_index}},
                                                   std::nullopt,
                                                   anim_eval_context,
                                                   eBezTriple_KeyframeType(ts->keyframe_type),
                                                   flag);
    if (result.get_count(SingleKeyingResult::SUCCESS) == 0) {
      result.generate_reports(reports);
    }
  }
  else {
    /* TODO: when layered action strips are allowed to have time offsets, that
     * mapping will need to be handled here. */
    assert_baklava_phase_1_invariants(action->wrap());

    /* Adjust current frame for NLA-scaling. */
    const float cfra = ANIM_nla_tweakedit_remap(
        ale, anim_eval_context.eval_time, NLATIME_CONVERT_UNMAP);

    const float curval = evaluate_fcurve(fcu, cfra);
    KeyframeSettings settings = get_keyframe_settings(true);
    settings.keyframe_type = eBezTriple_KeyframeType(ts->keyframe_type);
    insert_vert_fcurve(fcu, {cfra, curval}, settings, eInsertKeyFlags(0));
  }

  ale->update |= ANIM_UPDATE_DEFAULT;
}

/* this function is responsible for inserting new keyframes */
static void insert_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  Scene *scene = ac->scene;
  ToolSettings *ts = scene->toolsettings;
  eInsertKeyFlags flag;

  eGP_GetFrame_Mode add_frame_mode;
  bGPdata *gpd_old = nullptr;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_NODUPLIS);
  if (mode == 2) {
    filter |= ANIMFILTER_SEL;
  }
  else if (mode == 3) {
    filter |= ANIMFILTER_ACTGROUPED;
  }

  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Init keyframing flag. */
  flag = blender::animrig::get_keyframing_flags(scene);

  /* GPLayers specific flags */
  if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) {
    add_frame_mode = GP_GETFRAME_ADD_COPY;
  }
  else {
    add_frame_mode = GP_GETFRAME_ADD_NEW;
  }
  const bool grease_pencil_hold_previous = ((ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) != 0);

  /* insert keyframes */
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      ac->depsgraph, float(scene->r.cfra));
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->type) {
      case ANIMTYPE_GPLAYER:
        insert_gpencil_key(ac, ale, add_frame_mode, &gpd_old);
        break;

      case ANIMTYPE_GREASE_PENCIL_LAYER:
        insert_grease_pencil_key(ac, ale, grease_pencil_hold_previous);
        break;

      case ANIMTYPE_FCURVE:
        insert_fcurve_key(ac, ale, anim_eval_context, flag);
        break;

      default:
        BLI_assert_msg(false, "Keys cannot be inserted into this animation type.");
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_insertkey_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ac.datatype == ANIMCONT_MASK) {
    BKE_report(op->reports, RPT_ERROR, "Insert Keyframes is not yet implemented for this mode");
    return OPERATOR_CANCELLED;
  }

  /* what channels to affect? */
  mode = RNA_enum_get(op->ptr, "type");

  ANIM_deselect_keys_in_animation_editors(C);

  /* insert keyframes */
  insert_action_keys(&ac, mode);

  /* set notifier that keyframes have changed */
  if (ac.datatype == ANIMCONT_GPENCIL) {
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_keyframe_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Keyframes";
  ot->idname = "ACTION_OT_keyframe_insert";
  ot->description = "Insert keyframes for the specified channels";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_insertkey_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_insertkey_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes: Duplicate Operator
 * \{ */

static bool duplicate_action_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;
  bool changed = false;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* loop through filtered data and delete selected keys */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
      changed |= duplicate_fcurve_keys((FCurve *)ale->key_data);
    }
    else if (ale->type == ANIMTYPE_GPLAYER) {
      ED_gpencil_layer_frames_duplicate((bGPDlayer *)ale->data);
      changed |= ED_gpencil_layer_frame_select_check((bGPDlayer *)ale->data);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      changed |= blender::ed::greasepencil::duplicate_selected_frames(
          *reinterpret_cast<GreasePencil *>(ale->id),
          static_cast<GreasePencilLayer *>(ale->data)->wrap());
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      changed |= ED_masklayer_frames_duplicate((MaskLayer *)ale->data);
    }
    else {
      BLI_assert(0);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  return changed;
}

/* ------------------- */

static wmOperatorStatus actkeys_duplicate_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* duplicate keyframes */
  if (!duplicate_action_keys(&ac)) {
    return OPERATOR_CANCELLED;
  }

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Keyframes";
  ot->idname = "ACTION_OT_duplicate";
  ot->description = "Make a copy of all selected keyframes";

  /* API callbacks. */
  ot->exec = actkeys_duplicate_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes: Delete Operator
 * \{ */

static bool delete_action_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;
  bool changed_final = false;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* loop through filtered data and delete selected keys */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    bool changed = false;

    if (ale->type == ANIMTYPE_GPLAYER) {
      changed = ED_gpencil_layer_frames_delete((bGPDlayer *)ale->data);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
      changed = blender::ed::greasepencil::remove_all_selected_frames(
          *grease_pencil, static_cast<GreasePencilLayer *>(ale->data)->wrap());

      if (changed) {
        DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
      }
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      changed = ED_masklayer_frames_delete((MaskLayer *)ale->data);
    }
    else {
      FCurve *fcu = static_cast<FCurve *>(ale->key_data);
      changed = BKE_fcurve_delete_keys_selected(fcu);

      if (changed && BKE_fcurve_is_empty(fcu)) {
        ED_anim_ale_fcurve_delete(*ac, *ale);
        ale->key_data = nullptr;
      }
    }

    if (changed) {
      ale->update |= ANIM_UPDATE_DEFAULT;
      changed_final = true;
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  return changed_final;
}

/* ------------------- */

static wmOperatorStatus actkeys_delete_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* delete keyframes */
  if (!delete_action_keys(&ac)) {
    return OPERATOR_CANCELLED;
  }

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus actkeys_delete_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Delete selected keyframes?"),
                                  nullptr,
                                  IFACE_("Delete"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return actkeys_delete_exec(C, op);
}

void ACTION_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframes";
  ot->idname = "ACTION_OT_delete";
  ot->description = "Remove all selected keyframes";

  /* API callbacks. */
  ot->invoke = actkeys_delete_invoke;
  ot->exec = actkeys_delete_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes: Clean Operator
 * \{ */

static void clean_action_keys(bAnimContext *ac, float thresh, bool clean_chan)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);

  if (clean_chan) {
    filter |= ANIMFILTER_SEL;
  }

  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  const bool only_selected_keys = !clean_chan;
  /* loop through filtered data and clean curves */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    clean_fcurve(ale, thresh, clean_chan, only_selected_keys);

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_clean_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  float thresh;
  bool clean_chan;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented");
    return OPERATOR_PASS_THROUGH;
  }

  /* get cleaning threshold */
  thresh = RNA_float_get(op->ptr, "threshold");
  clean_chan = RNA_boolean_get(op->ptr, "channels");

  /* clean keyframes */
  clean_action_keys(&ac, thresh, clean_chan);

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_clean(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clean Keyframes";
  ot->idname = "ACTION_OT_clean";
  ot->description = "Simplify F-Curves by removing closely spaced keyframes";

  /* API callbacks. */
  // ot->invoke =  /* XXX we need that number popup for this! */
  ot->exec = actkeys_clean_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_float(
      ot->srna, "threshold", 0.001f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 1000.0f);
  RNA_def_boolean(ot->srna, "channels", false, "Channels", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframes: Sample Operator
 * \{ */

/* Evaluates the curves between each selected keyframe on each frame, and keys the value. */
static void bake_action_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and add keys between selected keyframes on every frame. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    blender::animrig::bake_fcurve_segments((FCurve *)ale->key_data);

    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_bake_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented");
    return OPERATOR_PASS_THROUGH;
  }

  /* sample keyframes */
  bake_action_keys(&ac);

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_bake_keys(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake Keyframes";
  ot->idname = "ACTION_OT_bake_keys";
  ot->description = "Add keyframes on every frame between the selected keyframes";

  /* API callbacks. */
  ot->exec = actkeys_bake_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings: Set Extrapolation-Type Operator
 * \{ */

/* defines for make/clear cyclic extrapolation tools */
#define MAKE_CYCLIC_EXPO -1
#define CLEAR_CYCLIC_EXPO -2

/* defines for set extrapolation-type for selected keyframes tool */
static const EnumPropertyItem prop_actkeys_expo_types[] = {
    {FCURVE_EXTRAPOLATE_CONSTANT,
     "CONSTANT",
     0,
     "Constant Extrapolation",
     "Values on endpoint keyframes are held"},
    {FCURVE_EXTRAPOLATE_LINEAR,
     "LINEAR",
     0,
     "Linear Extrapolation",
     "Straight-line slope of end segments are extended past the endpoint keyframes"},

    {MAKE_CYCLIC_EXPO,
     "MAKE_CYCLIC",
     0,
     "Make Cyclic (F-Modifier)",
     "Add Cycles F-Modifier if one does not exist already"},
    {CLEAR_CYCLIC_EXPO,
     "CLEAR_CYCLIC",
     0,
     "Clear Cyclic (F-Modifier)",
     "Remove Cycles F-Modifier if not needed anymore"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* this function is responsible for setting extrapolation mode for keyframes */
static void setexpo_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_SEL | ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* loop through setting mode per F-Curve */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;

    if (mode >= 0) {
      /* just set mode setting */
      fcu->extend = mode;
    }
    else {
      /* shortcuts for managing Cycles F-Modifiers to make it easier to toggle cyclic animation
       * without having to go through FModifier UI in Graph Editor to do so
       */
      if (mode == MAKE_CYCLIC_EXPO) {
        /* only add if one doesn't exist */
        if (list_has_suitable_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, -1) == 0) {
          /* TODO: add some more preset versions which set different extrapolation options? */
          add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, fcu);
        }
      }
      else if (mode == CLEAR_CYCLIC_EXPO) {
        /* remove all the modifiers fitting this description */
        FModifier *fcm, *fcn = nullptr;

        for (fcm = static_cast<FModifier *>(fcu->modifiers.first); fcm; fcm = fcn) {
          fcn = fcm->next;

          if (fcm->type == FMODIFIER_TYPE_CYCLES) {
            remove_fmodifier(&fcu->modifiers, fcm);
          }
        }
      }
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_expo_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented");
    return OPERATOR_PASS_THROUGH;
  }

  /* get handle setting mode */
  mode = RNA_enum_get(op->ptr, "type");

  /* set handle type */
  setexpo_action_keys(&ac, mode);

  /* set notifier that keyframe properties have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_extrapolation_type(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set F-Curve Extrapolation";
  ot->idname = "ACTION_OT_extrapolation_type";
  ot->description = "Set extrapolation mode for selected F-Curves";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_expo_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_expo_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings: Set Interpolation-Type Operator
 * \{ */

static wmOperatorStatus actkeys_ipo_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented");
    return OPERATOR_PASS_THROUGH;
  }

  /* get handle setting mode */
  mode = RNA_enum_get(op->ptr, "type");

  /* set handle type */
  ANIM_animdata_keyframe_callback(&ac,
                                  (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                   ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS |
                                   ANIMFILTER_FCURVESONLY),
                                  ANIM_editkeyframes_ipo(mode));

  /* set notifier that keyframe properties have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_interpolation_type(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Keyframe Interpolation";
  ot->idname = "ACTION_OT_interpolation_type";
  ot->description =
      "Set interpolation mode for the F-Curve segments starting from the selected keyframes";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_ipo_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_beztriple_interpolation_mode_items, 0, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_ACTION);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings: Set Easing Operator
 * \{ */

static wmOperatorStatus actkeys_easing_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get handle setting mode */
  mode = RNA_enum_get(op->ptr, "type");

  /* set handle type */
  ANIM_animdata_keyframe_callback(&ac,
                                  (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                   ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS |
                                   ANIMFILTER_FCURVESONLY),
                                  ANIM_editkeyframes_easing(mode));

  /* set notifier that keyframe properties have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_easing_type(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Keyframe Easing Type";
  ot->idname = "ACTION_OT_easing_type";
  ot->description =
      "Set easing type for the F-Curve segments starting from the selected keyframes";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_easing_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_beztriple_interpolation_easing_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings: Set Handle-Type Operator
 * \{ */

/* this function is responsible for setting handle-type of selected keyframes */
static void sethandles_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditFunc edit_cb = ANIM_editkeyframes_handles(mode);
  KeyframeEditFunc sel_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through setting flags for handles
   * NOTE: we do not supply KeyframeEditData to the looper yet.
   * Currently that's not necessary here.
   */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* any selected keyframes for editing? */
    if (ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, sel_cb, nullptr)) {
      /* change type of selected handles */
      ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, edit_cb, BKE_fcurve_handles_recalc);

      ale->update |= ANIM_UPDATE_DEFAULT;
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_handletype_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented");
    return OPERATOR_PASS_THROUGH;
  }

  /* get handle setting mode */
  mode = RNA_enum_get(op->ptr, "type");

  /* set handle type */
  sethandles_action_keys(&ac, mode);

  /* set notifier that keyframe properties have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_handle_type(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Keyframe Handle Type";
  ot->idname = "ACTION_OT_handle_type";
  ot->description = "Set type of handle for selected keyframes";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_handletype_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_keyframe_handle_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings: Set Keyframe-Type Operator
 * \{ */

/* this function is responsible for setting keyframe type for keyframes */
static void setkeytype_action_keys(bAnimContext *ac, eBezTriple_KeyframeType mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;
  KeyframeEditFunc set_cb = ANIM_editkeyframes_keytype(mode);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through setting BezTriple interpolation
   * NOTE: we do not supply KeyframeEditData to the looper yet.
   * Currently that's not necessary here.
   */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->type) {
      case ANIMTYPE_GPLAYER:
        ED_gpencil_layer_frames_keytype_set(static_cast<bGPDlayer *>(ale->data), mode);
        ale->update |= ANIM_UPDATE_DEPS;
        break;

      case ANIMTYPE_GREASE_PENCIL_LAYER:
        blender::ed::greasepencil::set_selected_frames_type(
            static_cast<GreasePencilLayer *>(ale->data)->wrap(), mode);
        ale->update |= ANIM_UPDATE_DEPS;
        break;

      case ANIMTYPE_FCURVE:
        ANIM_fcurve_keyframes_loop(
            nullptr, static_cast<FCurve *>(ale->key_data), nullptr, set_cb, nullptr);
        ale->update |= ANIM_UPDATE_DEPS | ANIM_UPDATE_HANDLES;
        break;

      default:
        BLI_assert_msg(false, "Keytype cannot be set into this animation type.");
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_keytype_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ac.datatype == ANIMCONT_MASK) {
    BKE_report(op->reports, RPT_ERROR, "Not implemented for Masks");
    return OPERATOR_PASS_THROUGH;
  }

  const int mode = RNA_enum_get(op->ptr, "type");
  setkeytype_action_keys(&ac, eBezTriple_KeyframeType(mode));

  /* set notifier that keyframe properties have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_keyframe_type(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Keyframe Type";
  ot->idname = "ACTION_OT_keyframe_type";
  ot->description = "Set type of keyframe for the selected keyframes";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_keytype_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_beztriple_keyframe_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform: Jump to Selected Frames Operator
 * \{ */

static bool actkeys_framejump_poll(bContext *C)
{
  /* prevent changes during render */
  if (G.is_rendering) {
    return false;
  }

  return ED_operator_action_active(C);
}

/* snap current-frame indicator to 'average time' of selected keyframe */
static wmOperatorStatus actkeys_framejump_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;
  KeyframeEditData ked = {{nullptr}};

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* init edit data */
  /* loop over action data, averaging values */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->datatype) {
      case ALE_GPFRAME: {
        bGPDlayer *gpl = static_cast<bGPDlayer *>(ale->data);

        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          /* only if selected */
          if (!(gpf->flag & GP_FRAME_SELECT)) {
            continue;
          }
          /* store average time in float 1 (only do rounding at last step) */
          ked.f1 += gpf->framenum;

          /* increment number of items */
          ked.i1++;
        }
        break;
      }

      case ALE_GREASE_PENCIL_CEL: {
        using namespace blender::bke::greasepencil;
        const Layer &layer = *static_cast<Layer *>(ale->data);
        for (auto [frame_number, frame] : layer.frames().items()) {
          if (!frame.is_selected()) {
            continue;
          }
          ked.f1 += frame_number;
          ked.i1++;
        }
        break;
      }

      case ALE_FCURVE: {
        FCurve *fcurve = static_cast<FCurve *>(ale->key_data);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, false, true);
        ANIM_fcurve_keyframes_loop(&ked, fcurve, nullptr, bezt_calc_average, nullptr);
        ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, true, true);
        break;
      }

      default:
        BLI_assert_msg(false, "Cannot jump to keyframe into this animation type.");
    }
  }

  ANIM_animdata_freelist(&anim_data);

  /* set the new current frame value, based on the average time */
  if (ked.i1) {
    Scene *scene = ac.scene;
    scene->r.cfra = round_fl_to_int(ked.f1 / ked.i1);
    scene->r.subframe = 0.0f;
  }

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);

  return OPERATOR_FINISHED;
}

void ACTION_OT_frame_jump(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Jump to Keyframes";
  ot->idname = "ACTION_OT_frame_jump";
  ot->description = "Set the current frame to the average frame value of selected keyframes";

  /* API callbacks. */
  ot->exec = actkeys_framejump_exec;
  ot->poll = actkeys_framejump_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform: Snap Keyframes Operator
 * \{ */

/* defines for snap keyframes tool */
static const EnumPropertyItem prop_actkeys_snap_types[] = {
    {ACTKEYS_SNAP_CFRA,
     "CFRA",
     0,
     "Selection to Current Frame",
     "Snap selected keyframes to the current frame"},
    {ACTKEYS_SNAP_NEAREST_FRAME,
     "NEAREST_FRAME",
     0,
     "Selection to Nearest Frame",
     "Snap selected keyframes to the nearest (whole) frame "
     "(use to fix accidental subframe offsets)"},
    {ACTKEYS_SNAP_NEAREST_SECOND,
     "NEAREST_SECOND",
     0,
     "Selection to Nearest Second",
     "Snap selected keyframes to the nearest second"},
    {ACTKEYS_SNAP_NEAREST_MARKER,
     "NEAREST_MARKER",
     0,
     "Selection to Nearest Marker",
     "Snap selected keyframes to the nearest marker"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* this function is responsible for snapping keyframes to frame-times */
static void snap_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditData ked = {{nullptr}};
  KeyframeEditFunc edit_cb;

  /* filter data */
  if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT);
  }
  else {
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
              ANIMFILTER_NODUPLIS);
  }
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* get beztriple editing callbacks */
  edit_cb = ANIM_editkeyframes_snap(mode);

  ked.scene = ac->scene;
  if (mode == ACTKEYS_SNAP_NEAREST_MARKER) {
    ked.list.first = (ac->markers) ? ac->markers->first : nullptr;
    ked.list.last = (ac->markers) ? ac->markers->last : nullptr;
  }

  /* snap keyframes */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ale->type == ANIMTYPE_GPLAYER) {
      ED_gpencil_layer_snap_frames(static_cast<bGPDlayer *>(ale->data), ac->scene, mode);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
      GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ale->data);

      const bool changed = blender::ed::greasepencil::snap_selected_frames(
          *grease_pencil, layer->wrap(), *(ac->scene), static_cast<eEditKeyframes_Snap>(mode));

      if (changed) {
        DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
      }
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      ED_masklayer_snap_frames(static_cast<MaskLayer *>(ale->data), ac->scene, mode);
    }
    else {
      FCurve *fcurve = static_cast<FCurve *>(ale->key_data);
      ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, false, false);
      ANIM_fcurve_keyframes_loop(&ked, fcurve, nullptr, edit_cb, BKE_fcurve_handles_recalc);
      BKE_fcurve_merge_duplicate_keys(
          fcurve, SELECT, false); /* only use handles in graph editor */
      ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, true, false);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_snap_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get snapping mode */
  mode = RNA_enum_get(op->ptr, "type");

  /* snap keyframes */
  snap_action_keys(&ac, mode);

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_snap(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Keys";
  ot->idname = "ACTION_OT_snap";
  ot->description = "Snap selected keyframes to the times specified";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_snap_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_snap_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform: Mirror Keyframes Operator
 * \{ */

/* defines for mirror keyframes tool */
static const EnumPropertyItem prop_actkeys_mirror_types[] = {
    {ACTKEYS_MIRROR_CFRA,
     "CFRA",
     0,
     "By Times Over Current Frame",
     "Flip times of selected keyframes using the current frame as the mirror line"},
    {ACTKEYS_MIRROR_XAXIS,
     "XAXIS",
     0,
     "By Values Over Zero Value",
     "Flip values of selected keyframes (i.e. negative values become positive, and vice versa)"},
    {ACTKEYS_MIRROR_MARKER,
     "MARKER",
     0,
     "By Times Over First Selected Marker",
     "Flip times of selected keyframes using the first selected marker as the reference point"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* this function is responsible for mirroring keyframes */
static void mirror_action_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter;

  KeyframeEditData ked = {{nullptr}};
  KeyframeEditFunc edit_cb;

  /* get beztriple editing callbacks */
  edit_cb = ANIM_editkeyframes_mirror(mode);

  ked.scene = ac->scene;

  /* for 'first selected marker' mode, need to find first selected marker first! */
  /* XXX should this be made into a helper func in the API? */
  if (mode == ACTKEYS_MIRROR_MARKER) {
    TimeMarker *marker = ED_markers_get_first_selected(ac->markers);

    if (marker) {
      ked.f1 = float(marker->frame);
    }
    else {
      return;
    }
  }

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* mirror keyframes */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ale->type == ANIMTYPE_GPLAYER) {
      ED_gpencil_layer_mirror_frames(static_cast<bGPDlayer *>(ale->data), ac->scene, mode);
    }
    else if (ale->type == ANIMTYPE_GREASE_PENCIL_LAYER) {
      GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
      GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ale->data);

      const bool changed = blender::ed::greasepencil::mirror_selected_frames(
          *grease_pencil, layer->wrap(), *(ac->scene), static_cast<eEditKeyframes_Mirror>(mode));

      if (changed) {
        DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
      }
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      /* TODO */
    }
    else {
      FCurve *fcurve = static_cast<FCurve *>(ale->key_data);
      ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, false, false);
      ANIM_fcurve_keyframes_loop(&ked, fcurve, nullptr, edit_cb, BKE_fcurve_handles_recalc);
      ANIM_nla_mapping_apply_if_needed_fcurve(ale, fcurve, true, false);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static wmOperatorStatus actkeys_mirror_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get mirroring mode */
  mode = RNA_enum_get(op->ptr, "type");

  /* mirror keyframes */
  mirror_action_keys(&ac, mode);

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror Keys";
  ot->idname = "ACTION_OT_mirror";
  ot->description = "Flip selected keyframes over the selected mirror line";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = actkeys_mirror_exec;
  ot->poll = ED_operator_action_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_mirror_types, 0, "Type", "");
}

/** \} */
