/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_function_ref.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_fcurve.hh"
#include "BKE_nla.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_markers.hh"

#include "ANIM_action.hh"

using namespace blender;

/* This file defines an API and set of callback-operators for
 * non-destructive editing of keyframe data.
 *
 * Two API functions are defined for actually performing the operations on the data:
 * ANIM_fcurve_keyframes_loop()
 * which take the data they operate on, a few callbacks defining what operations to perform.
 *
 * As operators which work on keyframes usually apply the same operation on all BezTriples in
 * every channel, the code has been optimized providing a set of functions which will get the
 * appropriate bezier-modify function to set. These functions (ANIM_editkeyframes_*) will need
 * to be called before getting any channels.
 *
 * A set of 'validation' callbacks are provided for checking if a BezTriple should be operated on.
 * These should only be used when using a 'general' BezTriple editor (i.e. selection setters which
 * don't check existing selection status).
 *
 * - Joshua Leung, Dec 2008
 */

/* ************************************************************************** */
/* Keyframe Editing Loops - Exposed API */

/* --------------------------- Base Functions ------------------------------------ */

short ANIM_fcurve_keyframes_loop(KeyframeEditData *ked,
                                 FCurve *fcu,
                                 KeyframeEditFunc key_ok,
                                 KeyframeEditFunc key_cb,
                                 FcuEditFunc fcu_cb)
{
  BezTriple *bezt;
  short ok = 0;
  uint i;

  /* sanity check */
  if (ELEM(nullptr, fcu, fcu->bezt)) {
    return 0;
  }

  /* Set the F-Curve into the edit-data so that it can be accessed. */
  if (ked) {
    ked->fcu = fcu;
    ked->curIndex = 0;
    ked->curflags = KEYFRAME_NONE;
  }

  /* if function to apply to bezier curves is set, then loop through executing it on beztriples */
  if (key_cb) {
    /* if there's a validation func, include that check in the loop
     * (this is should be more efficient than checking for it in every loop)
     */
    if (key_ok) {
      for (bezt = fcu->bezt, i = 0; i < fcu->totvert; bezt++, i++) {
        if (ked) {
          /* advance the index, and reset the ok flags (to not influence the result) */
          ked->curIndex = i;
          ked->curflags = KEYFRAME_NONE;
        }

        /* Only operate on this BezTriple if it fulfills the criteria of the validation func */
        if ((ok = key_ok(ked, bezt))) {
          if (ked) {
            ked->curflags = eKeyframeVertOk(ok);
          }

          /* Exit with return-code '1' if function returns positive
           * This is useful if finding if some BezTriple satisfies a condition.
           */
          if (key_cb(ked, bezt)) {
            return 1;
          }
        }
      }
    }
    else {
      for (bezt = fcu->bezt, i = 0; i < fcu->totvert; bezt++, i++) {
        if (ked) {
          ked->curIndex = i;
        }

        /* Exit with return-code '1' if function returns positive
         * This is useful if finding if some BezTriple satisfies a condition.
         */
        if (key_cb(ked, bezt)) {
          return 1;
        }
      }
    }
  }

  /* Unset the F-Curve from the edit-data now that it's done. */
  if (ked) {
    ked->fcu = nullptr;
    ked->curIndex = 0;
    ked->curflags = KEYFRAME_NONE;
  }

  /* if fcu_cb (F-Curve post-editing callback) has been specified then execute it */
  if (fcu_cb) {
    fcu_cb(fcu);
  }

  /* done */
  return 0;
}

/* --------------------- Further Abstracted (Not Exposed Directly) ----------------------------- */

/* This function is used to loop over the keyframe data in an Action Group */
static short agrp_keyframes_loop(KeyframeEditData *ked,
                                 bActionGroup *agrp,
                                 KeyframeEditFunc key_ok,
                                 KeyframeEditFunc key_cb,
                                 FcuEditFunc fcu_cb)
{
  /* sanity check */
  if (agrp == nullptr) {
    return 0;
  }

  /* Legacy actions. */
  if (agrp->wrap().is_legacy()) {
    LISTBASE_FOREACH (FCurve *, fcu, &agrp->channels) {
      if (fcu->grp == agrp) {
        if (ANIM_fcurve_keyframes_loop(ked, fcu, key_ok, key_cb, fcu_cb)) {
          return 1;
        }
      }
    }
    return 0;
  }

  /* Layered actions. */
  animrig::Channelbag &channelbag = agrp->channelbag->wrap();
  Span<FCurve *> fcurves = channelbag.fcurves().slice(agrp->fcurve_range_start,
                                                      agrp->fcurve_range_length);
  for (FCurve *fcurve : fcurves) {
    if (ANIM_fcurve_keyframes_loop(ked, fcurve, key_ok, key_cb, fcu_cb)) {
      return 1;
    }
  }

  return 0;
}

/* Loop over all keyframes in the layered Action. */
static short action_layered_keyframes_loop(KeyframeEditData *ked,
                                           animrig::Action &action,
                                           animrig::Slot *slot,
                                           KeyframeEditFunc key_ok,
                                           KeyframeEditFunc key_cb,
                                           FcuEditFunc fcu_cb)
{
  if (!slot) {
    /* Valid situation, and will not have any FCurves. */
    return 0;
  }

  Span<FCurve *> fcurves = animrig::fcurves_for_action_slot(action, slot->handle);
  for (FCurve *fcurve : fcurves) {
    if (ANIM_fcurve_keyframes_loop(ked, fcurve, key_ok, key_cb, fcu_cb)) {
      return 1;
    }
  }
  return 0;
}

/* This function is used to loop over the keyframe data in an Action */
static short action_legacy_keyframes_loop(KeyframeEditData *ked,
                                          bAction *act,
                                          KeyframeEditFunc key_ok,
                                          KeyframeEditFunc key_cb,
                                          FcuEditFunc fcu_cb)
{
  /* sanity check */
  if (act == nullptr) {
    return 0;
  }

  /* just loop through all F-Curves */
  LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
    if (ANIM_fcurve_keyframes_loop(ked, fcu, key_ok, key_cb, fcu_cb)) {
      return 1;
    }
  }

  return 0;
}

