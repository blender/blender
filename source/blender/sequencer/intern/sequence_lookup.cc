/* SPDX-FileCopyrightText: 2021-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "SEQ_sequencer.hh"
#include "sequencer.hh"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_sys_types.h"
#include "BLI_vector_set.hh"

#include <cstring>
#include <mutex>

#include "MEM_guardedalloc.h"

static std::mutex lookup_lock;

struct SequenceLookup {
  blender::Map<std::string, Sequence *> seq_by_name;
  blender::Map<const Sequence *, Sequence *> meta_by_seq;
  blender::Map<const Sequence *, blender::VectorSet<Sequence *>> effects_by_seq;
  blender::Map<const SeqTimelineChannel *, Sequence *> owner_by_channel;
  bool is_valid = false;
};

static void seq_sequence_lookup_append_effect(const Sequence *input,
                                              Sequence *effect,
                                              SequenceLookup *lookup)
{
  if (input == nullptr) {
    return;
  }

  blender::VectorSet<Sequence *> &effects = lookup->effects_by_seq.lookup_or_add_default(input);

  effects.add(effect);
}

static void seq_sequence_lookup_build_effect(Sequence *seq, SequenceLookup *lookup)
{
  if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
    return;
  }

  seq_sequence_lookup_append_effect(seq->seq1, seq, lookup);
  seq_sequence_lookup_append_effect(seq->seq2, seq, lookup);
}

static void seq_sequence_lookup_build_from_seqbase(Sequence *parent_meta,
                                                   const ListBase *seqbase,
                                                   SequenceLookup *lookup)
{
  if (parent_meta != nullptr) {
    LISTBASE_FOREACH (SeqTimelineChannel *, channel, &parent_meta->channels) {
      lookup->owner_by_channel.add(channel, parent_meta);
    }
  }

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    lookup->seq_by_name.add(seq->name + 2, seq);
    lookup->meta_by_seq.add(seq, parent_meta);
    seq_sequence_lookup_build_effect(seq, lookup);

    if (seq->type == SEQ_TYPE_META) {
      seq_sequence_lookup_build_from_seqbase(seq, &seq->seqbase, lookup);
    }
  }
}

static void seq_sequence_lookup_build(const Scene *scene, SequenceLookup *lookup)
{
  Editing *ed = SEQ_editing_get(scene);
  seq_sequence_lookup_build_from_seqbase(nullptr, &ed->seqbase, lookup);
  lookup->is_valid = true;
}

static SequenceLookup *seq_sequence_lookup_new()
{
  SequenceLookup *lookup = MEM_new<SequenceLookup>(__func__);
  return lookup;
}

static void seq_sequence_lookup_free(SequenceLookup **lookup)
{
  MEM_delete(*lookup);
  *lookup = nullptr;
}

static void seq_sequence_lookup_rebuild(const Scene *scene, SequenceLookup **lookup)
{
  seq_sequence_lookup_free(lookup);
  *lookup = seq_sequence_lookup_new();
  seq_sequence_lookup_build(scene, *lookup);
}

static void seq_sequence_lookup_update_if_needed(const Scene *scene, SequenceLookup **lookup)
{
  if (!scene->ed) {
    return;
  }
  if (*lookup && (*lookup)->is_valid) {
    return;
  }

  seq_sequence_lookup_rebuild(scene, lookup);
}

void SEQ_sequence_lookup_free(const Scene *scene)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  seq_sequence_lookup_free(&lookup);
}

Sequence *SEQ_sequence_lookup_seq_by_name(const Scene *scene, const char *key)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  return lookup->seq_by_name.lookup_default(key, nullptr);
}

Sequence *seq_sequence_lookup_meta_by_seq(const Scene *scene, const Sequence *key)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  return lookup->meta_by_seq.lookup_default(key, nullptr);
}

blender::Span<Sequence *> seq_sequence_lookup_effects_by_seq(const Scene *scene,
                                                             const Sequence *key)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  blender::VectorSet<Sequence *> &effects = lookup->effects_by_seq.lookup_or_add_default(key);
  return effects.as_span();
}

Sequence *SEQ_sequence_lookup_owner_by_channel(const Scene *scene,
                                               const SeqTimelineChannel *channel)
{
  BLI_assert(scene->ed);
  std::lock_guard lock(lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  return lookup->owner_by_channel.lookup_default(channel, nullptr);
}

void SEQ_sequence_lookup_invalidate(const Scene *scene)
{
  if (scene == nullptr || scene->ed == nullptr) {
    return;
  }

  std::lock_guard lock(lookup_lock);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  if (lookup != nullptr) {
    lookup->is_valid = false;
  }
}
