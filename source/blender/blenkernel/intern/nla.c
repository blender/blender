/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation, Joshua Leung. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"

#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_nla.h"
#include "BKE_sound.h"

#include "BLO_read_write.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "nla_private.h"

static CLG_LogRef LOG = {"bke.nla"};

/**
 * Find the active track and strip.
 *
 * The active strip may or may not be on the active track.
 */
static void nla_tweakmode_find_active(const ListBase /* NlaTrack */ *nla_tracks,
                                      NlaTrack **r_track_of_active_strip,
                                      NlaStrip **r_active_strip);

/* *************************************************** */
/* Data Management */

/* Freeing ------------------------------------------- */

void BKE_nlastrip_free(ListBase *strips, NlaStrip *strip, bool do_id_user)
{
  NlaStrip *cs, *csn;

  /* sanity checks */
  if (strip == NULL) {
    return;
  }

  /* free child-strips */
  for (cs = strip->strips.first; cs; cs = csn) {
    csn = cs->next;
    BKE_nlastrip_free(&strip->strips, cs, do_id_user);
  }

  /* remove reference to action */
  if (strip->act != NULL && do_id_user) {
    id_us_min(&strip->act->id);
  }

  /* free remapping info */
  // if (strip->remap)
  //  BKE_animremap_free();

  /* free own F-Curves */
  BKE_fcurves_free(&strip->fcurves);

  /* free own F-Modifiers */
  free_fmodifiers(&strip->modifiers);

  /* free the strip itself */
  if (strips) {
    BLI_freelinkN(strips, strip);
  }
  else {
    MEM_freeN(strip);
  }
}

void BKE_nlatrack_free(ListBase *tracks, NlaTrack *nlt, bool do_id_user)
{
  NlaStrip *strip, *stripn;

  /* sanity checks */
  if (nlt == NULL) {
    return;
  }

  /* free strips */
  for (strip = nlt->strips.first; strip; strip = stripn) {
    stripn = strip->next;
    BKE_nlastrip_free(&nlt->strips, strip, do_id_user);
  }

  /* free NLA track itself now */
  if (tracks) {
    BLI_freelinkN(tracks, nlt);
  }
  else {
    MEM_freeN(nlt);
  }
}

void BKE_nla_tracks_free(ListBase *tracks, bool do_id_user)
{
  NlaTrack *nlt, *nltn;

  /* sanity checks */
  if (ELEM(NULL, tracks, tracks->first)) {
    return;
  }

  /* free tracks one by one */
  for (nlt = tracks->first; nlt; nlt = nltn) {
    nltn = nlt->next;
    BKE_nlatrack_free(tracks, nlt, do_id_user);
  }

  /* clear the list's pointers to be safe */
  BLI_listbase_clear(tracks);
}

/* Copying ------------------------------------------- */

