/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "ANIM_action.hh"
#include "ANIM_nla.hh"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_action_tools.hh"
#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

/* Enum defines exported for `rna_animation.cc`. */

const EnumPropertyItem rna_enum_nla_mode_blend_items[] = {
    {NLASTRIP_MODE_REPLACE,
     "REPLACE",
     0,
     "Replace",
     "The strip values replace the accumulated results by amount specified by influence"},
    {NLASTRIP_MODE_COMBINE,
     "COMBINE",
     0,
     "Combine",
     "The strip values are combined with accumulated results by appropriately using addition, "
     "multiplication, or quaternion math, based on channel type"},
    RNA_ENUM_ITEM_SEPR,
    {NLASTRIP_MODE_ADD,
     "ADD",
     0,
     "Add",
     "Weighted result of strip is added to the accumulated results"},
    {NLASTRIP_MODE_SUBTRACT,
     "SUBTRACT",
     0,
     "Subtract",
     "Weighted result of strip is removed from the accumulated results"},
    {NLASTRIP_MODE_MULTIPLY,
     "MULTIPLY",
     0,
     "Multiply",
     "Weighted result of strip is multiplied with the accumulated results"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_nla_mode_extend_items[] = {
    {NLASTRIP_EXTEND_NOTHING, "NOTHING", 0, "Nothing", "Strip has no influence past its extents"},
    {NLASTRIP_EXTEND_HOLD,
     "HOLD",
     0,
     "Hold",
     "Hold the first frame if no previous strips in track, and always hold last frame"},
    {NLASTRIP_EXTEND_HOLD_FORWARD, "HOLD_FORWARD", 0, "Hold Forward", "Only hold last frame"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>
#  include <math.h>
#  include <stdio.h>

/* needed for some of the validation stuff... */
#  include "BKE_anim_data.hh"
#  include "BKE_fcurve.hh"
#  include "BKE_nla.hh"

#  include "DNA_object_types.h"

#  include "ED_anim_api.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

static void rna_NlaStrip_name_set(PointerRNA *ptr, const char *value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* copy the name first */
  STRNCPY_UTF8(data->name, value);

  /* validate if there's enough info to do so */
  if (ptr->owner_id) {
    AnimData *adt = BKE_animdata_from_id(ptr->owner_id);
    BKE_nlastrip_validate_name(adt, data);
  }
}

static std::optional<std::string> rna_NlaStrip_path(const PointerRNA *ptr)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;
  AnimData *adt = BKE_animdata_from_id(ptr->owner_id);

  /* if we're attached to AnimData, try to resolve path back to AnimData */
  if (adt) {
    NlaTrack *nlt;
    NlaStrip *nls;

    for (nlt = static_cast<NlaTrack *>(adt->nla_tracks.first); nlt; nlt = nlt->next) {
      for (nls = static_cast<NlaStrip *>(nlt->strips.first); nls; nls = nls->next) {
        if (nls == strip) {
          /* XXX but if we animate like this, the control will never work... */
          char name_esc_nlt[sizeof(nlt->name) * 2];
          char name_esc_strip[sizeof(strip->name) * 2];

          BLI_str_escape(name_esc_nlt, nlt->name, sizeof(name_esc_nlt));
          BLI_str_escape(name_esc_strip, strip->name, sizeof(name_esc_strip));
          return fmt::format(
              "animation_data.nla_tracks[\"{}\"].strips[\"{}\"]", name_esc_nlt, name_esc_strip);
        }
      }
    }
  }

  /* no path */
  return "";
}

static void rna_NlaStrip_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  ANIM_id_update(bmain, id);
}

static void rna_NlaStrip_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);

  rna_NlaStrip_update(bmain, scene, ptr);
}

static void rna_NlaStrip_transform_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;

  BKE_nlameta_flush_transforms(strip);

  /* set the flag */
  if ((strip->flag & NLASTRIP_FLAG_AUTO_BLENDS) && ptr->owner_id) {
    /* validate state to ensure that auto-blend gets applied immediately */
    IdAdtTemplate *iat = (IdAdtTemplate *)ptr->owner_id;

    if (iat->adt) {
      BKE_nla_validate_state(iat->adt);
    }
  }

  BKE_nlastrip_recalculate_blend(strip);

  rna_NlaStrip_update(bmain, scene, ptr);
}

