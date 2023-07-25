/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_nla.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_path.h"
#include "RNA_prototypes.h"

#include "anim_intern.h"

static KeyingSet *keyingset_get_from_op_with_error(wmOperator *op,
                                                   PropertyRNA *prop,
                                                   Scene *scene);

static int delete_key_using_keying_set(bContext *C, wmOperator *op, KeyingSet *ks);

/* ************************************************** */
/* Keyframing Setting Wrangling */

eInsertKeyFlags ANIM_get_keyframing_flags(Scene *scene, const bool use_autokey_mode)
{
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

  /* standard flags */
  {
    /* visual keying */
    if (IS_AUTOKEY_FLAG(scene, AUTOMATKEY)) {
      flag |= INSERTKEY_MATRIX;
    }

    /* only needed */
    if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED)) {
      flag |= INSERTKEY_NEEDED;
    }

    /* default F-Curve color mode - RGB from XYZ indices */
    if (IS_AUTOKEY_FLAG(scene, XYZ2RGB)) {
      flag |= INSERTKEY_XYZ2RGB;
    }
  }

  /* only if including settings from the autokeying mode... */
  if (use_autokey_mode) {
    /* keyframing mode - only replace existing keyframes */
    if (IS_AUTOKEY_MODE(scene, EDITKEYS)) {
      flag |= INSERTKEY_REPLACE;
    }

    /* cycle-aware keyframe insertion - preserve cycle period and flow */
    if (IS_AUTOKEY_FLAG(scene, CYCLEAWARE)) {
      flag |= INSERTKEY_CYCLE_AWARE;
    }
  }

  return flag;
}

/* ******************************************* */
/* Animation Data Validation */

bAction *ED_id_action_ensure(Main *bmain, ID *id)
{
  AnimData *adt;

  /* init animdata if none available yet */
  adt = BKE_animdata_from_id(id);
  if (adt == nullptr) {
    adt = BKE_animdata_ensure_id(id);
  }
  if (adt == nullptr) {
    /* if still none (as not allowed to add, or ID doesn't have animdata for some reason) */
    printf("ERROR: Couldn't add AnimData (ID = %s)\n", (id) ? (id->name) : "<None>");
    return nullptr;
  }

  /* init action if none available yet */
  /* TODO: need some wizardry to handle NLA stuff correct */
  if (adt->action == nullptr) {
    /* init action name from name of ID block */
    char actname[sizeof(id->name) - 2];
    SNPRINTF(actname, "%sAction", id->name + 2);

    /* create action */
    adt->action = BKE_action_add(bmain, actname);

    /* set ID-type from ID-block that this is going to be assigned to
     * so that users can't accidentally break actions by assigning them
     * to the wrong places
     */
    BKE_animdata_action_ensure_idroot(id, adt->action);

    /* Tag depsgraph to be rebuilt to include time dependency. */
    DEG_relations_tag_update(bmain);
  }

  DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);

  /* return the action */
  return adt->action;
}

FCurve *ED_action_fcurve_find(bAction *act, const char rna_path[], const int array_index)
{
  /* Sanity checks. */
  if (ELEM(nullptr, act, rna_path)) {
    return nullptr;
  }
  return BKE_fcurve_find(&act->curves, rna_path, array_index);
}

FCurve *ED_action_fcurve_ensure(Main *bmain,
                                bAction *act,
                                const char group[],
                                PointerRNA *ptr,
                                const char rna_path[],
                                const int array_index)
{
  bActionGroup *agrp;
  FCurve *fcu;

  /* Sanity checks. */
  if (ELEM(nullptr, act, rna_path)) {
    return nullptr;
  }

  /* try to find f-curve matching for this setting
   * - add if not found and allowed to add one
   *   TODO: add auto-grouping support? how this works will need to be resolved
   */
  fcu = BKE_fcurve_find(&act->curves, rna_path, array_index);

  if (fcu == nullptr) {
    /* use default settings to make a F-Curve */
    fcu = BKE_fcurve_create();

    fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
    fcu->auto_smoothing = U.auto_smoothing_new;
    if (BLI_listbase_is_empty(&act->curves)) {
      fcu->flag |= FCURVE_ACTIVE; /* first one added active */
    }

    /* store path - make copy, and store that */
    fcu->rna_path = BLI_strdup(rna_path);
    fcu->array_index = array_index;

    /* if a group name has been provided, try to add or find a group, then add F-Curve to it */
    if (group) {
      /* try to find group */
      agrp = BKE_action_group_find_name(act, group);

      /* no matching groups, so add one */
      if (agrp == nullptr) {
        agrp = action_groups_add_new(act, group);

        /* sync bone group colors if applicable */
        if (ptr && (ptr->type == &RNA_PoseBone)) {
          Object *ob = (Object *)ptr->owner_id;
          bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);
          bPose *pose = ob->pose;
          bActionGroup *grp;

          /* find bone group (if present), and use the color from that */
          grp = (bActionGroup *)BLI_findlink(&pose->agroups, (pchan->agrp_index - 1));
          if (grp) {
            agrp->customCol = grp->customCol;
            action_group_colors_sync(agrp, grp);
          }
        }
      }

      /* add F-Curve to group */
      action_groups_add_channel(act, agrp, fcu);
    }
    else {
      /* just add F-Curve to end of Action's list */
      BLI_addtail(&act->curves, fcu);
    }

    /* New f-curve was added, meaning it's possible that it affects
     * dependency graph component which wasn't previously animated.
     */
    DEG_relations_tag_update(bmain);
  }

  /* return the F-Curve */
  return fcu;
}

/** Helper for #update_autoflags_fcurve(). */
static void update_autoflags_fcurve_direct(FCurve *fcu, PropertyRNA *prop)
{
  /* set additional flags for the F-Curve (i.e. only integer values) */
  fcu->flag &= ~(FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES);
  switch (RNA_property_type(prop)) {
    case PROP_FLOAT:
      /* do nothing */
      break;
    case PROP_INT:
      /* do integer (only 'whole' numbers) interpolation between all points */
      fcu->flag |= FCURVE_INT_VALUES;
      break;
    default:
      /* do 'discrete' (i.e. enum, boolean values which cannot take any intermediate
       * values at all) interpolation between all points
       *    - however, we must also ensure that evaluated values are only integers still
       */
      fcu->flag |= (FCURVE_DISCRETE_VALUES | FCURVE_INT_VALUES);
      break;
  }
}