NlaStrip *BKE_nlastrip_copy(Main *bmain,
                            NlaStrip *strip,
                            const bool use_same_action,
                            const int flag)
{
  NlaStrip *strip_d;
  NlaStrip *cs, *cs_d;

  const bool do_id_user = (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0;

  /* sanity check */
  if (strip == NULL) {
    return NULL;
  }

  /* make a copy */
  strip_d = MEM_dupallocN(strip);
  strip_d->next = strip_d->prev = NULL;

  /* handle action */
  if (strip_d->act) {
    if (use_same_action) {
      if (do_id_user) {
        /* increase user-count of action */
        id_us_plus(&strip_d->act->id);
      }
    }
    else {
      /* use a copy of the action instead (user count shouldn't have changed yet) */
      BKE_id_copy_ex(bmain, &strip_d->act->id, (ID **)&strip_d->act, flag);
    }
  }

  /* copy F-Curves and modifiers */
  BKE_fcurves_copy(&strip_d->fcurves, &strip->fcurves);
  copy_fmodifiers(&strip_d->modifiers, &strip->modifiers);

  /* make a copy of all the child-strips, one at a time */
  BLI_listbase_clear(&strip_d->strips);

  for (cs = strip->strips.first; cs; cs = cs->next) {
    cs_d = BKE_nlastrip_copy(bmain, cs, use_same_action, flag);
    BLI_addtail(&strip_d->strips, cs_d);
  }

  /* return the strip */
  return strip_d;
}

NlaTrack *BKE_nlatrack_copy(Main *bmain,
                            NlaTrack *nlt,
                            const bool use_same_actions,
                            const int flag)
{
  NlaStrip *strip, *strip_d;
  NlaTrack *nlt_d;

  /* sanity check */
  if (nlt == NULL) {
    return NULL;
  }

  /* make a copy */
  nlt_d = MEM_dupallocN(nlt);
  nlt_d->next = nlt_d->prev = NULL;

  /* make a copy of all the strips, one at a time */
  BLI_listbase_clear(&nlt_d->strips);

  for (strip = nlt->strips.first; strip; strip = strip->next) {
    strip_d = BKE_nlastrip_copy(bmain, strip, use_same_actions, flag);
    BLI_addtail(&nlt_d->strips, strip_d);
  }

  /* return the copy */
  return nlt_d;
}

void BKE_nla_tracks_copy(Main *bmain, ListBase *dst, const ListBase *src, const int flag)
{
  NlaTrack *nlt, *nlt_d;

  /* sanity checks */
  if (ELEM(NULL, dst, src)) {
    return;
  }

  /* clear out the destination list first for precautions... */
  BLI_listbase_clear(dst);

  /* copy each NLA-track, one at a time */
  for (nlt = src->first; nlt; nlt = nlt->next) {
    /* make a copy, and add the copy to the destination list */
    /* XXX: we need to fix this sometime. */
    nlt_d = BKE_nlatrack_copy(bmain, nlt, true, flag);
    BLI_addtail(dst, nlt_d);
  }
}

/**
 * Find `active_strip` in `strips_source`, then return the strip with the same
 * index from `strips_dest`.
 */
static NlaStrip *find_active_strip_from_listbase(const NlaStrip *active_strip,
                                                 const ListBase /* NlaStrip */ *strips_source,
                                                 const ListBase /* NlaStrip */ *strips_dest)
{
  NlaStrip *strip_dest = strips_dest->first;
  LISTBASE_FOREACH (const NlaStrip *, strip_source, strips_source) {
    if (strip_dest == NULL) {
      /* The tracks are assumed to have an equal number of strips, but this is not the case when
       * dragging multiple strips. The transform system merges the selected strips into one
       * meta-strip, reducing the number of strips in `track_dest`. */
      break;
    }
    if (strip_source == active_strip) {
      return strip_dest;
    }

    const bool src_is_meta = strip_source->type == NLASTRIP_TYPE_META;
    const bool dst_is_meta = strip_dest->type == NLASTRIP_TYPE_META;
    BLI_assert_msg(src_is_meta == dst_is_meta,
                   "Expecting topology of source and destination strips to be equal");
    if (src_is_meta && dst_is_meta) {
      NlaStrip *found_in_meta = find_active_strip_from_listbase(
          active_strip, &strip_source->strips, &strip_dest->strips);
      if (found_in_meta != NULL) {
        return found_in_meta;
      }
    }

    strip_dest = strip_dest->next;
  }

  return NULL;
}

/* Set adt_dest->actstrip to the strip with the same index as adt_source->actstrip. */
static void update_active_strip(AnimData *adt_dest,
                                NlaTrack *track_dest,
                                const AnimData *adt_source,
                                const NlaTrack *track_source)
{
  BLI_assert(BLI_listbase_count(&track_source->strips) == BLI_listbase_count(&track_dest->strips));

  NlaStrip *active_strip = find_active_strip_from_listbase(
      adt_source->actstrip, &track_source->strips, &track_dest->strips);
  adt_dest->actstrip = active_strip;
}

/* Set adt_dest->act_track to the track with the same index as adt_source->act_track. */
static void update_active_track(AnimData *adt_dest, const AnimData *adt_source)
{
  BLI_assert(BLI_listbase_count(&adt_source->nla_tracks) ==
             BLI_listbase_count(&adt_dest->nla_tracks));

  NlaTrack *track_dest = adt_dest->nla_tracks.first;
  LISTBASE_FOREACH (NlaTrack *, track_source, &adt_source->nla_tracks) {
    if (track_source == adt_source->act_track) {
      adt_dest->act_track = track_dest;
    }
    update_active_strip(adt_dest, track_dest, adt_source, track_source);

    track_dest = track_dest->next;
  }

#ifndef NDEBUG
  {
    const bool source_has_actstrip = adt_source->actstrip != NULL;
    const bool dest_has_actstrip = adt_dest->actstrip != NULL;
    BLI_assert_msg(source_has_actstrip == dest_has_actstrip,
                   "Active strip did not copy correctly");
  }
#endif
}

void BKE_nla_tracks_copy_from_adt(Main *bmain,
                                  AnimData *adt_dest,
                                  const AnimData *adt_source,
                                  const int flag)
{
  adt_dest->act_track = NULL;
  adt_dest->actstrip = NULL;

  BKE_nla_tracks_copy(bmain, &adt_dest->nla_tracks, &adt_source->nla_tracks, flag);
  update_active_track(adt_dest, adt_source);
}

/* Adding ------------------------------------------- */

NlaTrack *BKE_nlatrack_add(AnimData *adt, NlaTrack *prev, const bool is_liboverride)
{
  NlaTrack *nlt;

  /* sanity checks */
  if (adt == NULL) {
    return NULL;
  }

  /* allocate new track */
  nlt = MEM_callocN(sizeof(NlaTrack), "NlaTrack");

  /* set settings requiring the track to not be part of the stack yet */
  nlt->flag = NLATRACK_SELECTED | NLATRACK_OVERRIDELIBRARY_LOCAL;
  nlt->index = BLI_listbase_count(&adt->nla_tracks);

  /* In liboverride case, we only add local tracks after all those coming from the linked data,
   * so we need to find the first local track. */
  if (is_liboverride && prev != NULL && (prev->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) == 0) {
    NlaTrack *first_local = prev->next;
    for (; first_local != NULL && (first_local->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) == 0;
         first_local = first_local->next) {
    }
    prev = first_local != NULL ? first_local->prev : NULL;
  }
  /* Add track to stack, and make it the active one. */
  if (prev != NULL) {
    BLI_insertlinkafter(&adt->nla_tracks, prev, nlt);
  }
  else {
    BLI_addtail(&adt->nla_tracks, nlt);
  }
  BKE_nlatrack_set_active(&adt->nla_tracks, nlt);

  /* must have unique name, but we need to seed this */
  strcpy(nlt->name, "NlaTrack");
  BLI_uniquename(
      &adt->nla_tracks, nlt, DATA_("NlaTrack"), '.', offsetof(NlaTrack, name), sizeof(nlt->name));

  /* return the new track */
  return nlt;
}

NlaStrip *BKE_nlastrip_new(bAction *act)
{
  NlaStrip *strip;

  /* sanity checks */
  if (act == NULL) {
    return NULL;
  }

  /* allocate new strip */
  strip = MEM_callocN(sizeof(NlaStrip), "NlaStrip");

  /* generic settings
   * - selected flag to highlight this to the user
   * - (XXX) disabled Auto-Blends, as this was often causing some unwanted effects
   */
  strip->flag = NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_SYNC_LENGTH;

  /* Disable sync for actions with a manual frame range, since it only syncs to range anyway. */
  if (act->flag & ACT_FRAME_RANGE) {
    strip->flag &= ~NLASTRIP_FLAG_SYNC_LENGTH;
  }

  /* Enable cyclic time for known cyclic actions. */
  if (BKE_action_is_cyclic(act)) {
    strip->flag |= NLASTRIP_FLAG_USR_TIME_CYCLIC;
  }

  /* assign the action reference */
  strip->act = act;
  id_us_plus(&act->id);

  /* determine initial range
   * - strip length cannot be 0... ever...
   */
  BKE_action_get_frame_range(strip->act, &strip->actstart, &strip->actend);

  strip->start = strip->actstart;
  strip->end = (IS_EQF(strip->actstart, strip->actend)) ? (strip->actstart + 1.0f) :
                                                          (strip->actend);

  /* strip should be referenced as-is */
  strip->scale = 1.0f;
  strip->repeat = 1.0f;

  /* return the new strip */
  return strip;
}

NlaStrip *BKE_nlastack_add_strip(AnimData *adt, bAction *act, const bool is_liboverride)
{
  NlaStrip *strip;
  NlaTrack *nlt;

  /* sanity checks */
  if (ELEM(NULL, adt, act)) {
    return NULL;
  }

  /* create a new NLA strip */
  strip = BKE_nlastrip_new(act);
  if (strip == NULL) {
    return NULL;
  }

  /* firstly try adding strip to last track, but if that fails, add to a new track */
  if (BKE_nlatrack_add_strip(adt->nla_tracks.last, strip, is_liboverride) == 0) {
    /* trying to add to the last track failed (no track or no space),
     * so add a new track to the stack, and add to that...
     */
    nlt = BKE_nlatrack_add(adt, NULL, is_liboverride);
    BKE_nlatrack_add_strip(nlt, strip, is_liboverride);
  }

  /* automatically name it too */
  BKE_nlastrip_validate_name(adt, strip);

  /* returns the strip added */
  return strip;
}

NlaStrip *BKE_nla_add_soundstrip(Main *bmain, Scene *scene, Speaker *speaker)
{
  NlaStrip *strip = MEM_callocN(sizeof(NlaStrip), "NlaSoundStrip");

  /* if speaker has a sound, set the strip length to the length of the sound,
   * otherwise default to length of 10 frames
   */
#ifdef WITH_AUDASPACE
  if (speaker->sound) {
    SoundInfo info;
    if (BKE_sound_info_get(bmain, speaker->sound, &info)) {
      strip->end = (float)ceil((double)info.length * FPS);
    }
  }
  else
#endif
  {
    strip->end = 10.0f;
    /* quiet compiler warnings */
    UNUSED_VARS(bmain, scene, speaker);
  }

  /* general settings */
  strip->type = NLASTRIP_TYPE_SOUND;

  strip->flag = NLASTRIP_FLAG_SELECT;
  strip->extendmode = NLASTRIP_EXTEND_NOTHING; /* nothing to extend... */

  /* strip should be referenced as-is */
  strip->scale = 1.0f;
  strip->repeat = 1.0f;

  /* return this strip */
  return strip;
}

void BKE_nla_strip_foreach_id(NlaStrip *strip, LibraryForeachIDData *data)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, strip->act, IDWALK_CB_USER);

  LISTBASE_FOREACH (FCurve *, fcu, &strip->fcurves) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_fcurve_foreach_id(fcu, data));
  }

  LISTBASE_FOREACH (NlaStrip *, substrip, &strip->strips) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_nla_strip_foreach_id(substrip, data));
  }
}

/* *************************************************** */
/* NLA Evaluation <-> Editing Stuff */

/* Strip Mapping ------------------------------------- */

/* non clipped mapping for strip-time <-> global time (for Action-Clips)
 * invert = convert action-strip time to global time
 */
static float nlastrip_get_frame_actionclip(NlaStrip *strip, float cframe, short mode)
{
  float actlength, scale;
  // float repeat; // UNUSED

  /* get number of repeats */
  if (IS_EQF(strip->repeat, 0.0f)) {
    strip->repeat = 1.0f;
  }
  // repeat = strip->repeat; /* UNUSED */

  /* scaling */
  if (IS_EQF(strip->scale, 0.0f)) {
    strip->scale = 1.0f;
  }

  /* Scale must be positive - we've got a special flag for reversing. */
  scale = fabsf(strip->scale);

  /* length of referenced action */
  actlength = strip->actend - strip->actstart;
  if (IS_EQF(actlength, 0.0f)) {
    actlength = 1.0f;
  }

  /* reversed = play strip backwards */
  if (strip->flag & NLASTRIP_FLAG_REVERSE) {
    /* FIXME: this won't work right with Graph Editor? */
    if (mode == NLATIME_CONVERT_MAP) {
      return strip->end - scale * (cframe - strip->actstart);
    }
    if (mode == NLATIME_CONVERT_UNMAP) {
      return (strip->end + (strip->actstart * scale - cframe)) / scale;
    }
    /* if (mode == NLATIME_CONVERT_EVAL) */
    if (IS_EQF((float)cframe, strip->end) && IS_EQF(strip->repeat, floorf(strip->repeat))) {
      /* This case prevents the motion snapping back to the first frame at the end of the strip
       * by catching the case where repeats is a whole number, which means that the end of the
       * strip could also be interpreted as the end of the start of a repeat. */
      return strip->actstart;
    }

    /* - the 'fmod(..., actlength * scale)' is needed to get the repeats working
     * - the '/ scale' is needed to ensure that scaling influences the timing within the repeat
     */
    return strip->actend - fmodf(cframe - strip->start, actlength * scale) / scale;
  }

  if (mode == NLATIME_CONVERT_MAP) {
    return strip->start + scale * (cframe - strip->actstart);
  }
  if (mode == NLATIME_CONVERT_UNMAP) {
    return strip->actstart + (cframe - strip->start) / scale;
  }
  /* if (mode == NLATIME_CONVERT_EVAL) */
  if (IS_EQF(cframe, strip->end) && IS_EQF(strip->repeat, floorf(strip->repeat))) {
    /* This case prevents the motion snapping back to the first frame at the end of the strip
     * by catching the case where repeats is a whole number, which means that the end of the
     * strip could also be interpreted as the end of the start of a repeat. */
    return strip->actend;
  }

  /* - the 'fmod(..., actlength * scale)' is needed to get the repeats working
   * - the '/ scale' is needed to ensure that scaling influences the timing within the repeat
   */
  return strip->actstart + fmodf(cframe - strip->start, actlength * scale) / scale;
}