/* This function is used to loop over the keyframe data in an Object */
static short ob_keyframes_loop(KeyframeEditData *ked,
                               bDopeSheet *ads,
                               Object *ob,
                               KeyframeEditFunc key_ok,
                               KeyframeEditFunc key_cb,
                               FcuEditFunc fcu_cb)
{
  bAnimContext ac = {nullptr};
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  int ret = 0;

  bAnimListElem dummy_chan = {nullptr};
  Base dummy_base = {nullptr};

  if (ob == nullptr) {
    return 0;
  }

  /* create a dummy wrapper data to work with */
  dummy_base.object = ob;

  dummy_chan.type = ANIMTYPE_OBJECT;
  dummy_chan.data = &dummy_base;
  dummy_chan.id = &ob->id;
  dummy_chan.adt = ob->adt;

  ac.ads = ads;
  ac.data = &dummy_chan;
  ac.datatype = ANIMCONT_CHANNEL;
  ac.filters.flag = eDopeSheet_FilterFlag(ads->filterflag);
  ac.filters.flag2 = eDopeSheet_FilterFlag2(ads->filterflag2);

  /* get F-Curves to take keyframes from */
  filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Loop through each F-Curve, applying the operation as required,
   * but stopping on the first one. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ANIM_fcurve_keyframes_loop(ked, static_cast<FCurve *>(ale->data), key_ok, key_cb, fcu_cb))
    {
      ret = 1;
      break;
    }
  }

  ANIM_animdata_freelist(&anim_data);

  /* Return the return code (defaults to zero if nothing happened). */
  return ret;
}

/* This function is used to loop over the keyframe data in a Scene */
static short scene_keyframes_loop(KeyframeEditData *ked,
                                  bDopeSheet *ads,
                                  Scene *sce,
                                  KeyframeEditFunc key_ok,
                                  KeyframeEditFunc key_cb,
                                  FcuEditFunc fcu_cb)
{
  bAnimContext ac = {nullptr};
  ListBase anim_data = {nullptr, nullptr};
  int filter;
  int ret = 0;

  bAnimListElem dummy_chan = {nullptr};

  if (sce == nullptr) {
    return 0;
  }

  /* create a dummy wrapper data to work with */
  dummy_chan.type = ANIMTYPE_SCENE;
  dummy_chan.data = sce;
  dummy_chan.id = &sce->id;
  dummy_chan.adt = sce->adt;

  ac.ads = ads;
  ac.data = &dummy_chan;
  ac.datatype = ANIMCONT_CHANNEL;
  ac.filters.flag = eDopeSheet_FilterFlag(ads->filterflag);
  ac.filters.flag2 = eDopeSheet_FilterFlag2(ads->filterflag2);

  /* get F-Curves to take keyframes from */
  filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;
  ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* Loop through each F-Curve, applying the operation as required,
   * but stopping on the first one. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ANIM_fcurve_keyframes_loop(ked, static_cast<FCurve *>(ale->data), key_ok, key_cb, fcu_cb))
    {
      ret = 1;
      break;
    }
  }

  ANIM_animdata_freelist(&anim_data);

  /* Return the return code (defaults to zero if nothing happened). */
  return ret;
}

/* This function is used to loop over the keyframe data in a DopeSheet summary */
static short summary_keyframes_loop(KeyframeEditData *ked,
                                    bAnimContext *ac,
                                    KeyframeEditFunc key_ok,
                                    KeyframeEditFunc key_cb,
                                    FcuEditFunc fcu_cb)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter, ret_code = 0;

  /* sanity check */
  if (ac == nullptr) {
    return 0;
  }

  /* get F-Curves to take keyframes from */
  filter = ANIMFILTER_DATA_VISIBLE;
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* loop through each F-Curve, working on the keyframes until the first curve aborts */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    switch (ale->datatype) {
      case ALE_MASKLAY:
      case ALE_GPFRAME:
      case ALE_GREASE_PENCIL_CEL:
        break;

      case ALE_FCURVE:
      default: {
        if (ked && ked->iterflags) {
          /* make backups of the current values, so that a localized fix
           * (e.g. NLA time remapping) can be applied to these values
           */
          float f1 = ked->f1;
          float f2 = ked->f2;

          if (ked->iterflags & KED_F1_NLA_UNMAP) {
            ked->f1 = ANIM_nla_tweakedit_remap(ale, f1, NLATIME_CONVERT_UNMAP);
          }
          if (ked->iterflags & KED_F2_NLA_UNMAP) {
            ked->f2 = ANIM_nla_tweakedit_remap(ale, f2, NLATIME_CONVERT_UNMAP);
          }

          /* now operate on the channel as per normal */
          ret_code = ANIM_fcurve_keyframes_loop(
              ked, static_cast<FCurve *>(ale->data), key_ok, key_cb, fcu_cb);

          /* reset */
          ked->f1 = f1;
          ked->f2 = f2;
        }
        else {
          /* no special handling required... */
          ret_code = ANIM_fcurve_keyframes_loop(
              ked, static_cast<FCurve *>(ale->data), key_ok, key_cb, fcu_cb);
        }
        break;
      }
    }

    if (ret_code) {
      break;
    }
  }

  ANIM_animdata_freelist(&anim_data);

  return ret_code;
}

/* --- */