static void rna_NlaStrip_start_frame_set(PointerRNA *ptr, float value)
{
  /* Simply set the frame start in a valid range : if there are any NLA strips before/after, clamp
   * the start value. If the new start value is past-the-end, clamp it. Otherwise, set it.
   *
   * NOTE: Unless neighboring strips are transitions, NLASTRIP_MIN_LEN_THRESH is not needed, as
   * strips can be 'glued' to one another. If they are however, ensure transitions have a bit of
   * time allotted in order to be performed.
   */
  NlaStrip *data = (NlaStrip *)ptr->data;

  const float limit_prev = BKE_nlastrip_compute_frame_from_previous_strip(data);
  const float limit_next = BKE_nlastrip_compute_frame_to_next_strip(data);
  CLAMP(value, limit_prev, limit_next);

  data->start = value;

  /* The ONLY case where we actively modify the value set by the user, is in case the start value
   * value is past the old end frame (here delta = NLASTRIP_MIN_LEN_THRESH) :
   * - if there's no "room" for the end frame to be placed at (new_start + delta), move old_end to
   *     the limit, and new_start to (limit - delta)
   * - otherwise, do _not_ change the end frame. This property is not accessible from the UI, and
   *     can only be set via scripts. The script should be responsible of setting the end frame.
   */
  if (data->start > (data->end - NLASTRIP_MIN_LEN_THRESH)) {
    /* If past-the-allowed-end : */
    if ((data->start + NLASTRIP_MIN_LEN_THRESH) > limit_next) {
      data->end = limit_next;
      data->start = data->end - NLASTRIP_MIN_LEN_THRESH;
    }
  }

  /* Ensure transitions are kept 'glued' to the strip : */
  if (data->prev && data->prev->type == NLASTRIP_TYPE_TRANSITION) {
    data->prev->end = data->start;
  }
}

static void rna_NlaStrip_frame_start_ui_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* Changing the NLA strip's start frame is exactly the same as translating it in the NLA editor.
   * When 'translating' the clip, the length of it should stay identical. Se we also need to set
   * this strip's end frame after modifying its start (to `start + (old_end - old_start)`).
   * Of course, we might have a few other strips on this NLA track, so we have to respect the
   * previous strip's end frame.
   *
   * Also, different types of NLA strips (*_CLIP, *_TRANSITION, *_META, *_SOUND) have their own
   * properties to respect. Needs testing on a real-world use case for the transition, meta, and
   * sound types.
   */

  /* The strip's total length before modifying it & also how long we'd like it to be afterwards. */
  const float striplen = data->end - data->start;

  /* We're only modifying one strip at a time. The start and end times of its neighbors should not
   * change. As such, here are the 'bookends' (frame limits) for the start position to respect :
   * - if a next strip exists, don't allow the strip to start after (next->end - striplen - delta),
   *   (delta being the min length of a Nla Strip : the NLASTRIP_MIN_THRESH macro)
   * - if a previous strip exists, don't allow this strip to start before it (data->prev) ends
   * - otherwise, limit to the program limit macros defined in DNA_scene_types.h : {MINA|MAX}FRAMEF
   */
  const float limit_prev = BKE_nlastrip_compute_frame_from_previous_strip(data);
  const float limit_next = BKE_nlastrip_compute_frame_to_next_strip(data) - striplen;
  /* For above : we want to be able to fit the entire strip before the next frame limit, so shift
   * the next limit by 'striplen' no matter the context. */

  CLAMP(value, limit_prev, limit_next);
  data->start = value;

  if (data->type != NLASTRIP_TYPE_TRANSITION) {
    data->end = data->start + striplen;
  }

  /* Update properties of the prev/next strips if they are transitions to ensure consistency : */
  if (data->prev && data->prev->type == NLASTRIP_TYPE_TRANSITION) {
    data->prev->end = data->start;
  }
  if (data->next && data->next->type == NLASTRIP_TYPE_TRANSITION) {
    data->next->start = data->end;
  }
}

static void rna_NlaStrip_end_frame_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  const float limit_prev = BKE_nlastrip_compute_frame_from_previous_strip(data);
  const float limit_next = BKE_nlastrip_compute_frame_to_next_strip(data);
  CLAMP(value, limit_prev, limit_next);

  data->end = value;

  /* The ONLY case where we actively modify the value set by the user, is in case the start value
   * value is past the old end frame (here delta = NLASTRIP_MIN_LEN_THRESH):
   * - if there's no "room" for the end frame to be placed at (new_start + delta), move old_end to
   *   the limit, and new_start to (limit - delta)
   * - otherwise, do _not_ change the end frame. This property is not accessible from the UI, and
   *   can only be set via scripts. The script should be responsible for setting the end frame.
   */
  if (data->end < (data->start + NLASTRIP_MIN_LEN_THRESH)) {
    /* If before-the-allowed-start : */
    if ((data->end - NLASTRIP_MIN_LEN_THRESH) < limit_prev) {
      data->start = limit_prev;
      data->end = data->start + NLASTRIP_MIN_LEN_THRESH;
    }
  }

  /* Ensure transitions are kept "glued" to the strip: */
  if (data->next && data->next->type == NLASTRIP_TYPE_TRANSITION) {
    data->next->start = data->end;
  }
}