/* non clipped mapping for strip-time <-> global time (for Transitions)
 * invert = convert action-strip time to global time
 */
static float nlastrip_get_frame_transition(NlaStrip *strip, float cframe, short mode)
{
  float length;

  /* length of strip */
  length = strip->end - strip->start;

  /* reversed = play strip backwards */
  if (strip->flag & NLASTRIP_FLAG_REVERSE) {
    if (mode == NLATIME_CONVERT_MAP) {
      return strip->end - (length * cframe);
    }

    return (strip->end - cframe) / length;
  }

  if (mode == NLATIME_CONVERT_MAP) {
    return (length * cframe) + strip->start;
  }

  return (cframe - strip->start) / length;
}

float nlastrip_get_frame(NlaStrip *strip, float cframe, short mode)
{
  switch (strip->type) {
    case NLASTRIP_TYPE_META:       /* Meta - for now, does the same as transition
                                    * (is really just an empty container). */
    case NLASTRIP_TYPE_TRANSITION: /* transition */
      return nlastrip_get_frame_transition(strip, cframe, mode);

    case NLASTRIP_TYPE_CLIP: /* action-clip (default) */
    default:
      return nlastrip_get_frame_actionclip(strip, cframe, mode);
  }
}

float BKE_nla_tweakedit_remap(AnimData *adt, float cframe, short mode)
{
  NlaStrip *strip;

  /* Sanity checks:
   * - Obviously we've got to have some starting data.
   * - When not in tweak-mode, the active Action does not have any scaling applied :)
   * - When in tweak-mode, if the no-mapping flag is set, do not map.
   */
  if ((adt == NULL) || (adt->flag & ADT_NLA_EDIT_ON) == 0 || (adt->flag & ADT_NLA_EDIT_NOMAP)) {
    return cframe;
  }

  /* if the active-strip info has been stored already, access this, otherwise look this up
   * and store for (very probable) future usage
   */
  if (adt->act_track == NULL) {
    if (adt->actstrip) {
      adt->act_track = BKE_nlatrack_find_tweaked(adt);
    }
    else {
      adt->act_track = BKE_nlatrack_find_active(&adt->nla_tracks);
    }
  }
  if (adt->actstrip == NULL) {
    adt->actstrip = BKE_nlastrip_find_active(adt->act_track);
  }
  strip = adt->actstrip;

  /* Sanity checks:
   * - In rare cases, we may not be able to find this strip for some reason (internal error)
   * - For now, if the user has defined a curve to control the time, this correction cannot be
   *   performed reliably.
   */
  if ((strip == NULL) || (strip->flag & NLASTRIP_FLAG_USR_TIME)) {
    return cframe;
  }

  /* perform the correction now... */
  return nlastrip_get_frame(strip, cframe, mode);
}

/* *************************************************** */
/* NLA API */

/* List of Strips ------------------------------------ */
/* (these functions are used for NLA-Tracks and also for nested/meta-strips) */

bool BKE_nlastrips_has_space(ListBase *strips, float start, float end)
{
  NlaStrip *strip;

  /* sanity checks */
  if ((strips == NULL) || IS_EQF(start, end)) {
    return false;
  }
  if (start > end) {
    puts("BKE_nlastrips_has_space() error... start and end arguments swapped");
    SWAP(float, start, end);
  }

  /* loop over NLA strips checking for any overlaps with this area... */
  for (strip = strips->first; strip; strip = strip->next) {
    /* if start frame of strip is past the target end-frame, that means that
     * we've gone past the window we need to check for, so things are fine
     */
    if (strip->start >= end) {
      return true;
    }

    /* if the end of the strip is greater than either of the boundaries, the range
     * must fall within the extents of the strip
     */
    if ((strip->end > start) || (strip->end > end)) {
      return false;
    }
  }

  /* if we are still here, we haven't encountered any overlapping strips */
  return true;
}

void BKE_nlastrips_sort_strips(ListBase *strips)
{
  ListBase tmp = {NULL, NULL};
  NlaStrip *strip, *sstrip, *stripn;

  /* sanity checks */
  if (ELEM(NULL, strips, strips->first)) {
    return;
  }

  /* we simply perform insertion sort on this list, since it is assumed that per track,
   * there are only likely to be at most 5-10 strips
   */
  for (strip = strips->first; strip; strip = stripn) {
    short not_added = 1;

    stripn = strip->next;

    /* remove this strip from the list, and add it to the new list, searching from the end of
     * the list, assuming that the lists are in order
     */
    BLI_remlink(strips, strip);

    for (sstrip = tmp.last; sstrip; sstrip = sstrip->prev) {
      /* check if add after */
      if (sstrip->end <= strip->start) {
        BLI_insertlinkafter(&tmp, sstrip, strip);
        not_added = 0;
        break;
      }
    }

    /* add before first? */
    if (not_added) {
      BLI_addhead(&tmp, strip);
    }
  }

  /* reassign the start and end points of the strips */
  strips->first = tmp.first;
  strips->last = tmp.last;
}

bool BKE_nlastrips_add_strip(ListBase *strips, NlaStrip *strip)
{
  NlaStrip *ns;
  bool not_added = true;

  /* sanity checks */
  if (ELEM(NULL, strips, strip)) {
    return false;
  }

  /* check if any space to add */
  if (BKE_nlastrips_has_space(strips, strip->start, strip->end) == 0) {
    return false;
  }

  /* find the right place to add the strip to the nominated track */
  for (ns = strips->first; ns; ns = ns->next) {
    /* if current strip occurs after the new strip, add it before */
    if (ns->start >= strip->end) {
      BLI_insertlinkbefore(strips, ns, strip);
      not_added = 0;
      break;
    }
  }
  if (not_added) {
    /* just add to the end of the list of the strips then... */
    BLI_addtail(strips, strip);
  }

  /* added... */
  return true;
}

/* Meta-Strips ------------------------------------ */

void BKE_nlastrips_make_metas(ListBase *strips, bool is_temp)
{
  NlaStrip *mstrip = NULL;
  NlaStrip *strip, *stripn;

  /* sanity checks */
  if (ELEM(NULL, strips, strips->first)) {
    return;
  }

  /* group all continuous chains of selected strips into meta-strips */
  for (strip = strips->first; strip; strip = stripn) {
    stripn = strip->next;

    if (strip->flag & NLASTRIP_FLAG_SELECT) {
      /* if there is an existing meta-strip, add this strip to it, otherwise, create a new one */
      if (mstrip == NULL) {
        /* add a new meta-strip, and add it before the current strip that it will replace... */
        mstrip = MEM_callocN(sizeof(NlaStrip), "Meta-NlaStrip");
        mstrip->type = NLASTRIP_TYPE_META;
        BLI_insertlinkbefore(strips, strip, mstrip);

        /* set flags */
        mstrip->flag = NLASTRIP_FLAG_SELECT;

        /* set temp flag if appropriate (i.e. for transform-type editing) */
        if (is_temp) {
          mstrip->flag |= NLASTRIP_FLAG_TEMP_META;
        }

        /* set default repeat/scale values to prevent warnings */
        mstrip->repeat = mstrip->scale = 1.0f;

        /* make its start frame be set to the start frame of the current strip */
        mstrip->start = strip->start;
      }

      /* remove the selected strips from the track, and add to the meta */
      BLI_remlink(strips, strip);
      BLI_addtail(&mstrip->strips, strip);

      /* expand the meta's dimensions to include the newly added strip- i.e. its last frame */
      mstrip->end = strip->end;
    }
    else {
      /* current strip wasn't selected, so the end of 'island' of selected strips has been reached,
       * so stop adding strips to the current meta
       */
      mstrip = NULL;
    }
  }
}