void update_autoflags_fcurve(FCurve *fcu, bContext *C, ReportList *reports, PointerRNA *ptr)
{
  PointerRNA tmp_ptr;
  PropertyRNA *prop;
  int old_flag = fcu->flag;

  if ((ptr->owner_id == nullptr) && (ptr->data == nullptr)) {
    BKE_report(reports, RPT_ERROR, "No RNA pointer available to retrieve values for this F-curve");
    return;
  }

  /* try to get property we should be affecting */
  if (RNA_path_resolve_property(ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
    /* property not found... */
    const char *idname = (ptr->owner_id) ? ptr->owner_id->name : TIP_("<No ID pointer>");

    BKE_reportf(reports,
                RPT_ERROR,
                "Could not update flags for this F-curve, as RNA path is invalid for the given ID "
                "(ID = %s, path = %s)",
                idname,
                fcu->rna_path);
    return;
  }

  /* update F-Curve flags */
  update_autoflags_fcurve_direct(fcu, prop);

  if (old_flag != fcu->flag) {
    /* Same as if keyframes had been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
  }
}

/* ************************************************** */
/* KEYFRAME INSERTION */

/**
 * Move the point where a key is about to be inserted to be inside the main cycle range.
 * Returns the type of the cycle if it is enabled and valid.
 */
static eFCU_Cycle_Type remap_cyclic_keyframe_location(FCurve *fcu, float *px, float *py)
{
  if (fcu->totvert < 2 || !fcu->bezt) {
    return FCU_CYCLE_NONE;
  }

  eFCU_Cycle_Type type = BKE_fcurve_get_cycle_type(fcu);

  if (type == FCU_CYCLE_NONE) {
    return FCU_CYCLE_NONE;
  }

  BezTriple *first = &fcu->bezt[0], *last = &fcu->bezt[fcu->totvert - 1];
  float start = first->vec[1][0], end = last->vec[1][0];

  if (start >= end) {
    return FCU_CYCLE_NONE;
  }

  if (*px < start || *px > end) {
    float period = end - start;
    float step = floorf((*px - start) / period);
    *px -= step * period;

    if (type == FCU_CYCLE_OFFSET) {
      /* Nasty check to handle the case when the modes are different better. */
      FMod_Cycles *data = static_cast<FMod_Cycles *>(((FModifier *)fcu->modifiers.first)->data);
      short mode = (step >= 0) ? data->after_mode : data->before_mode;

      if (mode == FCM_EXTRAPOLATE_CYCLIC_OFFSET) {
        *py -= step * (last->vec[1][1] - first->vec[1][1]);
      }
    }
  }

  return type;
}

/** Used to make curves newly added to a cyclic Action cycle with the correct period. */
static void make_new_fcurve_cyclic(const bAction *act, FCurve *fcu)
{
  /* The curve must contain one (newly-added) keyframe. */
  if (fcu->totvert != 1 || !fcu->bezt) {
    return;
  }

  const float period = act->frame_end - act->frame_start;

  if (period < 0.1f) {
    return;
  }

  /* Move the keyframe into the range. */
  const float frame_offset = fcu->bezt[0].vec[1][0] - act->frame_start;
  const float fix = floorf(frame_offset / period) * period;

  fcu->bezt[0].vec[0][0] -= fix;
  fcu->bezt[0].vec[1][0] -= fix;
  fcu->bezt[0].vec[2][0] -= fix;

  /* Duplicate and offset the keyframe. */
  fcu->bezt = static_cast<BezTriple *>(MEM_reallocN(fcu->bezt, sizeof(BezTriple) * 2));
  fcu->totvert = 2;

  fcu->bezt[1] = fcu->bezt[0];
  fcu->bezt[1].vec[0][0] += period;
  fcu->bezt[1].vec[1][0] += period;
  fcu->bezt[1].vec[2][0] += period;

  /* Add the cycles modifier. */
  if (!fcu->modifiers.first) {
    add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, fcu);
  }
}

/* -------------- BezTriple Insertion -------------------- */

/* Change the Y position of a keyframe to match the input, adjusting handles. */
static void replace_bezt_keyframe_ypos(BezTriple *dst, const BezTriple *bezt)
{
  /* just change the values when replacing, so as to not overwrite handles */
  float dy = bezt->vec[1][1] - dst->vec[1][1];

  /* just apply delta value change to the handle values */
  dst->vec[0][1] += dy;
  dst->vec[1][1] += dy;
  dst->vec[2][1] += dy;

  dst->f1 = bezt->f1;
  dst->f2 = bezt->f2;
  dst->f3 = bezt->f3;

  /* TODO: perform some other operations? */
}

int insert_bezt_fcurve(FCurve *fcu, const BezTriple *bezt, eInsertKeyFlags flag)
{
  int i = 0;

  /* are there already keyframes? */
  if (fcu->bezt) {
    bool replace;
    i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, bezt->vec[1][0], fcu->totvert, &replace);

    /* replace an existing keyframe? */
    if (replace) {
      /* sanity check: 'i' may in rare cases exceed arraylen */
      if ((i >= 0) && (i < fcu->totvert)) {
        if (flag & INSERTKEY_OVERWRITE_FULL) {
          fcu->bezt[i] = *bezt;
        }
        else {
          replace_bezt_keyframe_ypos(&fcu->bezt[i], bezt);
        }

        if (flag & INSERTKEY_CYCLE_AWARE) {
          /* If replacing an end point of a cyclic curve without offset,
           * modify the other end too. */
          if (ELEM(i, 0, fcu->totvert - 1) && BKE_fcurve_get_cycle_type(fcu) == FCU_CYCLE_PERFECT)
          {
            replace_bezt_keyframe_ypos(&fcu->bezt[i == 0 ? fcu->totvert - 1 : 0], bezt);
          }
        }
      }
    }
    /* Keyframing modes allow not replacing the keyframe. */
    else if ((flag & INSERTKEY_REPLACE) == 0) {
      /* insert new - if we're not restricted to replacing keyframes only */
      BezTriple *newb = static_cast<BezTriple *>(
          MEM_callocN((fcu->totvert + 1) * sizeof(BezTriple), "beztriple"));

      /* Add the beztriples that should occur before the beztriple to be pasted
       * (originally in fcu). */
      if (i > 0) {
        memcpy(newb, fcu->bezt, i * sizeof(BezTriple));
      }

      /* add beztriple to paste at index i */
      *(newb + i) = *bezt;

      /* add the beztriples that occur after the beztriple to be pasted (originally in fcu) */
      if (i < fcu->totvert) {
        memcpy(newb + i + 1, fcu->bezt + i, (fcu->totvert - i) * sizeof(BezTriple));
      }

      /* replace (+ free) old with new, only if necessary to do so */
      MEM_freeN(fcu->bezt);
      fcu->bezt = newb;

      fcu->totvert++;
    }
    else {
      return -1;
    }
  }
  /* no keyframes already, but can only add if...
   * 1) keyframing modes say that keyframes can only be replaced, so adding new ones won't know
   * 2) there are no samples on the curve
   *    NOTE: maybe we may want to allow this later when doing samples -> bezt conversions,
   *    but for now, having both is asking for trouble
   */
  else if ((flag & INSERTKEY_REPLACE) == 0 && (fcu->fpt == nullptr)) {
    /* create new keyframes array */
    fcu->bezt = static_cast<BezTriple *>(MEM_callocN(sizeof(BezTriple), "beztriple"));
    *(fcu->bezt) = *bezt;
    fcu->totvert = 1;
  }
  /* cannot add anything */
  else {
    /* return error code -1 to prevent any misunderstandings */
    return -1;
  }

  /* we need to return the index, so that some tools which do post-processing can
   * detect where we added the BezTriple in the array
   */
  return i;
}

/**
 * Update the FCurve to allow insertion of `bezt` without modifying the curve shape.
 *
 * Checks whether it is necessary to apply Bezier subdivision due to involvement of non-auto
 * handles. If necessary, changes `bezt` handles from Auto to Aligned.
 *
 * \param bezt: key being inserted
 * \param prev: keyframe before that key
 * \param next: keyframe after that key
 */
static void subdivide_nonauto_handles(const FCurve *fcu,
                                      BezTriple *bezt,
                                      BezTriple *prev,
                                      BezTriple *next)
{
  if (prev->ipo != BEZT_IPO_BEZ || bezt->ipo != BEZT_IPO_BEZ) {
    return;
  }

  /* Don't change Vector handles, or completely auto regions. */
  const bool bezt_auto = BEZT_IS_AUTOH(bezt) || (bezt->h1 == HD_VECT && bezt->h2 == HD_VECT);
  const bool prev_auto = BEZT_IS_AUTOH(prev) || (prev->h2 == HD_VECT);
  const bool next_auto = BEZT_IS_AUTOH(next) || (next->h1 == HD_VECT);
  if (bezt_auto && prev_auto && next_auto) {
    return;
  }

  /* Subdivide the curve. */
  float delta;
  if (!BKE_fcurve_bezt_subdivide_handles(bezt, prev, next, &delta)) {
    return;
  }

  /* Decide when to force auto to manual. */
  if (!BEZT_IS_AUTOH(bezt)) {
    return;
  }
  if ((prev_auto || next_auto) && fcu->auto_smoothing == FCURVE_SMOOTH_CONT_ACCEL) {
    const float hx = bezt->vec[1][0] - bezt->vec[0][0];
    const float dx = bezt->vec[1][0] - prev->vec[1][0];

    /* This mode always uses 1/3 of key distance for handle x size. */
    const bool auto_works_well = fabsf(hx - dx / 3.0f) < 0.001f;
    if (auto_works_well) {
      return;
    }
  }

  /* Turn off auto mode. */
  bezt->h1 = bezt->h2 = HD_ALIGN;
}

int insert_vert_fcurve(
    FCurve *fcu, float x, float y, eBezTriple_KeyframeType keyframe_type, eInsertKeyFlags flag)
{
  BezTriple beztr = {{{0}}};
  uint oldTot = fcu->totvert;
  int a;

  /* set all three points, for nicer start position
   * NOTE: +/- 1 on vec.x for left and right handles is so that 'free' handles work ok...
   */
  beztr.vec[0][0] = x - 1.0f;
  beztr.vec[0][1] = y;
  beztr.vec[1][0] = x;
  beztr.vec[1][1] = y;
  beztr.vec[2][0] = x + 1.0f;
  beztr.vec[2][1] = y;
  beztr.f1 = beztr.f2 = beztr.f3 = SELECT;

  /* set default handle types and interpolation mode */
  if (flag & INSERTKEY_NO_USERPREF) {
    /* for Py-API, we want scripts to have predictable behavior,
     * hence the option to not depend on the userpref defaults
     */
    beztr.h1 = beztr.h2 = HD_AUTO_ANIM;
    beztr.ipo = BEZT_IPO_BEZ;
  }
  else {
    /* For UI usage - defaults should come from the user-preferences and/or tool-settings. */
    beztr.h1 = beztr.h2 = U.keyhandles_new; /* use default handle type here */

    /* use default interpolation mode, with exceptions for int/discrete values */
    beztr.ipo = U.ipo_new;
  }

  /* interpolation type used is constrained by the type of values the curve can take */
  if (fcu->flag & FCURVE_DISCRETE_VALUES) {
    beztr.ipo = BEZT_IPO_CONST;
  }
  else if ((beztr.ipo == BEZT_IPO_BEZ) && (fcu->flag & FCURVE_INT_VALUES)) {
    beztr.ipo = BEZT_IPO_LIN;
  }

  /* set keyframe type value (supplied), which should come from the scene settings in most cases */
  BEZKEYTYPE(&beztr) = keyframe_type;

  /* set default values for "easing" interpolation mode settings
   * NOTE: Even if these modes aren't currently used, if users switch
   *       to these later, we want these to work in a sane way out of
   *       the box.
   */

  /* "back" easing - this value used to be used when overshoot=0, but that
   *                 introduced discontinuities in how the param worked. */
  beztr.back = 1.70158f;

  /* "elastic" easing - values here were hand-optimized for a default duration of
   *                    ~10 frames (typical mograph motion length) */
  beztr.amplitude = 0.8f;
  beztr.period = 4.1f;

  /* add temp beztriple to keyframes */
  a = insert_bezt_fcurve(fcu, &beztr, flag);
  BKE_fcurve_active_keyframe_set(fcu, &fcu->bezt[a]);

  /* what if 'a' is a negative index?
   * for now, just exit to prevent any segfaults
   */
  if (a < 0) {
    return -1;
  }

  /* Set handle-type and interpolation. */
  if ((fcu->totvert > 2) && (flag & INSERTKEY_REPLACE) == 0) {
    BezTriple *bezt = (fcu->bezt + a);

    /* Set interpolation from previous (if available),
     * but only if we didn't just replace some keyframe:
     * - Replacement is indicated by no-change in number of verts.
     * - When replacing, the user may have specified some interpolation that should be kept.
     */
    if (fcu->totvert > oldTot) {
      if (a > 0) {
        bezt->ipo = (bezt - 1)->ipo;
      }
      else if (a < fcu->totvert - 1) {
        bezt->ipo = (bezt + 1)->ipo;
      }

      if (0 < a && a < (fcu->totvert - 1) && (flag & INSERTKEY_OVERWRITE_FULL) == 0) {
        subdivide_nonauto_handles(fcu, bezt, bezt - 1, bezt + 1);
      }
    }
  }

  /* don't recalculate handles if fast is set
   * - this is a hack to make importers faster
   * - we may calculate twice (due to auto-handle needing to be calculated twice)
   */
  if ((flag & INSERTKEY_FAST) == 0) {
    BKE_fcurve_handles_recalc(fcu);
  }

  /* return the index at which the keyframe was added */
  return a;
}

/* -------------- 'Smarter' Keyframing Functions -------------------- */
/* return codes for new_key_needed */
enum {
  KEYNEEDED_DONTADD = 0,
  KEYNEEDED_JUSTADD,
  KEYNEEDED_DELPREV,
  KEYNEEDED_DELNEXT,
} /*eKeyNeededStatus*/;

/**
 * This helper function determines whether a new keyframe is needed.
 *
 * Cases where keyframes should not be added:
 * 1. Keyframe to be added between two keyframes with similar values.
 * 2. Keyframe to be added on frame where two keyframes are already situated.
 * 3. Keyframe lies at point that intersects the linear line between two keyframes.
 */
static short new_key_needed(FCurve *fcu, float cFrame, float nValue)
{
  /* safety checking */
  if (fcu == nullptr) {
    return KEYNEEDED_JUSTADD;
  }
  int totCount = fcu->totvert;
  if (totCount == 0) {
    return KEYNEEDED_JUSTADD;
  }

  /* loop through checking if any are the same */
  BezTriple *bezt = fcu->bezt;
  BezTriple *prev = nullptr;
  for (int i = 0; i < totCount; i++) {
    float prevPosi = 0.0f, prevVal = 0.0f;
    float beztPosi = 0.0f, beztVal = 0.0f;

    /* get current time+value */
    beztPosi = bezt->vec[1][0];
    beztVal = bezt->vec[1][1];

    if (prev) {
      /* there is a keyframe before the one currently being examined */

      /* get previous time+value */
      prevPosi = prev->vec[1][0];
      prevVal = prev->vec[1][1];

      /* keyframe to be added at point where there are already two similar points? */
      if (IS_EQF(prevPosi, cFrame) && IS_EQF(beztPosi, cFrame) && IS_EQF(beztPosi, prevPosi)) {
        return KEYNEEDED_DONTADD;
      }

      /* keyframe between prev+current points ? */
      if ((prevPosi <= cFrame) && (cFrame <= beztPosi)) {
        /* is the value of keyframe to be added the same as keyframes on either side ? */
        if (IS_EQF(prevVal, nValue) && IS_EQF(beztVal, nValue) && IS_EQF(prevVal, beztVal)) {
          return KEYNEEDED_DONTADD;
        }

        float realVal;

        /* get real value of curve at that point */
        realVal = evaluate_fcurve(fcu, cFrame);

        /* compare whether it's the same as proposed */
        if (IS_EQF(realVal, nValue)) {
          return KEYNEEDED_DONTADD;
        }
        return KEYNEEDED_JUSTADD;
      }

      /* new keyframe before prev beztriple? */
      if (cFrame < prevPosi) {
        /* A new keyframe will be added. However, whether the previous beztriple
         * stays around or not depends on whether the values of previous/current
         * beztriples and new keyframe are the same.
         */
        if (IS_EQF(prevVal, nValue) && IS_EQF(beztVal, nValue) && IS_EQF(prevVal, beztVal)) {
          return KEYNEEDED_DELNEXT;
        }

        return KEYNEEDED_JUSTADD;
      }
    }
    else {
      /* just add a keyframe if there's only one keyframe
       * and the new one occurs before the existing one does.
       */
      if ((cFrame < beztPosi) && (totCount == 1)) {
        return KEYNEEDED_JUSTADD;
      }
    }

    /* continue. frame to do not yet passed (or other conditions not met) */
    if (i < (totCount - 1)) {
      prev = bezt;
      bezt++;
    }
    else {
      break;
    }
  }

  /* Frame in which to add a new-keyframe occurs after all other keys
   * -> If there are at least two existing keyframes, then if the values of the
   *    last two keyframes and the new-keyframe match, the last existing keyframe
   *    gets deleted as it is no longer required.
   * -> Otherwise, a keyframe is just added. 1.0 is added so that fake-2nd-to-last
   *    keyframe is not equal to last keyframe.
   */
  bezt = (fcu->bezt + (fcu->totvert - 1));
  float valA = bezt->vec[1][1];
  float valB;
  if (prev) {
    valB = prev->vec[1][1];
  }
  else {
    valB = bezt->vec[1][1] + 1.0f;
  }

  if (IS_EQF(valA, nValue) && IS_EQF(valA, valB)) {
    return KEYNEEDED_DELPREV;
  }

  return KEYNEEDED_JUSTADD;
}

/* ------------------ RNA Data-Access Functions ------------------ */

/** Try to read value using RNA-properties obtained already. */
static float *setting_get_rna_values(
    PointerRNA *ptr, PropertyRNA *prop, float *buffer, int buffer_size, int *r_count)
{
  BLI_assert(buffer_size >= 1);

  float *values = buffer;

  if (RNA_property_array_check(prop)) {
    int length = *r_count = RNA_property_array_length(ptr, prop);
    bool *tmp_bool;
    int *tmp_int;

    if (length > buffer_size) {
      values = static_cast<float *>(MEM_malloc_arrayN(length, sizeof(float), __func__));
    }

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        tmp_bool = static_cast<bool *>(MEM_malloc_arrayN(length, sizeof(*tmp_bool), __func__));
        RNA_property_boolean_get_array(ptr, prop, tmp_bool);
        for (int i = 0; i < length; i++) {
          values[i] = float(tmp_bool[i]);
        }
        MEM_freeN(tmp_bool);
        break;
      case PROP_INT:
        tmp_int = static_cast<int *>(MEM_malloc_arrayN(length, sizeof(*tmp_int), __func__));
        RNA_property_int_get_array(ptr, prop, tmp_int);
        for (int i = 0; i < length; i++) {
          values[i] = float(tmp_int[i]);
        }
        MEM_freeN(tmp_int);
        break;
      case PROP_FLOAT:
        RNA_property_float_get_array(ptr, prop, values);
        break;
      default:
        memset(values, 0, sizeof(float) * length);
    }
  }
  else {
    *r_count = 1;

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        *values = float(RNA_property_boolean_get(ptr, prop));
        break;
      case PROP_INT:
        *values = float(RNA_property_int_get(ptr, prop));
        break;
      case PROP_FLOAT:
        *values = RNA_property_float_get(ptr, prop);
        break;
      case PROP_ENUM:
        *values = float(RNA_property_enum_get(ptr, prop));
        break;
      default:
        *values = 0.0f;
    }
  }

  return values;
}

