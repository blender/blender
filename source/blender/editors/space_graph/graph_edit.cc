/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 *
 * Insert duplicate and bake keyframes.
 */

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef WITH_AUDASPACE
#  include <AUD_Special.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_rotation.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "BLT_translation.hh"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_nla.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_build.hh"

#include "UI_view2d.hh"

#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"
#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_keyframing.hh"
#include "ED_markers.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "graph_intern.h"

/* -------------------------------------------------------------------- */
/** \name Insert Keyframes Operator
 * \{ */

/* Mode defines for insert keyframes tool. */
enum eGraphKeys_InsertKey_Types {
  GRAPHKEYS_INSERTKEY_ALL = (1 << 0),
  GRAPHKEYS_INSERTKEY_SEL = (1 << 1),
  GRAPHKEYS_INSERTKEY_CURSOR = (1 << 2),
  GRAPHKEYS_INSERTKEY_ACTIVE = (1 << 3),
};

/* RNA mode types for insert keyframes tool. */
static const EnumPropertyItem prop_graphkeys_insertkey_types[] = {
    {GRAPHKEYS_INSERTKEY_ALL,
     "ALL",
     0,
     "All Channels",
     "Insert a keyframe on all visible and editable F-Curves using each curve's current value"},
    {GRAPHKEYS_INSERTKEY_SEL,
     "SEL",
     0,
     "Only Selected Channels",
     "Insert a keyframe on selected F-Curves using each curve's current value"},
    {GRAPHKEYS_INSERTKEY_ACTIVE,
     "ACTIVE",
     0,
     "Only Active F-Curve",
     "Insert a keyframe on the active F-Curve using the curve's current value"},
    {GRAPHKEYS_INSERTKEY_ACTIVE | GRAPHKEYS_INSERTKEY_CURSOR,
     "CURSOR_ACTIVE",
     0,
     "Active Channels at Cursor",
     "Insert a keyframe for the active F-Curve at the cursor point"},
    {GRAPHKEYS_INSERTKEY_SEL | GRAPHKEYS_INSERTKEY_CURSOR,
     "CURSOR_SEL",
     0,
     "Selected Channels at Cursor",
     "Insert a keyframe for selected F-Curves at the cursor point"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* This function is responsible for snapping keyframes to frame-times. */
static void insert_graph_keys(bAnimContext *ac, eGraphKeys_InsertKey_Types mode)
{
  using namespace blender::animrig;
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  size_t num_items;

  ReportList *reports = ac->reports;
  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  Scene *scene = ac->scene;
  ToolSettings *ts = scene->toolsettings;
  eInsertKeyFlags flag = eInsertKeyFlags(0);

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  if (mode & GRAPHKEYS_INSERTKEY_SEL) {
    filter |= ANIMFILTER_SEL;
  }
  else if (mode & GRAPHKEYS_INSERTKEY_ACTIVE) {
    filter |= ANIMFILTER_ACTIVE;
  }

  num_items = ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));
  if (num_items == 0) {
    if (mode & GRAPHKEYS_INSERTKEY_ACTIVE) {
      BKE_report(reports,
                 RPT_ERROR,
                 "No active F-Curve to add a keyframe to. Select an editable F-Curve first");
    }
    else if (mode & GRAPHKEYS_INSERTKEY_SEL) {
      BKE_report(reports, RPT_ERROR, "No selected F-Curves to add keyframes to");
    }
    else {
      BKE_report(reports, RPT_ERROR, "No channels to add keyframes to");
    }

    return;
  }

  /* Init key-framing flag. */
  flag = ANIM_get_keyframing_flags(scene);
  KeyframeSettings settings = get_keyframe_settings(true);
  settings.keyframe_type = eBezTriple_KeyframeType(ts->keyframe_type);

  /* Insert keyframes. */
  if (mode & GRAPHKEYS_INSERTKEY_CURSOR) {
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      AnimData *adt = ANIM_nla_mapping_get(ac, ale);
      FCurve *fcu = (FCurve *)ale->key_data;

      short mapping_flag = ANIM_get_normalization_flags(ac->sl);
      float offset;
      float unit_scale = ANIM_unit_mapping_get_factor(
          ac->scene, ale->id, static_cast<FCurve *>(ale->key_data), mapping_flag, &offset);

      float x, y;

      /* perform time remapping for x-coordinate (if necessary) */
      if ((sipo) && (sipo->mode == SIPO_MODE_DRIVERS)) {
        x = sipo->cursorTime;
      }
      else if (adt) {
        x = BKE_nla_tweakedit_remap(adt, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);
      }
      else {
        x = float(scene->r.cfra);
      }

      /* Normalize units of cursor's value. */
      if (sipo) {
        y = (sipo->cursorVal / unit_scale) - offset;
      }
      else {
        y = 0.0f;
      }

      /* Insert keyframe directly into the F-Curve. */
      insert_vert_fcurve(fcu, {x, y}, settings, eInsertKeyFlags(0));

      ale->update |= ANIM_UPDATE_DEFAULT;
    }
  }
  else {
    const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
        ac->depsgraph, float(scene->r.cfra));
    LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
      FCurve *fcu = (FCurve *)ale->key_data;

      /* Read value from property the F-Curve represents, or from the curve only?
       *
       * - ale->id != nullptr:
       *   Typically, this means that we have enough info to try resolving the path.
       * - ale->owner != nullptr:
       *   If this is set, then the path may not be resolvable from the ID alone,
       *   so it's easier for now to just read the F-Curve directly.
       *   (TODO: add the full-blown PointerRNA relative parsing case here... (Joshua Leung 2015))
       * - fcu->driver != nullptr:
       *   If this is set, then it's a driver. If we don't check for this, we'd end
       *   up adding the keyframes on a new F-Curve in the action data instead.
       */
      if (ale->id && !ale->owner && !fcu->driver) {
        insert_keyframe(ac->bmain,
                        reports,
                        ale->id,
                        nullptr,
                        ((fcu->grp) ? (fcu->grp->name) : (nullptr)),
                        fcu->rna_path,
                        fcu->array_index,
                        &anim_eval_context,
                        eBezTriple_KeyframeType(ts->keyframe_type),
                        flag);
      }
      else {
        AnimData *adt = ANIM_nla_mapping_get(ac, ale);

        /* Adjust current frame for NLA-mapping. */
        float cfra = float(scene->r.cfra);
        if ((sipo) && (sipo->mode == SIPO_MODE_DRIVERS)) {
          cfra = sipo->cursorTime;
        }
        else if (adt) {
          cfra = BKE_nla_tweakedit_remap(adt, float(scene->r.cfra), NLATIME_CONVERT_UNMAP);
        }

        const float curval = evaluate_fcurve_only_curve(fcu, cfra);
        insert_vert_fcurve(fcu, {cfra, curval}, settings, eInsertKeyFlags(0));
      }

      ale->update |= ANIM_UPDATE_DEFAULT;
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_insertkey_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  eGraphKeys_InsertKey_Types mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Which channels to affect? */
  mode = eGraphKeys_InsertKey_Types(RNA_enum_get(op->ptr, "type"));

  /* Insert keyframes. */
  insert_graph_keys(&ac, mode);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_keyframe_insert(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Insert Keyframes";
  ot->idname = "GRAPH_OT_keyframe_insert";
  ot->description = "Insert keyframes for the specified channels";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graphkeys_insertkey_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_insertkey_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Click-Insert Keyframes Operator
 * \{ */

static int graphkeys_click_insert_exec(bContext *C, wmOperator *op)
{
  using namespace blender::animrig;
  bAnimContext ac;
  bAnimListElem *ale;
  AnimData *adt;
  FCurve *fcu;
  float frame, val;

  /* Get animation context. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get active F-Curve 'anim-list-element'. */
  ale = get_active_fcurve_channel(&ac);
  if (ELEM(nullptr, ale, ale->data)) {
    if (ale) {
      MEM_freeN(ale);
    }
    return OPERATOR_CANCELLED;
  }
  fcu = static_cast<FCurve *>(ale->data);

  /* When there are F-Modifiers on the curve, only allow adding
   * keyframes if these will be visible after doing so...
   */
  if (BKE_fcurve_is_keyframable(fcu)) {
    ListBase anim_data;
    ToolSettings *ts = ac.scene->toolsettings;

    short mapping_flag = ANIM_get_normalization_flags(ac.sl);
    float scale, offset;

    /* Preserve selection? */
    if (RNA_boolean_get(op->ptr, "extend") == false) {
      /* Deselect all keyframes first,
       * so that we can immediately start manipulating the newly added one(s)
       * - only affect the keyframes themselves, as we don't want channels popping in and out. */
      deselect_graph_keys(&ac, false, SELECT_SUBTRACT, false);
    }

    /* Get frame and value from props. */
    frame = RNA_float_get(op->ptr, "frame");
    val = RNA_float_get(op->ptr, "value");

    /* Apply inverse NLA-mapping to frame to get correct time in un-scaled action. */
    adt = ANIM_nla_mapping_get(&ac, ale);
    frame = BKE_nla_tweakedit_remap(adt, frame, NLATIME_CONVERT_UNMAP);

    /* Apply inverse unit-mapping to value to get correct value for F-Curves. */
    scale = ANIM_unit_mapping_get_factor(
        ac.scene, ale->id, fcu, mapping_flag | ANIM_UNITCONV_RESTORE, &offset);

    val = val * scale - offset;

    KeyframeSettings settings = get_keyframe_settings(true);
    settings.keyframe_type = eBezTriple_KeyframeType(ts->keyframe_type);

    /* Insert keyframe on the specified frame + value. */
    insert_vert_fcurve(fcu, {frame, val}, settings, eInsertKeyFlags(0));

    ale->update |= ANIM_UPDATE_DEPS;

    BLI_listbase_clear(&anim_data);
    BLI_addtail(&anim_data, ale);

    ANIM_animdata_update(&ac, &anim_data);
  }
  else {
    /* Warn about why this can't happen. */
    if (fcu->fpt) {
      BKE_report(op->reports, RPT_ERROR, "Keyframes cannot be added to sampled F-Curves");
    }
    else if (fcu->flag & FCURVE_PROTECTED) {
      BKE_report(op->reports, RPT_ERROR, "Active F-Curve is not editable");
    }
    else {
      BKE_report(op->reports, RPT_ERROR, "Remove F-Modifiers from F-Curve to add keyframes");
    }
  }

  /* Free temp data. */
  MEM_freeN(ale);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  /* Done */
  return OPERATOR_FINISHED;
}

static int graphkeys_click_insert_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;
  ARegion *region;
  View2D *v2d;
  int mval[2];
  float x, y;

  /* Get animation context. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Store mouse coordinates in View2D space, into the operator's properties. */
  region = ac.region;
  v2d = &region->v2d;

  mval[0] = (event->xy[0] - region->winrct.xmin);
  mval[1] = (event->xy[1] - region->winrct.ymin);

  UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);

  RNA_float_set(op->ptr, "frame", x);
  RNA_float_set(op->ptr, "value", y);

  /* Run exec now. */
  return graphkeys_click_insert_exec(C, op);
}