static void rna_NlaStrip_frame_end_ui_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* Changing the strip's end frame will update its action 'range' (defined by actstart->actend) to
   * accommodate the extra length of the strip. No other parameters of the strip will change. But
   * this means we have to get the current strip's end frame right now :
   */
  const float old_strip_end = data->end;

  /* clamp value to lie within valid limits
   * - must not have zero or negative length strip, so cannot start before the first frame
   *   + some minimum-strip-length threshold
   * - cannot end later than the start of the next strip (if present)
   *   -> relies on the BKE_nlastrip_compute_frame_to_next_strip() function
   */
  const float limit_prev = data->start + NLASTRIP_MIN_LEN_THRESH;
  const float limit_next = BKE_nlastrip_compute_frame_to_next_strip(data);

  CLAMP(value, limit_prev, limit_next);
  data->end = value;

  /* Only adjust transitions at this stage : */
  if (data->next && data->next->type == NLASTRIP_TYPE_TRANSITION) {
    data->next->start = value;
  }

  /* calculate the lengths the strip and its action : *
   * (Meta and transitions shouldn't be updated, but clip and sound should) */
  if (data->type == NLASTRIP_TYPE_CLIP || data->type == NLASTRIP_TYPE_SOUND) {
    const float actlen = BKE_nla_clip_length_get_nonzero(data);

    /* Modify the strip's action end frame, or repeat based on :
     * - if data->repeat == 1.0f, modify the action end frame :
     *   - if the number of frames to subtract is the number of frames, set the action end frame
     *     to the action start + 1 and modify the end of the strip to add that frame
     *   - if the number of frames
     * - otherwise, modify the repeat property to accommodate for the new length
     */
    float action_length_delta = (old_strip_end - data->end) / data->scale;
    /* If no repeats are used, then modify the action end frame : */
    if (IS_EQF(data->repeat, 1.0f)) {
      /* If they're equal, strip has been reduced by the same amount as the whole strip length,
       * so clamp the action clip length to 1 frame, and add a frame to end so that
       * `len(strip) != 0`. */
      if (IS_EQF(action_length_delta, actlen)) {
        data->actend = data->actstart + 1.0f;
        data->end += 1.0f;
      }
      else if (action_length_delta < actlen) {
        /* Now, adjust the new strip's actend to the value it's supposed to have : */
        data->actend = data->actend - action_length_delta;
      }
      /* The case where the delta is bigger than the action length should not be possible, since
       * data->end is guaranteed to be clamped to data->start + threshold above.
       */
    }
    else {
      data->repeat -= (action_length_delta / actlen);
    }
  }
}

static void rna_NlaStrip_scale_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* set scale value */
  /* NOTE: these need to be synced with the values in the
   * property definition in rna_def_nlastrip() */
  CLAMP(value, 0.0001f, 1000.0f);
  data->scale = value;

  /* adjust the strip extents in response to this */
  BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_repeat_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* set repeat value */
  /* NOTE: these need to be synced with the values in the
   * property definition in rna_def_nlastrip() */
  CLAMP(value, 0.01f, 1000.0f);
  data->repeat = value;

  /* adjust the strip extents in response to this */
  BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_blend_in_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;
  float len;

  /* blend-in is limited to the length of the strip, and also cannot overlap with blendout */
  len = (data->end - data->start) - data->blendout;
  CLAMP(value, 0, len);

  data->blendin = value;
}

static void rna_NlaStrip_blend_out_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;
  float len;

  /* blend-out is limited to the length of the strip */
  len = (data->end - data->start);
  CLAMP(value, 0, len);

  /* it also cannot overlap with blendin */
  if ((len - value) < data->blendin) {
    value = len - data->blendin;
  }

  data->blendout = value;
}

static void rna_NlaStrip_use_auto_blend_set(PointerRNA *ptr, bool value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  if (value) {
    /* set the flag */
    data->flag |= NLASTRIP_FLAG_AUTO_BLENDS;

    /* validate state to ensure that auto-blend gets applied immediately */
    if (ptr->owner_id) {
      IdAdtTemplate *iat = (IdAdtTemplate *)ptr->owner_id;

      if (iat->adt) {
        BKE_nla_validate_state(iat->adt);
      }
    }
  }
  else {
    /* clear the flag */
    data->flag &= ~NLASTRIP_FLAG_AUTO_BLENDS;

    /* clear the values too, so that it's clear that there has been an effect */
    /* TODO: it's somewhat debatable whether it's better to leave these in instead... */
    data->blendin = 0.0f;
    data->blendout = 0.0f;
  }
}