short ANIM_animchannel_keyframes_loop(KeyframeEditData *ked,
                                      bDopeSheet *ads,
                                      bAnimListElem *ale,
                                      KeyframeEditFunc key_ok,
                                      KeyframeEditFunc key_cb,
                                      FcuEditFunc fcu_cb)
{
  /* sanity checks */
  if (ale == nullptr) {
    return 0;
  }

  /* method to use depends on the type of keyframe data */
  switch (ale->datatype) {
    /* direct keyframe data (these loops are exposed) */
    case ALE_FCURVE: /* F-Curve */
      return ANIM_fcurve_keyframes_loop(
          ked, static_cast<FCurve *>(ale->key_data), key_ok, key_cb, fcu_cb);

    /* indirect 'summaries' (these are not exposed directly)
     * NOTE: must keep this code in sync with the drawing code and also the filtering code!
     */
    case ALE_GROUP: /* action group */
      return agrp_keyframes_loop(
          ked, static_cast<bActionGroup *>(ale->data), key_ok, key_cb, fcu_cb);
    case ALE_ACTION_LAYERED: { /* Layered Action. */
      /* This assumes that the ALE_ACTION_LAYERED channel is shown in the dope-sheet context,
       * underneath the data-block that owns `ale->adt`. So that means that the loop is limited to
       * the keys that belong to that slot. */
      animrig::Action &action = static_cast<bAction *>(ale->key_data)->wrap();
      animrig::Slot *slot = action.slot_for_handle(ale->adt->slot_handle);
      return action_layered_keyframes_loop(ked, action, slot, key_ok, key_cb, fcu_cb);
    }
    case ALE_ACTION_SLOT: {
      animrig::Action *action = static_cast<animrig::Action *>(ale->key_data);
      BLI_assert(action);
      animrig::Slot *slot = static_cast<animrig::Slot *>(ale->data);
      return action_layered_keyframes_loop(ked, *action, slot, key_ok, key_cb, fcu_cb);
    }

    case ALE_ACT: /* Legacy Action. */
      return action_legacy_keyframes_loop(
          ked, static_cast<bAction *>(ale->key_data), key_ok, key_cb, fcu_cb);
    case ALE_OB: /* object */
      return ob_keyframes_loop(
          ked, ads, static_cast<Object *>(ale->key_data), key_ok, key_cb, fcu_cb);
    case ALE_SCE: /* scene */
      return scene_keyframes_loop(
          ked, ads, static_cast<Scene *>(ale->data), key_ok, key_cb, fcu_cb);
    case ALE_ALL: /* 'all' (DopeSheet summary) */
      return summary_keyframes_loop(
          ked, static_cast<bAnimContext *>(ale->data), key_ok, key_cb, fcu_cb);

    case ALE_NONE:
    case ALE_GPFRAME:
    case ALE_MASKLAY:
    case ALE_NLASTRIP:
    case ALE_GREASE_PENCIL_CEL:
    case ALE_GREASE_PENCIL_DATA:
    case ALE_GREASE_PENCIL_GROUP:
      break;
  }

  return 0;
}

void ANIM_animdata_keyframe_callback(bAnimContext *ac,
                                     eAnimFilter_Flags filter,
                                     KeyframeEditFunc callback_fn)
{
  ListBase anim_data = {nullptr, nullptr};

  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    ANIM_fcurve_keyframes_loop(nullptr,
                               static_cast<FCurve *>(ale->key_data),
                               nullptr,
                               callback_fn,
                               BKE_fcurve_handles_recalc);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************** */
/* Keyframe Integrity Tools */

void ANIM_editkeyframes_refresh(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};
  int filter;

  /* filter animation data */
  filter = ANIMFILTER_DATA_VISIBLE;
  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Loop over F-Curves that are likely to have been edited, and tag them to
   * ensure the keyframes are in order and handles are in a valid position. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    ale->update |= ANIM_UPDATE_DEPS | ANIM_UPDATE_HANDLES | ANIM_UPDATE_ORDER;
  }

  /* free temp data */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ************************************************************************** */
/* BezTriple Validation Callbacks */

/* ------------------------ */

static bool handles_visible(KeyframeEditData *ked, BezTriple *bezt)
{
  const bool handles_shown = (ked->iterflags & KEYFRAME_ITER_HANDLES_INVISIBLE) == 0;
  if (!handles_shown) {
    return false;
  }
  const bool handles_shown_only_selected = ked->iterflags &
                                           KEYFRAME_ITER_HANDLES_DEFAULT_INVISIBLE;

  return handles_shown_only_selected ? BEZT_ISSEL_ANY(bezt) : true;
}

static short keyframe_ok_checks(
    KeyframeEditData *ked,
    BezTriple *bezt,
    blender::FunctionRef<bool(KeyframeEditData *ked, BezTriple *bezt, const int index)> check)
{
  short ok = 0;
  if (check(ked, bezt, 1)) {
    ok |= KEYFRAME_OK_KEY;
  }
  if (ked && (ked->iterflags & KEYFRAME_ITER_INCL_HANDLES))
  { /* Only act on visible items, so check handle visibility state. */
    if (handles_visible(ked, bezt)) {
      if (check(ked, bezt, 0)) {
        ok |= KEYFRAME_OK_H1;
      }
      if (check(ked, bezt, 2)) {
        ok |= KEYFRAME_OK_H2;
      }
    }
  }
  return ok;
}

/* ------------------------ */

static short ok_bezier_frame(KeyframeEditData *ked, BezTriple *bezt)
{
  /* frame is stored in f1 property (this float accuracy check may need to be dropped?) */
  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return IS_EQF(bezt->vec[index][0], ked->f1);
      });

  return ok;
}

static short ok_bezier_framerange(KeyframeEditData *ked, BezTriple *bezt)
{
  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return (bezt->vec[index][0] > ked->f1) && (bezt->vec[index][0] < ked->f2);
      });

  return ok;
}

static short ok_bezier_selected(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* this macro checks all beztriple handles for selection...
   * only one of the verts has to be selected for this to be ok...
   */
  if (BEZT_ISSEL_ANY(bezt)) {
    return KEYFRAME_OK_ALL;
  }
  return 0;
}

static short ok_bezier_selected_key(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* This macro checks the beztriple key (f2) selection. */
  if (BEZT_ISSEL_IDX(bezt, 1)) {
    return KEYFRAME_OK_KEY;
  }
  return 0;
}

static short ok_bezier_value(KeyframeEditData *ked, BezTriple *bezt)
{
  /* Value is stored in f1 property:
   * - This float accuracy check may need to be dropped?
   * - Should value be stored in f2 instead
   *   so that we won't have conflicts when using f1 for frames too?
   */
  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return IS_EQF(bezt->vec[index][1], ked->f1);
      });

  return ok;
}

static short ok_bezier_valuerange(KeyframeEditData *ked, BezTriple *bezt)
{
  /* value range is stored in float properties */
  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return (bezt->vec[index][1] > ked->f1) && (bezt->vec[index][1] < ked->f2);
      });

  return ok;
}

static short ok_bezier_region(KeyframeEditData *ked, BezTriple *bezt)
{
  /* rect is stored in data property (it's of type rectf, but may not be set) */
  if (!ked->data) {
    return 0;
  }

  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return BLI_rctf_isect_pt_v(static_cast<rctf *>(ked->data), bezt->vec[index]);
      });

  return ok;
}