void GRAPH_OT_click_insert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers */
  ot->name = "Click-Insert Keyframes";
  ot->idname = "GRAPH_OT_click_insert";
  ot->description = "Insert new keyframe at the cursor position for the active F-Curve";

  /* API callbacks */
  ot->invoke = graphkeys_click_insert_invoke;
  ot->exec = graphkeys_click_insert_exec;
  ot->poll = graphop_active_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  RNA_def_float(ot->srna,
                "frame",
                1.0f,
                -FLT_MAX,
                FLT_MAX,
                "Frame Number",
                "Frame to insert keyframe on",
                0,
                100);
  RNA_def_float(
      ot->srna, "value", 1.0f, -FLT_MAX, FLT_MAX, "Value", "Value for keyframe on", 0, 100);

  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection instead of deselecting everything first");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy/Paste Keyframes Operator
 *
 * \note the back-end code for this is shared with the dope-sheet editor.
 * \{ */

static short copy_graph_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter, ok = 0;

  /* Clear buffer first. */
  ANIM_fcurves_copybuf_free();

  /* Filter data
   * - First time we try to filter more strictly, allowing only selected channels
   *   to allow copying animation between channels.
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);

  if (ANIM_animdata_filter(ac,
                           &anim_data,
                           eAnimFilter_Flags(filter | ANIMFILTER_SEL),
                           ac->data,
                           eAnimCont_Types(ac->datatype)) == 0)
  {
    ANIM_animdata_filter(
        ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));
  }

  /* Copy keyframes. */
  ok = copy_animedit_keys(ac, &anim_data);

  /* Clean up. */
  ANIM_animdata_freelist(&anim_data);

  return ok;
}

static eKeyPasteError paste_graph_keys(bAnimContext *ac,
                                       const eKeyPasteOffset offset_mode,
                                       const eKeyPasteValueOffset value_offset_mode,
                                       const eKeyMergeMode merge_mode,
                                       bool flip)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* Filter data
   * - First time we try to filter more strictly, allowing only selected channels
   *   to allow copying animation between channels
   * - Second time, we loosen things up if nothing was found the first time, allowing
   *   users to just paste keyframes back into the original curve again #31670.
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);

  if (ANIM_animdata_filter(ac,
                           &anim_data,
                           eAnimFilter_Flags(filter | ANIMFILTER_SEL),
                           ac->data,
                           eAnimCont_Types(ac->datatype)) == 0)
  {
    ANIM_animdata_filter(
        ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));
  }

  /* Paste keyframes. */
  const eKeyPasteError ok = paste_animedit_keys(
      ac, &anim_data, offset_mode, value_offset_mode, merge_mode, flip);

  /* Clean up. */
  ANIM_animdata_freelist(&anim_data);

  return ok;
}

/* ------------------- */

static int graphkeys_copy_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Copy keyframes. */
  if (copy_graph_keys(&ac)) {
    BKE_report(op->reports, RPT_ERROR, "No keyframes copied to the internal clipboard");
    return OPERATOR_CANCELLED;
  }

  /* Just return - no operator needed here (no changes). */
  return OPERATOR_FINISHED;
}