void BKE_nlastrips_clear_metastrip(ListBase *strips, NlaStrip *strip)
{
  NlaStrip *cs, *csn;

  /* sanity check */
  if (ELEM(NULL, strips, strip)) {
    return;
  }

  /* move each one of the meta-strip's children before the meta-strip
   * in the list of strips after unlinking them from the meta-strip
   */
  for (cs = strip->strips.first; cs; cs = csn) {
    csn = cs->next;
    BLI_remlink(&strip->strips, cs);
    BLI_insertlinkbefore(strips, strip, cs);
  }

  /* free the meta-strip now */
  BKE_nlastrip_free(strips, strip, true);
}

void BKE_nlastrips_clear_metas(ListBase *strips, bool only_sel, bool only_temp)
{
  NlaStrip *strip, *stripn;

  /* sanity checks */
  if (ELEM(NULL, strips, strips->first)) {
    return;
  }

  /* remove meta-strips fitting the criteria of the arguments */
  for (strip = strips->first; strip; strip = stripn) {
    stripn = strip->next;

    /* check if strip is a meta-strip */
    if (strip->type == NLASTRIP_TYPE_META) {
      /* if check if selection and 'temporary-only' considerations are met */
      if ((!only_sel) || (strip->flag & NLASTRIP_FLAG_SELECT)) {
        if ((!only_temp) || (strip->flag & NLASTRIP_FLAG_TEMP_META)) {
          BKE_nlastrips_clear_metastrip(strips, strip);
        }
      }
    }
  }
}

bool BKE_nlameta_add_strip(NlaStrip *mstrip, NlaStrip *strip)
{
  /* sanity checks */
  if (ELEM(NULL, mstrip, strip)) {
    return false;
  }

  /* firstly, check if the meta-strip has space for this */
  if (BKE_nlastrips_has_space(&mstrip->strips, strip->start, strip->end) == 0) {
    return false;
  }

  /* check if this would need to be added to the ends of the meta,
   * and subsequently, if the neighboring strips allow us enough room
   */
  if (strip->start < mstrip->start) {
    /* check if strip to the left (if it exists) ends before the
     * start of the strip we're trying to add
     */
    if ((mstrip->prev == NULL) || (mstrip->prev->end <= strip->start)) {
      /* add strip to start of meta's list, and expand dimensions */
      BLI_addhead(&mstrip->strips, strip);
      mstrip->start = strip->start;

      return true;
    }
    /* failed... no room before */
    return false;
  }
  if (strip->end > mstrip->end) {
    /* check if strip to the right (if it exists) starts before the
     * end of the strip we're trying to add
     */
    if ((mstrip->next == NULL) || (mstrip->next->start >= strip->end)) {
      /* add strip to end of meta's list, and expand dimensions */
      BLI_addtail(&mstrip->strips, strip);
      mstrip->end = strip->end;

      return true;
    }
    /* failed... no room after */
    return false;
  }

  /* just try to add to the meta-strip (no dimension changes needed) */
  return BKE_nlastrips_add_strip(&mstrip->strips, strip);
}

void BKE_nlameta_flush_transforms(NlaStrip *mstrip)
{
  NlaStrip *strip;
  float oStart, oEnd, offset;
  float oLen, nLen;
  short scaleChanged = 0;

  /* sanity checks
   * - strip must exist
   * - strip must be a meta-strip with some contents
   */
  if (ELEM(NULL, mstrip, mstrip->strips.first)) {
    return;
  }
  if (mstrip->type != NLASTRIP_TYPE_META) {
    return;
  }

  /* get the original start/end points, and calculate the start-frame offset
   * - these are simply the start/end frames of the child strips,
   *   since we assume they weren't transformed yet
   */
  oStart = ((NlaStrip *)mstrip->strips.first)->start;
  oEnd = ((NlaStrip *)mstrip->strips.last)->end;
  offset = mstrip->start - oStart;

  /* optimization:
   * don't flush if nothing changed yet
   * TODO: maybe we need a flag to say always flush?
   */
  if (IS_EQF(oStart, mstrip->start) && IS_EQF(oEnd, mstrip->end)) {
    return;
  }

  /* check if scale changed */
  oLen = oEnd - oStart;
  nLen = mstrip->end - mstrip->start;
  if (IS_EQF(nLen, oLen) == 0) {
    scaleChanged = 1;
  }

  /* for each child-strip, calculate new start/end points based on this new info */
  for (strip = mstrip->strips.first; strip; strip = strip->next) {
    if (scaleChanged) {
      float p1, p2;

      /* compute positions of endpoints relative to old extents of strip */
      p1 = (strip->start - oStart) / oLen;
      p2 = (strip->end - oStart) / oLen;

      /* Apply new strip endpoints using the proportions,
       * then wait for second pass to flush scale properly. */
      strip->start = (p1 * nLen) + mstrip->start;
      strip->end = (p2 * nLen) + mstrip->start;
    }
    else {
      /* just apply the changes in offset to both ends of the strip */
      strip->start += offset;
      strip->end += offset;
    }
  }

  /* apply a second pass over child strips, to finish up unfinished business */
  for (strip = mstrip->strips.first; strip; strip = strip->next) {
    /* only if scale changed, need to perform RNA updates */
    if (scaleChanged) {
      PointerRNA ptr;

      /* use RNA updates to compute scale properly */
      RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &ptr);

      RNA_float_set(&ptr, "frame_start", strip->start);
      RNA_float_set(&ptr, "frame_end", strip->end);
    }

    /* finally, make sure the strip's children (if it is a meta-itself), get updated */
    BKE_nlameta_flush_transforms(strip);
  }
}

/* NLA-Tracks ---------------------------------------- */

NlaTrack *BKE_nlatrack_find_active(ListBase *tracks)
{
  NlaTrack *nlt;

  /* sanity check */
  if (ELEM(NULL, tracks, tracks->first)) {
    return NULL;
  }

  /* try to find the first active track */
  for (nlt = tracks->first; nlt; nlt = nlt->next) {
    if (nlt->flag & NLATRACK_ACTIVE) {
      return nlt;
    }
  }

  /* none found */
  return NULL;
}

NlaTrack *BKE_nlatrack_find_tweaked(AnimData *adt)
{
  NlaTrack *nlt;

  /* sanity check */
  if (adt == NULL) {
    return NULL;
  }

  /* Since the track itself gets disabled, we want the first disabled... */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    if (nlt->flag & (NLATRACK_ACTIVE | NLATRACK_DISABLED)) {
      /* For good measure, make sure that strip actually exists there */
      if (BLI_findindex(&nlt->strips, adt->actstrip) != -1) {
        return nlt;
      }
      if (G.debug & G_DEBUG) {
        printf("%s: Active strip (%p, %s) not in NLA track found (%p, %s)\n",
               __func__,
               adt->actstrip,
               (adt->actstrip) ? adt->actstrip->name : "<None>",
               nlt,
               nlt->name);
      }
    }
  }

  /* Not found! */
  return NULL;
}

void BKE_nlatrack_solo_toggle(AnimData *adt, NlaTrack *nlt)
{
  NlaTrack *nt;

  /* sanity check */
  if (ELEM(NULL, adt, adt->nla_tracks.first)) {
    return;
  }

  /* firstly, make sure 'solo' flag for all tracks is disabled */
  for (nt = adt->nla_tracks.first; nt; nt = nt->next) {
    if (nt != nlt) {
      nt->flag &= ~NLATRACK_SOLO;
    }
  }

  /* now, enable 'solo' for the given track if appropriate */
  if (nlt) {
    /* toggle solo status */
    nlt->flag ^= NLATRACK_SOLO;

    /* set or clear solo-status on AnimData */
    if (nlt->flag & NLATRACK_SOLO) {
      adt->flag |= ADT_NLA_SOLO_TRACK;
    }
    else {
      adt->flag &= ~ADT_NLA_SOLO_TRACK;
    }
  }
  else {
    adt->flag &= ~ADT_NLA_SOLO_TRACK;
  }
}

void BKE_nlatrack_set_active(ListBase *tracks, NlaTrack *nlt_a)
{
  NlaTrack *nlt;

  /* sanity check */
  if (ELEM(NULL, tracks, tracks->first)) {
    return;
  }

  /* deactivate all the rest */
  for (nlt = tracks->first; nlt; nlt = nlt->next) {
    nlt->flag &= ~NLATRACK_ACTIVE;
  }

  /* set the given one as the active one */
  if (nlt_a) {
    nlt_a->flag |= NLATRACK_ACTIVE;
  }
}

