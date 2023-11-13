/* SPDX-FileCopyrightText: 2021 Blender Authors
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

#include "SEQ_iterator.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_sys_types.h"
#include "BLI_threads.h"
#include "BLI_vector_set.hh"

#include <cstring>

#include "MEM_guardedalloc.h"

static ThreadMutex lookup_lock = BLI_MUTEX_INITIALIZER;

struct SequenceLookup {
  GHash *seq_by_name;
  GHash *meta_by_seq;
  blender::Map<const Sequence *, blender::VectorSet<Sequence *>> effects_by_seq;
  eSequenceLookupTag tag;
};

static void seq_sequence_lookup_init(SequenceLookup *lookup)
{
  lookup->seq_by_name = BLI_ghash_str_new(__func__);
  lookup->meta_by_seq = BLI_ghash_ptr_new(__func__);
  lookup->tag |= SEQ_LOOKUP_TAG_INVALID;
}

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
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    BLI_ghash_insert(lookup->seq_by_name, seq->name + 2, seq);
    BLI_ghash_insert(lookup->meta_by_seq, seq, parent_meta);
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
  lookup->tag &= ~SEQ_LOOKUP_TAG_INVALID;
}

static SequenceLookup *seq_sequence_lookup_new()
{
  SequenceLookup *lookup = MEM_new<SequenceLookup>(__func__);
  seq_sequence_lookup_init(lookup);
  return lookup;
}

static void seq_sequence_lookup_free(SequenceLookup **lookup)
{
  if (*lookup == nullptr) {
    return;
  }

  BLI_ghash_free((*lookup)->seq_by_name, nullptr, nullptr);
  BLI_ghash_free((*lookup)->meta_by_seq, nullptr, nullptr);
  (*lookup)->seq_by_name = nullptr;
  (*lookup)->meta_by_seq = nullptr;
  MEM_delete(*lookup);
  *lookup = nullptr;
}

static void seq_sequence_lookup_rebuild(const Scene *scene, SequenceLookup **lookup)
{
  seq_sequence_lookup_free(lookup);
  *lookup = seq_sequence_lookup_new();
  seq_sequence_lookup_build(scene, *lookup);
}

static bool seq_sequence_lookup_is_valid(const SequenceLookup *lookup)
{
  return (lookup->tag & SEQ_LOOKUP_TAG_INVALID) == 0;
}

static void seq_sequence_lookup_update_if_needed(const Scene *scene, SequenceLookup **lookup)
{
  if (!scene->ed) {
    return;
  }
  if (*lookup && seq_sequence_lookup_is_valid(*lookup)) {
    return;
  }

  seq_sequence_lookup_rebuild(scene, lookup);
}

void SEQ_sequence_lookup_free(const Scene *scene)
{
  BLI_assert(scene->ed);
  BLI_mutex_lock(&lookup_lock);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  seq_sequence_lookup_free(&lookup);
  BLI_mutex_unlock(&lookup_lock);
}

Sequence *SEQ_sequence_lookup_seq_by_name(const Scene *scene, const char *key)
{
  BLI_assert(scene->ed);
  BLI_mutex_lock(&lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  Sequence *seq = static_cast<Sequence *>(BLI_ghash_lookup(lookup->seq_by_name, key));
  BLI_mutex_unlock(&lookup_lock);
  return seq;
}

Sequence *seq_sequence_lookup_meta_by_seq(const Scene *scene, const Sequence *key)
{
  BLI_assert(scene->ed);
  BLI_mutex_lock(&lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  Sequence *seq = static_cast<Sequence *>(BLI_ghash_lookup(lookup->meta_by_seq, key));
  BLI_mutex_unlock(&lookup_lock);
  return seq;
}

blender::Span<Sequence *> seq_sequence_lookup_effects_by_seq(const Scene *scene,
                                                             const Sequence *key)
{
  BLI_assert(scene->ed);
  BLI_mutex_lock(&lookup_lock);
  seq_sequence_lookup_update_if_needed(scene, &scene->ed->runtime.sequence_lookup);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  blender::VectorSet<Sequence *> &effects = lookup->effects_by_seq.lookup_or_add_default(key);
  BLI_mutex_unlock(&lookup_lock);
  return effects.as_span();
}

void SEQ_sequence_lookup_tag(const Scene *scene, eSequenceLookupTag tag)
{
  if (!scene->ed) {
    return;
  }

  BLI_mutex_lock(&lookup_lock);
  SequenceLookup *lookup = scene->ed->runtime.sequence_lookup;
  if (lookup != nullptr) {
    lookup->tag |= tag;
  }
  BLI_mutex_unlock(&lookup_lock);
}