/* ------------------ 'Visual' Keyframing Functions ------------------ */

/* internal status codes for visualkey_can_use */
enum {
  VISUALKEY_NONE = 0,
  VISUALKEY_LOC,
  VISUALKEY_ROT,
  VISUALKEY_SCA,
};

/**
 * This helper function determines if visual-keyframing should be used when
 * inserting keyframes for the given channel. As visual-keyframing only works
 * on Object and Pose-Channel blocks, this should only get called for those
 * block-types, when using "standard" keying but 'Visual Keying' option in Auto-Keying
 * settings is on.
 */
static bool visualkey_can_use(PointerRNA *ptr, PropertyRNA *prop)
{
  bConstraint *con = nullptr;
  short searchtype = VISUALKEY_NONE;
  bool has_rigidbody = false;
  bool has_parent = false;
  const char *identifier = nullptr;

  /* validate data */
  if (ELEM(nullptr, ptr, ptr->data, prop)) {
    return false;
  }

  /* get first constraint and determine type of keyframe constraints to check for
   * - constraints can be on either Objects or PoseChannels, so we only check if the
   *   ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
   *   those structs, allowing us to identify the owner of the data
   */
  if (ptr->type == &RNA_Object) {
    /* Object */
    Object *ob = static_cast<Object *>(ptr->data);
    RigidBodyOb *rbo = ob->rigidbody_object;

    con = static_cast<bConstraint *>(ob->constraints.first);
    identifier = RNA_property_identifier(prop);
    has_parent = (ob->parent != nullptr);

    /* active rigidbody objects only, as only those are affected by sim */
    has_rigidbody = ((rbo) && (rbo->type == RBO_TYPE_ACTIVE));
  }
  else if (ptr->type == &RNA_PoseBone) {
    /* Pose Channel */
    bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);

    con = static_cast<bConstraint *>(pchan->constraints.first);
    identifier = RNA_property_identifier(prop);
    has_parent = (pchan->parent != nullptr);
  }

  /* check if any data to search using */
  if (ELEM(nullptr, con, identifier) && (has_parent == false) && (has_rigidbody == false)) {
    return false;
  }

  /* location or rotation identifiers only... */
  if (identifier == nullptr) {
    printf("%s failed: nullptr identifier\n", __func__);
    return false;
  }

  if (strstr(identifier, "location")) {
    searchtype = VISUALKEY_LOC;
  }
  else if (strstr(identifier, "rotation")) {
    searchtype = VISUALKEY_ROT;
  }
  else if (strstr(identifier, "scale")) {
    searchtype = VISUALKEY_SCA;
  }
  else {
    printf("%s failed: identifier - '%s'\n", __func__, identifier);
    return false;
  }

  /* only search if a searchtype and initial constraint are available */
  if (searchtype) {
    /* parent or rigidbody are always matching */
    if (has_parent || has_rigidbody) {
      return true;
    }

    /* constraints */
    for (; con; con = con->next) {
      /* only consider constraint if it is not disabled, and has influence */
      if (con->flag & CONSTRAINT_DISABLE) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* some constraints may alter these transforms */
      switch (con->type) {
        /* multi-transform constraints */
        case CONSTRAINT_TYPE_CHILDOF:
        case CONSTRAINT_TYPE_ARMATURE:
          return true;
        case CONSTRAINT_TYPE_TRANSFORM:
        case CONSTRAINT_TYPE_TRANSLIKE:
          return true;
        case CONSTRAINT_TYPE_FOLLOWPATH:
          return true;
        case CONSTRAINT_TYPE_KINEMATIC:
          return true;

        /* Single-transform constraints. */
        case CONSTRAINT_TYPE_TRACKTO:
          if (searchtype == VISUALKEY_ROT) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_DAMPTRACK:
          if (searchtype == VISUALKEY_ROT) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_ROTLIMIT:
          if (searchtype == VISUALKEY_ROT) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_LOCLIMIT:
          if (searchtype == VISUALKEY_LOC) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_SIZELIMIT:
          if (searchtype == VISUALKEY_SCA) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_DISTLIMIT:
          if (searchtype == VISUALKEY_LOC) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_ROTLIKE:
          if (searchtype == VISUALKEY_ROT) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_LOCLIKE:
          if (searchtype == VISUALKEY_LOC) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_SIZELIKE:
          if (searchtype == VISUALKEY_SCA) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_LOCKTRACK:
          if (searchtype == VISUALKEY_ROT) {
            return true;
          }
          break;
        case CONSTRAINT_TYPE_MINMAX:
          if (searchtype == VISUALKEY_LOC) {
            return true;
          }
          break;

        default:
          break;
      }
    }
  }

  /* when some condition is met, this function returns, so that means we've got nothing */
  return false;
}

/**
 * This helper function extracts the value to use for visual-keyframing
 * In the event that it is not possible to perform visual keying, try to fall-back
 * to using the default method. Assumes that all data it has been passed is valid.
 */
static float *visualkey_get_values(
    PointerRNA *ptr, PropertyRNA *prop, float *buffer, int buffer_size, int *r_count)
{
  BLI_assert(buffer_size >= 4);

  const char *identifier = RNA_property_identifier(prop);
  float tmat[4][4];
  int rotmode;

  /* handle for Objects or PoseChannels only
   * - only Location, Rotation or Scale keyframes are supported currently
   * - constraints can be on either Objects or PoseChannels, so we only check if the
   *   ptr->type is RNA_Object or RNA_PoseBone, which are the RNA wrapping-info for
   *       those structs, allowing us to identify the owner of the data
   * - assume that array_index will be sane
   */
  if (ptr->type == &RNA_Object) {
    Object *ob = static_cast<Object *>(ptr->data);
    /* Loc code is specific... */
    if (strstr(identifier, "location")) {
      copy_v3_v3(buffer, ob->object_to_world[3]);
      *r_count = 3;
      return buffer;
    }

    copy_m4_m4(tmat, ob->object_to_world);
    rotmode = ob->rotmode;
  }
  else if (ptr->type == &RNA_PoseBone) {
    bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);

    BKE_armature_mat_pose_to_bone(pchan, pchan->pose_mat, tmat);
    rotmode = pchan->rotmode;

    /* Loc code is specific... */
    if (strstr(identifier, "location")) {
      /* only use for non-connected bones */
      if ((pchan->bone->parent == nullptr) || !(pchan->bone->flag & BONE_CONNECTED)) {
        copy_v3_v3(buffer, tmat[3]);
        *r_count = 3;
        return buffer;
      }
    }
  }
  else {
    return setting_get_rna_values(ptr, prop, buffer, buffer_size, r_count);
  }

  /* Rot/Scale code are common! */
  if (strstr(identifier, "rotation_euler")) {
    mat4_to_eulO(buffer, rotmode, tmat);

    *r_count = 3;
    return buffer;
  }

  if (strstr(identifier, "rotation_quaternion")) {
    mat4_to_quat(buffer, tmat);

    *r_count = 4;
    return buffer;
  }

  if (strstr(identifier, "rotation_axis_angle")) {
    /* w = 0, x,y,z = 1,2,3 */
    mat4_to_axis_angle(buffer + 1, buffer, tmat);

    *r_count = 4;
    return buffer;
  }

  if (strstr(identifier, "scale")) {
    mat4_to_size(buffer, tmat);

    *r_count = 3;
    return buffer;
  }

  /* as the function hasn't returned yet, read value from system in the default way */
  return setting_get_rna_values(ptr, prop, buffer, buffer_size, r_count);
}

/* ------------------------- Insert Key API ------------------------- */