bool BKE_nlatrack_has_space(NlaTrack *nlt, float start, float end)
{
  /* sanity checks
   * - track must exist
   * - track must be editable
   * - bounds cannot be equal (0-length is nasty)
   */
  if ((nlt == NULL) || (nlt->flag & NLATRACK_PROTECTED) || IS_EQF(start, end)) {
    return false;
  }

  if (start > end) {
    puts("BKE_nlatrack_has_space() error... start and end arguments swapped");
    SWAP(float, start, end);
  }

  /* check if there's any space left in the track for a strip of the given length */
  return BKE_nlastrips_has_space(&nlt->strips, start, end);
}

void BKE_nlatrack_sort_strips(NlaTrack *nlt)
{
  /* sanity checks */
  if (ELEM(NULL, nlt, nlt->strips.first)) {
    return;
  }

  /* sort the strips with a more generic function */
  BKE_nlastrips_sort_strips(&nlt->strips);
}

bool BKE_nlatrack_add_strip(NlaTrack *nlt, NlaStrip *strip, const bool is_liboverride)
{
  /* sanity checks */
  if (ELEM(NULL, nlt, strip)) {
    return false;
  }

  /* Do not allow adding strips if this track is locked, or not a local one in liboverride case. */
  if (nlt->flag & NLATRACK_PROTECTED ||
      (is_liboverride && (nlt->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) == 0)) {
    return false;
  }

  /* try to add the strip to the track using a more generic function */
  return BKE_nlastrips_add_strip(&nlt->strips, strip);
}

bool BKE_nlatrack_get_bounds(NlaTrack *nlt, float bounds[2])
{
  NlaStrip *strip;

  /* initialize bounds */
  if (bounds) {
    bounds[0] = bounds[1] = 0.0f;
  }
  else {
    return false;
  }

  /* sanity checks */
  if (ELEM(NULL, nlt, nlt->strips.first)) {
    return false;
  }

  /* lower bound is first strip's start frame */
  strip = nlt->strips.first;
  bounds[0] = strip->start;

  /* upper bound is last strip's end frame */
  strip = nlt->strips.last;
  bounds[1] = strip->end;

  /* done */
  return true;
}

bool BKE_nlatrack_is_nonlocal_in_liboverride(const ID *id, const NlaTrack *nlt)
{
  return (ID_IS_OVERRIDE_LIBRARY(id) &&
          (nlt == NULL || (nlt->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) == 0));
}

/* NLA Strips -------------------------------------- */

static NlaStrip *nlastrip_find_active(ListBase /* NlaStrip */ *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    if (strip->flag & NLASTRIP_FLAG_ACTIVE) {
      return strip;
    }

    if (strip->type != NLASTRIP_TYPE_META) {
      continue;
    }

    NlaStrip *inner_active = nlastrip_find_active(&strip->strips);
    if (inner_active != NULL) {
      return inner_active;
    }
  }

  return NULL;
}

NlaStrip *BKE_nlastrip_find_active(NlaTrack *nlt)
{
  if (nlt == NULL) {
    return NULL;
  }

  return nlastrip_find_active(&nlt->strips);
}

void BKE_nlastrip_set_active(AnimData *adt, NlaStrip *strip)
{
  NlaTrack *nlt;
  NlaStrip *nls;

  /* sanity checks */
  if (adt == NULL) {
    return;
  }

  /* Loop over tracks, deactivating. */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    for (nls = nlt->strips.first; nls; nls = nls->next) {
      if (nls != strip) {
        nls->flag &= ~NLASTRIP_FLAG_ACTIVE;
      }
      else {
        nls->flag |= NLASTRIP_FLAG_ACTIVE;
      }
    }
  }
}

bool BKE_nlastrip_within_bounds(NlaStrip *strip, float min, float max)
{
  const float stripLen = (strip) ? strip->end - strip->start : 0.0f;
  const float boundsLen = fabsf(max - min);

  /* sanity checks */
  if ((strip == NULL) || IS_EQF(stripLen, 0.0f) || IS_EQF(boundsLen, 0.0f)) {
    return false;
  }

  /* only ok if at least part of the strip is within the bounding window
   * - first 2 cases cover when the strip length is less than the bounding area
   * - second 2 cases cover when the strip length is greater than the bounding area
   */
  if ((stripLen < boundsLen) &&
      !(IN_RANGE(strip->start, min, max) || IN_RANGE(strip->end, min, max))) {
    return false;
  }
  if ((stripLen > boundsLen) &&
      !(IN_RANGE(min, strip->start, strip->end) || IN_RANGE(max, strip->start, strip->end))) {
    return false;
  }

  /* should be ok! */
  return true;
}

/* Ensure that strip doesn't overlap those around it after resizing
 * by offsetting those which follow. */
static void nlastrip_fix_resize_overlaps(NlaStrip *strip)
{
  /* next strips - do this first, since we're often just getting longer */
  if (strip->next) {
    NlaStrip *nls = strip->next;
    float offset = 0.0f;

    if (nls->type == NLASTRIP_TYPE_TRANSITION) {
      /* transition strips should grow/shrink to accommodate the resized strip,
       * but if the strip's bounds now exceed the transition, we're forced to
       * offset everything to maintain the balance
       */
      if (strip->end <= nls->start) {
        /* grow the transition to fill the void */
        nls->start = strip->end;
      }
      else if (strip->end < nls->end) {
        /* shrink the transition to give the strip room */
        nls->start = strip->end;
      }
      else {
        /* Shrink transition down to 1 frame long (so that it can still be found),
         * then offset everything else by the remaining deficit to give the strip room. */
        nls->start = nls->end - 1.0f;

        /* XXX: review whether preventing fractional values is good here... */
        offset = ceilf(strip->end - nls->start);

        /* apply necessary offset to ensure that the strip has enough space */
        for (; nls; nls = nls->next) {
          nls->start += offset;
          nls->end += offset;
        }
      }
    }
    else if (strip->end > nls->start) {
      /* NOTE: need to ensure we don't have a fractional frame offset, even if that leaves a gap,
       * otherwise it will be very hard to get rid of later
       */
      offset = ceilf(strip->end - nls->start);

      /* apply to times of all strips in this direction */
      for (; nls; nls = nls->next) {
        nls->start += offset;
        nls->end += offset;
      }
    }
  }

  /* previous strips - same routine as before */
  /* NOTE: when strip bounds are recalculated, this is not considered! */
  if (strip->prev) {
    NlaStrip *nls = strip->prev;
    float offset = 0.0f;

    if (nls->type == NLASTRIP_TYPE_TRANSITION) {
      /* transition strips should grow/shrink to accommodate the resized strip,
       * but if the strip's bounds now exceed the transition, we're forced to
       * offset everything to maintain the balance
       */
      if (strip->start >= nls->end) {
        /* grow the transition to fill the void */
        nls->end = strip->start;
      }
      else if (strip->start > nls->start) {
        /* shrink the transition to give the strip room */
        nls->end = strip->start;
      }
      else {
        /* Shrink transition down to 1 frame long (so that it can still be found),
         * then offset everything else by the remaining deficit to give the strip room. */
        nls->end = nls->start + 1.0f;

        /* XXX: review whether preventing fractional values is good here... */
        offset = ceilf(nls->end - strip->start);

        /* apply necessary offset to ensure that the strip has enough space */
        for (; nls; nls = nls->prev) {
          nls->start -= offset;
          nls->end -= offset;
        }
      }
    }
    else if (strip->start < nls->end) {
      /* NOTE: need to ensure we don't have a fractional frame offset, even if that leaves a gap,
       * otherwise it will be very hard to get rid of later
       */
      offset = ceilf(nls->end - strip->start);

      /* apply to times of all strips in this direction */
      for (; nls; nls = nls->prev) {
        nls->start -= offset;
        nls->end -= offset;
      }
    }
  }
}

void BKE_nlastrip_recalculate_bounds_sync_action(NlaStrip *strip)
{
  float prev_actstart;

  if (strip == NULL || strip->type != NLASTRIP_TYPE_CLIP) {
    return;
  }

  prev_actstart = strip->actstart;

  BKE_action_get_frame_range(strip->act, &strip->actstart, &strip->actend);

  /* Set start such that key's do not visually move, to preserve the overall animation result. */
  strip->start += (strip->actstart - prev_actstart) * strip->scale;

  BKE_nlastrip_recalculate_bounds(strip);
}
void BKE_nlastrip_recalculate_bounds(NlaStrip *strip)
{
  float actlen, mapping;

  /* sanity checks
   * - must have a strip
   * - can only be done for action clips
   */
  if ((strip == NULL) || (strip->type != NLASTRIP_TYPE_CLIP)) {
    return;
  }

  /* calculate new length factors */
  actlen = strip->actend - strip->actstart;
  if (IS_EQF(actlen, 0.0f)) {
    actlen = 1.0f;
  }

  mapping = strip->scale * strip->repeat;

  /* adjust endpoint of strip in response to this */
  if (IS_EQF(mapping, 0.0f) == 0) {
    strip->end = (actlen * mapping) + strip->start;
  }

  /* make sure we don't overlap our neighbors */
  nlastrip_fix_resize_overlaps(strip);
}