bool keyframe_region_lasso_test(const KeyframeEdit_LassoData *data_lasso, const float xy[2])
{
  if (BLI_rctf_isect_pt_v(data_lasso->rectf_scaled, xy)) {
    float xy_view[2];

    BLI_rctf_transform_pt_v(data_lasso->rectf_view, data_lasso->rectf_scaled, xy_view, xy);

    if (BLI_lasso_is_point_inside(data_lasso->mcoords, xy_view[0], xy_view[1], INT_MAX)) {
      return true;
    }
  }

  return false;
}

static short ok_bezier_region_lasso(KeyframeEditData *ked, BezTriple *bezt)
{
  /* check for lasso customdata (KeyframeEdit_LassoData) */
  if (!ked->data) {
    return 0;
  }

  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return keyframe_region_lasso_test(static_cast<KeyframeEdit_LassoData *>(ked->data),
                                          bezt->vec[index]);
      });

  return ok;
}

static short ok_bezier_channel_lasso(KeyframeEditData *ked, BezTriple *bezt)
{
  /* check for lasso customdata (KeyframeEdit_LassoData) */
  if (ked->data) {
    KeyframeEdit_LassoData *data = static_cast<KeyframeEdit_LassoData *>(ked->data);
    float pt[2];

    /* late-binding remap of the x values (for summary channels) */
    /* XXX: Ideally we reset, but it should be fine just leaving it as-is
     * as the next channel will reset it properly, while the next summary-channel
     * curve will also reset by itself...
     */
    if (ked->iterflags & (KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP)) {
      data->rectf_scaled->xmin = ked->f1;
      data->rectf_scaled->xmax = ked->f2;
    }

    /* only use the x-coordinate of the point; the y is the channel range... */
    pt[0] = bezt->vec[1][0];
    pt[1] = ked->channel_y;

    if (keyframe_region_lasso_test(data, pt)) {
      return KEYFRAME_OK_KEY;
    }
  }
  return 0;
}

bool keyframe_region_circle_test(const KeyframeEdit_CircleData *data_circle, const float xy[2])
{
  if (BLI_rctf_isect_pt_v(data_circle->rectf_scaled, xy)) {
    float xy_view[2];

    BLI_rctf_transform_pt_v(data_circle->rectf_view, data_circle->rectf_scaled, xy_view, xy);

    xy_view[0] = xy_view[0] - data_circle->mval[0];
    xy_view[1] = xy_view[1] - data_circle->mval[1];
    return len_squared_v2(xy_view) < data_circle->radius_squared;
  }

  return false;
}

static short ok_bezier_region_circle(KeyframeEditData *ked, BezTriple *bezt)
{
  /* check for circle select customdata (KeyframeEdit_CircleData) */
  if (!ked->data) {
    return 0;
  }

  const short ok = keyframe_ok_checks(
      ked, bezt, [](KeyframeEditData *ked, BezTriple *bezt, int index) -> bool {
        return keyframe_region_circle_test(static_cast<KeyframeEdit_CircleData *>(ked->data),
                                           bezt->vec[index]);
      });

  return ok;
}

static short ok_bezier_channel_circle(KeyframeEditData *ked, BezTriple *bezt)
{
  /* check for circle select customdata (KeyframeEdit_CircleData) */
  if (ked->data) {
    KeyframeEdit_CircleData *data = static_cast<KeyframeEdit_CircleData *>(ked->data);
    float pt[2];

    /* late-binding remap of the x values (for summary channels) */
    /* XXX: Ideally we reset, but it should be fine just leaving it as-is
     * as the next channel will reset it properly, while the next summary-channel
     * curve will also reset by itself...
     */
    if (ked->iterflags & (KED_F1_NLA_UNMAP | KED_F2_NLA_UNMAP)) {
      data->rectf_scaled->xmin = ked->f1;
      data->rectf_scaled->xmax = ked->f2;
    }

    /* only use the x-coordinate of the point; the y is the channel range... */
    pt[0] = bezt->vec[1][0];
    pt[1] = ked->channel_y;

    if (keyframe_region_circle_test(data, pt)) {
      return KEYFRAME_OK_KEY;
    }
  }
  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_ok(short mode)
{
  /* eEditKeyframes_Validate */
  switch (mode) {
    case BEZT_OK_FRAME:
      /* only if bezt falls on the right frame (float) */
      return ok_bezier_frame;
    case BEZT_OK_FRAMERANGE:
      /* only if bezt falls within the specified frame range (floats) */
      return ok_bezier_framerange;
    case BEZT_OK_SELECTED:
      /* only if bezt is selected (any of f1, f2, f3) */
      return ok_bezier_selected;
    case BEZT_OK_SELECTED_KEY:
      /* only if bezt is selected (f2 is enough) */
      return ok_bezier_selected_key;
    case BEZT_OK_VALUE:
      /* only if bezt value matches (float) */
      return ok_bezier_value;
    case BEZT_OK_VALUERANGE:
      /* only if bezier falls within the specified value range (floats) */
      return ok_bezier_valuerange;
    case BEZT_OK_REGION:
      /* only if bezier falls within the specified rect (data -> rectf) */
      return ok_bezier_region;
    case BEZT_OK_REGION_LASSO:
      /* only if the point falls within KeyframeEdit_LassoData defined data */
      return ok_bezier_region_lasso;
    case BEZT_OK_REGION_CIRCLE:
      /* only if the point falls within KeyframeEdit_CircleData defined data */
      return ok_bezier_region_circle;
    case BEZT_OK_CHANNEL_LASSO:
      /* same as BEZT_OK_REGION_LASSO, but we're only using the x-value of the points */
      return ok_bezier_channel_lasso;
    case BEZT_OK_CHANNEL_CIRCLE:
      /* same as BEZT_OK_REGION_CIRCLE, but we're only using the x-value of the points */
      return ok_bezier_channel_circle;
    default: /* nothing was ok */
      return nullptr;
  }
}

/* ******************************************* */
/* Assorted Utility Functions */

short bezt_calc_average(KeyframeEditData *ked, BezTriple *bezt)
{
  /* only if selected */
  if (bezt->f2 & SELECT) {
    /* store average time in float 1 (only do rounding at last step) */
    ked->f1 += bezt->vec[1][0];

    /* store average value in float 2 (only do rounding at last step)
     * - this isn't always needed, but some operators may also require this
     */
    ked->f2 += bezt->vec[1][1];

    /* increment number of items */
    ked->i1++;
  }

  return 0;
}

short bezt_to_cfraelem(KeyframeEditData *ked, BezTriple *bezt)
{
  /* only if selected */
  if ((bezt->f2 & SELECT) == 0) {
    return 0;
  }

  CfraElem *ce = MEM_callocN<CfraElem>("cfraElem");
  BLI_addtail(&ked->list, ce);

  /* bAnimListElem so we can do NLA mapping, we want the cfra to be in "global" time */
  bAnimListElem *ale = static_cast<bAnimListElem *>(ked->data);
  if (ale != nullptr) {
    ce->cfra = ANIM_nla_tweakedit_remap(ale, bezt->vec[1][0], NLATIME_CONVERT_MAP);
  }
  else {
    ce->cfra = bezt->vec[1][0];
  }

  return 0;
}

void bezt_remap_times(KeyframeEditData *ked, BezTriple *bezt)
{
  KeyframeEditCD_Remap *rmap = static_cast<KeyframeEditCD_Remap *>(ked->data);
  const float scale = (rmap->newMax - rmap->newMin) / (rmap->oldMax - rmap->oldMin);

  /* perform transform on all three handles unless indicated otherwise */
  /* TODO: need to include some checks for that */

  bezt->vec[0][0] = scale * (bezt->vec[0][0] - rmap->oldMin) + rmap->newMin;
  bezt->vec[1][0] = scale * (bezt->vec[1][0] - rmap->oldMin) + rmap->newMin;
  bezt->vec[2][0] = scale * (bezt->vec[2][0] - rmap->oldMin) + rmap->newMin;
}

/* ******************************************* */
/* Transform */

/* snaps the keyframe to the nearest frame */
static short snap_bezier_nearest(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BKE_fcurve_keyframe_move_time_with_handles(bezt, floorf(bezt->vec[1][0] + 0.5f));
  }
  return 0;
}