void GRAPH_OT_copy(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Copy Keyframes";
  ot->idname = "GRAPH_OT_copy";
  ot->description = "Copy selected keyframes to the internal clipboard";

  /* API callbacks */
  ot->exec = graphkeys_copy_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int graphkeys_paste_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  const eKeyPasteOffset offset_mode = eKeyPasteOffset(RNA_enum_get(op->ptr, "offset"));
  const eKeyPasteValueOffset value_offset_mode = eKeyPasteValueOffset(
      RNA_enum_get(op->ptr, "value_offset"));
  const eKeyMergeMode merge_mode = eKeyMergeMode(RNA_enum_get(op->ptr, "merge"));
  const bool flipped = RNA_boolean_get(op->ptr, "flipped");

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Ac.reports by default will be the global reports list, which won't show warnings. */
  ac.reports = op->reports;

  const eKeyPasteError kf_empty = paste_graph_keys(
      &ac, offset_mode, value_offset_mode, merge_mode, flipped);
  switch (kf_empty) {
    case KEYFRAME_PASTE_OK:
      break;

    case KEYFRAME_PASTE_NOWHERE_TO_PASTE:
      BKE_report(op->reports, RPT_ERROR, "No selected F-Curves to paste into");
      return OPERATOR_CANCELLED;

    case KEYFRAME_PASTE_NOTHING_TO_PASTE:
      BKE_report(op->reports, RPT_ERROR, "No data in the internal clipboard to paste");
      return OPERATOR_CANCELLED;
  }

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static std::string graphkeys_paste_description(bContext * /*C*/,
                                               wmOperatorType * /*ot*/,
                                               PointerRNA *ptr)
{
  /* Custom description if the 'flipped' option is used. */
  if (RNA_boolean_get(ptr, "flipped")) {
    return TIP_("Paste keyframes from mirrored bones if they exist");
  }

  /* Use the default description in the other cases. */
  return "";
}

void GRAPH_OT_paste(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers */
  ot->name = "Paste Keyframes";
  ot->idname = "GRAPH_OT_paste";
  ot->description =
      "Paste keyframes from the internal clipboard for the selected channels, starting on the "
      "current "
      "frame";

  /* API callbacks */

  // ot->invoke = WM_operator_props_popup; /* better wait for graph redo panel */
  ot->get_description = graphkeys_paste_description;
  ot->exec = graphkeys_paste_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Props */
  RNA_def_enum(ot->srna,
               "offset",
               rna_enum_keyframe_paste_offset_items,
               KEYFRAME_PASTE_OFFSET_CFRA_START,
               "Frame Offset",
               "Paste time offset of keys");
  RNA_def_enum(ot->srna,
               "value_offset",
               rna_enum_keyframe_paste_offset_value_items,
               KEYFRAME_PASTE_VALUE_OFFSET_NONE,
               "Value Offset",
               "Paste keys with a value offset");
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
/** \name Duplicate Keyframes Operator
 * \{ */

static bool duplicate_graph_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  bool changed = false;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and delete selected keys. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    changed |= duplicate_fcurve_keys((FCurve *)ale->key_data);

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  return changed;
}

/* ------------------- */

static int graphkeys_duplicate_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Duplicate keyframes. */
  if (!duplicate_graph_keys(&ac)) {
    return OPERATOR_CANCELLED;
  }

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_duplicate(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Duplicate Keyframes";
  ot->idname = "GRAPH_OT_duplicate";
  ot->description = "Make a copy of all selected keyframes";

  /* API callbacks */
  ot->exec = graphkeys_duplicate_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* To give to transform. */
  RNA_def_enum(ot->srna, "mode", rna_enum_transform_mode_type_items, TFM_TRANSLATION, "Mode", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Keyframes Operator
 * \{ */

static bool delete_graph_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  bool changed_final = false;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and delete selected keys. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    AnimData *adt = ale->adt;
    bool changed;

    /* Delete selected keyframes only. */
    changed = BKE_fcurve_delete_keys_selected(fcu);

    if (changed) {
      ale->update |= ANIM_UPDATE_DEFAULT;
      changed_final = true;
    }

    /* Only delete curve too if it won't be doing anything anymore. */
    if (BKE_fcurve_is_empty(fcu)) {
      blender::animrig::animdata_fcurve_delete(ac, adt, fcu);
      ale->key_data = nullptr;
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  return changed_final;
}

/* ------------------- */

static int graphkeys_delete_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Delete keyframes. */
  if (!delete_graph_keys(&ac)) {
    return OPERATOR_CANCELLED;
  }

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_delete(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Delete Keyframes";
  ot->idname = "GRAPH_OT_delete";
  ot->description = "Remove all selected keyframes";

  /* API callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = graphkeys_delete_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clean Keyframes Operator
 * \{ */

static void clean_graph_keys(bAnimContext *ac, float thresh, bool clean_chan)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  if (clean_chan) {
    filter |= ANIMFILTER_SEL;
  }
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  const bool only_selected_keys = !clean_chan;
  /* Loop through filtered data and clean curves. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {

    clean_fcurve(ac, ale, thresh, clean_chan, only_selected_keys);

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_clean_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  float thresh;
  bool clean_chan;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get cleaning threshold. */
  thresh = RNA_float_get(op->ptr, "threshold");
  clean_chan = RNA_boolean_get(op->ptr, "channels");
  /* Clean keyframes. */
  clean_graph_keys(&ac, thresh, clean_chan);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_clean(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Clean Keyframes";
  ot->idname = "GRAPH_OT_clean";
  ot->description = "Simplify F-Curves by removing closely spaced keyframes";

  /* API callbacks */
  // ot->invoke = ???; /* XXX we need that number popup for this! */
  ot->exec = graphkeys_clean_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  ot->prop = RNA_def_float(
      ot->srna, "threshold", 0.001f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 1000.0f);
  RNA_def_boolean(ot->srna, "channels", false, "Channels", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keys to Samples Operator
 *
 * This operator bakes the data of the selected F-Curves to F-Points.
 * \{ */

/* Bake each F-Curve into a set of samples. */
static void convert_keys_to_samples(bAnimContext *ac, int start, int end)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and add keys between selected keyframes on every frame. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ChannelDriver *driver = fcu->driver;

    /* Disable driver so that it don't muck up the sampling process. */
    fcu->driver = nullptr;

    /* Create samples. */
    fcurve_store_samples(fcu, nullptr, start, end, fcurve_samplingcb_evalcurve);

    /* Restore driver. */
    fcu->driver = driver;

    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_keys_to_samples_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  Scene *scene = nullptr;
  int start, end;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* For now, init start/end from preview-range extents. */
  /* TODO: add properties for this. (Joshua Leung 2009) */
  scene = ac.scene;
  start = PSFRA;
  end = PEFRA;

  /* Sample keyframes. */
  convert_keys_to_samples(&ac, start, end);

  /* Set notifier that keyframes have changed. */
  /* NOTE: some distinction between order/number of keyframes and type should be made? */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_keys_to_samples(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Keys to Samples";
  ot->idname = "GRAPH_OT_keys_to_samples";
  ot->description =
      "Convert selected channels to an uneditable set of samples to save storage space";

  /* API callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = graphkeys_keys_to_samples_exec;
  ot->poll = graphop_selected_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);

  /* TODO: add props for start/end frames (Joshua Leung 2009) */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Samples to Keys Operator
 *
 * This operator converts the data of the selected F-Points to F-Curves.
 * \{ */

/* Convert F-Points into F-Curves. */
static void convert_samples_to_keys(bAnimContext *ac, int start, int end)
{
  ListBase anim_data = {nullptr, nullptr};

  /* Filter data. */
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                      ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and add keys between selected keyframes on every frame. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    fcurve_samples_to_keyframes(fcu, start, end);

    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_samples_to_keys_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  Scene *scene = nullptr;
  int start, end;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  scene = ac.scene;
  start = PSFRA;
  end = PEFRA;

  convert_samples_to_keys(&ac, start, end);

  /* Set notifier that keyframes have changed. */
  /* NOTE: some distinction between order/number of keyframes and type should be made? */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_samples_to_keys(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Samples to Keys";
  ot->idname = "GRAPH_OT_samples_to_keys";
  ot->description = "Convert selected channels from samples to keyframes";

  /* API callbacks */
  ot->exec = graphkeys_samples_to_keys_exec;
  ot->poll = graphop_selected_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#ifdef WITH_AUDASPACE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sound to Samples Operator
 *
 * This operator converts the given sound to samples on the selected F-Curves.
 * \{ */

/* ------------------- */

/* Custom data storage passed to the F-Sample-ing function,
 * which provides the necessary info for baking the sound.
 */
struct tSoundBakeInfo {
  float *samples;
  int length;
  int cfra;
};

/* ------------------- */

/* Sampling callback used to determine the value from the sound to
 * save in the F-Curve at the specified frame.
 */
static float fcurve_samplingcb_sound(FCurve * /*fcu*/, void *data, float evaltime)
{
  tSoundBakeInfo *sbi = (tSoundBakeInfo *)data;

  int position = evaltime - sbi->cfra;
  if ((position < 0) || (position >= sbi->length)) {
    return 0.0f;
  }

  return sbi->samples[position];
}

/* ------------------- */

static int graphkeys_sound_to_samples_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  tSoundBakeInfo sbi;
  Scene *scene = nullptr;
  int start, end;

  char filepath[FILE_MAX];

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);

  if (!BLI_is_file(filepath)) {
    BKE_reportf(op->reports, RPT_ERROR, "File not found '%s'", filepath);
    return OPERATOR_CANCELLED;
  }

  scene = ac.scene; /* Current scene. */

  /* Store necessary data for the baking steps. */
  sbi.samples = AUD_readSoundBuffer(filepath,
                                    RNA_float_get(op->ptr, "low"),
                                    RNA_float_get(op->ptr, "high"),
                                    RNA_float_get(op->ptr, "attack"),
                                    RNA_float_get(op->ptr, "release"),
                                    RNA_float_get(op->ptr, "threshold"),
                                    RNA_boolean_get(op->ptr, "use_accumulate"),
                                    RNA_boolean_get(op->ptr, "use_additive"),
                                    RNA_boolean_get(op->ptr, "use_square"),
                                    RNA_float_get(op->ptr, "sthreshold"),
                                    FPS,
                                    &sbi.length,
                                    0);

  if (sbi.samples == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Unsupported audio format");
    return OPERATOR_CANCELLED;
  }

  /* Determine extents of the baking. */
  sbi.cfra = start = scene->r.cfra;
  end = scene->r.cfra + sbi.length - 1;

  /* Filter anim channels. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Loop through all selected F-Curves, replacing its data with the sound samples. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* Sample the sound. */
    fcurve_store_samples(fcu, &sbi, start, end, fcurve_samplingcb_sound);

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  /* Free sample data. */
  free(sbi.samples);

  /* Validate keyframes after editing. */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* Set notifier that 'keyframes' have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

#else /* WITH_AUDASPACE */

static int graphkeys_sound_to_samples_exec(bContext * /*C*/, wmOperator *op)
{
  BKE_report(op->reports, RPT_ERROR, "Compiled without sound support");

  return OPERATOR_CANCELLED;
}

#endif /* WITH_AUDASPACE */

static int graphkeys_sound_to_samples_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;

  /* Verify editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  return WM_operator_filesel(C, op, event);
}

void GRAPH_OT_sound_to_samples(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Sound to Samples";
  ot->idname = "GRAPH_OT_sound_to_samples";
  ot->description = "Bakes a sound wave to samples on selected channels";

  /* API callbacks */
  ot->invoke = graphkeys_sound_to_samples_invoke;
  ot->exec = graphkeys_sound_to_samples_exec;
  ot->poll = graphop_selected_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_SOUND | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_SHOW_PROPS,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  RNA_def_float(ot->srna,
                "low",
                0.0f,
                0.0,
                100000.0,
                "Lowest Frequency",
                "Cutoff frequency of a high-pass filter that is applied to the audio data",
                0.1,
                1000.00);
  RNA_def_float(ot->srna,
                "high",
                100000.0,
                0.0,
                100000.0,
                "Highest Frequency",
                "Cutoff frequency of a low-pass filter that is applied to the audio data",
                0.1,
                1000.00);
  RNA_def_float(ot->srna,
                "attack",
                0.005,
                0.0,
                2.0,
                "Attack Time",
                "Value for the envelope calculation that tells how fast the envelope can rise "
                "(the lower the value the steeper it can rise)",
                0.01,
                0.1);
  RNA_def_float(ot->srna,
                "release",
                0.2,
                0.0,
                5.0,
                "Release Time",
                "Value for the envelope calculation that tells how fast the envelope can fall "
                "(the lower the value the steeper it can fall)",
                0.01,
                0.2);
  RNA_def_float(ot->srna,
                "threshold",
                0.0,
                0.0,
                1.0,
                "Threshold",
                "Minimum amplitude value needed to influence the envelope",
                0.01,
                0.1);
  RNA_def_boolean(ot->srna,
                  "use_accumulate",
                  false,
                  "Accumulate",
                  "Only the positive differences of the envelope amplitudes are summarized to "
                  "produce the output");
  RNA_def_boolean(ot->srna,
                  "use_additive",
                  false,
                  "Additive",
                  "The amplitudes of the envelope are summarized (or, when Accumulate is enabled, "
                  "both positive and negative differences are accumulated)");
  RNA_def_boolean(ot->srna,
                  "use_square",
                  false,
                  "Square",
                  "The output is a square curve (negative values always result in -1, and "
                  "positive ones in 1)");
  RNA_def_float(ot->srna,
                "sthreshold",
                0.1,
                0.0,
                1.0,
                "Square Threshold",
                "Square only: all values with an absolute amplitude lower than that result in 0",
                0.01,
                0.1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Keyframes Operator
 *
 * This operator 'bakes' the values of the curve into new keyframes between pairs
 * of selected keyframes. It is useful for creating keyframes for tweaking overlap.
 * \{ */

/* Evaluates the curves between each selected keyframe on each frame, and keys the value. */
static void bake_graph_keys(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and add keys between selected keyframes on every frame. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    bake_fcurve_segments((FCurve *)ale->key_data);

    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_bake_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Bake keyframes. */
  bake_graph_keys(&ac);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_bake_keys(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Bake Keyframes";
  ot->idname = "GRAPH_OT_bake_keys";
  ot->description = "Add keyframes on every frame between the selected keyframes";

  /* API callbacks */
  ot->exec = graphkeys_bake_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ************************************************************************** */
/* EXTRAPOLATION MODE AND KEYFRAME HANDLE SETTINGS */

/* -------------------------------------------------------------------- */
/** \name Set Extrapolation-Type Operator
 * \{ */

/* Defines for make/clear cyclic extrapolation tools. */
#define MAKE_CYCLIC_EXPO -1
#define CLEAR_CYCLIC_EXPO -2

/* Defines for set extrapolation-type for selected keyframes tool. */
static const EnumPropertyItem prop_graphkeys_expo_types[] = {
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
     "Add Cycles F-Modifier if one doesn't exist already"},
    {CLEAR_CYCLIC_EXPO,
     "CLEAR_CYCLIC",
     0,
     "Clear Cyclic (F-Modifier)",
     "Remove Cycles F-Modifier if not needed anymore"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* This function is responsible for setting extrapolation mode for keyframes. */
static void setexpo_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through setting mode per F-Curve. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;

    if (mode >= 0) {
      /* Just set mode setting. */
      fcu->extend = mode;

      ale->update |= ANIM_UPDATE_HANDLES;
    }
    else {
      /* Shortcuts for managing Cycles F-Modifiers to make it easier to toggle cyclic animation
       * without having to go through FModifier UI in Graph Editor to do so.
       */
      if (mode == MAKE_CYCLIC_EXPO) {
        /* Only add if one doesn't exist. */
        if (list_has_suitable_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, -1) == 0) {
          /* TODO: add some more preset versions which set different extrapolation options?
           * (Joshua Leung 2011) */
          add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, fcu);
        }
      }
      else if (mode == CLEAR_CYCLIC_EXPO) {
        /* Remove all the modifiers fitting this description. */
        FModifier *fcm, *fcn = nullptr;

        for (fcm = static_cast<FModifier *>(fcu->modifiers.first); fcm; fcm = fcn) {
          fcn = fcm->next;

          if (fcm->type == FMODIFIER_TYPE_CYCLES) {
            remove_fmodifier(&fcu->modifiers, fcm);
          }
        }
      }
    }

    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_expo_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get handle setting mode. */
  mode = RNA_enum_get(op->ptr, "type");

  /* Set handle type. */
  setexpo_graph_keys(&ac, mode);

  /* Set notifier that keyframe properties have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_extrapolation_type(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Set F-Curve Extrapolation";
  ot->idname = "GRAPH_OT_extrapolation_type";
  ot->description = "Set extrapolation mode for selected F-Curves";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graphkeys_expo_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_expo_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Interpolation-Type Operator
 * \{ */

/* This function is responsible for setting interpolation mode for keyframes. */
static void setipo_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  KeyframeEditFunc set_cb = ANIM_editkeyframes_ipo(mode);

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through setting BezTriple interpolation
   * NOTE: we do not supply KeyframeEditData to the looper yet.
   * Currently that's not necessary here.
   */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    ANIM_fcurve_keyframes_loop(
        nullptr, static_cast<FCurve *>(ale->key_data), nullptr, set_cb, BKE_fcurve_handles_recalc);

    ale->update |= ANIM_UPDATE_DEFAULT_NOHANDLES;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_ipo_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get handle setting mode. */
  mode = RNA_enum_get(op->ptr, "type");

  /* Set handle type. */
  setipo_graph_keys(&ac, mode);

  /* Set notifier that keyframe properties have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_interpolation_type(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Set Keyframe Interpolation";
  ot->idname = "GRAPH_OT_interpolation_type";
  ot->description =
      "Set interpolation mode for the F-Curve segments starting from the selected keyframes";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graphkeys_ipo_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_beztriple_interpolation_mode_items, 0, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_ACTION);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Easing Operator
 * \{ */

static void seteasing_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  KeyframeEditFunc set_cb = ANIM_editkeyframes_easing(mode);

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through setting BezTriple easing.
   * NOTE: we do not supply KeyframeEditData to the looper yet.
   * Currently that's not necessary here.
   */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    ANIM_fcurve_keyframes_loop(
        nullptr, static_cast<FCurve *>(ale->key_data), nullptr, set_cb, BKE_fcurve_handles_recalc);

    ale->update |= ANIM_UPDATE_DEFAULT_NOHANDLES;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static int graphkeys_easing_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get handle setting mode. */
  mode = RNA_enum_get(op->ptr, "type");

  /* Set handle type. */
  seteasing_graph_keys(&ac, mode);

  /* Set notifier that keyframe properties have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_easing_type(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Set Keyframe Easing Type";
  ot->idname = "GRAPH_OT_easing_type";
  ot->description =
      "Set easing type for the F-Curve segments starting from the selected keyframes";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graphkeys_easing_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_beztriple_interpolation_easing_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Handle-Type Operator
 * \{ */

/* This function is responsible for setting handle-type of selected keyframes. */
static void sethandles_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  KeyframeEditFunc edit_cb = ANIM_editkeyframes_handles(mode);
  KeyframeEditFunc sel_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through setting flags for handles.
   * NOTE: we do not supply KeyframeEditData to the looper yet.
   * Currently that's not necessary here.
   */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* Any selected keyframes for editing? */
    if (ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, sel_cb, nullptr)) {
      /* Change type of selected handles. */
      ANIM_fcurve_keyframes_loop(nullptr, fcu, nullptr, edit_cb, BKE_fcurve_handles_recalc);

      ale->update |= ANIM_UPDATE_DEFAULT;
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}
/* ------------------- */

static int graphkeys_handletype_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get handle setting mode. */
  mode = RNA_enum_get(op->ptr, "type");

  /* Set handle type. */
  sethandles_graph_keys(&ac, mode);

  /* Set notifier that keyframe properties have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_handle_type(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Set Keyframe Handle Type";
  ot->idname = "GRAPH_OT_handle_type";
  ot->description = "Set type of handle for selected keyframes";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graphkeys_handletype_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_keyframe_handle_type_items, 0, "Type", "");
}

/** \} */

/* ************************************************************************** */
/* EULER FILTER */

/* -------------------------------------------------------------------- */
/** \name 'Euler Filter' Operator
 *
 * Euler filter tools (as seen in Maya), are necessary for working with 'baked'
 * rotation curves (with Euler rotations). The main purpose of such tools is to
 * resolve any discontinuities that may arise in the curves due to the clamping
 * of values to -180 degrees to 180 degrees.
 * \{ */

/* Set of three euler-rotation F-Curves. */
struct tEulerFilter {
  tEulerFilter *next, *prev;

  /** ID-block which owns the channels */
  ID *id;
  /** 3 Pointers to F-Curves. */
  FCurve *fcurves[3];
  /** Pointer to one of the RNA Path's used by one of the F-Curves. */
  const char *rna_path;
};

static bool keyframe_time_differs(BezTriple *keyframes[3])
{
  const float precision = 1e-5;
  return fabs(keyframes[0]->vec[1][0] - keyframes[1]->vec[1][0]) > precision ||
         fabs(keyframes[1]->vec[1][0] - keyframes[2]->vec[1][0]) > precision ||
         fabs(keyframes[0]->vec[1][0] - keyframes[2]->vec[1][0]) > precision;
}

/* Find groups of `rotation_euler` channels. */
static ListBase /*tEulerFilter*/ euler_filter_group_channels(
    const ListBase /*bAnimListElem*/ *anim_data, ReportList *reports, int *r_num_groups)
{
  ListBase euler_groups = {nullptr, nullptr};
  tEulerFilter *euf = nullptr;
  *r_num_groups = 0;

  LISTBASE_FOREACH (bAnimListElem *, ale, anim_data) {
    FCurve *const fcu = (FCurve *)ale->data;

    /* Check if this is an appropriate F-Curve:
     * - Only rotation curves.
     * - For pchan curves, make sure we're only using the euler curves.
     */
    if (strstr(fcu->rna_path, "rotation_euler") == nullptr) {
      continue;
    }
    if (ELEM(fcu->array_index, 0, 1, 2) == 0) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Euler Rotation F-Curve has invalid index (ID='%s', Path='%s', Index=%d)",
                  (ale->id) ? ale->id->name : RPT_("<No ID>"),
                  fcu->rna_path,
                  fcu->array_index);
      continue;
    }

    /* Assume that this animation channel will be touched by the Euler filter. Doing this here
     * saves another loop over the animation data. */
    ale->update |= ANIM_UPDATE_DEFAULT;

    /* Optimization: assume that xyz curves will always be stored consecutively,
     * so if the paths or the ID's don't match up, then a curve needs to be added
     * to a new group.
     */
    if ((euf) && (euf->id == ale->id) && STREQ(euf->rna_path, fcu->rna_path)) {
      /* This should be fine to add to the existing group then. */
      euf->fcurves[fcu->array_index] = fcu;
      continue;
    }

    /* Just add to a new block. */
    euf = static_cast<tEulerFilter *>(MEM_callocN(sizeof(tEulerFilter), "tEulerFilter"));
    BLI_addtail(&euler_groups, euf);
    ++*r_num_groups;

    euf->id = ale->id;
    /* This should be safe, since we're only using it for a short time. */
    euf->rna_path = fcu->rna_path;
    euf->fcurves[fcu->array_index] = fcu;
  }

  return euler_groups;
}

/* Perform discontinuity filter based on conversion to matrix and back.
 * Return true if the curves were filtered (which may have been a no-op), false otherwise. */
static bool euler_filter_multi_channel(tEulerFilter *euf, ReportList *reports)
{
  /* Sanity check: ensure that there are enough F-Curves to work on in this group. */
  if (ELEM(nullptr, euf->fcurves[0], euf->fcurves[1], euf->fcurves[2])) {
    /* Report which components are missing. */
    BKE_reportf(reports,
                RPT_INFO,
                "Missing %s%s%s component(s) of euler rotation for ID='%s' and RNA-Path='%s'",
                (euf->fcurves[0] == nullptr) ? "X" : "",
                (euf->fcurves[1] == nullptr) ? "Y" : "",
                (euf->fcurves[2] == nullptr) ? "Z" : "",
                euf->id->name,
                euf->rna_path);
    return false;
  }

  FCurve *fcu_rot_x = euf->fcurves[0];
  FCurve *fcu_rot_y = euf->fcurves[1];
  FCurve *fcu_rot_z = euf->fcurves[2];
  if (fcu_rot_x->totvert != fcu_rot_y->totvert || fcu_rot_y->totvert != fcu_rot_z->totvert) {
    BKE_reportf(reports,
                RPT_INFO,
                "XYZ rotations not equally keyed for ID='%s' and RNA-Path='%s'",
                euf->id->name,
                euf->rna_path);
    return false;
  }

  if (fcu_rot_x->totvert < 2) {
    /* Empty curves and single keyframes are trivially "filtered". */
    return false;
  }

  float filtered_euler[3] = {
      fcu_rot_x->bezt[0].vec[1][1],
      fcu_rot_y->bezt[0].vec[1][1],
      fcu_rot_z->bezt[0].vec[1][1],
  };

  for (int keyframe_index = 1; keyframe_index < fcu_rot_x->totvert; ++keyframe_index) {
    BezTriple *keyframes[3] = {
        &fcu_rot_x->bezt[keyframe_index],
        &fcu_rot_y->bezt[keyframe_index],
        &fcu_rot_z->bezt[keyframe_index],
    };

    if (keyframe_time_differs(keyframes)) {
      /* The X-coordinates of the keyframes are different, so we cannot correct this key. */
      continue;
    }

    const float unfiltered_euler[3] = {
        keyframes[0]->vec[1][1],
        keyframes[1]->vec[1][1],
        keyframes[2]->vec[1][1],
    };

    /* The conversion back from matrix to Euler angles actually performs the filtering. */
    float matrix[3][3];
    eul_to_mat3(matrix, unfiltered_euler);
    mat3_normalized_to_compatible_eul(filtered_euler, filtered_euler, matrix);

    /* TODO(Sybren): it might be a nice touch to compare `filtered_euler` with `unfiltered_euler`,
     * to see if there was actually a change. This could improve reporting for the artist. */

    BKE_fcurve_keyframe_move_value_with_handles(keyframes[0], filtered_euler[0]);
    BKE_fcurve_keyframe_move_value_with_handles(keyframes[1], filtered_euler[1]);
    BKE_fcurve_keyframe_move_value_with_handles(keyframes[2], filtered_euler[2]);
  }

  return true;
}

/* Remove 360-degree flips from a single FCurve.
 * Return true if the curve was modified, false otherwise. */
static bool euler_filter_single_channel(FCurve *fcu)
{
  /* Simple method: just treat any difference between keys of greater than 180 degrees as being a
   * flip. */
  BezTriple *bezt, *prev;
  uint i;

  /* Skip if not enough verts to do a decent analysis. */
  if (fcu->totvert <= 2) {
    return false;
  }

  /* Skip baked FCurves. */
  if (fcu->bezt == nullptr) {
    return false;
  }

  /* `prev` follows bezt, bezt = "current" point to be fixed. */
  /* Our method depends on determining a "difference" from the previous vert. */
  bool is_modified = false;
  for (i = 1, prev = fcu->bezt, bezt = fcu->bezt + 1; i < fcu->totvert; i++, prev = bezt++) {
    const float sign = (prev->vec[1][1] > bezt->vec[1][1]) ? 1.0f : -1.0f;

    /* >= 180 degree flip? */
    if ((sign * (prev->vec[1][1] - bezt->vec[1][1])) < float(M_PI)) {
      continue;
    }

    /* 360 degrees to add/subtract frame value until difference
     * is acceptably small that there's no more flip. */
    const float fac = sign * 2.0f * float(M_PI);

    while ((sign * (prev->vec[1][1] - bezt->vec[1][1])) >= float(M_PI)) {
      bezt->vec[0][1] += fac;
      bezt->vec[1][1] += fac;
      bezt->vec[2][1] += fac;
    }

    is_modified = true;
  }

  return is_modified;
}

static void euler_filter_perform_filter(ListBase /*tEulerFilter*/ *eulers,
                                        ReportList *reports,
                                        int *r_curves_filtered,
                                        int *r_curves_seen)
{
  *r_curves_filtered = 0;
  *r_curves_seen = 0;

  LISTBASE_FOREACH (tEulerFilter *, euf, eulers) {
    int curves_filtered_this_group = 0;

    if (euler_filter_multi_channel(euf, reports)) {
      curves_filtered_this_group = 3;
    }

    for (int channel_index = 0; channel_index < 3; channel_index++) {
      FCurve *fcu = euf->fcurves[channel_index];
      if (fcu == nullptr) {
        continue;
      }
      ++*r_curves_seen;

      if (euler_filter_single_channel(fcu)) {
        ++curves_filtered_this_group;
      }
    }

    *r_curves_filtered += min_ii(3, curves_filtered_this_group);
  }
}

static int graphkeys_euler_filter_exec(bContext *C, wmOperator *op)
{
  /* Get editor data. */
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* The process is done in two passes:
   * 1) Sets of three related rotation curves are identified from the selected channels,
   *    and are stored as a single 'operation unit' for the next step.
   * 2) Each set of three F-Curves is processed for each keyframe, with the values being
   *    processed as necessary.
   */

  /* Step 1: extract only the rotation f-curves. */
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_CURVE_VISIBLE |
                      ANIMFILTER_FCURVESONLY | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  int groups = 0;
  ListBase eulers = euler_filter_group_channels(&anim_data, op->reports, &groups);
  BLI_assert(BLI_listbase_count(&eulers) == groups);

  if (groups == 0) {
    ANIM_animdata_freelist(&anim_data);
    BKE_report(op->reports, RPT_WARNING, "No Euler Rotation F-Curves to fix up");
    return OPERATOR_CANCELLED;
  }

  /* Step 2: go through each set of curves, processing the values at each keyframe.
   * - It is assumed that there must be a full set of keyframes at each keyframe position.
   */
  int curves_filtered;
  int curves_seen;
  euler_filter_perform_filter(&eulers, op->reports, &curves_filtered, &curves_seen);

  BLI_freelistN(&eulers);
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  if (curves_filtered == 0) {
    if (curves_seen < 3) {
      /* Showing the entire error message makes no sense when the artist is only trying to filter
       * one or two curves. */
      BKE_report(op->reports, RPT_WARNING, "No Euler Rotations could be corrected");
    }
    else {
      BKE_report(op->reports,
                 RPT_ERROR,
                 "No Euler Rotations could be corrected, ensure each rotation has keys for all "
                 "components, "
                 "and that F-Curves for these are in consecutive XYZ order and selected");
    }
    return OPERATOR_CANCELLED;
  }

  if (curves_filtered != curves_seen) {
    BLI_assert(curves_filtered < curves_seen);
    BKE_reportf(op->reports,
                RPT_INFO,
                "%d of %d rotation channels were filtered (see the Info window for details)",
                curves_filtered,
                curves_seen);
  }
  else if (curves_seen == 1) {
    BKE_report(op->reports, RPT_INFO, "The rotation channel was filtered");
  }
  else {
    BKE_reportf(op->reports, RPT_INFO, "All %d rotation channels were filtered", curves_seen);
  }

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  /* Done at last. */
  return OPERATOR_FINISHED;
}

void GRAPH_OT_euler_filter(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Euler Discontinuity Filter";
  ot->idname = "GRAPH_OT_euler_filter";
  ot->description =
      "Fix large jumps and flips in the selected "
      "Euler Rotation F-Curves arising from rotation "
      "values being clipped when baking physics";

  /* API callbacks */
  ot->exec = graphkeys_euler_filter_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ************************************************************************** */
/* SNAPPING */

/* -------------------------------------------------------------------- */
/** \name Jump to Selected Frames Operator
 * \{ */

static bool graphkeys_framejump_poll(bContext *C)
{
  /* Prevent changes during render. */
  if (G.is_rendering) {
    return false;
  }

  return graphop_visible_keyframes_poll(C);
}

static KeyframeEditData sum_selected_keyframes(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  KeyframeEditData ked;

  /* Init edit data. */
  memset(&ked, 0, sizeof(KeyframeEditData));

  /* Loop over action data, averaging values. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);
    short mapping_flag = ANIM_get_normalization_flags(ac->sl);
    KeyframeEditData current_ked;
    float offset;
    float unit_scale = ANIM_unit_mapping_get_factor(ac->scene,
                                                    ale->id,
                                                    static_cast<FCurve *>(ale->key_data),
                                                    mapping_flag | ANIM_UNITCONV_ONLYKEYS,
                                                    &offset);

    memset(&current_ked, 0, sizeof(current_ked));

    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, true);
      ANIM_fcurve_keyframes_loop(
          &current_ked, static_cast<FCurve *>(ale->key_data), nullptr, bezt_calc_average, nullptr);
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, true);
    }
    else {
      ANIM_fcurve_keyframes_loop(
          &current_ked, static_cast<FCurve *>(ale->key_data), nullptr, bezt_calc_average, nullptr);
    }

    ked.f1 += current_ked.f1;
    ked.i1 += current_ked.i1;
    ked.f2 += (current_ked.f2 + offset) * unit_scale;
    ked.i2 += current_ked.i2;
  }

  ANIM_animdata_freelist(&anim_data);

  return ked;
}

/* Snap current-frame indicator to 'average time' of selected keyframe. */
static int graphkeys_framejump_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const KeyframeEditData keyframe_sum = sum_selected_keyframes(&ac);
  const float sum_time = keyframe_sum.f1;
  const float sum_value = keyframe_sum.f2;
  const int num_keyframes = keyframe_sum.i1;

  if (num_keyframes == 0) {
    return OPERATOR_FINISHED;
  }

  /* Set the new current frame and cursor values, based on the average time and value. */
  SpaceGraph *sipo = (SpaceGraph *)ac.sl;
  Scene *scene = ac.scene;

  /* Take the average values, rounding to the nearest int as necessary for int results. */
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    /* Drivers Mode - Affects cursor (float) */
    sipo->cursorTime = sum_time / float(num_keyframes);
  }
  else {
    /* Animation Mode - Affects current frame (int) */
    scene->r.cfra = round_fl_to_int(sum_time / num_keyframes);
    scene->r.subframe = 0.0f;
  }
  sipo->cursorVal = sum_value / float(num_keyframes);

  /* Set notifier that things have changed. */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_frame_jump(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Jump to Keyframes";
  ot->idname = "GRAPH_OT_frame_jump";
  ot->description = "Place the cursor on the midpoint of selected keyframes";

  /* API callbacks */
  ot->exec = graphkeys_framejump_exec;
  ot->poll = graphkeys_framejump_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool find_closest_frame(const FCurve *fcu,
                               const float frame,
                               const bool next,
                               float *r_closest_frame)
{
  bool replace;
  int bezt_index = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, frame, fcu->totvert, &replace);

  BezTriple *bezt;
  if (next) {
    if (replace) {
      bezt_index++;
    }
    if (bezt_index > fcu->totvert - 1) {
      return false;
    }
    bezt = &fcu->bezt[bezt_index];
  }
  else {
    if (bezt_index - 1 < 0) {
      return false;
    }
    bezt = &fcu->bezt[bezt_index - 1];
  }

  *r_closest_frame = bezt->vec[1][0];
  return true;
}

static int keyframe_jump_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  Scene *scene = CTX_data_scene(C);

  bool next = RNA_boolean_get(op->ptr, "next");

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  ListBase anim_data = {nullptr, nullptr};
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                ANIMFILTER_NODUPLIS);
  if (U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) {
    filter |= ANIMFILTER_SEL;
  }

  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  float closest_frame = next ? FLT_MAX : -FLT_MAX;
  bool found = false;

  const float current_frame = BKE_scene_frame_get(scene);
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = static_cast<FCurve *>(ale->key_data);
    if (!fcu->bezt) {
      continue;
    }
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    ANIM_nla_mapping_apply_fcurve(adt, fcu, false, true);
    float closest_fcu_frame;
    if (!find_closest_frame(fcu, current_frame, next, &closest_fcu_frame)) {
      continue;
    }
    if ((next && closest_fcu_frame < closest_frame) ||
        (!next && closest_fcu_frame > closest_frame))
    {
      closest_frame = closest_fcu_frame;
      found = true;
    }
    ANIM_nla_mapping_apply_fcurve(adt, fcu, true, true);
  }

  if (!found) {
    BKE_report(op->reports, RPT_INFO, "No more keyframes to jump to in this direction");
    return OPERATOR_CANCELLED;
  }

  BKE_scene_frame_set(scene, closest_frame);

  ANIM_animdata_freelist(&anim_data);

  /* Set notifier that things have changed. */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);
  return OPERATOR_FINISHED;
}

void GRAPH_OT_keyframe_jump(wmOperatorType *ot)
{
  ot->name = "Jump to Keyframe";
  ot->description = "Jump to previous/next keyframe";
  ot->idname = "GRAPH_OT_keyframe_jump";

  ot->exec = keyframe_jump_exec;

  ot->poll = graphkeys_framejump_poll;
  ot->flag = OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* properties */
  RNA_def_boolean(ot->srna, "next", true, "Next Keyframe", "");
}

/* snap 2D cursor value to the average value of selected keyframe */
static int graphkeys_snap_cursor_value_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const KeyframeEditData keyframe_sum = sum_selected_keyframes(&ac);
  const float sum_value = keyframe_sum.f2;
  const int num_keyframes = keyframe_sum.i1;

  if (num_keyframes == 0) {
    return OPERATOR_FINISHED;
  }

  SpaceGraph *sipo = (SpaceGraph *)ac.sl;
  sipo->cursorVal = sum_value / float(num_keyframes);
  // WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void GRAPH_OT_snap_cursor_value(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Snap Cursor Value to Selected";
  ot->idname = "GRAPH_OT_snap_cursor_value";
  ot->description = "Place the cursor value on the average value of selected keyframes";

  /* API callbacks. */
  ot->exec = graphkeys_snap_cursor_value_exec;
  ot->poll = graphkeys_framejump_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Keyframes Operator
 * \{ */

/* Defines for snap keyframes tool. */
static const EnumPropertyItem prop_graphkeys_snap_types[] = {
    {GRAPHKEYS_SNAP_CFRA,
     "CFRA",
     0,
     "Selection to Current Frame",
     "Snap selected keyframes to the current frame"},
    {GRAPHKEYS_SNAP_VALUE,
     "VALUE",
     0,
     "Selection to Cursor Value",
     "Set values of selected keyframes to the cursor value (Y/Horizontal component)"},
    {GRAPHKEYS_SNAP_NEAREST_FRAME,
     "NEAREST_FRAME",
     0,
     "Selection to Nearest Frame",
     "Snap selected keyframes to the nearest (whole) frame (use to fix accidental subframe "
     "offsets)"},
    {GRAPHKEYS_SNAP_NEAREST_SECOND,
     "NEAREST_SECOND",
     0,
     "Selection to Nearest Second",
     "Snap selected keyframes to the nearest second"},
    {GRAPHKEYS_SNAP_NEAREST_MARKER,
     "NEAREST_MARKER",
     0,
     "Selection to Nearest Marker",
     "Snap selected keyframes to the nearest marker"},
    {GRAPHKEYS_SNAP_HORIZONTAL,
     "HORIZONTAL",
     0,
     "Flatten Handles",
     "Flatten handles for a smoother transition"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* This function is responsible for snapping keyframes to frame-times. */
static void snap_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  KeyframeEditData ked;
  KeyframeEditFunc edit_cb;
  float cursor_value = 0.0f;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Init custom data for iterating over keyframes. */
  memset(&ked, 0, sizeof(KeyframeEditData));
  ked.scene = ac->scene;
  if (mode == GRAPHKEYS_SNAP_NEAREST_MARKER) {
    ked.list.first = (ac->markers) ? ac->markers->first : nullptr;
    ked.list.last = (ac->markers) ? ac->markers->last : nullptr;
  }
  else if (mode == GRAPHKEYS_SNAP_VALUE) {
    cursor_value = (sipo) ? sipo->cursorVal : 0.0f;
  }
  else if (mode == GRAPHKEYS_SNAP_CFRA) {
    /* In drivers mode, use the cursor value instead
     * (We need to use a different callback for that though)
     */
    if (sipo->mode == SIPO_MODE_DRIVERS) {
      ked.f1 = sipo->cursorTime;
      mode = SNAP_KEYS_TIME;
    }
  }

  /* Get beztriple editing callbacks. */
  edit_cb = ANIM_editkeyframes_snap(mode);

  /* Snap keyframes. */
  const bool use_handle = (sipo->flag & SIPO_NOHANDLES) == 0;
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    /* Normalize cursor value (for normalized F-Curves display). */
    if (mode == GRAPHKEYS_SNAP_VALUE) {
      short mapping_flag = ANIM_get_normalization_flags(ac->sl);
      float offset;
      float unit_scale = ANIM_unit_mapping_get_factor(
          ac->scene, ale->id, static_cast<FCurve *>(ale->key_data), mapping_flag, &offset);

      ked.f1 = (cursor_value / unit_scale) - offset;
    }

    /* Perform snapping. */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, false);
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), nullptr, edit_cb, BKE_fcurve_handles_recalc);
      BKE_fcurve_merge_duplicate_keys(
          static_cast<FCurve *>(ale->key_data), BEZT_FLAG_TEMP_TAG, use_handle);
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, false);
    }
    else {
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), nullptr, edit_cb, BKE_fcurve_handles_recalc);
      BKE_fcurve_merge_duplicate_keys(
          static_cast<FCurve *>(ale->key_data), BEZT_FLAG_TEMP_TAG, use_handle);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_snap_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get snapping mode. */
  mode = RNA_enum_get(op->ptr, "type");

  /* Snap keyframes. */
  snap_graph_keys(&ac, mode);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool graph_has_selected_control_points(bContext *C)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return false;
  }

  /* Filter data. */
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                      ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Check if any of the visible and editable f-curves have at least one selected control point. */
  bool has_selected_control_points = false;
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    const FCurve *fcu = static_cast<const FCurve *>(ale->key_data);
    if (BKE_fcurve_has_selected_control_points(fcu)) {
      has_selected_control_points = true;
      break;
    }
  }

  ANIM_animdata_freelist(&anim_data);

  return has_selected_control_points;
}

static int graphkeys_selected_control_points_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  if (!graph_has_selected_control_points(C)) {
    BKE_report(op->reports, RPT_ERROR, "No control points are selected");
    return OPERATOR_CANCELLED;
  }

  return WM_menu_invoke(C, op, event);
}

