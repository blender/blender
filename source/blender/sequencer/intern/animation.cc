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

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

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

bool SEQ_fcurve_matches(const Sequence &seq, const FCurve &fcurve)
{
  return animrig::fcurve_matches_collection_path(
      fcurve, "sequence_editor.sequences_all[", seq.name + 2);
}

void SEQ_offset_animdata(Scene *scene, Sequence *seq, int ofs)
{
  if (!SEQ_animation_keyframes_exist(scene) || ofs == 0) {
    return;
  }

  Vector<FCurve *> fcurves = animrig::fcurves_in_listbase_filtered(
      scene->adt->action->curves,
      [&](const FCurve &fcurve) { return SEQ_fcurve_matches(*seq, fcurve); });

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

void SEQ_free_animdata(Scene *scene, Sequence *seq)
{
  if (!SEQ_animation_keyframes_exist(scene)) {
    return;
  }

  Vector<FCurve *> fcurves = animrig::fcurves_in_listbase_filtered(
      scene->adt->action->curves,
      [&](const FCurve &fcurve) { return SEQ_fcurve_matches(*seq, fcurve); });

  for (FCurve *fcu : fcurves) {
    BLI_remlink(&scene->adt->action->curves, fcu);
    BKE_fcurve_free(fcu);
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
    else if (animrig::ChannelBag *channel_bag = animrig::channelbag_for_action_slot(
                 action, scene->adt->slot_handle))
    {
      animrig::channelbag_fcurves_move(backup->channel_bag, *channel_bag);
    }
  }

  if (SEQ_animation_drivers_exist(scene)) {
    BLI_movelisttolist(&backup->drivers, &scene->adt->drivers);
  }
}

void SEQ_animation_restore_original(Scene *scene, SeqAnimationBackup *backup)
{
  if (!BLI_listbase_is_empty(&backup->curves) || !backup->channel_bag.fcurves().is_empty()) {
    BLI_assert(scene->adt != nullptr && scene->adt->action != nullptr);

    animrig::Action &action = scene->adt->action->wrap();

    assert_baklava_phase_1_invariants(action);

    if (action.is_action_legacy()) {
      BLI_movelisttolist(&scene->adt->action->curves, &backup->curves);
    }
    else {
      animrig::ChannelBag *channel_bag = animrig::channelbag_for_action_slot(
          action, scene->adt->slot_handle);
      /* The channel bag should exist if we got here, because otherwise the
       * backup channel bag would have been empty. */
      BLI_assert(channel_bag != nullptr);

      animrig::channelbag_fcurves_move(*channel_bag, backup->channel_bag);
    }
  }

  if (!BLI_listbase_is_empty(&backup->drivers)) {
    BLI_assert(scene->adt != nullptr);
    BLI_movelisttolist(&scene->adt->drivers, &backup->drivers);
  }
}

static void seq_animation_duplicate(Scene *scene, Sequence *seq, ListBase *dst, ListBase *src)
{
  if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, meta_child, &seq->seqbase) {
      seq_animation_duplicate(scene, meta_child, dst, src);
    }
  }

  Vector<FCurve *> fcurves = animrig::fcurves_in_listbase_filtered(
      *src, [&](const FCurve &fcurve) { return SEQ_fcurve_matches(*seq, fcurve); });

  for (const FCurve *fcu : fcurves) {
    FCurve *fcu_cpy = BKE_fcurve_copy(fcu);
    BLI_addtail(dst, fcu_cpy);
  }
}

void SEQ_animation_duplicate_backup_to_scene(Scene *scene,
                                             Sequence *seq,
                                             SeqAnimationBackup *backup)
{
  if (!BLI_listbase_is_empty(&backup->curves)) {
    seq_animation_duplicate(scene, seq, &scene->adt->action->curves, &backup->curves);
  }
  if (!BLI_listbase_is_empty(&backup->drivers)) {
    seq_animation_duplicate(scene, seq, &scene->adt->drivers, &backup->drivers);
  }
}