/* snaps the keyframe to the nearest second */
static short snap_bezier_nearestsec(KeyframeEditData *ked, BezTriple *bezt)
{
  const Scene *scene = ked->scene;
  const float secf = float(scene->frames_per_second());

  if (bezt->f2 & SELECT) {
    BKE_fcurve_keyframe_move_time_with_handles(bezt, floorf(bezt->vec[1][0] / secf + 0.5f) * secf);
  }
  return 0;
}

/* snaps the keyframe to the current frame */
static short snap_bezier_cframe(KeyframeEditData *ked, BezTriple *bezt)
{
  const Scene *scene = ked->scene;
  if (bezt->f2 & SELECT) {
    BKE_fcurve_keyframe_move_time_with_handles(bezt, float(scene->r.cfra));
  }
  return 0;
}

/* snaps the keyframe time to the nearest marker's frame */
static short snap_bezier_nearmarker(KeyframeEditData *ked, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BKE_fcurve_keyframe_move_time_with_handles(
        bezt, float(ED_markers_find_nearest_marker_time(&ked->list, bezt->vec[1][0])));
  }
  return 0;
}

/* make the handles have the same value as the key */
static short snap_bezier_horizontal(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->vec[0][1] = bezt->vec[2][1] = bezt->vec[1][1];

    if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM, HD_VECT)) {
      bezt->h1 = HD_ALIGN;
    }
    if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM, HD_VECT)) {
      bezt->h2 = HD_ALIGN;
    }
  }
  return 0;
}

/* frame to snap to is stored in the custom data -> first float value slot */
static short snap_bezier_time(KeyframeEditData *ked, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BKE_fcurve_keyframe_move_time_with_handles(bezt, ked->f1);
  }
  return 0;
}

/* value to snap to is stored in the custom data -> first float value slot */
static short snap_bezier_value(KeyframeEditData *ked, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BKE_fcurve_keyframe_move_value_with_handles(bezt, ked->f1);
  }
  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_snap(short mode)
{
  /* eEditKeyframes_Snap */
  switch (mode) {
    case SNAP_KEYS_NEARFRAME: /* snap to nearest frame */
      return snap_bezier_nearest;
    case SNAP_KEYS_CURFRAME: /* snap to current frame */
      return snap_bezier_cframe;
    case SNAP_KEYS_NEARMARKER: /* snap to nearest marker */
      return snap_bezier_nearmarker;
    case SNAP_KEYS_NEARSEC: /* snap to nearest second */
      return snap_bezier_nearestsec;
    case SNAP_KEYS_HORIZONTAL: /* snap handles to same value */
      return snap_bezier_horizontal;
    case SNAP_KEYS_TIME: /* snap to given frame/time */
      return snap_bezier_time;
    case SNAP_KEYS_VALUE: /* snap to given value */
      return snap_bezier_value;
    default: /* just in case */
      return snap_bezier_nearest;
  }
}

/* --------- */

static void mirror_bezier_xaxis_ex(BezTriple *bezt, const float center)
{
  for (int i = 0; i < 3; i++) {
    float diff = (center - bezt->vec[i][0]);
    bezt->vec[i][0] = (center + diff);
  }
  swap_v3_v3(bezt->vec[0], bezt->vec[2]);

  std::swap(bezt->h1, bezt->h2);
  std::swap(bezt->f1, bezt->f3);
}

static void mirror_bezier_yaxis_ex(BezTriple *bezt, const float center)
{
  for (int i = 0; i < 3; i++) {
    float diff = (center - bezt->vec[i][1]);
    bezt->vec[i][1] = (center + diff);
  }
}

static short mirror_bezier_cframe(KeyframeEditData *ked, BezTriple *bezt)
{
  const Scene *scene = ked->scene;

  if (bezt->f2 & SELECT) {
    mirror_bezier_xaxis_ex(bezt, scene->r.cfra);
  }

  return 0;
}

static short mirror_bezier_yaxis(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    /* Yes, names are inverted, we are mirroring across y axis, hence along x axis... */
    mirror_bezier_xaxis_ex(bezt, 0.0f);
  }

  return 0;
}

static short mirror_bezier_xaxis(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    /* Yes, names are inverted, we are mirroring across x axis, hence along y axis... */
    mirror_bezier_yaxis_ex(bezt, 0.0f);
  }

  return 0;
}