/* Animated Strips ------------------------------------------- */

bool BKE_nlatrack_has_animated_strips(NlaTrack *nlt)
{
  NlaStrip *strip;

  /* sanity checks */
  if (ELEM(NULL, nlt, nlt->strips.first)) {
    return false;
  }

  /* check each strip for F-Curves only (don't care about whether the flags are set) */
  for (strip = nlt->strips.first; strip; strip = strip->next) {
    if (strip->fcurves.first) {
      return true;
    }
  }

  /* none found */
  return false;
}

bool BKE_nlatracks_have_animated_strips(ListBase *tracks)
{
  NlaTrack *nlt;

  /* sanity checks */
  if (ELEM(NULL, tracks, tracks->first)) {
    return false;
  }

  /* check each track, stopping on the first hit */
  for (nlt = tracks->first; nlt; nlt = nlt->next) {
    if (BKE_nlatrack_has_animated_strips(nlt)) {
      return true;
    }
  }

  /* none found */
  return false;
}

void BKE_nlastrip_validate_fcurves(NlaStrip *strip)
{
  FCurve *fcu;

  /* sanity checks */
  if (strip == NULL) {
    return;
  }

  /* if controlling influence... */
  if (strip->flag & NLASTRIP_FLAG_USR_INFLUENCE) {
    /* try to get F-Curve */
    fcu = BKE_fcurve_find(&strip->fcurves, "influence", 0);

    /* add one if not found */
    if (fcu == NULL) {
      /* make new F-Curve */
      fcu = BKE_fcurve_create();
      BLI_addtail(&strip->fcurves, fcu);

      /* set default flags */
      fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
      fcu->auto_smoothing = U.auto_smoothing_new;

      /* store path - make copy, and store that */
      fcu->rna_path = BLI_strdupn("influence", 9);

      /* insert keyframe to ensure current value stays on first refresh */
      fcu->bezt = MEM_callocN(sizeof(BezTriple), "nlastrip influence bezt");
      fcu->totvert = 1;

      fcu->bezt->vec[1][0] = strip->start;
      fcu->bezt->vec[1][1] = strip->influence;

      /* Respect User Preferences for default interpolation and handles. */
      fcu->bezt->h1 = fcu->bezt->h2 = U.keyhandles_new;
      fcu->bezt->ipo = U.ipo_new;
    }
  }

  /* if controlling time... */
  if (strip->flag & NLASTRIP_FLAG_USR_TIME) {
    /* try to get F-Curve */
    fcu = BKE_fcurve_find(&strip->fcurves, "strip_time", 0);

    /* add one if not found */
    if (fcu == NULL) {
      /* make new F-Curve */
      fcu = BKE_fcurve_create();
      BLI_addtail(&strip->fcurves, fcu);

      /* set default flags */
      fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
      fcu->auto_smoothing = U.auto_smoothing_new;

      /* store path - make copy, and store that */
      fcu->rna_path = BLI_strdupn("strip_time", 10);

      /* TODO: insert a few keyframes to ensure default behavior? */
    }
  }
}

bool BKE_nlastrip_has_curves_for_property(const PointerRNA *ptr, const PropertyRNA *prop)
{
  /* sanity checks */
  if (ELEM(NULL, ptr, prop)) {
    return false;
  }

  /* 1) Must be NLA strip */
  if (ptr->type == &RNA_NlaStrip) {
    /* 2) Must be one of the predefined properties */
    static PropertyRNA *prop_influence = NULL;
    static PropertyRNA *prop_time = NULL;
    static bool needs_init = true;

    /* Init the properties on first use */
    if (needs_init) {
      prop_influence = RNA_struct_type_find_property(&RNA_NlaStrip, "influence");
      prop_time = RNA_struct_type_find_property(&RNA_NlaStrip, "strip_time");

      needs_init = false;
    }

    /* Check if match */
    if (ELEM(prop, prop_influence, prop_time)) {
      return true;
    }
  }

  /* No criteria met */
  return false;
}

/* Sanity Validation ------------------------------------ */

static bool nla_editbone_name_check(void *arg, const char *name)
{
  return BLI_ghash_haskey((GHash *)arg, (const void *)name);
}

void BKE_nlastrip_validate_name(AnimData *adt, NlaStrip *strip)
{
  GHash *gh;
  NlaStrip *tstrip;
  NlaTrack *nlt;

  /* sanity checks */
  if (ELEM(NULL, adt, strip)) {
    return;
  }

  /* give strip a default name if none already */
  if (strip->name[0] == 0) {
    switch (strip->type) {
      case NLASTRIP_TYPE_CLIP: /* act-clip */
        BLI_strncpy(strip->name,
                    (strip->act) ? (strip->act->id.name + 2) : ("<No Action>"),
                    sizeof(strip->name));
        break;
      case NLASTRIP_TYPE_TRANSITION: /* transition */
        BLI_strncpy(strip->name, "Transition", sizeof(strip->name));
        break;
      case NLASTRIP_TYPE_META: /* meta */
        BLI_strncpy(strip->name, "Meta", sizeof(strip->name));
        break;
      default:
        BLI_strncpy(strip->name, "NLA Strip", sizeof(strip->name));
        break;
    }
  }

  /* build a hash-table of all the strips in the tracks
   * - this is easier than iterating over all the tracks+strips hierarchy every time
   *   (and probably faster)
   */
  gh = BLI_ghash_str_new("nlastrip_validate_name gh");

  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    for (tstrip = nlt->strips.first; tstrip; tstrip = tstrip->next) {
      /* don't add the strip of interest */
      if (tstrip == strip) {
        continue;
      }

      /* Use the name of the strip as the key, and the strip as the value,
       * since we're mostly interested in the keys. */
      BLI_ghash_insert(gh, tstrip->name, tstrip);
    }
  }

  /* If the hash-table has a match for this name, try other names...
   * - In an extreme case, it might not be able to find a name,
   *   but then everything else in Blender would fail too :).
   */
  BLI_uniquename_cb(nla_editbone_name_check,
                    (void *)gh,
                    DATA_("NlaStrip"),
                    '.',
                    strip->name,
                    sizeof(strip->name));

  /* free the hash... */
  BLI_ghash_free(gh, NULL, NULL);
}

/* ---- */

/* Get strips which overlap the given one at the start/end of its range
 * - strip: strip that we're finding overlaps for
 * - track: nla-track that the overlapping strips should be found from
 * - start, end: frames for the offending endpoints
 */
static void nlastrip_get_endpoint_overlaps(NlaStrip *strip,
                                           NlaTrack *track,
                                           float **start,
                                           float **end)
{
  NlaStrip *nls;

  /* find strips that overlap over the start/end of the given strip,
   * but which don't cover the entire length
   */
  /* TODO: this scheme could get quite slow for doing this on many strips... */
  for (nls = track->strips.first; nls; nls = nls->next) {
    /* Check if strip overlaps (extends over or exactly on)
     * the entire range of the strip we're validating. */
    if ((nls->start <= strip->start) && (nls->end >= strip->end)) {
      *start = NULL;
      *end = NULL;
      return;
    }

    /* check if strip doesn't even occur anywhere near... */
    if (nls->end < strip->start) {
      continue; /* skip checking this strip... not worthy of mention */
    }
    if (nls->start > strip->end) {
      return; /* the range we're after has already passed */
    }

    /* if this strip is not part of an island of continuous strips, it can be used
     * - this check needs to be done for each end of the strip we try and use...
     */
    if ((nls->next == NULL) || IS_EQF(nls->next->start, nls->end) == 0) {
      if ((nls->end > strip->start) && (nls->end < strip->end)) {
        *start = &nls->end;
      }
    }
    if ((nls->prev == NULL) || IS_EQF(nls->prev->end, nls->start) == 0) {
      if ((nls->start < strip->end) && (nls->start > strip->start)) {
        *end = &nls->start;
      }
    }
  }
}