void GRAPH_OT_snap(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Snap Keys";
  ot->idname = "GRAPH_OT_snap";
  ot->description = "Snap selected keyframes to the chosen times/values";

  /* API callbacks */
  ot->invoke = graphkeys_selected_control_points_invoke;
  ot->exec = graphkeys_snap_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_snap_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Equalize Handles Operator
 * \{ */

/* Defines for equalize handles tool. */
static const EnumPropertyItem prop_graphkeys_equalize_handles_sides[] = {
    {GRAPHKEYS_EQUALIZE_LEFT, "LEFT", 0, "Left", "Equalize selected keyframes' left handles"},
    {GRAPHKEYS_EQUALIZE_RIGHT, "RIGHT", 0, "Right", "Equalize selected keyframes' right handles"},
    {GRAPHKEYS_EQUALIZE_BOTH, "BOTH", 0, "Both", "Equalize both of a keyframe's handles"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* ------------------- */

/* Equalize selected keyframes' bezier handles. */
static void equalize_graph_keys(bAnimContext *ac, int mode, float handle_length, bool flatten)
{
  /* Filter data. */
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
                      ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Equalize keyframes. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    ANIM_fcurve_equalize_keyframes_loop(static_cast<FCurve *>(ale->key_data),
                                        eEditKeyframes_Equalize(mode),
                                        handle_length,
                                        flatten);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static int graphkeys_equalize_handles_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get equalize mode. */
  int mode = RNA_enum_get(op->ptr, "side");
  float handle_length = RNA_float_get(op->ptr, "handle_length");
  bool flatten = RNA_boolean_get(op->ptr, "flatten");

  /* Equalize graph keyframes. */
  equalize_graph_keys(&ac, mode, handle_length, flatten);

  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_equalize_handles(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Equalize Handles";
  ot->idname = "GRAPH_OT_equalize_handles";
  ot->description =
      "Ensure selected keyframes' handles have equal length, optionally making them horizontal. "
      "Automatic, Automatic Clamped, or Vector handle types will be converted to Aligned";
  /* API callbacks */
  ot->invoke = graphkeys_selected_control_points_invoke;
  ot->exec = graphkeys_equalize_handles_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "side",
                          prop_graphkeys_equalize_handles_sides,
                          0,
                          "Side",
                          "Side of the keyframes' Bézier handles to affect");
  RNA_def_float(ot->srna,
                "handle_length",
                5.0f,
                0.1f,
                FLT_MAX,
                "Handle Length",
                "Length to make selected keyframes' Bézier handles",
                1.0f,
                50.0f);
  RNA_def_boolean(
      ot->srna,
      "flatten",
      false,
      "Flatten",
      "Make the values of the selected keyframes' handles the same as their respective keyframes");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mirror Keyframes Operator
 * \{ */

/* Defines for mirror keyframes tool. */
static const EnumPropertyItem prop_graphkeys_mirror_types[] = {
    {GRAPHKEYS_MIRROR_CFRA,
     "CFRA",
     0,
     "By Times Over Current Frame",
     "Flip times of selected keyframes using the current frame as the mirror line"},
    {GRAPHKEYS_MIRROR_VALUE,
     "VALUE",
     0,
     "By Values Over Cursor Value",
     "Flip values of selected keyframes using the cursor value (Y/Horizontal component) as the "
     "mirror line"},
    {GRAPHKEYS_MIRROR_YAXIS,
     "YAXIS",
     0,
     "By Times Over Zero Time",
     "Flip times of selected keyframes, effectively reversing the order they appear in"},
    {GRAPHKEYS_MIRROR_XAXIS,
     "XAXIS",
     0,
     "By Values Over Zero Value",
     "Flip values of selected keyframes (i.e. negative values become positive, and vice versa)"},
    {GRAPHKEYS_MIRROR_MARKER,
     "MARKER",
     0,
     "By Times Over First Selected Marker",
     "Flip times of selected keyframes using the first selected marker as the reference point"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* This function is responsible for mirroring keyframes. */
static void mirror_graph_keys(bAnimContext *ac, short mode)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  SpaceGraph *sipo = (SpaceGraph *)ac->sl;
  KeyframeEditData ked;
  KeyframeEditFunc edit_cb;
  float cursor_value = 0.0f;

  /* Init custom data for looping over keyframes. */
  memset(&ked, 0, sizeof(KeyframeEditData));
  ked.scene = ac->scene;

  /* Store mode-specific custom data... */
  if (mode == GRAPHKEYS_MIRROR_MARKER) {
    TimeMarker *marker = nullptr;

    /* Find first selected marker. */
    marker = ED_markers_get_first_selected(ac->markers);

    /* Store marker's time (if available). */
    if (marker) {
      ked.f1 = float(marker->frame);
    }
    else {
      return;
    }
  }
  else if (mode == GRAPHKEYS_MIRROR_VALUE) {
    cursor_value = (sipo) ? sipo->cursorVal : 0.0f;
  }
  else if (mode == GRAPHKEYS_MIRROR_CFRA) {
    /* In drivers mode, use the cursor value instead
     * (We need to use a different callback for that though)
     */
    if (sipo->mode == SIPO_MODE_DRIVERS) {
      ked.f1 = sipo->cursorTime;
      mode = MIRROR_KEYS_TIME;
    }
  }

  /* Get beztriple editing callbacks. */
  edit_cb = ANIM_editkeyframes_mirror(mode);

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Mirror keyframes. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    /* Apply unit corrections. */
    if (mode == GRAPHKEYS_MIRROR_VALUE) {
      short mapping_flag = ANIM_get_normalization_flags(ac->sl);
      float offset;
      float unit_scale = ANIM_unit_mapping_get_factor(ac->scene,
                                                      ale->id,
                                                      static_cast<FCurve *>(ale->key_data),
                                                      mapping_flag | ANIM_UNITCONV_ONLYKEYS,
                                                      &offset);

      ked.f1 = (cursor_value - offset) / unit_scale;
    }

    /* Perform actual mirroring. */
    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), false, false);
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), nullptr, edit_cb, BKE_fcurve_handles_recalc);
      ANIM_nla_mapping_apply_fcurve(adt, static_cast<FCurve *>(ale->key_data), true, false);
    }
    else {
      ANIM_fcurve_keyframes_loop(
          &ked, static_cast<FCurve *>(ale->key_data), nullptr, edit_cb, BKE_fcurve_handles_recalc);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int graphkeys_mirror_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  short mode;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get mirroring mode. */
  mode = RNA_enum_get(op->ptr, "type");

  /* Mirror keyframes. */
  mirror_graph_keys(&ac, mode);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_mirror(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Mirror Keys";
  ot->idname = "GRAPH_OT_mirror";
  ot->description = "Flip selected keyframes over the selected mirror line";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graphkeys_mirror_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_graphkeys_mirror_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Keyframes Operator
 * \{ */

static int graphkeys_smooth_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Smooth keyframes. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    /* For now, we can only smooth by flattening handles AND smoothing curve values.
     * Perhaps the mode argument could be removed, as that functionality is offered through
     * Snap->Flatten Handles anyway.
     */
    smooth_fcurve(static_cast<FCurve *>(ale->key_data));

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_smooth(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Smooth Keys";
  ot->idname = "GRAPH_OT_smooth";
  ot->description = "Apply weighted moving means to make selected F-Curves less bumpy";

  /* API callbacks */
  ot->exec = graphkeys_smooth_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ************************************************************************** */
/* F-CURVE MODIFIERS */

/* -------------------------------------------------------------------- */
/** \name Add F-Modifier Operator
 * \{ */

static const EnumPropertyItem *graph_fmodifier_itemf(bContext *C,
                                                     PointerRNA * /*ptr*/,
                                                     PropertyRNA * /*prop*/,
                                                     bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;
  int i = 0;

  if (C == nullptr) {
    return rna_enum_fmodifier_type_items;
  }

  /* Start from 1 to skip the 'Invalid' modifier type. */
  for (i = 1; i < FMODIFIER_NUM_TYPES; i++) {
    const FModifierTypeInfo *fmi = get_fmodifier_typeinfo(i);
    int index;

    /* Check if modifier is valid for this context. */
    if (fmi == nullptr) {
      continue;
    }

    index = RNA_enum_from_value(rna_enum_fmodifier_type_items, fmi->type);
    if (index != -1) { /* Not all types are implemented yet... */
      RNA_enum_item_add(&item, &totitem, &rna_enum_fmodifier_type_items[index]);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int graph_fmodifier_add_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  short type;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get type of modifier to add. */
  type = RNA_enum_get(op->ptr, "type");

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS |
            ANIMFILTER_FCURVESONLY);
  if (RNA_boolean_get(op->ptr, "only_active")) {
    /* FIXME: enforce in this case only a single channel to get handled? */
    filter |= ANIMFILTER_ACTIVE;
  }
  else {
    filter |= (ANIMFILTER_SEL | ANIMFILTER_CURVE_VISIBLE);
  }
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Add f-modifier to each curve. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;
    FModifier *fcm;

    /* Add F-Modifier of specified type to active F-Curve, and make it the active one. */
    fcm = add_fmodifier(&fcu->modifiers, type, fcu);
    if (fcm) {
      set_active_fmodifier(&fcu->modifiers, fcm);
    }
    else {
      BKE_report(op->reports, RPT_ERROR, "Modifier could not be added (see console for details)");
      break;
    }

    ale->update |= ANIM_UPDATE_DEPS;
  }

  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* Set notifier that things have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_fmodifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers */
  ot->name = "Add F-Curve Modifier";
  ot->idname = "GRAPH_OT_fmodifier_add";
  ot->description = "Add F-Modifier to the active/selected F-Curves";

  /* API callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = graph_fmodifier_add_exec;
  ot->poll = graphop_selected_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Id-props */
  prop = RNA_def_enum(ot->srna, "type", rna_enum_fmodifier_type_items, 0, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ACTION);
  RNA_def_enum_funcs(prop, graph_fmodifier_itemf);
  ot->prop = prop;

  RNA_def_boolean(
      ot->srna, "only_active", false, "Only Active", "Only add F-Modifier to active F-Curve");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy F-Modifiers Operator
 * \{ */

static int graph_fmodifier_copy_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  bAnimListElem *ale;
  bool ok = false;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Clear buffer first. */
  ANIM_fmodifiers_copybuf_free();

  /* Get the active F-Curve. */
  ale = get_active_fcurve_channel(&ac);

  /* If this exists, call the copy F-Modifiers API function. */
  if (ale && ale->data) {
    FCurve *fcu = (FCurve *)ale->data;

    /* TODO: When 'active' vs 'all' boolean is added, change last param! (Joshua Leung 2010) */
    ok = ANIM_fmodifiers_copy_to_buf(&fcu->modifiers, false);

    /* Free temp data now. */
    MEM_freeN(ale);
  }

  /* Successful or not? */
  if (ok == 0) {
    BKE_report(op->reports, RPT_ERROR, "No F-Modifiers available to be copied");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void GRAPH_OT_fmodifier_copy(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Copy F-Modifiers";
  ot->idname = "GRAPH_OT_fmodifier_copy";
  ot->description = "Copy the F-Modifier(s) of the active F-Curve";

  /* API callbacks */
  ot->exec = graph_fmodifier_copy_exec;
  ot->poll = graphop_active_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

/* Id-props */
#if 0
  ot->prop = RNA_def_boolean(ot->srna,
                             "all",
                             1,
                             "All F-Modifiers",
                             "Copy all the F-Modifiers, instead of just the active one");
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste F-Modifiers Operator
 * \{ */

static int graph_fmodifier_paste_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {nullptr, nullptr};
  int filter;

  const bool replace = RNA_boolean_get(op->ptr, "replace");
  bool ok = false;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Filter data. */
  if (RNA_boolean_get(op->ptr, "only_active")) {
    /* This should be the default (for buttons) - Just paste to the active FCurve. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY | ANIMFILTER_ACTIVE |
              ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  }
  else {
    /* This is only if the operator gets called from a hotkey or search -
     * Paste to all visible curves. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
              ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
  }

  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Paste modifiers. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;
    int tot;

    tot = ANIM_fmodifiers_paste_from_buf(&fcu->modifiers, replace, fcu);

    if (tot) {
      ale->update |= ANIM_UPDATE_DEPS;
      ok = true;
    }
  }

  if (ok) {
    ANIM_animdata_update(&ac, &anim_data);
  }
  ANIM_animdata_freelist(&anim_data);

  /* Successful or not? */
  if (ok) {
    /* Set notifier that keyframes have changed. */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports, RPT_ERROR, "No F-Modifiers to paste");
  return OPERATOR_CANCELLED;
}

void GRAPH_OT_fmodifier_paste(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Paste F-Modifiers";
  ot->idname = "GRAPH_OT_fmodifier_paste";
  ot->description = "Add copied F-Modifiers to the selected F-Curves";

  /* API callbacks */
  ot->exec = graph_fmodifier_paste_exec;
  ot->poll = graphop_active_fcurve_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  RNA_def_boolean(
      ot->srna, "only_active", false, "Only Active", "Only paste F-Modifiers on active F-Curve");
  RNA_def_boolean(
      ot->srna,
      "replace",
      false,
      "Replace Existing",
      "Replace existing F-Modifiers, instead of just appending to the end of the existing list");
}

/** \} */

/* ************************************************************************** */
/* Drivers */

/* -------------------------------------------------------------------- */
/** \name Copy Driver Variables Operator
 * \{ */

static int graph_driver_vars_copy_exec(bContext *C, wmOperator *op)
{
  bool ok = false;

  PointerRNA ptr = CTX_data_pointer_get_type(C, "active_editable_fcurve", &RNA_FCurve);

  /* If this exists, call the copy driver vars API function. */
  FCurve *fcu = static_cast<FCurve *>(ptr.data);

  if (fcu) {
    ok = ANIM_driver_vars_copy(op->reports, fcu);
  }

  /* Successful or not? */
  if (ok) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void GRAPH_OT_driver_variables_copy(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Copy Driver Variables";
  ot->idname = "GRAPH_OT_driver_variables_copy";
  ot->description = "Copy the driver variables of the active driver";

  /* API callbacks */
  ot->exec = graph_driver_vars_copy_exec;
  ot->poll = graphop_active_editable_fcurve_ctx_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Driver Variables Operator
 * \{ */

static int graph_driver_vars_paste_exec(bContext *C, wmOperator *op)
{
  const bool replace = RNA_boolean_get(op->ptr, "replace");
  bool ok = false;

  PointerRNA ptr = CTX_data_pointer_get_type(C, "active_editable_fcurve", &RNA_FCurve);

  /* If this exists, call the paste driver vars API function. */
  FCurve *fcu = static_cast<FCurve *>(ptr.data);

  if (fcu) {
    ok = ANIM_driver_vars_paste(op->reports, fcu, replace);
  }

  /* Successful or not? */
  if (ok) {
    /* Rebuild depsgraph, now that there are extra dependencies here. */
    DEG_relations_tag_update(CTX_data_main(C));

    /* Set notifier that keyframes have changed. */
    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, CTX_data_scene(C));

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void GRAPH_OT_driver_variables_paste(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Paste Driver Variables";
  ot->idname = "GRAPH_OT_driver_variables_paste";
  ot->description = "Add copied driver variables to the active driver";

  /* API callbacks */
  ot->exec = graph_driver_vars_paste_exec;
  ot->poll = graphop_active_editable_fcurve_ctx_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  RNA_def_boolean(ot->srna,
                  "replace",
                  false,
                  "Replace Existing",
                  "Replace existing driver variables, instead of just appending to the end of the "
                  "existing list");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Invalid Drivers Operator
 * \{ */

static int graph_driver_delete_invalid_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  bool ok = false;
  uint deleted = 0;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* NOTE: We might need a scene update to evaluate the driver flags. */

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY |
            ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Find invalid drivers. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->data;
    if (ELEM(nullptr, fcu, fcu->driver)) {
      continue;
    }
    if (!(fcu->driver->flag & DRIVER_FLAG_INVALID)) {
      continue;
    }

    ok |= ANIM_remove_driver(op->reports, ale->id, fcu->rna_path, fcu->array_index, 0);
    if (!ok) {
      break;
    }
    deleted += 1;
  }

  /* Cleanup. */
  ANIM_animdata_freelist(&anim_data);

  if (deleted > 0) {
    /* Notify the world of any changes. */
    DEG_relations_tag_update(CTX_data_main(C));
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);
    WM_reportf(RPT_INFO, "Deleted %u drivers", deleted);
  }
  else {
    WM_report(RPT_INFO, "No drivers deleted");
  }

  /* Successful or not? */
  if (!ok) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static bool graph_driver_delete_invalid_poll(bContext *C)
{
  bAnimContext ac;
  ScrArea *area = CTX_wm_area(C);

  /* Firstly, check if in Graph Editor. */
  if ((area == nullptr) || (area->spacetype != SPACE_GRAPH)) {
    return false;
  }

  /* Try to init Anim-Context stuff ourselves and check. */
  return ANIM_animdata_get_context(C, &ac) != 0;
}

void GRAPH_OT_driver_delete_invalid(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Delete Invalid Drivers";
  ot->idname = "GRAPH_OT_driver_delete_invalid";
  ot->description = "Delete all visible drivers considered invalid";

  /* API callbacks */
  ot->exec = graph_driver_delete_invalid_exec;
  ot->poll = graph_driver_delete_invalid_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