static short mirror_bezier_marker(KeyframeEditData *ked, BezTriple *bezt)
{
  /* mirroring time stored in f1 */
  if (bezt->f2 & SELECT) {
    mirror_bezier_xaxis_ex(bezt, ked->f1);
  }

  return 0;
}

static short mirror_bezier_time(KeyframeEditData *ked, BezTriple *bezt)
{
  /* value to mirror over is stored in f1 */
  if (bezt->f2 & SELECT) {
    mirror_bezier_xaxis_ex(bezt, ked->f1);
  }

  return 0;
}

static short mirror_bezier_value(KeyframeEditData *ked, BezTriple *bezt)
{
  /* value to mirror over is stored in the custom data -> first float value slot */
  if (bezt->f2 & SELECT) {
    mirror_bezier_yaxis_ex(bezt, ked->f1);
  }

  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_mirror(short mode)
{
  switch (mode) {
    case MIRROR_KEYS_CURFRAME: /* mirror over current frame */
      return mirror_bezier_cframe;
    case MIRROR_KEYS_YAXIS: /* mirror over frame 0 */
      return mirror_bezier_yaxis;
    case MIRROR_KEYS_XAXIS: /* mirror over value 0 */
      return mirror_bezier_xaxis;
    case MIRROR_KEYS_MARKER: /* mirror over marker */
      return mirror_bezier_marker;
    case MIRROR_KEYS_TIME: /* mirror over frame/time */
      return mirror_bezier_time;
    case MIRROR_KEYS_VALUE: /* mirror over given value */
      return mirror_bezier_value;
    default: /* just in case */
      return mirror_bezier_yaxis;
  }
}

/* ******************************************* */
/* Settings */

/**
 * Standard validation step for a few of these
 * (implemented as macro for inlining without fn-call overhead):
 * "if the handles are not of the same type, set them to type free".
 */
#define ENSURE_HANDLES_MATCH(bezt) \
  if (bezt->h1 != bezt->h2) { \
    if (ELEM(bezt->h1, HD_ALIGN, HD_AUTO, HD_AUTO_ANIM)) { \
      bezt->h1 = HD_FREE; \
    } \
    if (ELEM(bezt->h2, HD_ALIGN, HD_AUTO, HD_AUTO_ANIM)) { \
      bezt->h2 = HD_FREE; \
    } \
  } \
  (void)0

/* Sets the selected bezier handles to type 'auto' */
static short set_bezier_auto(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* If the key is selected, always apply to both handles. */
  if (bezt->f2 & SELECT) {
    bezt->h1 = bezt->h2 = HD_AUTO;
  }
  else {
    if (bezt->f1 & SELECT) {
      bezt->h1 = HD_AUTO;
    }
    if (bezt->f3 & SELECT) {
      bezt->h2 = HD_AUTO;
    }

    ENSURE_HANDLES_MATCH(bezt);
  }

  return 0;
}

/* Sets the selected bezier handles to type 'auto-clamped'
 * NOTE: this is like auto above, but they're handled a bit different
 */
static short set_bezier_auto_clamped(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* If the key is selected, always apply to both handles. */
  if (bezt->f2 & SELECT) {
    bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
  }
  else {
    if (bezt->f1 & SELECT) {
      bezt->h1 = HD_AUTO_ANIM;
    }
    if (bezt->f3 & SELECT) {
      bezt->h2 = HD_AUTO_ANIM;
    }

    ENSURE_HANDLES_MATCH(bezt);
  }

  return 0;
}

/* Sets the selected bezier handles to type 'vector'. */
static short set_bezier_vector(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* If the key is selected, always apply to both handles. */
  if (bezt->f2 & SELECT) {
    bezt->h1 = bezt->h2 = HD_VECT;
  }
  else {
    if (bezt->f1 & SELECT) {
      bezt->h1 = HD_VECT;
    }
    if (bezt->f3 & SELECT) {
      bezt->h2 = HD_VECT;
    }
  }

  return 0;
}

/**
 * Queries if the handle should be set to 'free' or 'align'.
 *
 * \note This was used for the 'toggle free/align' option
 * currently this isn't used, but may be restored later.
 */
static short bezier_isfree(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if ((bezt->f1 & SELECT) && (bezt->h1)) {
    return 1;
  }
  if ((bezt->f3 & SELECT) && (bezt->h2)) {
    return 1;
  }
  return 0;
}

/* Sets selected bezier handles to type 'align' */
static short set_bezier_align(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* If the key is selected, always apply to both handles. */
  if (bezt->f2 & SELECT) {
    bezt->h1 = bezt->h2 = HD_ALIGN;
  }
  else {
    if (bezt->f1 & SELECT) {
      bezt->h1 = HD_ALIGN;
    }
    if (bezt->f3 & SELECT) {
      bezt->h2 = HD_ALIGN;
    }
  }

  return 0;
}

/* Sets selected bezier handles to type 'free'. */
static short set_bezier_free(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* If the key is selected, always apply to both handles. */
  if (bezt->f2 & SELECT) {
    bezt->h1 = bezt->h2 = HD_FREE;
  }
  else {
    if (bezt->f1 & SELECT) {
      bezt->h1 = HD_FREE;
    }
    if (bezt->f3 & SELECT) {
      bezt->h2 = HD_FREE;
    }
  }

  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_handles(short mode)
{
  switch (mode) {
    case HD_AUTO: /* auto */
      return set_bezier_auto;
    case HD_AUTO_ANIM: /* auto clamped */
      return set_bezier_auto_clamped;

    case HD_VECT: /* vector */
      return set_bezier_vector;
    case HD_FREE: /* free */
      return set_bezier_free;
    case HD_ALIGN: /* align */
      return set_bezier_align;

    default: /* check for toggle free or align? */
      return bezier_isfree;
  }
}

/* ------- */

static short set_bezt_constant(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_CONST;
  }
  return 0;
}

static short set_bezt_linear(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_LIN;
  }
  return 0;
}

static short set_bezt_bezier(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_BEZ;
  }
  return 0;
}

static short set_bezt_back(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_BACK;
  }
  return 0;
}

static short set_bezt_bounce(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_BOUNCE;
  }
  return 0;
}