/* Check indices that were intended to be remapped and report any failed remaps. */
static void get_keyframe_values_create_reports(ReportList *reports,
                                               PointerRNA ptr,
                                               PropertyRNA *prop,
                                               const int index,
                                               const int count,
                                               const bool force_all,
                                               const BLI_bitmap *successful_remaps)
{

  DynStr *ds_failed_indices = BLI_dynstr_new();

  int total_failed = 0;
  for (int i = 0; i < count; i++) {
    const bool cur_index_evaluated = ELEM(index, i, -1) || force_all;
    if (!cur_index_evaluated) {
      /* `values[i]` was never intended to be remapped. */
      continue;
    }

    if (BLI_BITMAP_TEST_BOOL(successful_remaps, i)) {
      /* `values[i]` successfully remapped. */
      continue;
    }

    total_failed++;
    /* Report that `values[i]` were intended to be remapped but failed remapping process. */
    BLI_dynstr_appendf(ds_failed_indices, "%d, ", i);
  }

  if (total_failed == 0) {
    BLI_dynstr_free(ds_failed_indices);
    return;
  }

  char *str_failed_indices = BLI_dynstr_get_cstring(ds_failed_indices);
  BLI_dynstr_free(ds_failed_indices);

  BKE_reportf(reports,
              RPT_WARNING,
              "Could not insert %i keyframe(s) due to zero NLA influence, base value, or value "
              "remapping failed: %s.%s for indices [%s]",
              total_failed,
              ptr.owner_id->name,
              RNA_property_ui_name(prop),
              str_failed_indices);

  MEM_freeN(str_failed_indices);
}

/**
 * Retrieve current property values to keyframe,
 * possibly applying NLA correction when necessary.
 *
 * \param r_successful_remaps: Enables bits for indices which are both intended to be remapped and
 * were successfully remapped. Bitmap allocated so it must be freed afterward.
 */
static float *get_keyframe_values(ReportList *reports,
                                  PointerRNA ptr,
                                  PropertyRNA *prop,
                                  int index,
                                  NlaKeyframingContext *nla_context,
                                  eInsertKeyFlags flag,
                                  float *buffer,
                                  int buffer_size,
                                  const AnimationEvalContext *anim_eval_context,
                                  int *r_count,
                                  bool *r_force_all,
                                  BLI_bitmap **r_successful_remaps)
{
  float *values;

  if ((flag & INSERTKEY_MATRIX) && visualkey_can_use(&ptr, prop)) {
    /* visual-keying is only available for object and pchan datablocks, as
     * it works by keyframing using a value extracted from the final matrix
     * instead of using the kt system to extract a value.
     */
    values = visualkey_get_values(&ptr, prop, buffer, buffer_size, r_count);
  }
  else {
    /* read value from system */
    values = setting_get_rna_values(&ptr, prop, buffer, buffer_size, r_count);
  }

  *r_successful_remaps = BLI_BITMAP_NEW(*r_count, __func__);

  /* adjust the value for NLA factors */
  BKE_animsys_nla_remap_keyframe_values(nla_context,
                                        &ptr,
                                        prop,
                                        values,
                                        *r_count,
                                        index,
                                        anim_eval_context,
                                        r_force_all,
                                        *r_successful_remaps);
  get_keyframe_values_create_reports(reports,
                                     ptr,
                                     prop,
                                     index,
                                     *r_count,
                                     r_force_all ? *r_force_all : false,
                                     *r_successful_remaps);

  return values;
}

/* Insert the specified keyframe value into a single F-Curve. */
static bool insert_keyframe_value(ReportList *reports,
                                  PointerRNA *ptr,
                                  PropertyRNA *prop,
                                  FCurve *fcu,
                                  const AnimationEvalContext *anim_eval_context,
                                  float curval,
                                  eBezTriple_KeyframeType keytype,
                                  eInsertKeyFlags flag)
{
  /* F-Curve not editable? */
  if (BKE_fcurve_is_keyframable(fcu) == 0) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "F-Curve with path '%s[%d]' cannot be keyframed, ensure that it is not locked or sampled, "
        "and try removing F-Modifiers",
        fcu->rna_path,
        fcu->array_index);
    return false;
  }

  float cfra = anim_eval_context->eval_time;

  /* adjust frame on which to add keyframe */
  if ((flag & INSERTKEY_DRIVER) && (fcu->driver)) {
    PathResolvedRNA anim_rna;

    if (RNA_path_resolved_create(ptr, prop, fcu->array_index, &anim_rna)) {
      /* for making it easier to add corrective drivers... */
      cfra = evaluate_driver(&anim_rna, fcu->driver, fcu->driver, anim_eval_context);
    }
    else {
      cfra = 0.0f;
    }
  }

  /* adjust coordinates for cycle aware insertion */
  if (flag & INSERTKEY_CYCLE_AWARE) {
    if (remap_cyclic_keyframe_location(fcu, &cfra, &curval) != FCU_CYCLE_PERFECT) {
      /* inhibit action from insert_vert_fcurve unless it's a perfect cycle */
      flag &= ~INSERTKEY_CYCLE_AWARE;
    }
  }

  /* only insert keyframes where they are needed */
  if (flag & INSERTKEY_NEEDED) {
    short insert_mode;

    /* check whether this curve really needs a new keyframe */
    insert_mode = new_key_needed(fcu, cfra, curval);

    /* only return success if keyframe added */
    if (insert_mode == KEYNEEDED_DONTADD) {
      return false;
    }

    /* insert new keyframe at current frame */
    if (insert_vert_fcurve(fcu, cfra, curval, keytype, flag) < 0) {
      return false;
    }

    /* delete keyframe immediately before/after newly added */
    switch (insert_mode) {
      case KEYNEEDED_DELPREV:
        BKE_fcurve_delete_key(fcu, fcu->totvert - 2);
        BKE_fcurve_handles_recalc(fcu);
        break;
      case KEYNEEDED_DELNEXT:
        BKE_fcurve_delete_key(fcu, 1);
        BKE_fcurve_handles_recalc(fcu);
        break;
    }

    return true;
  }

  /* just insert keyframe */
  return insert_vert_fcurve(fcu, cfra, curval, keytype, flag) >= 0;
}