/* Determine auto-blending for the given strip */
static void BKE_nlastrip_validate_autoblends(NlaTrack *nlt, NlaStrip *nls)
{
  float *ps = NULL, *pe = NULL;
  float *ns = NULL, *ne = NULL;

  /* sanity checks */
  if (ELEM(NULL, nls, nlt)) {
    return;
  }
  if ((nlt->prev == NULL) && (nlt->next == NULL)) {
    return;
  }
  if ((nls->flag & NLASTRIP_FLAG_AUTO_BLENDS) == 0) {
    return;
  }

  /* get test ranges */
  if (nlt->prev) {
    nlastrip_get_endpoint_overlaps(nls, nlt->prev, &ps, &pe);
  }
  if (nlt->next) {
    nlastrip_get_endpoint_overlaps(nls, nlt->next, &ns, &ne);
  }

  /* set overlaps for this strip
   * - don't use the values obtained though if the end in question
   *   is directly followed/preceded by another strip, forming an
   *   'island' of continuous strips
   */
  if ((ps || ns) && ((nls->prev == NULL) || IS_EQF(nls->prev->end, nls->start) == 0)) {
    /* start overlaps - pick the largest overlap */
    if (((ps && ns) && (*ps > *ns)) || (ps)) {
      nls->blendin = *ps - nls->start;
    }
    else {
      nls->blendin = *ns - nls->start;
    }
  }
  else { /* no overlap allowed/needed */
    nls->blendin = 0.0f;
  }

  if ((pe || ne) && ((nls->next == NULL) || IS_EQF(nls->next->start, nls->end) == 0)) {
    /* end overlaps - pick the largest overlap */
    if (((pe && ne) && (*pe > *ne)) || (pe)) {
      nls->blendout = nls->end - *pe;
    }
    else {
      nls->blendout = nls->end - *ne;
    }
  }
  else { /* no overlap allowed/needed */
    nls->blendout = 0.0f;
  }
}

void BKE_nla_validate_state(AnimData *adt)
{
  NlaStrip *strip = NULL;
  NlaTrack *nlt;

  /* sanity checks */
  if (ELEM(NULL, adt, adt->nla_tracks.first)) {
    return;
  }

  /* Adjust blending values for auto-blending,
   * and also do an initial pass to find the earliest strip. */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* auto-blending first */
      BKE_nlastrip_validate_autoblends(nlt, strip);
    }
  }
}

/* Action Stashing -------------------------------------- */

/* name of stashed tracks - the translation stuff is included here to save extra work */
#define STASH_TRACK_NAME DATA_("[Action Stash]")

bool BKE_nla_action_is_stashed(AnimData *adt, bAction *act)
{
  NlaTrack *nlt;
  NlaStrip *strip;

  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    if (strstr(nlt->name, STASH_TRACK_NAME)) {
      for (strip = nlt->strips.first; strip; strip = strip->next) {
        if (strip->act == act) {
          return true;
        }
      }
    }
  }

  return false;
}

bool BKE_nla_action_stash(AnimData *adt, const bool is_liboverride)
{
  NlaTrack *prev_track = NULL;
  NlaTrack *nlt;
  NlaStrip *strip;

  /* sanity check */
  if (ELEM(NULL, adt, adt->action)) {
    CLOG_ERROR(&LOG, "Invalid argument - %p %p", adt, adt->action);
    return false;
  }

  /* do not add if it is already stashed */
  if (BKE_nla_action_is_stashed(adt, adt->action)) {
    return false;
  }

  /* create a new track, and add this immediately above the previous stashing track */
  for (prev_track = adt->nla_tracks.last; prev_track; prev_track = prev_track->prev) {
    if (strstr(prev_track->name, STASH_TRACK_NAME)) {
      break;
    }
  }

  nlt = BKE_nlatrack_add(adt, prev_track, is_liboverride);
  BLI_assert(nlt != NULL);

  /* We need to ensure that if there wasn't any previous instance,
   * it must go to be bottom of the stack. */
  if (prev_track == NULL) {
    BLI_remlink(&adt->nla_tracks, nlt);
    BLI_addhead(&adt->nla_tracks, nlt);
  }

  BLI_strncpy(nlt->name, STASH_TRACK_NAME, sizeof(nlt->name));
  BLI_uniquename(
      &adt->nla_tracks, nlt, STASH_TRACK_NAME, '.', offsetof(NlaTrack, name), sizeof(nlt->name));

  /* add the action as a strip in this new track
   * NOTE: a new user is created here
   */
  strip = BKE_nlastrip_new(adt->action);
  BLI_assert(strip != NULL);

  BKE_nlatrack_add_strip(nlt, strip, is_liboverride);
  BKE_nlastrip_validate_name(adt, strip);

  /* mark the stash track and strip so that they doesn't disturb the stack animation,
   * and are unlikely to draw attention to itself (or be accidentally bumped around)
   *
   * NOTE: this must be done *after* adding the strip to the track, or else
   *       the strip locking will prevent the strip from getting added
   */
  nlt->flag = (NLATRACK_MUTED | NLATRACK_PROTECTED);
  strip->flag &= ~(NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_ACTIVE);

  /* also mark the strip for auto syncing the length, so that the strips accurately
   * reflect the length of the action
   * XXX: we could do with some extra flags here to prevent repeats/scaling options from working!
   */
  strip->flag |= NLASTRIP_FLAG_SYNC_LENGTH;

  /* succeeded */
  return true;
}

/* Core Tools ------------------------------------------- */

void BKE_nla_action_pushdown(AnimData *adt, const bool is_liboverride)
{
  NlaStrip *strip;

  /* sanity checks */
  /* TODO: need to report the error for this */
  if (ELEM(NULL, adt, adt->action)) {
    return;
  }

  /* if the action is empty, we also shouldn't try to add to stack,
   * as that will cause us grief down the track
   */
  /* TODO: what about modifiers? */
  if (action_has_motion(adt->action) == 0) {
    CLOG_ERROR(&LOG, "action has no data");
    return;
  }

  /* add a new NLA strip to the track, which references the active action */
  strip = BKE_nlastack_add_strip(adt, adt->action, is_liboverride);
  if (strip == NULL) {
    return;
  }

  /* clear reference to action now that we've pushed it onto the stack */
  id_us_min(&adt->action->id);
  adt->action = NULL;

  /* copy current "action blending" settings from adt to the strip,
   * as it was keyframed with these settings, so omitting them will
   * change the effect  [T54233]
   */
  strip->blendmode = adt->act_blendmode;
  strip->influence = adt->act_influence;
  strip->extendmode = adt->act_extendmode;

  if (adt->act_influence < 1.0f) {
    /* enable "user-controlled" influence (which will insert a default keyframe)
     * so that the influence doesn't get lost on the new update
     *
     * NOTE: An alternative way would have been to instead hack the influence
     * to not get always get reset to full strength if NLASTRIP_FLAG_USR_INFLUENCE
     * is disabled but auto-blending isn't being used. However, that approach
     * is a bit hacky/hard to discover, and may cause backwards compatibility issues,
     * so it's better to just do it this way.
     */
    strip->flag |= NLASTRIP_FLAG_USR_INFLUENCE;
    BKE_nlastrip_validate_fcurves(strip);
  }

  /* make strip the active one... */
  BKE_nlastrip_set_active(adt, strip);
}

static void nla_tweakmode_find_active(const ListBase /* NlaTrack */ *nla_tracks,
                                      NlaTrack **r_track_of_active_strip,
                                      NlaStrip **r_active_strip)
{
  NlaTrack *nlt, *activeTrack = NULL;
  NlaStrip *strip, *activeStrip = NULL;

  /* go over the tracks, finding the active one, and its active strip
   * - if we cannot find both, then there's nothing to do
   */
  for (nlt = nla_tracks->first; nlt; nlt = nlt->next) {
    /* check if active */
    if (nlt->flag & NLATRACK_ACTIVE) {
      /* store reference to this active track */
      activeTrack = nlt;

      /* now try to find active strip */
      activeStrip = BKE_nlastrip_find_active(nlt);
      break;
    }
  }

  /* There are situations where we may have multiple strips selected and we want to enter
   * tweak-mode on all of those at once. Usually in those cases,
   * it will usually just be a single strip per AnimData.
   * In such cases, compromise and take the last selected track and/or last selected strip, T28468.
   */
  if (activeTrack == NULL) {
    /* try last selected track for active strip */
    for (nlt = nla_tracks->last; nlt; nlt = nlt->prev) {
      if (nlt->flag & NLATRACK_SELECTED) {
        /* assume this is the active track */
        activeTrack = nlt;

        /* try to find active strip */
        activeStrip = BKE_nlastrip_find_active(nlt);
        break;
      }
    }
  }
  if ((activeTrack) && (activeStrip == NULL)) {
    /* No active strip in active or last selected track;
     * compromise for first selected (assuming only single). */
    for (strip = activeTrack->strips.first; strip; strip = strip->next) {
      if (strip->flag & (NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_ACTIVE)) {
        activeStrip = strip;
        break;
      }
    }
  }

  *r_track_of_active_strip = activeTrack;
  *r_active_strip = activeStrip;
}