static short set_bezt_circle(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_CIRC;
  }
  return 0;
}

static short set_bezt_cubic(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_CUBIC;
  }
  return 0;
}

static short set_bezt_elastic(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_ELASTIC;
  }
  return 0;
}

static short set_bezt_expo(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_EXPO;
  }
  return 0;
}

static short set_bezt_quad(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_QUAD;
  }
  return 0;
}

static short set_bezt_quart(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_QUART;
  }
  return 0;
}

static short set_bezt_quint(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_QUINT;
  }
  return 0;
}

static short set_bezt_sine(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->ipo = BEZT_IPO_SINE;
  }
  return 0;
}

static void handle_flatten(float vec[3][3], const int idx, const float direction[2])
{
  BLI_assert_msg(idx == 0 || idx == 2, "handle_flatten() expects a handle index");

  add_v2_v2v2(vec[idx], vec[1], direction);
}

static void handle_set_length(float vec[3][3], const int idx, const float handle_length)
{
  BLI_assert_msg(idx == 0 || idx == 2, "handle_set_length() expects a handle index");

  float handle_direction[2];
  sub_v2_v2v2(handle_direction, vec[idx], vec[1]);
  normalize_v2_length(handle_direction, handle_length);
  add_v2_v2v2(vec[idx], vec[1], handle_direction);
}

void ANIM_fcurve_equalize_keyframes_loop(FCurve *fcu,
                                         const eEditKeyframes_Equalize mode,
                                         const float handle_length,
                                         const bool flatten)
{
  uint i;
  BezTriple *bezt;
  const float flat_direction_left[2] = {-handle_length, 0.0f};
  const float flat_direction_right[2] = {handle_length, 0.0f};

  /* Loop through an F-Curves keyframes. */
  for (bezt = fcu->bezt, i = 0; i < fcu->totvert; bezt++, i++) {
    if ((bezt->f2 & SELECT) == 0) {
      continue;
    }

    /* Perform handle equalization if mode is 'Both' or 'Left'. */
    if (mode & EQUALIZE_HANDLES_LEFT) {
      /* If left handle type is 'Auto', 'Auto Clamped', or 'Vector', convert handles to
       * 'Aligned'.
       */
      if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM, HD_VECT)) {
        bezt->h1 = HD_ALIGN;
        bezt->h2 = HD_ALIGN;
      }

      if (flatten) {
        handle_flatten(bezt->vec, 0, flat_direction_left);
      }
      else {
        handle_set_length(bezt->vec, 0, handle_length);
      }
    }

    /* Perform handle equalization if mode is 'Both' or 'Right'. */
    if (mode & EQUALIZE_HANDLES_RIGHT) {
      /* If right handle type is 'Auto', 'Auto Clamped', or 'Vector', convert handles to
       * 'Aligned'. */
      if (ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM, HD_VECT)) {
        bezt->h1 = HD_ALIGN;
        bezt->h2 = HD_ALIGN;
      }

      if (flatten) {
        handle_flatten(bezt->vec, 2, flat_direction_right);
      }
      else {
        handle_set_length(bezt->vec, 2, handle_length);
      }
    }
  }
}

KeyframeEditFunc ANIM_editkeyframes_ipo(short mode)
{
  switch (mode) {
    /* interpolation */
    case BEZT_IPO_CONST: /* constant */
      return set_bezt_constant;
    case BEZT_IPO_LIN: /* linear */
      return set_bezt_linear;

    /* easing */
    case BEZT_IPO_BACK:
      return set_bezt_back;
    case BEZT_IPO_BOUNCE:
      return set_bezt_bounce;
    case BEZT_IPO_CIRC:
      return set_bezt_circle;
    case BEZT_IPO_CUBIC:
      return set_bezt_cubic;
    case BEZT_IPO_ELASTIC:
      return set_bezt_elastic;
    case BEZT_IPO_EXPO:
      return set_bezt_expo;
    case BEZT_IPO_QUAD:
      return set_bezt_quad;
    case BEZT_IPO_QUART:
      return set_bezt_quart;
    case BEZT_IPO_QUINT:
      return set_bezt_quint;
    case BEZT_IPO_SINE:
      return set_bezt_sine;

    default: /* bezier */
      return set_bezt_bezier;
  }
}

/* ------- */

static short set_keytype_keyframe(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BEZKEYTYPE_LVALUE(bezt) = BEZT_KEYTYPE_KEYFRAME;
  }
  return 0;
}

static short set_keytype_breakdown(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BEZKEYTYPE_LVALUE(bezt) = BEZT_KEYTYPE_BREAKDOWN;
  }
  return 0;
}

static short set_keytype_extreme(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BEZKEYTYPE_LVALUE(bezt) = BEZT_KEYTYPE_EXTREME;
  }
  return 0;
}

static short set_keytype_jitter(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BEZKEYTYPE_LVALUE(bezt) = BEZT_KEYTYPE_JITTER;
  }
  return 0;
}

static short set_keytype_moving_hold(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BEZKEYTYPE_LVALUE(bezt) = BEZT_KEYTYPE_MOVEHOLD;
  }
  return 0;
}

static short set_keytype_generated(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    BEZKEYTYPE_LVALUE(bezt) = BEZT_KEYTYPE_GENERATED;
  }
  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_keytype(const eBezTriple_KeyframeType keyframe_type)
{
  switch (keyframe_type) {
    case BEZT_KEYTYPE_BREAKDOWN:
      return set_keytype_breakdown;

    case BEZT_KEYTYPE_EXTREME:
      return set_keytype_extreme;

    case BEZT_KEYTYPE_JITTER:
      return set_keytype_jitter;

    case BEZT_KEYTYPE_MOVEHOLD:
      return set_keytype_moving_hold;

    case BEZT_KEYTYPE_KEYFRAME:
      return set_keytype_keyframe;

    case BEZT_KEYTYPE_GENERATED:
      return set_keytype_generated;
  }

  BLI_assert_unreachable();
  return nullptr;
}

/* ------- */

static short set_easingtype_easein(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->easing = BEZT_IPO_EASE_IN;
  }
  return 0;
}

static short set_easingtype_easeout(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->easing = BEZT_IPO_EASE_OUT;
  }
  return 0;
}