bool insert_keyframe_direct(ReportList *reports,
                            PointerRNA ptr,
                            PropertyRNA *prop,
                            FCurve *fcu,
                            const AnimationEvalContext *anim_eval_context,
                            eBezTriple_KeyframeType keytype,
                            NlaKeyframingContext *nla_context,
                            eInsertKeyFlags flag)
{
  float curval = 0.0f;

  /* no F-Curve to add keyframe to? */
  if (fcu == nullptr) {
    BKE_report(reports, RPT_ERROR, "No F-Curve to add keyframes to");
    return false;
  }

  /* if no property given yet, try to validate from F-Curve info */
  if ((ptr.owner_id == nullptr) && (ptr.data == nullptr)) {
    BKE_report(
        reports, RPT_ERROR, "No RNA pointer available to retrieve values for keyframing from");
    return false;
  }
  if (prop == nullptr) {
    PointerRNA tmp_ptr;

    /* try to get property we should be affecting */
    if (RNA_path_resolve_property(&ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
      /* property not found... */
      const char *idname = (ptr.owner_id) ? ptr.owner_id->name : TIP_("<No ID pointer>");

      BKE_reportf(reports,
                  RPT_ERROR,
                  "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, "
                  "path = %s)",
                  idname,
                  fcu->rna_path);
      return false;
    }

    /* property found, so overwrite 'ptr' to make later code easier */
    ptr = tmp_ptr;
  }

  /* update F-Curve flags to ensure proper behavior for property type */
  update_autoflags_fcurve_direct(fcu, prop);

  /* Obtain the value to insert. */
  float value_buffer[RNA_MAX_ARRAY_LENGTH];
  int value_count;
  int index = fcu->array_index;

  BLI_bitmap *successful_remaps = nullptr;
  float *values = get_keyframe_values(reports,
                                      ptr,
                                      prop,
                                      index,
                                      nla_context,
                                      flag,
                                      value_buffer,
                                      RNA_MAX_ARRAY_LENGTH,
                                      anim_eval_context,
                                      &value_count,
                                      nullptr,
                                      &successful_remaps);

  if (index >= 0 && index < value_count) {
    curval = values[index];
  }

  if (values != value_buffer) {
    MEM_freeN(values);
  }

  const bool curval_valid = BLI_BITMAP_TEST_BOOL(successful_remaps, index);
  MEM_freeN(successful_remaps);

  /* This happens if NLA rejects this insertion. */
  if (!curval_valid) {
    return false;
  }

  return insert_keyframe_value(reports, &ptr, prop, fcu, anim_eval_context, curval, keytype, flag);
}

/** Find or create the #FCurve based on the given path, and insert the specified value into it. */
static bool insert_keyframe_fcurve_value(Main *bmain,
                                         ReportList *reports,
                                         PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         bAction *act,
                                         const char group[],
                                         const char rna_path[],
                                         int array_index,
                                         const AnimationEvalContext *anim_eval_context,
                                         float curval,
                                         eBezTriple_KeyframeType keytype,
                                         eInsertKeyFlags flag)
{
  /* make sure the F-Curve exists
   * - if we're replacing keyframes only, DO NOT create new F-Curves if they do not exist yet
   *   but still try to get the F-Curve if it exists...
   */
  bool can_create_curve = (flag & (INSERTKEY_REPLACE | INSERTKEY_AVAILABLE)) == 0;
  FCurve *fcu = can_create_curve ?
                    ED_action_fcurve_ensure(bmain, act, group, ptr, rna_path, array_index) :
                    ED_action_fcurve_find(act, rna_path, array_index);

  /* we may not have a F-Curve when we're replacing only... */
  if (fcu) {
    const bool is_new_curve = (fcu->totvert == 0);

    /* set color mode if the F-Curve is new (i.e. without any keyframes) */
    if (is_new_curve && (flag & INSERTKEY_XYZ2RGB)) {
      /* for Loc/Rot/Scale and also Color F-Curves, the color of the F-Curve in the Graph Editor,
       * is determined by the array index for the F-Curve
       */
      PropertySubType prop_subtype = RNA_property_subtype(prop);
      if (ELEM(prop_subtype, PROP_TRANSLATION, PROP_XYZ, PROP_EULER, PROP_COLOR, PROP_COORDS)) {
        fcu->color_mode = FCURVE_COLOR_AUTO_RGB;
      }
      else if (ELEM(prop_subtype, PROP_QUATERNION)) {
        fcu->color_mode = FCURVE_COLOR_AUTO_YRGB;
      }
    }

    /* If the curve has only one key, make it cyclic if appropriate. */
    const bool is_cyclic_action = (flag & INSERTKEY_CYCLE_AWARE) && BKE_action_is_cyclic(act);

    if (is_cyclic_action && fcu->totvert == 1) {
      make_new_fcurve_cyclic(act, fcu);
    }

    /* update F-Curve flags to ensure proper behavior for property type */
    update_autoflags_fcurve_direct(fcu, prop);

    /* insert keyframe */
    const bool success = insert_keyframe_value(
        reports, ptr, prop, fcu, anim_eval_context, curval, keytype, flag);

    /* If the curve is new, make it cyclic if appropriate. */
    if (is_cyclic_action && is_new_curve) {
      make_new_fcurve_cyclic(act, fcu);
    }

    return success;
  }

  return false;
}

static AnimationEvalContext nla_time_remap(const AnimationEvalContext *anim_eval_context,
                                           PointerRNA *id_ptr,
                                           AnimData *adt,
                                           bAction *act,
                                           ListBase *nla_cache,
                                           NlaKeyframingContext **r_nla_context)
{
  if (adt && adt->action == act) {
    /* Get NLA context for value remapping. */
    *r_nla_context = BKE_animsys_get_nla_keyframing_context(
        nla_cache, id_ptr, adt, anim_eval_context);

    /* Apply NLA-mapping to frame. */
    const float remapped_frame = BKE_nla_tweakedit_remap(
        adt, anim_eval_context->eval_time, NLATIME_CONVERT_UNMAP);
    return BKE_animsys_eval_context_construct_at(anim_eval_context, remapped_frame);
  }

  *r_nla_context = nullptr;
  return *anim_eval_context;
}

int insert_keyframe(Main *bmain,
                    ReportList *reports,
                    ID *id,
                    bAction *act,
                    const char group[],
                    const char rna_path[],
                    int array_index,
                    const AnimationEvalContext *anim_eval_context,
                    eBezTriple_KeyframeType keytype,
                    ListBase *nla_cache,
                    eInsertKeyFlags flag)
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop = nullptr;
  AnimData *adt;
  ListBase tmp_nla_cache = {nullptr, nullptr};
  NlaKeyframingContext *nla_context = nullptr;
  int ret = 0;

  /* validate pointer first - exit if failure */
  if (id == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "No ID block to insert keyframe in (path = %s)", rna_path);
    return 0;
  }

  if (!BKE_id_is_editable(bmain, id)) {
    BKE_reportf(reports, RPT_ERROR, "'%s' on %s is not editable", rna_path, id->name + 2);
    return 0;
  }

  RNA_id_pointer_create(id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        (id) ? id->name : TIP_("<Missing ID block>"),
        rna_path);
    return 0;
  }

  /* if no action is provided, keyframe to the default one attached to this ID-block */
  if (act == nullptr) {
    /* get action to add F-Curve+keyframe to */
    act = ED_id_action_ensure(bmain, id);

    if (act == nullptr) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Could not insert keyframe, as this type does not support animation data (ID = "
                  "%s, path = %s)",
                  id->name,
                  rna_path);
      return 0;
    }
  }

  /* apply NLA-mapping to frame to use (if applicable) */
  adt = BKE_animdata_from_id(id);
  const AnimationEvalContext remapped_context = nla_time_remap(
      anim_eval_context, &id_ptr, adt, act, nla_cache ? nla_cache : &tmp_nla_cache, &nla_context);

  /* Obtain values to insert. */
  float value_buffer[RNA_MAX_ARRAY_LENGTH];
  int value_count;
  bool force_all;

  BLI_bitmap *successful_remaps = nullptr;
  float *values = get_keyframe_values(reports,
                                      ptr,
                                      prop,
                                      array_index,
                                      nla_context,
                                      flag,
                                      value_buffer,
                                      RNA_MAX_ARRAY_LENGTH,
                                      anim_eval_context,
                                      &value_count,
                                      &force_all,
                                      &successful_remaps);

  /* Key the entire array. */
  if (array_index == -1 || force_all) {
    /* In force mode, if any of the curves succeeds, drop the replace mode and restart. */
    if (force_all && (flag & (INSERTKEY_REPLACE | INSERTKEY_AVAILABLE)) != 0) {
      int exclude = -1;

      for (array_index = 0; array_index < value_count; array_index++) {
        if (!BLI_BITMAP_TEST_BOOL(successful_remaps, array_index)) {
          continue;
        }

        if (insert_keyframe_fcurve_value(bmain,
                                         reports,
                                         &ptr,
                                         prop,
                                         act,
                                         group,
                                         rna_path,
                                         array_index,
                                         &remapped_context,
                                         values[array_index],
                                         keytype,
                                         flag))
        {
          ret++;
          exclude = array_index;
          break;
        }
      }

      if (exclude != -1) {
        flag &= ~(INSERTKEY_REPLACE | INSERTKEY_AVAILABLE);

        for (array_index = 0; array_index < value_count; array_index++) {
          if (!BLI_BITMAP_TEST_BOOL(successful_remaps, array_index)) {
            continue;
          }

          if (array_index != exclude) {
            ret += insert_keyframe_fcurve_value(bmain,
                                                reports,
                                                &ptr,
                                                prop,
                                                act,
                                                group,
                                                rna_path,
                                                array_index,
                                                &remapped_context,
                                                values[array_index],
                                                keytype,
                                                flag);
          }
        }
      }
    }
    /* Simply insert all channels. */
    else {
      for (array_index = 0; array_index < value_count; array_index++) {
        if (!BLI_BITMAP_TEST_BOOL(successful_remaps, array_index)) {
          continue;
        }

        ret += insert_keyframe_fcurve_value(bmain,
                                            reports,
                                            &ptr,
                                            prop,
                                            act,
                                            group,
                                            rna_path,
                                            array_index,
                                            &remapped_context,
                                            values[array_index],
                                            keytype,
                                            flag);
      }
    }
  }
  /* Key a single index. */
  else {
    if (array_index >= 0 && array_index < value_count &&
        BLI_BITMAP_TEST_BOOL(successful_remaps, array_index))
    {
      ret += insert_keyframe_fcurve_value(bmain,
                                          reports,
                                          &ptr,
                                          prop,
                                          act,
                                          group,
                                          rna_path,
                                          array_index,
                                          &remapped_context,
                                          values[array_index],
                                          keytype,
                                          flag);
    }
  }

  if (values != value_buffer) {
    MEM_freeN(values);
  }

  MEM_freeN(successful_remaps);
  BKE_animsys_free_nla_keyframing_context_cache(&tmp_nla_cache);

  if (ret) {
    if (act != nullptr) {
      DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
    if (adt != nullptr && adt->action != nullptr && adt->action != act) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
  }

  return ret;
}

void ED_keyframes_add(FCurve *fcu, int num_keys_to_add)
{
  BLI_assert_msg(num_keys_to_add >= 0, "cannot remove keyframes with this function");

  if (num_keys_to_add == 0) {
    return;
  }

  fcu->bezt = static_cast<BezTriple *>(
      MEM_recallocN(fcu->bezt, sizeof(BezTriple) * (fcu->totvert + num_keys_to_add)));
  BezTriple *bezt = fcu->bezt + fcu->totvert; /* Pointer to the first new one. '*/

  fcu->totvert += num_keys_to_add;

  /* Iterate over the new keys to update their settings. */
  while (num_keys_to_add--) {
    /* Defaults, ignoring user-preference gives predictable results for API. */
    bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
    bezt->ipo = BEZT_IPO_BEZ;
    bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
    bezt++;
  }
}

/* ************************************************** */
/* KEYFRAME DELETION */

/* Main Keyframing API call:
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will delete a keyframe at the current frame.
 *
 * The flag argument is used for special settings that alter the behavior of
 * the keyframe deletion. These include the quick refresh options.
 */

/**
 * \note caller needs to run #BKE_nla_tweakedit_remap to get NLA relative frame.
 *       caller should also check #BKE_fcurve_is_protected before keying.
 */
static bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra)
{
  bool found;
  int i;

  /* try to find index of beztriple to get rid of */
  i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
  if (found) {
    /* delete the key at the index (will sanity check + do recalc afterwards) */
    BKE_fcurve_delete_key(fcu, i);
    BKE_fcurve_handles_recalc(fcu);

    /* Only delete curve too if it won't be doing anything anymore */
    if (BKE_fcurve_is_empty(fcu)) {
      ANIM_fcurve_delete_from_animdata(nullptr, adt, fcu);
    }

    /* return success */
    return true;
  }
  return false;
}

static void deg_tag_after_keyframe_delete(Main *bmain, ID *id, AnimData *adt)
{
  if (adt->action == nullptr) {
    /* In the case last f-curve was removed need to inform dependency graph
     * about relations update, since it needs to get rid of animation operation
     * for this data-block. */
    DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION_NO_FLUSH);
    DEG_relations_tag_update(bmain);
  }
  else {
    DEG_id_tag_update_ex(bmain, &adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
}

int delete_keyframe(Main *bmain,
                    ReportList *reports,
                    ID *id,
                    bAction *act,
                    const char rna_path[],
                    int array_index,
                    float cfra)
{
  AnimData *adt = BKE_animdata_from_id(id);
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  int array_index_max = array_index + 1;
  int ret = 0;

  /* sanity checks */
  if (ELEM(nullptr, id, adt)) {
    BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
    return 0;
  }

  /* validate pointer first - exit if failure */
  RNA_id_pointer_create(id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not delete keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path);
    return 0;
  }

  /* get F-Curve
   * NOTE: here is one of the places where we don't want new Action + F-Curve added!
   *      so 'add' var must be 0
   */
  if (act == nullptr) {
    /* if no action is provided, use the default one attached to this ID-block
     * - if it doesn't exist, then we're out of options...
     */
    if (adt->action) {
      act = adt->action;

      /* apply NLA-mapping to frame to use (if applicable) */
      cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
    }
    else {
      BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
      return 0;
    }
  }

  /* key entire array convenience method */
  if (array_index == -1) {
    array_index = 0;
    array_index_max = RNA_property_array_length(&ptr, prop);

    /* for single properties, increase max_index so that the property itself gets included,
     * but don't do this for standard arrays since that can cause corruption issues
     * (extra unused curves)
     */
    if (array_index_max == array_index) {
      array_index_max++;
    }
  }

  /* will only loop once unless the array index was -1 */
  for (; array_index < array_index_max; array_index++) {
    FCurve *fcu = ED_action_fcurve_find(act, rna_path, array_index);

    /* check if F-Curve exists and/or whether it can be edited */
    if (fcu == nullptr) {
      continue;
    }

    if (BKE_fcurve_is_protected(fcu)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Not deleting keyframe for locked F-Curve '%s' for %s '%s'",
                  fcu->rna_path,
                  BKE_idtype_idcode_to_name(GS(id->name)),
                  id->name + 2);
      continue;
    }

    ret += delete_keyframe_fcurve(adt, fcu, cfra);
  }
  if (ret) {
    deg_tag_after_keyframe_delete(bmain, id, adt);
  }
  /* return success/failure */
  return ret;
}

/* ************************************************** */
/* KEYFRAME CLEAR */

/**
 * Main Keyframing API call:
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will clear the current buttons fcurve(s).
 *
 * The flag argument is used for special settings that alter the behavior of
 * the keyframe deletion. These include the quick refresh options.
 *
 * \return The number of f-curves removed.
 */