static void rna_NlaStrip_action_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  using namespace blender::animrig;
  BLI_assert(ptr->owner_id);
  BLI_assert(ptr->data);

  ID &animated_id = *ptr->owner_id;
  NlaStrip &strip = *static_cast<NlaStrip *>(ptr->data);
  Action *action = static_cast<Action *>(value.data);

  if (!action) {
    nla::unassign_action(strip, animated_id);
    return;
  }

  if (!nla::assign_action(strip, *action, animated_id)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not assign action %s to NLA strip %s",
                action->id.name + 2,
                strip.name);
  }
}

static int rna_NlaStrip_action_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;

  /* Strip actions shouldn't be editable if NLA tweak-mode is on. */
  if (ptr->owner_id) {
    AnimData *adt = BKE_animdata_from_id(ptr->owner_id);

    if (adt) {
      /* active action is only editable when it is not a tweaking strip */
      if ((adt->flag & ADT_NLA_EDIT_ON) || (adt->actstrip) || (adt->tmpact)) {
        return 0;
      }
    }
  }

  /* check for clues that strip probably shouldn't be used... */
  if (strip->flag & NLASTRIP_FLAG_TWEAKUSER) {
    return 0;
  }

  /* should be ok, though we may still miss some cases */
  return PROP_EDITABLE;
}

static void rna_NlaStrip_action_slot_handle_set(
    PointerRNA *ptr, const blender::animrig::slot_handle_t new_slot_handle)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;
  rna_generic_action_slot_handle_set(new_slot_handle,
                                     *ptr->owner_id,
                                     strip->act,
                                     strip->action_slot_handle,
                                     strip->last_slot_identifier);
}

/**
 * Emit a 'diff' for the .action_slot_handle property whenever the .action property differs.
 *
 * \see rna_generic_action_slot_handle_override_diff()
 */
static void rna_NlaStrip_action_slot_handle_override_diff(
    Main *bmain, RNAPropertyOverrideDiffContext &rnadiff_ctx)
{
  const NlaStrip *strip_a = static_cast<NlaStrip *>(rnadiff_ctx.prop_a->ptr->data);
  const NlaStrip *strip_b = static_cast<NlaStrip *>(rnadiff_ctx.prop_b->ptr->data);

  rna_generic_action_slot_handle_override_diff(bmain, rnadiff_ctx, strip_a->act, strip_b->act);
}

static PointerRNA rna_NlaStrip_action_slot_get(PointerRNA *ptr)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;
  return rna_generic_action_slot_get(strip->act, strip->action_slot_handle);
}

static void rna_NlaStrip_action_slot_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;
  rna_generic_action_slot_set(value,
                              *ptr->owner_id,
                              strip->act,
                              strip->action_slot_handle,
                              strip->last_slot_identifier,
                              reports);
}

static void rna_iterator_nlastrip_action_suitable_slots_begin(CollectionPropertyIterator *iter,
                                                              PointerRNA *ptr)
{
  NlaStrip *strip = (NlaStrip *)ptr->data;
  rna_iterator_generic_action_suitable_slots_begin(iter, ptr, strip->act);
}

static void rna_NlaStrip_action_start_frame_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* prevent start frame from occurring after end of action */
  CLAMP(value, MINAFRAME, data->actend);
  data->actstart = value;

  /* adjust the strip extents in response to this */
  /* TODO: should the strip be moved backwards instead as a special case? */
  BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_action_end_frame_set(PointerRNA *ptr, float value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  /* prevent end frame from starting before start of action */
  CLAMP(value, data->actstart, MAXFRAME);
  data->actend = value;

  /* adjust the strip extents in response to this */
  BKE_nlastrip_recalculate_bounds(data);
}

static void rna_NlaStrip_animated_influence_set(PointerRNA *ptr, bool value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  if (value) {
    /* set the flag, then make sure a curve for this exists */
    data->flag |= NLASTRIP_FLAG_USR_INFLUENCE;
    BKE_nlastrip_validate_fcurves(data);
  }
  else {
    data->flag &= ~NLASTRIP_FLAG_USR_INFLUENCE;
  }
}

static void rna_NlaStrip_animated_time_set(PointerRNA *ptr, bool value)
{
  NlaStrip *data = (NlaStrip *)ptr->data;

  if (value) {
    /* set the flag, then make sure a curve for this exists */
    data->flag |= NLASTRIP_FLAG_USR_TIME;
    BKE_nlastrip_validate_fcurves(data);
  }
  else {
    data->flag &= ~NLASTRIP_FLAG_USR_TIME;
  }
}

static FCurve *rna_NlaStrip_fcurve_find(NlaStrip *strip,
                                        ReportList *reports,
                                        const char *data_path,
                                        int index)
{
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  /* Returns nullptr if not found. */
  return BKE_fcurve_find(&strip->fcurves, data_path, index);
}