bool BKE_nla_tweakmode_enter(AnimData *adt)
{
  NlaTrack *nlt, *activeTrack = NULL;
  NlaStrip *strip, *activeStrip = NULL;

  /* verify that data is valid */
  if (ELEM(NULL, adt, adt->nla_tracks.first)) {
    return false;
  }

  /* If block is already in tweak-mode, just leave, but we should report
   * that this block is in tweak-mode (as our returncode). */
  if (adt->flag & ADT_NLA_EDIT_ON) {
    return true;
  }

  nla_tweakmode_find_active(&adt->nla_tracks, &activeTrack, &activeStrip);

  if (ELEM(NULL, activeTrack, activeStrip, activeStrip->act)) {
    if (G.debug & G_DEBUG) {
      printf("NLA tweak-mode enter - neither active requirement found\n");
      printf("\tactiveTrack = %p, activeStrip = %p\n", (void *)activeTrack, (void *)activeStrip);
    }
    return false;
  }

  /* Go over all the tracks, tagging each strip that uses the same
   * action as the active strip, but leaving everything else alone.
   */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      if (strip->act == activeStrip->act) {
        strip->flag |= NLASTRIP_FLAG_TWEAKUSER;
      }
      else {
        strip->flag &= ~NLASTRIP_FLAG_TWEAKUSER;
      }
    }
  }

  /* Untag tweaked track. This leads to non tweaked actions being drawn differently than the
   * tweaked action. */
  activeStrip->flag &= ~NLASTRIP_FLAG_TWEAKUSER;

  /* go over all the tracks after AND INCLUDING the active one, tagging them as being disabled
   * - the active track needs to also be tagged, otherwise, it'll overlap with the tweaks going on
   */
  activeTrack->flag |= NLATRACK_DISABLED;
  if ((adt->flag & ADT_NLA_EVAL_UPPER_TRACKS) == 0) {
    for (nlt = activeTrack->next; nlt; nlt = nlt->next) {
      nlt->flag |= NLATRACK_DISABLED;
    }
  }

  /* handle AnimData level changes:
   * - 'real' active action to temp storage (no need to change user-counts).
   * - Action of active strip set to be the 'active action', and have its usercount incremented.
   * - Editing-flag for this AnimData block should also get turned on
   *   (for more efficient restoring).
   * - Take note of the active strip for mapping-correction of keyframes
   *   in the action being edited.
   */
  adt->tmpact = adt->action;
  adt->action = activeStrip->act;
  adt->act_track = activeTrack;
  adt->actstrip = activeStrip;
  id_us_plus(&activeStrip->act->id);
  adt->flag |= ADT_NLA_EDIT_ON;

  /* done! */
  return true;
}

void BKE_nla_tweakmode_exit(AnimData *adt)
{
  NlaStrip *strip;
  NlaTrack *nlt;

  /* verify that data is valid */
  if (ELEM(NULL, adt, adt->nla_tracks.first)) {
    return;
  }

  /* hopefully the flag is correct - skip if not on */
  if ((adt->flag & ADT_NLA_EDIT_ON) == 0) {
    return;
  }

  /* sync the length of the user-strip with the new state of the action
   * but only if the user has explicitly asked for this to happen
   * (see T34645 for things to be careful about)
   */
  if ((adt->actstrip) && (adt->actstrip->flag & NLASTRIP_FLAG_SYNC_LENGTH)) {
    strip = adt->actstrip;

    /* must be action-clip only (transitions don't have scale) */
    if ((strip->type == NLASTRIP_TYPE_CLIP) && (strip->act)) {
      BKE_nlastrip_recalculate_bounds_sync_action(strip);
    }
  }

  /* for all Tracks, clear the 'disabled' flag
   * for all Strips, clear the 'tweak-user' flag
   */
  for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
    nlt->flag &= ~NLATRACK_DISABLED;

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* sync strip extents if this strip uses the same action */
      if ((adt->actstrip) && (adt->actstrip->act == strip->act) &&
          (strip->flag & NLASTRIP_FLAG_SYNC_LENGTH)) {
        BKE_nlastrip_recalculate_bounds_sync_action(strip);
      }

      /* clear tweakuser flag */
      strip->flag &= ~NLASTRIP_FLAG_TWEAKUSER;
    }
  }

  /* handle AnimData level changes:
   * - 'temporary' active action needs its usercount decreased, since we're removing this reference
   * - 'real' active action is restored from storage
   * - storage pointer gets cleared (to avoid having bad notes hanging around)
   * - editing-flag for this AnimData block should also get turned off
   * - clear pointer to active strip
   */
  if (adt->action) {
    id_us_min(&adt->action->id);
  }
  adt->action = adt->tmpact;
  adt->tmpact = NULL;
  adt->act_track = NULL;
  adt->actstrip = NULL;
  adt->flag &= ~ADT_NLA_EDIT_ON;
}

static void blend_write_nla_strips(BlendWriter *writer, ListBase *strips)
{
  BLO_write_struct_list(writer, NlaStrip, strips);
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* write the strip's F-Curves and modifiers */
    BKE_fcurve_blend_write(writer, &strip->fcurves);
    BKE_fmodifiers_blend_write(writer, &strip->modifiers);

    /* write the strip's children */
    blend_write_nla_strips(writer, &strip->strips);
  }
}

static void blend_data_read_nla_strips(BlendDataReader *reader, ListBase *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* strip's child strips */
    BLO_read_list(reader, &strip->strips);
    blend_data_read_nla_strips(reader, &strip->strips);

    /* strip's F-Curves */
    BLO_read_list(reader, &strip->fcurves);
    BKE_fcurve_blend_read_data(reader, &strip->fcurves);

    /* strip's F-Modifiers */
    BLO_read_list(reader, &strip->modifiers);
    BKE_fmodifiers_blend_read_data(reader, &strip->modifiers, NULL);
  }
}

static void blend_lib_read_nla_strips(BlendLibReader *reader, ID *id, ListBase *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* check strip's children */
    blend_lib_read_nla_strips(reader, id, &strip->strips);

    /* check strip's F-Curves */
    BKE_fcurve_blend_read_lib(reader, id, &strip->fcurves);

    /* reassign the counted-reference to action */
    BLO_read_id_address(reader, id->lib, &strip->act);
  }
}

static void blend_read_expand_nla_strips(BlendExpander *expander, ListBase *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* check child strips */
    blend_read_expand_nla_strips(expander, &strip->strips);

    /* check F-Curves */
    BKE_fcurve_blend_read_expand(expander, &strip->fcurves);

    /* check F-Modifiers */
    BKE_fmodifiers_blend_read_expand(expander, &strip->modifiers);

    /* relink referenced action */
    BLO_expand(expander, strip->act);
  }
}

void BKE_nla_blend_write(BlendWriter *writer, ListBase *tracks)
{
  /* write all the tracks */
  LISTBASE_FOREACH (NlaTrack *, nlt, tracks) {
    /* write the track first */
    BLO_write_struct(writer, NlaTrack, nlt);

    /* write the track's strips */
    blend_write_nla_strips(writer, &nlt->strips);
  }
}

void BKE_nla_blend_read_data(BlendDataReader *reader, ListBase *tracks)
{
  LISTBASE_FOREACH (NlaTrack *, nlt, tracks) {
    /* relink list of strips */
    BLO_read_list(reader, &nlt->strips);

    /* relink strip data */
    blend_data_read_nla_strips(reader, &nlt->strips);
  }
}

void BKE_nla_blend_read_lib(BlendLibReader *reader, ID *id, ListBase *tracks)
{
  /* we only care about the NLA strips inside the tracks */
  LISTBASE_FOREACH (NlaTrack *, nlt, tracks) {
    /* If linking from a library, clear 'local' library override flag. */
    if (ID_IS_LINKED(id)) {
      nlt->flag &= ~NLATRACK_OVERRIDELIBRARY_LOCAL;
    }

    blend_lib_read_nla_strips(reader, id, &nlt->strips);
  }
}

void BKE_nla_blend_read_expand(struct BlendExpander *expander, struct ListBase *tracks)
{
  /* nla-data - referenced actions */
  LISTBASE_FOREACH (NlaTrack *, nlt, tracks) {
    blend_read_expand_nla_strips(expander, &nlt->strips);
  }
}