static int clear_keyframe(Main *bmain,
                          ReportList *reports,
                          ID *id,
                          bAction *act,
                          const char rna_path[],
                          int array_index,
                          eInsertKeyFlags /*flag*/)
{
  AnimData *adt = BKE_animdata_from_id(id);
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  int array_index_max = array_index + 1;
  int ret = 0;

  /* sanity checks */
  if (ELEM(nullptr, id, adt)) {
    BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
    return 0;
  }

  /* validate pointer first - exit if failure */
  RNA_id_pointer_create(id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not clear keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path);
    return 0;
  }

  /* get F-Curve
   * NOTE: here is one of the places where we don't want new Action + F-Curve added!
   *      so 'add' var must be 0
   */
  if (act == nullptr) {
    /* if no action is provided, use the default one attached to this ID-block
     * - if it doesn't exist, then we're out of options...
     */
    if (adt->action) {
      act = adt->action;
    }
    else {
      BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
      return 0;
    }
  }

  /* key entire array convenience method */
  if (array_index == -1) {
    array_index = 0;
    array_index_max = RNA_property_array_length(&ptr, prop);

    /* for single properties, increase max_index so that the property itself gets included,
     * but don't do this for standard arrays since that can cause corruption issues
     * (extra unused curves)
     */
    if (array_index_max == array_index) {
      array_index_max++;
    }
  }

  /* will only loop once unless the array index was -1 */
  for (; array_index < array_index_max; array_index++) {
    FCurve *fcu = ED_action_fcurve_find(act, rna_path, array_index);

    /* check if F-Curve exists and/or whether it can be edited */
    if (fcu == nullptr) {
      continue;
    }

    if (BKE_fcurve_is_protected(fcu)) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Not clearing all keyframes from locked F-Curve '%s' for %s '%s'",
                  fcu->rna_path,
                  BKE_idtype_idcode_to_name(GS(id->name)),
                  id->name + 2);
      continue;
    }

    ANIM_fcurve_delete_from_animdata(nullptr, adt, fcu);

    /* return success */
    ret++;
  }
  if (ret) {
    deg_tag_after_keyframe_delete(bmain, id, adt);
  }
  /* return success/failure */
  return ret;
}

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* mode for commonkey_modifykey */
enum {
  COMMONKEY_MODE_INSERT = 0,
  COMMONKEY_MODE_DELETE,
} /*eCommonModifyKey_Modes*/;

/**
 * Polling callback for use with `ANIM_*_keyframe()` operators
 * This is based on the standard ED_operator_areaactive callback,
 * except that it does special checks for a few space-types too.
 */
static bool modify_key_op_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);

  /* if no area or active scene */
  if (ELEM(nullptr, area, scene)) {
    return false;
  }

  /* should be fine */
  return true;
}

/* Insert Key Operator ------------------------ */

static int insert_key_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  bool ob_edit_mode = false;

  const float cfra = float(
      scene->r.cfra); /* XXX for now, don't bother about all the yucky offset crap */
  int num_channels;
  const bool confirm = op->flag & OP_IS_INVOKE;

  KeyingSet *ks = keyingset_get_from_op_with_error(op, op->type->prop, scene);
  if (ks == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* exit the edit mode to make sure that those object data properties that have been
   * updated since the last switching to the edit mode will be keyframed correctly
   */
  if (obedit && ANIM_keyingset_find_id(ks, (ID *)obedit->data)) {
    ED_object_mode_set(C, OB_MODE_OBJECT);
    ob_edit_mode = true;
  }

  /* try to insert keyframes for the channels specified by KeyingSet */
  num_channels = ANIM_apply_keyingset(C, nullptr, nullptr, ks, MODIFYKEY_MODE_INSERT, cfra);
  if (G.debug & G_DEBUG) {
    BKE_reportf(op->reports,
                RPT_INFO,
                "Keying set '%s' - successfully added %d keyframes",
                ks->name,
                num_channels);
  }

  /* restore the edit mode if necessary */
  if (ob_edit_mode) {
    ED_object_mode_set(C, OB_MODE_EDIT);
  }

  /* report failure or do updates? */
  if (num_channels < 0) {
    BKE_report(op->reports, RPT_ERROR, "No suitable context info for active keying set");
    return OPERATOR_CANCELLED;
  }

  if (num_channels > 0) {
    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
  }

  if (confirm) {
    /* if called by invoke (from the UI), make a note that we've inserted keyframes */
    if (num_channels > 0) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "Successfully added %d keyframes for keying set '%s'",
                  num_channels,
                  ks->name);
    }
    else {
      BKE_report(op->reports, RPT_WARNING, "Keying set failed to insert any keyframes");
    }
  }

  return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_insert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Keyframe";
  ot->idname = "ANIM_OT_keyframe_insert";
  ot->description =
      "Insert keyframes on the current frame for all properties in the specified Keying Set";

  /* callbacks */
  ot->exec = insert_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (dynamic enum) */
  prop = RNA_def_enum(
      ot->srna, "type", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

void ANIM_OT_keyframe_insert_by_name(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Keyframe (by name)";
  ot->idname = "ANIM_OT_keyframe_insert_by_name";
  ot->description = "Alternate access to 'Insert Keyframe' for keymaps to use";

  /* callbacks */
  ot->exec = insert_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (idname) */
  prop = RNA_def_string(
      ot->srna, "type", nullptr, MAX_ID_NAME - 2, "Keying Set", "The Keying Set to use");
  RNA_def_property_string_search_func_runtime(
      prop, ANIM_keyingset_visit_for_search_no_poll, PROP_STRING_SEARCH_SUGGESTION);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

/* Insert Key Operator (With Menu) ------------------------ */
/* This operator checks if a menu should be shown for choosing the KeyingSet to use,
 * then calls the menu if necessary before
 */

static int insert_key_menu_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_scene(C);

  /* When there is an active keying set and no request to prompt, keyframe immediately. */
  if ((scene->active_keyingset != 0) && !RNA_boolean_get(op->ptr, "always_prompt")) {
    /* Just call the exec() on the active keying-set. */
    RNA_enum_set(op->ptr, "type", 0);
    return op->type->exec(C, op);
  }

  /* Show a menu listing all keying-sets, the enum is expanded here to make use of the
   * operator that accesses the keying-set by name. This is important for the ability
   * to assign shortcuts to arbitrarily named keying sets. See #89560.
   * These menu items perform the key-frame insertion (not this operator)
   * hence the #OPERATOR_INTERFACE return. */
  uiPopupMenu *pup = UI_popup_menu_begin(C, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  /* Even though `ANIM_OT_keyframe_insert_menu` can show a menu in one line,
   * prefer `ANIM_OT_keyframe_insert_by_name` so users can bind keys to specific
   * keying sets by name in the key-map instead of the index which isn't stable. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "type");
  const EnumPropertyItem *item_array = nullptr;
  int totitem;
  bool free;

  RNA_property_enum_items_gettexted(C, op->ptr, prop, &item_array, &totitem, &free);

  for (int i = 0; i < totitem; i++) {
    const EnumPropertyItem *item = &item_array[i];
    if (item->identifier[0] != '\0') {
      uiItemStringO(layout,
                    item->name,
                    item->icon,
                    "ANIM_OT_keyframe_insert_by_name",
                    "type",
                    item->identifier);
    }
    else {
      /* This enum shouldn't contain headings, assert there are none.
       * NOTE: If in the future the enum includes them, additional layout code can be
       * added to show them - although that doesn't seem likely. */
      BLI_assert(item->name == nullptr);
      uiItemS(layout);
    }
  }

  if (free) {
    MEM_freeN((void *)item_array);
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void ANIM_OT_keyframe_insert_menu(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Keyframe Menu";
  ot->idname = "ANIM_OT_keyframe_insert_menu";
  ot->description =
      "Insert Keyframes for specified Keying Set, with menu of available Keying Sets if undefined";

  /* callbacks */
  ot->invoke = insert_key_menu_invoke;
  ot->exec = insert_key_exec;
  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (dynamic enum) */
  prop = RNA_def_enum(
      ot->srna, "type", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;

  /* whether the menu should always be shown
   * - by default, the menu should only be shown when there is no active Keying Set (2.5 behavior),
   *   although in some cases it might be useful to always shown (pre 2.5 behavior)
   */
  prop = RNA_def_boolean(ot->srna, "always_prompt", false, "Always Show Menu", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* Delete Key Operator ------------------------ */

static int delete_key_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = keyingset_get_from_op_with_error(op, op->type->prop, scene);
  if (ks == nullptr) {
    return OPERATOR_CANCELLED;
  }

  return delete_key_using_keying_set(C, op, ks);
}

static int delete_key_using_keying_set(bContext *C, wmOperator *op, KeyingSet *ks)
{
  Scene *scene = CTX_data_scene(C);
  float cfra = float(
      scene->r.cfra); /* XXX for now, don't bother about all the yucky offset crap */
  int num_channels;
  const bool confirm = op->flag & OP_IS_INVOKE;

  /* try to delete keyframes for the channels specified by KeyingSet */
  num_channels = ANIM_apply_keyingset(C, nullptr, nullptr, ks, MODIFYKEY_MODE_DELETE, cfra);
  if (G.debug & G_DEBUG) {
    printf("KeyingSet '%s' - Successfully removed %d Keyframes\n", ks->name, num_channels);
  }

  /* report failure or do updates? */
  if (num_channels < 0) {
    BKE_report(op->reports, RPT_ERROR, "No suitable context info for active keying set");
    return OPERATOR_CANCELLED;
  }

  if (num_channels > 0) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);
  }

  if (confirm) {
    /* if called by invoke (from the UI), make a note that we've removed keyframes */
    if (num_channels > 0) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "Successfully removed %d keyframes for keying set '%s'",
                  num_channels,
                  ks->name);
    }
    else {
      BKE_report(op->reports, RPT_WARNING, "Keying set failed to remove any keyframes");
    }
  }
  return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_delete(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Delete Keying-Set Keyframe";
  ot->idname = "ANIM_OT_keyframe_delete";
  ot->description =
      "Delete keyframes on the current frame for all properties in the specified Keying Set";

  /* callbacks */
  ot->exec = delete_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (dynamic enum) */
  prop = RNA_def_enum(
      ot->srna, "type", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

void ANIM_OT_keyframe_delete_by_name(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Delete Keying-Set Keyframe (by name)";
  ot->idname = "ANIM_OT_keyframe_delete_by_name";
  ot->description = "Alternate access to 'Delete Keyframe' for keymaps to use";

  /* callbacks */
  ot->exec = delete_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (idname) */
  prop = RNA_def_string(
      ot->srna, "type", nullptr, MAX_ID_NAME - 2, "Keying Set", "The Keying Set to use");
  RNA_def_property_string_search_func_runtime(
      prop, ANIM_keyingset_visit_for_search_no_poll, PROP_STRING_SEARCH_SUGGESTION);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

/* Delete Key Operator ------------------------ */
/* NOTE: Although this version is simpler than the more generic version for KeyingSets,
 * it is more useful for animators working in the 3D view.
 */

static int clear_anim_v3d_exec(bContext *C, wmOperator * /*op*/)
{
  bool changed = false;

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    /* just those in active action... */
    if ((ob->adt) && (ob->adt->action)) {
      AnimData *adt = ob->adt;
      bAction *act = adt->action;
      FCurve *fcu, *fcn;

      for (fcu = static_cast<FCurve *>(act->curves.first); fcu; fcu = fcn) {
        bool can_delete = false;

        fcn = fcu->next;

        /* in pose mode, only delete the F-Curve if it belongs to a selected bone */
        if (ob->mode & OB_MODE_POSE) {
          if (fcu->rna_path) {
            /* Get bone-name, and check if this bone is selected. */
            bPoseChannel *pchan = nullptr;
            char bone_name[sizeof(pchan->name)];
            if (BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name)))
            {
              pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
              /* Delete if bone is selected. */
              if ((pchan) && (pchan->bone)) {
                if (pchan->bone->flag & BONE_SELECTED) {
                  can_delete = true;
                }
              }
            }
          }
        }
        else {
          /* object mode - all of Object's F-Curves are affected */
          can_delete = true;
        }

        /* delete F-Curve completely */
        if (can_delete) {
          ANIM_fcurve_delete_from_animdata(nullptr, adt, fcu);
          DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
          changed = true;
        }
      }

      /* Delete the action itself if it is empty. */
      if (ANIM_remove_empty_action_from_animdata(adt)) {
        changed = true;
      }
    }
  }
  CTX_DATA_END;

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  /* send updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, nullptr);

  return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_clear_v3d(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Animation";
  ot->description = "Remove all keyframe animation for selected objects";
  ot->idname = "ANIM_OT_keyframe_clear_v3d";

  /* callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = clear_anim_v3d_exec;

  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

static int delete_key_v3d_without_keying_set(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  float cfra = float(scene->r.cfra);

  int selected_objects_len = 0;
  int selected_objects_success_len = 0;
  int success_multi = 0;

  const bool confirm = op->flag & OP_IS_INVOKE;

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    ID *id = &ob->id;
    int success = 0;

    selected_objects_len += 1;

    /* just those in active action... */
    if ((ob->adt) && (ob->adt->action)) {
      AnimData *adt = ob->adt;
      bAction *act = adt->action;
      FCurve *fcu, *fcn;
      const float cfra_unmap = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);

      for (fcu = static_cast<FCurve *>(act->curves.first); fcu; fcu = fcn) {
        fcn = fcu->next;

        /* don't touch protected F-Curves */
        if (BKE_fcurve_is_protected(fcu)) {
          BKE_reportf(op->reports,
                      RPT_WARNING,
                      "Not deleting keyframe for locked F-Curve '%s', object '%s'",
                      fcu->rna_path,
                      id->name + 2);
          continue;
        }

        /* Special exception for bones, as this makes this operator more convenient to use
         * NOTE: This is only done in pose mode.
         * In object mode, we're dealing with the entire object.
         */
        if (ob->mode & OB_MODE_POSE) {
          bPoseChannel *pchan = nullptr;

          /* Get bone-name, and check if this bone is selected. */
          char bone_name[sizeof(pchan->name)];
          if (!BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name))) {
            continue;
          }
          pchan = BKE_pose_channel_find_name(ob->pose, bone_name);

          /* skip if bone is not selected */
          if ((pchan) && (pchan->bone)) {
            /* bones are only selected/editable if visible... */
            bArmature *arm = (bArmature *)ob->data;

            /* skipping - not visible on currently visible layers */
            if ((arm->layer & pchan->bone->layer) == 0) {
              continue;
            }
            /* skipping - is currently hidden */
            if (pchan->bone->flag & BONE_HIDDEN_P) {
              continue;
            }

            /* selection flag... */
            if ((pchan->bone->flag & BONE_SELECTED) == 0) {
              continue;
            }
          }
        }

        /* delete keyframes on current frame
         * WARNING: this can delete the next F-Curve, hence the "fcn" copying
         */
        success += delete_keyframe_fcurve(adt, fcu, cfra_unmap);
      }
      DEG_id_tag_update(&ob->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }

    /* Only for reporting. */
    if (success) {
      selected_objects_success_len += 1;
      success_multi += success;
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
  CTX_DATA_END;

  if (selected_objects_success_len) {
    /* send updates */
    WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, nullptr);
  }

  if (confirm) {
    /* if called by invoke (from the UI), make a note that we've removed keyframes */
    if (selected_objects_success_len) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "%d object(s) successfully had %d keyframes removed",
                  selected_objects_success_len,
                  success_multi);
    }
    else {
      BKE_reportf(
          op->reports, RPT_ERROR, "No keyframes removed from %d object(s)", selected_objects_len);
    }
  }
  return OPERATOR_FINISHED;
}