static NlaStrip *rna_NlaStrip_new(ID *id,
                                  NlaTrack *track,
                                  Main *bmain,
                                  bContext *C,
                                  ReportList *reports,
                                  const char * /*name*/,
                                  int start,
                                  bAction *action)
{
  BLI_assert(id);
  NlaStrip *strip = BKE_nlastrip_new(action, *id);

  if (strip == nullptr) {
    BKE_report(reports, RPT_ERROR, "Unable to create new strip");
    return nullptr;
  }

  strip->end += (start - strip->start);
  strip->start = start;

  if (!BKE_nlastrips_add_strip(&track->strips, strip)) {
    BKE_report(
        reports,
        RPT_ERROR,
        "Unable to add strip (the track does not have any space to accommodate this new strip)");
    BKE_nlastrip_free(strip, true);
    return nullptr;
  }

  /* create dummy AnimData block so that BKE_nlastrip_validate_name()
   * can be used to ensure a valid name, as we don't have one here...
   * - only the nla_tracks list is needed there, which we aim to reverse engineer here...
   */
  {
    AnimData adt = {nullptr};
    NlaTrack *nlt, *nlt_p;

    /* 'first' NLA track is found by going back up chain of given
     * track's parents until we fall off. */
    nlt_p = track;
    nlt = track;
    while ((nlt = nlt->prev) != nullptr) {
      nlt_p = nlt;
    }
    adt.nla_tracks.first = nlt_p;

    /* do the same thing to find the last track */
    nlt_p = track;
    nlt = track;
    while ((nlt = nlt->next) != nullptr) {
      nlt_p = nlt;
    }
    adt.nla_tracks.last = nlt_p;

    /* now we can just auto-name as usual */
    BKE_nlastrip_validate_name(&adt, strip);
  }

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, nullptr);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION | ID_RECALC_SYNC_TO_EVAL);

  return strip;
}

static void rna_NlaStrip_remove(
    ID *id, NlaTrack *track, Main *bmain, bContext *C, ReportList *reports, PointerRNA *strip_ptr)
{
  NlaStrip *strip = static_cast<NlaStrip *>(strip_ptr->data);
  if (BLI_findindex(&track->strips, strip) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "NLA strip '%s' not found in track '%s'", strip->name, track->name);
    return;
  }

  BKE_nlastrip_remove_and_free(&track->strips, strip, true);
  strip_ptr->invalidate();

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, nullptr);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION | ID_RECALC_SYNC_TO_EVAL);
}

/* Set the 'solo' setting for the given NLA-track, making sure that it is the only one
 * that has this status in its AnimData block.
 */
static void rna_NlaTrack_solo_set(PointerRNA *ptr, bool value)
{
  NlaTrack *data = (NlaTrack *)ptr->data;
  AnimData *adt = BKE_animdata_from_id(ptr->owner_id);
  NlaTrack *nt;

  if (data == nullptr) {
    return;
  }

  /* firstly, make sure 'solo' flag for all tracks is disabled */
  for (nt = data; nt; nt = nt->next) {
    nt->flag &= ~NLATRACK_SOLO;
  }
  for (nt = data; nt; nt = nt->prev) {
    nt->flag &= ~NLATRACK_SOLO;
  }

  /* now, enable 'solo' for the given track if appropriate */
  if (value) {
    /* set solo status */
    data->flag |= NLATRACK_SOLO;

    /* set solo-status on AnimData */
    adt->flag |= ADT_NLA_SOLO_TRACK;
  }
  else {
    /* solo status was already cleared on track */

    /* clear solo-status on AnimData */
    adt->flag &= ~ADT_NLA_SOLO_TRACK;
  }
}

#else

static void rna_def_strip_fcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "NlaStripFCurves");
  srna = RNA_def_struct(brna, "NlaStripFCurves", nullptr);
  RNA_def_struct_sdna(srna, "NlaStrip");
  RNA_def_struct_ui_text(srna, "NLA-Strip F-Curves", "Collection of NLA strip F-Curves");

  /* `Strip.fcurves.find(...)`. */
  func = RNA_def_function(srna, "find", "rna_NlaStrip_fcurve_find");
  RNA_def_function_ui_description(
      func,
      "Find an F-Curve. Note that this function performs a linear scan "
      "of all F-Curves in the NLA strip.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);

  parm = RNA_def_pointer(
      func, "fcurve", "FCurve", "", "The found F-Curve, or None if it doesn't exist");
  RNA_def_function_return(func, parm);
}

