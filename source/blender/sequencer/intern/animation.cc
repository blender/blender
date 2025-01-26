/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_fcurve.hh"

#include "BLI_listbase.h"

#include "DEG_depsgraph.hh"

#include "SEQ_animation.hh"

using namespace blender;

bool SEQ_animation_keyframes_exist(Scene *scene)
{
  return scene->adt != nullptr && scene->adt->action != nullptr &&
         scene->adt->action->wrap().has_keyframes(scene->adt->slot_handle);
}

bool SEQ_animation_drivers_exist(Scene *scene)
{
  return scene->adt != nullptr && !BLI_listbase_is_empty(&scene->adt->drivers);
}

bool SEQ_fcurve_matches(const Strip &strip, const FCurve &fcurve)
{
  return animrig::fcurve_matches_collection_path(
      fcurve, "sequence_editor.strips_all[", strip.name + 2);
}

void SEQ_offset_animdata(Scene *scene, Strip *strip, int ofs)
{
  if (!SEQ_animation_keyframes_exist(scene) || ofs == 0) {
    return;
  }

  Vector<FCurve *> fcurves = animrig::fcurves_in_action_slot_filtered(
      scene->adt->action, scene->adt->slot_handle, [&](const FCurve &fcurve) {
        return SEQ_fcurve_matches(*strip, fcurve);
      });

  for (FCurve *fcu : fcurves) {
    uint i;
    if (fcu->bezt) {
      for (i = 0; i < fcu->totvert; i++) {
        BezTriple *bezt = &fcu->bezt[i];
        bezt->vec[0][0] += ofs;
        bezt->vec[1][0] += ofs;
        bezt->vec[2][0] += ofs;
      }
    }
    if (fcu->fpt) {
      for (i = 0; i < fcu->totvert; i++) {
        FPoint *fpt = &fcu->fpt[i];
        fpt->vec[0] += ofs;
      }
    }
  }

  DEG_id_tag_update(&scene->adt->action->id, ID_RECALC_ANIMATION);
}

void SEQ_free_animdata(Scene *scene, Strip *strip)
{
  if (!SEQ_animation_keyframes_exist(scene)) {
    return;
  }

  Vector<FCurve *> fcurves = animrig::fcurves_in_action_slot_filtered(
      scene->adt->action, scene->adt->slot_handle, [&](const FCurve &fcurve) {
        return SEQ_fcurve_matches(*strip, fcurve);
      });

  animrig::Action &action = scene->adt->action->wrap();
  for (FCurve *fcu : fcurves) {
    action_fcurve_remove(action, *fcu);
  }
}

void SEQ_animation_backup_original(Scene *scene, SeqAnimationBackup *backup)
{
  if (SEQ_animation_keyframes_exist(scene)) {
    animrig::Action &action = scene->adt->action->wrap();

    assert_baklava_phase_1_invariants(action);

    if (action.is_action_legacy()) {
      BLI_movelisttolist(&backup->curves, &scene->adt->action->curves);
    }
    else if (animrig::Channelbag *channelbag = animrig::channelbag_for_action_slot(
                 action, scene->adt->slot_handle))
    {
      animrig::channelbag_fcurves_move(backup->channelbag, *channelbag);
    }
  }

  if (SEQ_animation_drivers_exist(scene)) {
    BLI_movelisttolist(&backup->drivers, &scene->adt->drivers);
  }
}