static int delete_key_v3d_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = ANIM_scene_get_active_keyingset(scene);

  if (ks == nullptr) {
    return delete_key_v3d_without_keying_set(C, op);
  }

  return delete_key_using_keying_set(C, op, ks);
}

void ANIM_OT_keyframe_delete_v3d(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframe";
  ot->description = "Remove keyframes on current frame for selected objects and bones";
  ot->idname = "ANIM_OT_keyframe_delete_v3d";

  /* callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = delete_key_v3d_exec;

  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/* Insert Key Button Operator ------------------------ */

static int insert_key_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  PointerRNA ptr = {nullptr};
  PropertyRNA *prop = nullptr;
  char *path;
  uiBut *but;
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      CTX_data_depsgraph_pointer(C), float(scene->r.cfra));
  bool changed = false;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

  /* flags for inserting keyframes */
  flag = ANIM_get_keyframing_flags(scene, true);

  if (!(but = UI_context_active_but_prop_get(C, &ptr, &prop, &index))) {
    /* pass event on if no active button found */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if ((ptr.owner_id && ptr.data && prop) && RNA_property_animateable(&ptr, prop)) {
    if (ptr.type == &RNA_NlaStrip) {
      /* Handle special properties for NLA Strips, whose F-Curves are stored on the
       * strips themselves. These are stored separately or else the properties will
       * not have any effect.
       */
      NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
      FCurve *fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), index);

      if (fcu) {
        changed = insert_keyframe_direct(op->reports,
                                         ptr,
                                         prop,
                                         fcu,
                                         &anim_eval_context,
                                         eBezTriple_KeyframeType(ts->keyframe_type),
                                         nullptr,
                                         eInsertKeyFlags(0));
      }
      else {
        BKE_report(op->reports,
                   RPT_ERROR,
                   "This property cannot be animated as it will not get updated correctly");
      }
    }
    else if (UI_but_flag_is_set(but, UI_BUT_DRIVEN)) {
      /* Driven property - Find driver */
      FCurve *fcu;
      bool driven, special;

      fcu = BKE_fcurve_find_by_rna_context_ui(
          C, &ptr, prop, index, nullptr, nullptr, &driven, &special);

      if (fcu && driven) {
        changed = insert_keyframe_direct(op->reports,
                                         ptr,
                                         prop,
                                         fcu,
                                         &anim_eval_context,
                                         eBezTriple_KeyframeType(ts->keyframe_type),
                                         nullptr,
                                         INSERTKEY_DRIVER);
      }
    }
    else {
      /* standard properties */
      path = RNA_path_from_ID_to_property(&ptr, prop);

      if (path) {
        const char *identifier = RNA_property_identifier(prop);
        const char *group = nullptr;

        /* Special exception for keyframing transforms:
         * Set "group" for this manually, instead of having them appearing at the bottom
         * (ungrouped) part of the channels list.
         * Leaving these ungrouped is not a nice user behavior in this case.
         *
         * TODO: Perhaps we can extend this behavior in future for other properties...
         */
        if (ptr.type == &RNA_PoseBone) {
          bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr.data);
          group = pchan->name;
        }
        else if ((ptr.type == &RNA_Object) &&
                 (strstr(identifier, "location") || strstr(identifier, "rotation") ||
                  strstr(identifier, "scale")))
        {
          /* NOTE: Keep this label in sync with the "ID" case in
           * keyingsets_utils.py :: get_transform_generators_base_info()
           */
          group = "Object Transforms";
        }

        if (all) {
          /* -1 indicates operating on the entire array (or the property itself otherwise) */
          index = -1;
        }

        changed = (insert_keyframe(bmain,
                                   op->reports,
                                   ptr.owner_id,
                                   nullptr,
                                   group,
                                   path,
                                   index,
                                   &anim_eval_context,
                                   eBezTriple_KeyframeType(ts->keyframe_type),
                                   nullptr,
                                   flag) != 0);

        MEM_freeN(path);
      }
      else {
        BKE_report(op->reports,
                   RPT_WARNING,
                   "Failed to resolve path to property, "
                   "try manually specifying this using a Keying Set instead");
      }
    }
  }
  else {
    if (prop && !RNA_property_animateable(&ptr, prop)) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "\"%s\" property cannot be animated",
                  RNA_property_identifier(prop));
    }
    else {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Button doesn't appear to have any property information attached (ptr.data = "
                  "%p, prop = %p)",
                  ptr.data,
                  (void *)prop);
    }
  }

  if (changed) {
    ID *id = ptr.owner_id;
    AnimData *adt = BKE_animdata_from_id(id);
    if (adt->action != nullptr) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
    DEG_id_tag_update(id, ID_RECALC_ANIMATION_NO_FLUSH);

    /* send updates */
    UI_context_update_anim_flag(C);

    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_insert_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Keyframe (Buttons)";
  ot->idname = "ANIM_OT_keyframe_insert_button";
  ot->description = "Insert a keyframe for current UI-active property";

  /* callbacks */
  ot->exec = insert_key_button_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Insert a keyframe for all element of the array");
}

/* Delete Key Button Operator ------------------------ */

static int delete_key_button_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  PointerRNA ptr = {nullptr};
  PropertyRNA *prop = nullptr;
  Main *bmain = CTX_data_main(C);
  char *path;
  float cfra = float(
      scene->r.cfra); /* XXX for now, don't bother about all the yucky offset crap */
  bool changed = false;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
    /* pass event on if no active button found */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if (ptr.owner_id && ptr.data && prop) {
    if (BKE_nlastrip_has_curves_for_property(&ptr, prop)) {
      /* Handle special properties for NLA Strips, whose F-Curves are stored on the
       * strips themselves. These are stored separately or else the properties will
       * not have any effect.
       */
      ID *id = ptr.owner_id;
      NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
      FCurve *fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), 0);

      if (fcu) {
        if (BKE_fcurve_is_protected(fcu)) {
          BKE_reportf(
              op->reports,
              RPT_WARNING,
              "Not deleting keyframe for locked F-Curve for NLA Strip influence on %s - %s '%s'",
              strip->name,
              BKE_idtype_idcode_to_name(GS(id->name)),
              id->name + 2);
        }
        else {
          /* remove the keyframe directly
           * NOTE: cannot use delete_keyframe_fcurve(), as that will free the curve,
           *       and delete_keyframe() expects the FCurve to be part of an action
           */
          bool found = false;
          int i;

          /* try to find index of beztriple to get rid of */
          i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
          if (found) {
            /* delete the key at the index (will sanity check + do recalc afterwards) */
            BKE_fcurve_delete_key(fcu, i);
            BKE_fcurve_handles_recalc(fcu);
            changed = true;
          }
        }
      }
    }
    else {
      /* standard properties */
      path = RNA_path_from_ID_to_property(&ptr, prop);

      if (path) {
        if (all) {
          /* -1 indicates operating on the entire array (or the property itself otherwise) */
          index = -1;
        }

        changed = delete_keyframe(bmain, op->reports, ptr.owner_id, nullptr, path, index, cfra) !=
                  0;
        MEM_freeN(path);
      }
      else if (G.debug & G_DEBUG) {
        printf("Button Delete-Key: no path to property\n");
      }
    }
  }
  else if (G.debug & G_DEBUG) {
    printf("ptr.data = %p, prop = %p\n", ptr.data, (void *)prop);
  }

  if (changed) {
    /* send updates */
    UI_context_update_anim_flag(C);

    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_delete_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframe (Buttons)";
  ot->idname = "ANIM_OT_keyframe_delete_button";
  ot->description = "Delete current keyframe of current UI-active property";

  /* callbacks */
  ot->exec = delete_key_button_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Delete keyframes from all elements of the array");
}

/* Clear Key Button Operator ------------------------ */