static void rna_def_nlastrip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Enum definitions. */
  static const EnumPropertyItem prop_type_items[] = {
      {NLASTRIP_TYPE_CLIP, "CLIP", 0, "Action Clip", "NLA Strip references some Action"},
      {NLASTRIP_TYPE_TRANSITION,
       "TRANSITION",
       0,
       "Transition",
       "NLA Strip 'transitions' between adjacent strips"},
      {NLASTRIP_TYPE_META, "META", 0, "Meta", "NLA Strip acts as a container for adjacent strips"},
      {NLASTRIP_TYPE_SOUND,
       "SOUND",
       0,
       "Sound Clip",
       "NLA Strip representing a sound event for speakers"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* struct definition */
  srna = RNA_def_struct(brna, "NlaStrip", nullptr);
  RNA_def_struct_ui_text(srna, "NLA Strip", "A container referencing an existing Action");
  RNA_def_struct_path_func(srna, "rna_NlaStrip_path");
  RNA_def_struct_ui_icon(srna, ICON_NLA); /* XXX */

  RNA_define_lib_overridable(true);

  /* name property */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_NlaStrip_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  /* Enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_clear_flag(
      prop, PROP_EDITABLE); /* XXX for now, not editable, since this is dangerous */
  RNA_def_property_enum_items(prop, prop_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ACTION);
  RNA_def_property_ui_text(prop, "Type", "Type of NLA Strip");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "extrapolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "extendmode");
  RNA_def_property_enum_items(prop, rna_enum_nla_mode_extend_items);
  RNA_def_property_ui_text(
      prop, "Extrapolation", "Action to take for gaps past the strip extents");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blendmode");
  RNA_def_property_enum_items(prop, rna_enum_nla_mode_blend_items);
  RNA_def_property_ui_text(
      prop, "Blending", "Method used for combining strip's result with accumulated result");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  /* Strip extents */
  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "start");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_start_frame_set", nullptr);
  RNA_def_property_ui_text(prop, "Start Frame", "");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");
  /* The `frame_start` and `frame_end` properties should NOT be considered for library overrides,
   * as their setters always enforce a valid state. While library overrides are applied, the
   * intermediate state may be invalid, even when the end state is valid. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "end");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_end_frame_set", nullptr);
  RNA_def_property_ui_text(prop, "End Frame", "");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");
  /* The `frame_start` and `frame_end` properties should NOT be considered for library overrides,
   * as their setters always enforce a valid state. While library overrides are applied, the
   * intermediate state may be invalid, even when the end state is valid. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  /* Strip extents without enforcing a valid state. */
  prop = RNA_def_property(srna, "frame_start_raw", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "start");
  RNA_def_property_ui_text(prop,
                           "Start Frame (raw value)",
                           "Same as frame_start, except that any value can be set, including ones "
                           "that create an invalid state");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  prop = RNA_def_property(srna, "frame_end_raw", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "end");
  RNA_def_property_ui_text(prop,
                           "End Frame (raw value)",
                           "Same as frame_end, except that any value can be set, including ones "
                           "that create an invalid state");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  /* Strip extents, when called from UI elements : */
  prop = RNA_def_property(srna, "frame_start_ui", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "start");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_frame_start_ui_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Start Frame (manipulated from UI)",
      "Start frame of the NLA strip. Note: changing this value also updates the value of "
      "the strip's end frame. If only the start frame should be changed, see the \"frame_start\" "
      "property instead.");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");
  /* The `..._ui` properties should NOT be considered for library overrides, as they are meant to
   * have different behavior than when setting their non-`..._ui` counterparts. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  prop = RNA_def_property(srna, "frame_end_ui", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "end");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_frame_end_ui_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "End Frame (manipulated from UI)",
      "End frame of the NLA strip. Note: changing this value also updates the value of "
      "the strip's repeats or its action's end frame. If only the end frame should be "
      "changed, see the \"frame_end\" property instead.");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");
  /* The `..._ui` properties should NOT be considered for library overrides, as they are meant to
   * have different behavior than when setting their non-`..._ui` counterparts. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);

  /* Blending */
  prop = RNA_def_property(srna, "blend_in", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "blendin");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_blend_in_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Blend In", "Number of frames at start of strip to fade in influence");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "blend_out", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "blendout");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_blend_out_set", nullptr);
  RNA_def_property_ui_text(prop, "Blend Out", "");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "use_auto_blend", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_AUTO_BLENDS);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NlaStrip_use_auto_blend_set");
  RNA_def_property_ui_text(prop,
                           "Auto Blend In/Out",
                           "Number of frames for Blending In/Out is automatically determined from "
                           "overlapping strips");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  /* Action */
  prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "act");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_NlaStrip_action_set", nullptr, "rna_Action_id_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_NlaStrip_action_editable");
  RNA_def_property_ui_text(prop, "Action", "Action referenced by this strip");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_dependency_update");

  /* This property is not necessary for the Python API (that is better off using
   * slot references/pointers directly), but it is needed for library overrides
   * to work. */
  prop = RNA_def_property(srna, "action_slot_handle", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "action_slot_handle");
  RNA_def_property_int_funcs(prop, nullptr, "rna_NlaStrip_action_slot_handle_set", nullptr);
  RNA_def_property_ui_text(prop,
                           "Action Slot Handle",
                           "A number that identifies which sub-set of the Action is considered "
                           "to be for this NLA strip");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_funcs(
      prop, "rna_NlaStrip_action_slot_handle_override_diff", nullptr, nullptr);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_NlaStrip_dependency_update");

  prop = RNA_def_property(srna, "last_slot_identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "last_slot_identifier");
  RNA_def_property_ui_text(
      prop,
      "Last Action Slot Identifier",
      "The identifier of the most recently assigned action slot. The slot identifies which "
      "sub-set of the Action is considered to be for this strip, and its identifier is used to "
      "find the right slot when assigning an Action.");

  prop = RNA_def_property(srna, "action_slot", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Action Slot",
      "The slot identifies which sub-set of the Action is considered to be for this "
      "strip, and its name is used to find the right slot when assigning another Action");
  RNA_def_property_pointer_funcs(
      prop, "rna_NlaStrip_action_slot_get", "rna_NlaStrip_action_slot_set", nullptr, nullptr);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_NlaStrip_dependency_update");
  /* `strip.action_slot` is exposed to RNA as a pointer for things like the action slot selector in
   * the GUI. The ground truth of the assigned slot, however, is `action_slot_handle` declared
   * above. That property is used for library override operations, and this pointer property should
   * just be ignored.
   *
   * This needs PROPOVERRIDE_IGNORE; PROPOVERRIDE_NO_COMPARISON is not suitable here. This property
   * should act as if it is an overridable property (as from the user's perspective, it is), but an
   * override operation should not be created for it. It will be created for `action_slot_handle`,
   * and that's enough. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);

  prop = RNA_def_property(srna, "action_suitable_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_nlastrip_action_suitable_slots_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Action Slots", "The list of action slots suitable for this NLA strip");

  /* Action extents */
  prop = RNA_def_property(srna, "action_frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "actstart");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_action_start_frame_set", nullptr);
  RNA_def_property_ui_text(prop, "Action Start Frame", "First frame from action to use");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  prop = RNA_def_property(srna, "action_frame_end", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "actend");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_action_end_frame_set", nullptr);
  RNA_def_property_ui_text(prop, "Action End Frame", "Last frame from action to use");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  /* Action Reuse */
  prop = RNA_def_property(srna, "repeat", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "repeat");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_repeat_set", nullptr);
  /* these limits have currently be chosen arbitrarily, but could be extended
   * (minimum should still be > 0 though) if needed... */
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.1f, 1000.0f);
  RNA_def_property_ui_text(prop, "Repeat", "Number of times to repeat the action range");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_float_funcs(prop, nullptr, "rna_NlaStrip_scale_set", nullptr);
  /* these limits can be extended, but beyond this, we can get some crazy+annoying bugs
   * due to numeric errors */
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0001f, 1000.0f);
  RNA_def_property_ui_text(prop, "Scale", "Scaling factor for action");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  /* Strip's F-Curves */
  prop = RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "fcurves", nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_ui_text(
      prop, "F-Curves", "F-Curves for controlling the strip's influence and timing");
  rna_def_strip_fcurves(brna, prop);

  /* Strip's F-Modifiers */
  prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "FModifier");
  RNA_def_property_ui_text(
      prop, "Modifiers", "Modifiers affecting all the F-Curves in the referenced Action");

  /* Strip's Sub-Strips (for Meta-Strips) */
  prop = RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "NlaStrip");
  RNA_def_property_ui_text(
      prop,
      "NLA Strips",
      "NLA Strips that this strip acts as a container for (if it is of type Meta)");

  /* Settings - Values necessary for evaluation */
  prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Influence", "Amount the strip contributes to the current result");
  /* XXX: Update temporarily disabled so that the property can be edited at all!
   * Even auto-key only applies after the curves have been re-evaluated,
   * causing the unkeyed values to be lost. */
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, /*"rna_NlaStrip_update"*/ nullptr);

  prop = RNA_def_property(srna, "strip_time", PROP_FLOAT, PROP_TIME);
  RNA_def_property_ui_text(prop, "Strip Time", "Frame of referenced Action to evaluate");
  /* XXX: Update temporarily disabled so that the property can be edited at all!
   * Even auto-key only applies after the curves have been re-evaluated,
   * causing the unkeyed values to be lost. */
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, /*"rna_NlaStrip_update"*/ nullptr);

  /* TODO: should the animated_influence/time settings be animatable themselves? */
  prop = RNA_def_property(srna, "use_animated_influence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_USR_INFLUENCE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NlaStrip_animated_influence_set");
  RNA_def_property_ui_text(
      prop,
      "Animated Influence",
      "Influence setting is controlled by an F-Curve rather than automatically determined");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "use_animated_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_USR_TIME);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NlaStrip_animated_time_set");
  RNA_def_property_ui_text(
      prop,
      "Animated Strip Time",
      "Strip time is controlled by an F-Curve rather than automatically determined");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "use_animated_time_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_USR_TIME_CYCLIC);
  RNA_def_property_ui_text(
      prop, "Cyclic Strip Time", "Cycle the animated time within the action start and end");
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_transform_update");

  /* settings */
  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  /* can be made editable by hooking it up to the necessary NLA API methods */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_ACTIVE);
  RNA_def_property_ui_text(prop, "Active", "NLA Strip is active");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_SELECT);
  RNA_def_property_ui_text(prop, "Select", "NLA Strip is selected");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_MUTED);
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, -1);
  RNA_def_property_ui_text(prop, "Mute", "Disable NLA Strip evaluation");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "use_reverse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_REVERSE);
  RNA_def_property_ui_text(prop,
                           "Reversed",
                           "NLA Strip is played back in reverse order (only when timing is "
                           "automatically determined)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "use_sync_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLASTRIP_FLAG_SYNC_LENGTH);
  RNA_def_property_ui_text(prop,
                           "Sync Action Length",
                           "Update range of frames referenced from action "
                           "after tweaking strip and its keyframes");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  RNA_define_lib_overridable(false);
}