void SEQ_animation_restore_original(Scene *scene, SeqAnimationBackup *backup)
{
  if (!BLI_listbase_is_empty(&backup->curves) || !backup->channelbag.fcurves().is_empty()) {
    BLI_assert(scene->adt != nullptr && scene->adt->action != nullptr);

    animrig::Action &action = scene->adt->action->wrap();

    assert_baklava_phase_1_invariants(action);

    if (action.is_action_legacy()) {
      BLI_movelisttolist(&scene->adt->action->curves, &backup->curves);
    }
    else {
      animrig::Channelbag *channelbag = animrig::channelbag_for_action_slot(
          action, scene->adt->slot_handle);
      /* The channel bag should exist if we got here, because otherwise the
       * backup channel bag would have been empty. */
      BLI_assert(channelbag != nullptr);

      animrig::channelbag_fcurves_move(*channelbag, backup->channelbag);
    }
  }

  if (!BLI_listbase_is_empty(&backup->drivers)) {
    BLI_assert(scene->adt != nullptr);
    BLI_movelisttolist(&scene->adt->drivers, &backup->drivers);
  }
}

/**
 * Duplicate the animation in `src` that matches items in `strip` into `dst`.
 */
static void strip_animation_duplicate(Strip *strip,
                                      animrig::Action &dst,
                                      const animrig::slot_handle_t dst_slot_handle,
                                      SeqAnimationBackup *src)
{
  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, meta_child, &strip->seqbase) {
      strip_animation_duplicate(meta_child, dst, dst_slot_handle, src);
    }
  }

  Vector<FCurve *> fcurves = {};
  BLI_assert_msg(BLI_listbase_is_empty(&src->curves) || src->channelbag.fcurves().is_empty(),
                 "SeqAnimationBackup has fcurves for both legacy and layered actions, which "
                 "should never happen.");
  if (BLI_listbase_is_empty(&src->curves)) {
    fcurves = animrig::fcurves_in_span_filtered(
        src->channelbag.fcurves(),
        [&](const FCurve &fcurve) { return SEQ_fcurve_matches(*strip, fcurve); });
  }
  else {
    fcurves = animrig::fcurves_in_listbase_filtered(
        src->curves, [&](const FCurve &fcurve) { return SEQ_fcurve_matches(*strip, fcurve); });
  }

  for (const FCurve *fcu : fcurves) {
    FCurve *fcu_copy = BKE_fcurve_copy(fcu);

    /* Handling groups properly requires more work, so we ignore them for now.
     *
     * Note that when legacy actions are deprecated, then we can handle channel
     * groups way more easily because we know they're stored in the
     * already-duplicated channelbag in `src`, and we therefore don't have to
     * worry that they might have already been freed. */
    fcu_copy->grp = nullptr;

    animrig::action_fcurve_attach(dst, dst_slot_handle, *fcu_copy, std::nullopt);
  }
}

/**
 * Duplicate the drivers in `src` that matches items in `strip` into `dst`.
 */
static void strip_drivers_duplicate(Strip *strip, AnimData *dst, SeqAnimationBackup *src)
{
  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, meta_child, &strip->seqbase) {
      strip_drivers_duplicate(meta_child, dst, src);
    }
  }

  Vector<FCurve *> fcurves = animrig::fcurves_in_listbase_filtered(
      src->drivers, [&](const FCurve &fcurve) { return SEQ_fcurve_matches(*strip, fcurve); });

  for (const FCurve *fcu : fcurves) {
    FCurve *fcu_cpy = BKE_fcurve_copy(fcu);
    BLI_addtail(&dst->drivers, fcu_cpy);
  }
}

void SEQ_animation_duplicate_backup_to_scene(Scene *scene,
                                             Strip *strip,
                                             SeqAnimationBackup *backup)
{
  BLI_assert(scene != nullptr);

  if (!BLI_listbase_is_empty(&backup->curves) || !backup->channelbag.fcurves().is_empty()) {
    BLI_assert(scene->adt != nullptr);
    BLI_assert(scene->adt->action != nullptr);
    strip_animation_duplicate(strip, scene->adt->action->wrap(), scene->adt->slot_handle, backup);
  }

  if (!BLI_listbase_is_empty(&backup->drivers)) {
    BLI_assert(scene->adt != nullptr);
    strip_drivers_duplicate(strip, scene->adt, backup);
  }
}