static short set_easingtype_easeinout(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->easing = BEZT_IPO_EASE_IN_OUT;
  }
  return 0;
}

static short set_easingtype_easeauto(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  if (bezt->f2 & SELECT) {
    bezt->easing = BEZT_IPO_EASE_AUTO;
  }
  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_easing(short mode)
{
  switch (mode) {
    case BEZT_IPO_EASE_IN: /* ease in */
      return set_easingtype_easein;

    case BEZT_IPO_EASE_OUT: /* ease out */
      return set_easingtype_easeout;

    case BEZT_IPO_EASE_IN_OUT: /* both */
      return set_easingtype_easeinout;

    default: /* auto */
      return set_easingtype_easeauto;
  }
}

/* ******************************************* */
/* Selection */

static short select_bezier_add(KeyframeEditData *ked, BezTriple *bezt)
{
  /* Only act on visible items, so check handle visibility state. */
  /* if we've got info on what to select, use it, otherwise select all */
  if ((ked) && (ked->iterflags & KEYFRAME_ITER_INCL_HANDLES) && handles_visible(ked, bezt)) {
    if (ked->curflags & KEYFRAME_OK_KEY) {
      bezt->f2 |= SELECT;
    }
    if (ked->curflags & KEYFRAME_OK_H1) {
      bezt->f1 |= SELECT;
    }
    if (ked->curflags & KEYFRAME_OK_H2) {
      bezt->f3 |= SELECT;
    }
  }
  else {
    BEZT_SEL_ALL(bezt);
  }

  return 0;
}

static short select_bezier_subtract(KeyframeEditData *ked, BezTriple *bezt)
{
  /* Only act on visible items, so check handle visibility state. */
  /* if we've got info on what to deselect, use it, otherwise deselect all */
  if ((ked) && (ked->iterflags & KEYFRAME_ITER_INCL_HANDLES) && handles_visible(ked, bezt)) {
    if (ked->curflags & KEYFRAME_OK_KEY) {
      bezt->f2 &= ~SELECT;
    }
    if (ked->curflags & KEYFRAME_OK_H1) {
      bezt->f1 &= ~SELECT;
    }
    if (ked->curflags & KEYFRAME_OK_H2) {
      bezt->f3 &= ~SELECT;
    }
  }
  else {
    BEZT_DESEL_ALL(bezt);
  }

  return 0;
}

static short select_bezier_invert(KeyframeEditData * /*ked*/, BezTriple *bezt)
{
  /* Invert the selection for the whole bezier triple */
  bezt->f2 ^= SELECT;
  if (bezt->f2 & SELECT) {
    bezt->f1 |= SELECT;
    bezt->f3 |= SELECT;
  }
  else {
    bezt->f1 &= ~SELECT;
    bezt->f3 &= ~SELECT;
  }
  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_select(const eEditKeyframes_Select selectmode)
{
  switch (selectmode) {
    case SELECT_ADD: /* add */
      return select_bezier_add;
    case SELECT_SUBTRACT: /* subtract */
      return select_bezier_subtract;
    case SELECT_INVERT: /* invert */
      return select_bezier_invert;
    default: /* replace (need to clear all, then add) */
      return select_bezier_add;
  }
}

/* ******************************************* */
/* Selection Maps */

/* Selection maps are simply fancy names for char arrays that store on/off
 * info for whether the selection status. The main purpose for these is to
 * allow extra info to be tagged to the keyframes without influencing their
 * values or having to be removed later.
 */

/* ----------- */

static short selmap_build_bezier_more(KeyframeEditData *ked, BezTriple *bezt)
{
  const FCurve *fcu = ked->fcu;
  char *map = static_cast<char *>(ked->data);
  int i = ked->curIndex;

  /* if current is selected, just make sure it stays this way */
  if (BEZT_ISSEL_ANY(bezt)) {
    map[i] = 1;
    return 0;
  }

  /* if previous is selected, that means that selection should extend across */
  if (i > 0) {
    BezTriple *prev = bezt - 1;

    if (BEZT_ISSEL_ANY(prev)) {
      map[i] = 1;
      return 0;
    }
  }

  /* if next is selected, that means that selection should extend across */
  if (i < (fcu->totvert - 1)) {
    BezTriple *next = bezt + 1;

    if (BEZT_ISSEL_ANY(next)) {
      map[i] = 1;
      return 0;
    }
  }

  return 0;
}

static short selmap_build_bezier_less(KeyframeEditData *ked, BezTriple *bezt)
{
  const FCurve *fcu = ked->fcu;
  char *map = static_cast<char *>(ked->data);
  int i = ked->curIndex;

  /* if current is selected, check the left/right keyframes
   * since it might need to be deselected (but otherwise no)
   */
  if (BEZT_ISSEL_ANY(bezt)) {
    /* if previous is not selected, we're on the tip of an iceberg */
    if (i > 0) {
      BezTriple *prev = bezt - 1;

      if (BEZT_ISSEL_ANY(prev) == 0) {
        return 0;
      }
    }
    else if (i == 0) {
      /* current keyframe is selected at an endpoint, so should get deselected */
      return 0;
    }

    /* if next is not selected, we're on the tip of an iceberg */
    if (i < (fcu->totvert - 1)) {
      BezTriple *next = bezt + 1;

      if (BEZT_ISSEL_ANY(next) == 0) {
        return 0;
      }
    }
    else if (i == (fcu->totvert - 1)) {
      /* current keyframe is selected at an endpoint, so should get deselected */
      return 0;
    }

    /* if we're still here, that means that keyframe should remain untouched */
    map[i] = 1;
  }

  return 0;
}

KeyframeEditFunc ANIM_editkeyframes_buildselmap(short mode)
{
  switch (mode) {
    case SELMAP_LESS: /* less */
      return selmap_build_bezier_less;

    case SELMAP_MORE: /* more */
    default:
      return selmap_build_bezier_more;
  }
}

/* ----------- */

short bezt_selmap_flush(KeyframeEditData *ked, BezTriple *bezt)
{
  const char *map = static_cast<char *>(ked->data);
  short on = map[ked->curIndex];

  /* select or deselect based on whether the map allows it or not */
  if (on) {
    BEZT_SEL_ALL(bezt);
  }
  else {
    BEZT_DESEL_ALL(bezt);
  }

  return 0;
}