static void rna_api_nlatrack_strips(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "NlaStrips");
  srna = RNA_def_struct(brna, "NlaStrips", nullptr);
  RNA_def_struct_sdna(srna, "NlaTrack");
  RNA_def_struct_ui_text(srna, "NLA Strips", "Collection of NLA Strips");

  func = RNA_def_function(srna, "new", "rna_NlaStrip_new");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new Action-Clip strip to the track");
  parm = RNA_def_string(func, "name", "NlaStrip", 0, "", "Name for the NLA Strips");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "start",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "Start Frame",
                     "Start frame for this strip",
                     INT_MIN,
                     INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "action", "Action", "", "Action to assign to this strip");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "strip", "NlaStrip", "", "New NLA Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NlaStrip_remove");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a NLA Strip");
  parm = RNA_def_pointer(func, "strip", "NlaStrip", "", "NLA Strip to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

static void rna_def_nlatrack(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NlaTrack", nullptr);
  RNA_def_struct_ui_text(
      srna, "NLA Track", "An animation layer containing Actions referenced as NLA strips");
  RNA_def_struct_ui_icon(srna, ICON_NLA);

  /* strips collection */
  prop = RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "NlaStrip");
  /* We do not support inserting or removing strips in overrides of tracks for now. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "NLA Strips", "NLA Strips on this NLA-track");

  rna_api_nlatrack_strips(brna, prop);

  prop = RNA_def_boolean(srna,
                         "is_override_data",
                         false,
                         "Override Track",
                         "In a local override data, whether this NLA track comes from the linked "
                         "reference data, or is local to the override");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", NLATRACK_OVERRIDELIBRARY_LOCAL);

  RNA_define_lib_overridable(true);

  /* name property */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  /* settings */
  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  /* can be made editable by hooking it up to the necessary NLA API methods */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLATRACK_ACTIVE);
  RNA_def_property_ui_text(prop, "Active", "NLA Track is active");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  prop = RNA_def_property(srna, "is_solo", PROP_BOOLEAN, PROP_NONE);
  /* can be made editable by hooking it up to the necessary NLA API methods */
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLATRACK_SOLO);
  RNA_def_property_ui_text(
      prop,
      "Solo",
      "NLA Track is evaluated itself (i.e. active Action and all other NLA Tracks in the "
      "same AnimData block are disabled)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_NlaTrack_solo_set");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLATRACK_SELECTED);
  RNA_def_property_ui_text(prop, "Select", "NLA Track is selected");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLATRACK_MUTED);
  RNA_def_property_ui_text(prop, "Muted", "Disable NLA Track evaluation");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_EDITED, "rna_NlaStrip_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", NLATRACK_PROTECTED);
  RNA_def_property_ui_text(prop, "Locked", "NLA Track is locked");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, nullptr); /* this will do? */

  RNA_define_lib_overridable(false);
}

/* --------- */

void RNA_def_nla(BlenderRNA *brna)
{
  rna_def_nlatrack(brna);
  rna_def_nlastrip(brna);
}

#endif