static int clear_key_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = {nullptr};
  PropertyRNA *prop = nullptr;
  Main *bmain = CTX_data_main(C);
  char *path;
  bool changed = false;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
    /* pass event on if no active button found */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if (ptr.owner_id && ptr.data && prop) {
    path = RNA_path_from_ID_to_property(&ptr, prop);

    if (path) {
      if (all) {
        /* -1 indicates operating on the entire array (or the property itself otherwise) */
        index = -1;
      }

      changed |=
          (clear_keyframe(
               bmain, op->reports, ptr.owner_id, nullptr, path, index, eInsertKeyFlags(0)) != 0);
      MEM_freeN(path);
    }
    else if (G.debug & G_DEBUG) {
      printf("Button Clear-Key: no path to property\n");
    }
  }
  else if (G.debug & G_DEBUG) {
    printf("ptr.data = %p, prop = %p\n", ptr.data, (void *)prop);
  }

  if (changed) {
    /* send updates */
    UI_context_update_anim_flag(C);

    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_clear_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Keyframe (Buttons)";
  ot->idname = "ANIM_OT_keyframe_clear_button";
  ot->description = "Clear all keyframes on the currently active property";

  /* callbacks */
  ot->exec = clear_key_button_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Clear keyframes from all elements of the array");
}

/* ******************************************* */
/* AUTO KEYFRAME */

bool autokeyframe_cfra_can_key(const Scene *scene, ID *id)
{
  float cfra = float(scene->r.cfra); /* XXX for now, this will do */

  /* only filter if auto-key mode requires this */
  if (IS_AUTOKEY_ON(scene) == 0) {
    return false;
  }

  if (IS_AUTOKEY_MODE(scene, EDITKEYS)) {
    /* Replace Mode:
     * For whole block, only key if there's a keyframe on that frame already
     * This is a valid assumption when we're blocking + tweaking
     */
    return id_frame_has_keyframe(id, cfra);
  }

  /* Normal Mode (or treat as being normal mode):
   *
   * Just in case the flags aren't set properly (i.e. only on/off is set, without a mode)
   * let's set the "normal" flag too, so that it will all be sane everywhere...
   */
  scene->toolsettings->autokey_mode = AUTOKEY_MODE_NORMAL;

  /* Can insert anytime we like... */
  return true;
}

/* ******************************************* */
/* KEYFRAME DETECTION */

/* --------------- API/Per-Datablock Handling ------------------- */

bool fcurve_frame_has_keyframe(const FCurve *fcu, float frame)
{
  /* quick sanity check */
  if (ELEM(nullptr, fcu, fcu->bezt)) {
    return false;
  }

  if ((fcu->flag & FCURVE_MUTED) == 0) {
    bool replace;
    int i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, frame, fcu->totvert, &replace);

    /* BKE_fcurve_bezt_binarysearch_index will set replace to be 0 or 1
     * - obviously, 1 represents a match
     */
    if (replace) {
      /* sanity check: 'i' may in rare cases exceed arraylen */
      if ((i >= 0) && (i < fcu->totvert)) {
        return true;
      }
    }
  }

  return false;
}

bool fcurve_is_changed(PointerRNA ptr,
                       PropertyRNA *prop,
                       FCurve *fcu,
                       const AnimationEvalContext *anim_eval_context)
{
  PathResolvedRNA anim_rna;
  anim_rna.ptr = ptr;
  anim_rna.prop = prop;
  anim_rna.prop_index = fcu->array_index;

  float buffer[RNA_MAX_ARRAY_LENGTH];
  int count, index = fcu->array_index;
  float *values = setting_get_rna_values(&ptr, prop, buffer, RNA_MAX_ARRAY_LENGTH, &count);

  float fcurve_val = calculate_fcurve(&anim_rna, fcu, anim_eval_context);
  float cur_val = (index >= 0 && index < count) ? values[index] : 0.0f;

  if (values != buffer) {
    MEM_freeN(values);
  }

  return !compare_ff_relative(fcurve_val, cur_val, FLT_EPSILON, 64);
}

/**
 * Checks whether an Action has a keyframe for a given frame
 * Since we're only concerned whether a keyframe exists,
 * we can simply loop until a match is found.
 */
static bool action_frame_has_keyframe(bAction *act, float frame)
{
  FCurve *fcu;

  /* can only find if there is data */
  if (act == nullptr) {
    return false;
  }

  if (act->flag & ACT_MUTED) {
    return false;
  }

  /* loop over F-Curves, using binary-search to try to find matches
   * - this assumes that keyframes are only beztriples
   */
  for (fcu = static_cast<FCurve *>(act->curves.first); fcu; fcu = fcu->next) {
    /* only check if there are keyframes (currently only of type BezTriple) */
    if (fcu->bezt && fcu->totvert) {
      if (fcurve_frame_has_keyframe(fcu, frame)) {
        return true;
      }
    }
  }

  /* nothing found */
  return false;
}

/* Checks whether an Object has a keyframe for a given frame */
static bool object_frame_has_keyframe(Object *ob, float frame)
{
  /* error checking */
  if (ob == nullptr) {
    return false;
  }

  /* check own animation data - specifically, the action it contains */
  if ((ob->adt) && (ob->adt->action)) {
    /* #41525 - When the active action is a NLA strip being edited,
     * we need to correct the frame number to "look inside" the
     * remapped action
     */
    float ob_frame = BKE_nla_tweakedit_remap(ob->adt, frame, NLATIME_CONVERT_UNMAP);

    if (action_frame_has_keyframe(ob->adt->action, ob_frame)) {
      return true;
    }
  }

  /* nothing found */
  return false;
}

/* --------------- API ------------------- */

bool id_frame_has_keyframe(ID *id, float frame)
{
  /* sanity checks */
  if (id == nullptr) {
    return false;
  }

  /* perform special checks for 'macro' types */
  switch (GS(id->name)) {
    case ID_OB: /* object */
      return object_frame_has_keyframe((Object *)id, frame);
#if 0
    /* XXX TODO... for now, just use 'normal' behavior */
    case ID_SCE: /* scene */
      break;
#endif
    default: /* 'normal type' */
    {
      AnimData *adt = BKE_animdata_from_id(id);

      /* only check keyframes in active action */
      if (adt) {
        return action_frame_has_keyframe(adt->action, frame);
      }
      break;
    }
  }

  /* no keyframe found */
  return false;
}

/* ************************************************** */

bool ED_autokeyframe_object(bContext *C, Scene *scene, Object *ob, KeyingSet *ks)
{
  /* auto keyframing */
  if (autokeyframe_cfra_can_key(scene, &ob->id)) {
    ListBase dsources = {nullptr, nullptr};

    /* Now insert the key-frame(s) using the Keying Set:
     * 1) Add data-source override for the Object.
     * 2) Insert key-frames.
     * 3) Free the extra info.
     */
    ANIM_relative_keyingset_add_source(&dsources, &ob->id, nullptr, nullptr);
    ANIM_apply_keyingset(C, &dsources, nullptr, ks, MODIFYKEY_MODE_INSERT, float(scene->r.cfra));
    BLI_freelistN(&dsources);

    return true;
  }
  return false;
}

bool ED_autokeyframe_pchan(
    bContext *C, Scene *scene, Object *ob, bPoseChannel *pchan, KeyingSet *ks)
{
  if (autokeyframe_cfra_can_key(scene, &ob->id)) {
    ListBase dsources = {nullptr, nullptr};

    /* Now insert the keyframe(s) using the Keying Set:
     * 1) Add data-source override for the pose-channel.
     * 2) Insert key-frames.
     * 3) Free the extra info.
     */
    ANIM_relative_keyingset_add_source(&dsources, &ob->id, &RNA_PoseBone, pchan);
    ANIM_apply_keyingset(C, &dsources, nullptr, ks, MODIFYKEY_MODE_INSERT, float(scene->r.cfra));
    BLI_freelistN(&dsources);

    return true;
  }

  return false;
}

bool ED_autokeyframe_property(bContext *C,
                              Scene *scene,
                              PointerRNA *ptr,
                              PropertyRNA *prop,
                              int rnaindex,
                              float cfra,
                              const bool only_if_property_keyed)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    cfra);
  ID *id;
  bAction *action;
  FCurve *fcu;
  bool driven;
  bool special;
  bool changed = false;

  /* for entire array buttons we check the first component, it's not perfect
   * but works well enough in typical cases */
  const int rnaindex_check = (rnaindex == -1) ? 0 : rnaindex;
  fcu = BKE_fcurve_find_by_rna_context_ui(
      C, ptr, prop, rnaindex_check, nullptr, &action, &driven, &special);

  /* Only early out when we actually want an existing F-curve already
   * (e.g. auto-keyframing from buttons). */
  if (fcu == nullptr && (driven || special || only_if_property_keyed)) {
    return changed;
  }

  if (special) {
    /* NLA Strip property */
    if (IS_AUTOKEY_ON(scene)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;

      changed = insert_keyframe_direct(reports,
                                       *ptr,
                                       prop,
                                       fcu,
                                       &anim_eval_context,
                                       eBezTriple_KeyframeType(ts->keyframe_type),
                                       nullptr,
                                       eInsertKeyFlags(0));
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  else if (driven) {
    /* Driver - Try to insert keyframe using the driver's input as the frame,
     * making it easier to set up corrective drivers
     */
    if (IS_AUTOKEY_ON(scene)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;

      changed = insert_keyframe_direct(reports,
                                       *ptr,
                                       prop,
                                       fcu,
                                       &anim_eval_context,
                                       eBezTriple_KeyframeType(ts->keyframe_type),
                                       nullptr,
                                       INSERTKEY_DRIVER);
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  else {
    id = ptr->owner_id;

    /* TODO: this should probably respect the keyingset only option for anim */
    if (autokeyframe_cfra_can_key(scene, id)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;
      const eInsertKeyFlags flag = ANIM_get_keyframing_flags(scene, true);
      char *path = RNA_path_from_ID_to_property(ptr, prop);

      if (only_if_property_keyed) {
        /* NOTE: We use rnaindex instead of fcu->array_index,
         *       because a button may control all items of an array at once.
         *       E.g., color wheels (see #42567). */
        BLI_assert((fcu->array_index == rnaindex) || (rnaindex == -1));
      }
      changed = insert_keyframe(bmain,
                                reports,
                                id,
                                action,
                                (fcu && fcu->grp) ? fcu->grp->name : nullptr,
                                fcu ? fcu->rna_path : path,
                                rnaindex,
                                &anim_eval_context,
                                eBezTriple_KeyframeType(ts->keyframe_type),
                                nullptr,
                                flag) != 0;
      if (path) {
        MEM_freeN(path);
      }
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  return changed;
}

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/** Use for insert/delete key-frame. */
static KeyingSet *keyingset_get_from_op_with_error(wmOperator *op, PropertyRNA *prop, Scene *scene)
{
  KeyingSet *ks = nullptr;
  const int prop_type = RNA_property_type(prop);
  if (prop_type == PROP_ENUM) {
    int type = RNA_property_enum_get(op->ptr, prop);
    ks = ANIM_keyingset_get_from_enum_type(scene, type);
    if (ks == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "No active Keying Set");
    }
  }
  else if (prop_type == PROP_STRING) {
    char type_id[MAX_ID_NAME - 2];
    RNA_property_string_get(op->ptr, prop, type_id);
    ks = ANIM_keyingset_get_from_idname(scene, type_id);

    if (ks == nullptr) {
      BKE_reportf(op->reports, RPT_ERROR, "Keying set '%s' not found", type_id);
    }
  }
  else {
    BLI_assert(0);
  }
  return ks;
}

/** \} */
