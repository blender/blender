/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BKE_scene.h"

#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_time.h"
#include "render.h"

/* -------------------------------------------------------------------- */
/** \name Iterator API
 * \{ */

bool SEQ_iterator_ensure(SeqCollection *collection, SeqIterator *iterator, Sequence **r_seq)
{
  if (iterator->iterator_initialized) {
    return true;
  }

  if (BLI_gset_len(collection->set) == 0) {
    return false;
  }

  iterator->collection = collection;
  BLI_gsetIterator_init(&iterator->gsi, iterator->collection->set);
  iterator->iterator_initialized = true;

  *r_seq = static_cast<Sequence *>(BLI_gsetIterator_getKey(&iterator->gsi));
  BLI_gsetIterator_step(&iterator->gsi);

  return true;
}

Sequence *SEQ_iterator_yield(SeqIterator *iterator)
{
  Sequence *seq = static_cast<Sequence *>(
      BLI_gsetIterator_done(&iterator->gsi) ? nullptr : BLI_gsetIterator_getKey(&iterator->gsi));
  BLI_gsetIterator_step(&iterator->gsi);
  return seq;
}

static bool seq_for_each_recursive(ListBase *seqbase, SeqForEachFunc callback, void *user_data)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (!callback(seq, user_data)) {
      /* Callback signaled stop, return. */
      return false;
    }
    if (seq->type == SEQ_TYPE_META) {
      if (!seq_for_each_recursive(&seq->seqbase, callback, user_data)) {
        return false;
      }
    }
  }
  return true;
}

void SEQ_for_each_callback(ListBase *seqbase, SeqForEachFunc callback, void *user_data)
{
  seq_for_each_recursive(seqbase, callback, user_data);
}

void SEQ_collection_free(SeqCollection *collection)
{
  BLI_gset_free(collection->set, nullptr);
  MEM_freeN(collection);
}

SeqCollection *SEQ_collection_create(const char *name)
{
  SeqCollection *collection = static_cast<SeqCollection *>(
      MEM_callocN(sizeof(SeqCollection), name));
  collection->set = BLI_gset_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "SeqCollection GSet");
  return collection;
}

uint SEQ_collection_len(const SeqCollection *collection)
{
  return BLI_gset_len(collection->set);
}

bool SEQ_collection_has_strip(const Sequence *seq, const SeqCollection *collection)
{
  return BLI_gset_haskey(collection->set, seq);
}

SeqCollection *SEQ_query_by_reference(Sequence *seq_reference,
                                      const Scene *scene,
                                      ListBase *seqbase,
                                      void seq_query_func(const Scene *scene,
                                                          Sequence *seq_reference,
                                                          ListBase *seqbase,
                                                          SeqCollection *collection))
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  seq_query_func(scene, seq_reference, seqbase, collection);
  return collection;
}
bool SEQ_collection_append_strip(Sequence *seq, SeqCollection *collection)
{
  void **key;
  if (BLI_gset_ensure_p_ex(collection->set, seq, &key)) {
    return false;
  }

  *key = (void *)seq;
  return true;
}

bool SEQ_collection_remove_strip(Sequence *seq, SeqCollection *collection)
{
  return BLI_gset_remove(collection->set, seq, nullptr);
}

void SEQ_collection_merge(SeqCollection *collection_dst, SeqCollection *collection_src)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection_src) {
    SEQ_collection_append_strip(seq, collection_dst);
  }
  SEQ_collection_free(collection_src);
}

void SEQ_collection_exclude(SeqCollection *collection, SeqCollection *exclude_elements)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, exclude_elements) {
    SEQ_collection_remove_strip(seq, collection);
  }
  SEQ_collection_free(exclude_elements);
}

void SEQ_collection_expand(const Scene *scene,
                           ListBase *seqbase,
                           SeqCollection *collection,
                           void seq_query_func(const Scene *scene,
                                               Sequence *seq_reference,
                                               ListBase *seqbase,
                                               SeqCollection *collection))
{
  /* Collect expanded results for each sequence in provided SeqIteratorCollection. */
  SeqCollection *query_matches = SEQ_collection_create(__func__);

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    SEQ_collection_merge(query_matches,
                         SEQ_query_by_reference(seq, scene, seqbase, seq_query_func));
  }

  /* Merge all expanded results in provided SeqIteratorCollection. */
  SEQ_collection_merge(collection, query_matches);
}

SeqCollection *SEQ_collection_duplicate(SeqCollection *collection)
{
  SeqCollection *duplicate = SEQ_collection_create(__func__);
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    SEQ_collection_append_strip(seq, duplicate);
  }
  return duplicate;
}

/** \} */

static void query_all_strips_recursive(ListBase *seqbase, SeqCollection *collection)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_META) {
      query_all_strips_recursive(&seq->seqbase, collection);
    }
    SEQ_collection_append_strip(seq, collection);
  }
}

SeqCollection *SEQ_query_all_strips_recursive(ListBase *seqbase)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_META) {
      query_all_strips_recursive(&seq->seqbase, collection);
    }
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}

SeqCollection *SEQ_query_all_strips(ListBase *seqbase)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}

SeqCollection *SEQ_query_selected_strips(ListBase *seqbase)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if ((seq->flag & SELECT) == 0) {
      continue;
    }
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}

static SeqCollection *query_strips_at_frame(const Scene *scene,
                                            ListBase *seqbase,
                                            const int timeline_frame)
{
  SeqCollection *collection = SEQ_collection_create(__func__);

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (SEQ_time_strip_intersects_frame(scene, seq, timeline_frame)) {
      SEQ_collection_append_strip(seq, collection);
    }
  }
  return collection;
}

static void collection_filter_channel_up_to_incl(SeqCollection *collection, const int channel)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    if (seq->machine <= channel) {
      continue;
    }
    SEQ_collection_remove_strip(seq, collection);
  }
}

/* Check if seq must be rendered. This depends on whole stack in some cases, not only seq itself.
 * Order of applying these conditions is important. */
static bool must_render_strip(const Sequence *seq, SeqCollection *strips_at_timeline_frame)
{
  bool seq_have_effect_in_stack = false;
  Sequence *seq_iter;
  SEQ_ITERATOR_FOREACH (seq_iter, strips_at_timeline_frame) {
    /* Strips is below another strip with replace blending are not rendered. */
    if (seq_iter->blend_mode == SEQ_BLEND_REPLACE && seq->machine < seq_iter->machine) {
      return false;
    }

    if ((seq_iter->type & SEQ_TYPE_EFFECT) != 0 && SEQ_relation_is_effect_of_strip(seq_iter, seq))
    {
      /* Strips in same channel or higher than its effect are rendered. */
      if (seq->machine >= seq_iter->machine) {
        return true;
      }
      /* Mark that this strip has effect in stack, that is above the strip. */
      seq_have_effect_in_stack = true;
    }
  }

  /* All effects are rendered (with respect to conditions above). */
  if ((seq->type & SEQ_TYPE_EFFECT) != 0) {
    return true;
  }

  /* If strip has effects in stack, and all effects are above this strip, it is not rendered. */
  if (seq_have_effect_in_stack) {
    return false;
  }

  return true;
}

/* Remove strips we don't want to render from collection. */
static void collection_filter_rendered_strips(ListBase *channels, SeqCollection *collection)
{
  Sequence *seq;

  /* Remove sound strips and muted strips from collection, because these are not rendered.
   * Function #must_render_strip() don't have to check for these strips anymore. */
  SEQ_ITERATOR_FOREACH (seq, collection) {
    if (seq->type == SEQ_TYPE_SOUND_RAM || SEQ_render_is_muted(channels, seq)) {
      SEQ_collection_remove_strip(seq, collection);
    }
  }

  SEQ_ITERATOR_FOREACH (seq, collection) {
    if (must_render_strip(seq, collection)) {
      continue;
    }
    SEQ_collection_remove_strip(seq, collection);
  }
}

SeqCollection *SEQ_query_rendered_strips(const Scene *scene,
                                         ListBase *channels,
                                         ListBase *seqbase,
                                         const int timeline_frame,
                                         const int displayed_channel)
{
  SeqCollection *collection = query_strips_at_frame(scene, seqbase, timeline_frame);
  if (displayed_channel != 0) {
    collection_filter_channel_up_to_incl(collection, displayed_channel);
  }
  collection_filter_rendered_strips(channels, collection);
  return collection;
}

SeqCollection *SEQ_query_unselected_strips(ListBase *seqbase)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if ((seq->flag & SELECT) != 0) {
      continue;
    }
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}

void SEQ_query_strip_effect_chain(const Scene *scene,
                                  Sequence *seq_reference,
                                  ListBase *seqbase,
                                  SeqCollection *collection)
{
  if (!SEQ_collection_append_strip(seq_reference, collection)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }

  /* Find all strips that seq_reference is connected to. */
  if (seq_reference->type & SEQ_TYPE_EFFECT) {
    if (seq_reference->seq1) {
      SEQ_query_strip_effect_chain(scene, seq_reference->seq1, seqbase, collection);
    }
    if (seq_reference->seq2) {
      SEQ_query_strip_effect_chain(scene, seq_reference->seq2, seqbase, collection);
    }
    if (seq_reference->seq3) {
      SEQ_query_strip_effect_chain(scene, seq_reference->seq3, seqbase, collection);
    }
  }

  /* Find all strips connected to seq_reference. */
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->seq1 == seq_reference || seq_test->seq2 == seq_reference ||
        seq_test->seq3 == seq_reference)
    {
      SEQ_query_strip_effect_chain(scene, seq_test, seqbase, collection);
    }
  }
}

void SEQ_filter_selected_strips(SeqCollection *collection)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    if ((seq->flag & SELECT) == 0) {
      SEQ_collection_remove_strip(seq, collection);
    }
  }
}
