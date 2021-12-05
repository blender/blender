/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BKE_scene.h"

#include "SEQ_iterator.h"
#include "SEQ_time.h"
#include "render.h"

/* -------------------------------------------------------------------- */
/** \Iterator API
 * \{ */

/**
 * Utility function for SEQ_ITERATOR_FOREACH macro.
 * Ensure, that iterator is initialized. During initialization return pointer to collection element
 * and step gset iterator. When this function is called after iterator has been initialized, it
 * will do nothing and return true.
 *
 * \param collection: collection to iterate
 * \param iterator: iterator to be initialized
 * \param r_seq: pointer to Sequence pointer
 *
 * \return false when iterator can not be initialized, true otherwise
 */
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

  *r_seq = BLI_gsetIterator_getKey(&iterator->gsi);
  BLI_gsetIterator_step(&iterator->gsi);

  return true;
}

/**
 * Utility function for SEQ_ITERATOR_FOREACH macro.
 * Yield collection element
 *
 * \param iterator: iterator to be initialized
 *
 * \return collection element or NULL when iteration has ended
 */
Sequence *SEQ_iterator_yield(SeqIterator *iterator)
{
  Sequence *seq = BLI_gsetIterator_done(&iterator->gsi) ? NULL :
                                                          BLI_gsetIterator_getKey(&iterator->gsi);
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

/**
 * Utility function to recursively iterate through all sequence strips in a `seqbase` list.
 * Uses callback to do operations on each sequence element.
 * The callback can stop the iteration if needed.
 *
 * \param seqbase: #ListBase of sequences to be iterated over.
 * \param callback: query function callback, returns false if iteration should stop.
 * \param user_data: pointer to user data that can be used in the callback function.
 */
void SEQ_for_each_callback(ListBase *seqbase, SeqForEachFunc callback, void *user_data)
{
  seq_for_each_recursive(seqbase, callback, user_data);
}

/**
 * Free strip collection.
 *
 * \param collection: collection to be freed
 */
void SEQ_collection_free(SeqCollection *collection)
{
  BLI_gset_free(collection->set, NULL);
  MEM_freeN(collection);
}

/**
 * Create new empty strip collection.
 *
 * \return empty strip collection.
 */
SeqCollection *SEQ_collection_create(const char *name)
{
  SeqCollection *collection = MEM_callocN(sizeof(SeqCollection), name);
  collection->set = BLI_gset_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "SeqCollection GSet");
  return collection;
}

/**
 * Return number of items in collection.
 */
uint SEQ_collection_len(const SeqCollection *collection)
{
  return BLI_gset_len(collection->set);
}

/**
 * Check if seq is in collection.
 */
bool SEQ_collection_has_strip(const Sequence *seq, const SeqCollection *collection)
{
  return BLI_gset_haskey(collection->set, seq);
}

/**
 * Query strips from seqbase. seq_reference is used by query function as filter condition.
 *
 * \param seq_reference: reference strip for query function
 * \param seqbase: ListBase in which strips are queried
 * \param seq_query_func: query function callback
 * \return strip collection
 */
SeqCollection *SEQ_query_by_reference(Sequence *seq_reference,
                                      ListBase *seqbase,
                                      void seq_query_func(Sequence *seq_reference,
                                                          ListBase *seqbase,
                                                          SeqCollection *collection))
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  seq_query_func(seq_reference, seqbase, collection);
  return collection;
}
/**
 * Add strip to collection.
 *
 * \param seq: strip to be added
 * \param collection: collection to which strip will be added
 * \return false if strip is already in set, otherwise true
 */
bool SEQ_collection_append_strip(Sequence *seq, SeqCollection *collection)
{
  void **key;
  if (BLI_gset_ensure_p_ex(collection->set, seq, &key)) {
    return false;
  }

  *key = (void *)seq;
  return true;
}

/**
 * Remove strip from collection.
 *
 * \param seq: strip to be removed
 * \param collection: collection from which strip will be removed
 * \return true if strip exists in set and it was removed from set, otherwise false
 */
bool SEQ_collection_remove_strip(Sequence *seq, SeqCollection *collection)
{
  return BLI_gset_remove(collection->set, seq, NULL);
}

/**
 * Move strips from collection_src to collection_dst. Source collection will be freed.
 *
 * \param collection_dst: destination collection
 * \param collection_src: source collection
 */
void SEQ_collection_merge(SeqCollection *collection_dst, SeqCollection *collection_src)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection_src) {
    SEQ_collection_append_strip(seq, collection_dst);
  }
  SEQ_collection_free(collection_src);
}

/**
 * Remove strips from collection that are also in `exclude_elements`. Source collection will be
 * freed.
 *
 * \param collection: collection from which strips are removed
 * \param exclude_elements: collection of strips to be removed
 */
void SEQ_collection_exclude(SeqCollection *collection, SeqCollection *exclude_elements)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, exclude_elements) {
    SEQ_collection_remove_strip(seq, collection);
  }
  SEQ_collection_free(exclude_elements);
}

/**
 * Expand collection by running SEQ_query() for each strip, which will be used as reference.
 * Results of these queries will be merged into provided collection.
 *
 * \param seqbase: ListBase in which strips are queried
 * \param collection: SeqCollection to be expanded
 * \param seq_query_func: query function callback
 */
void SEQ_collection_expand(ListBase *seqbase,
                           SeqCollection *collection,
                           void seq_query_func(Sequence *seq_reference,
                                               ListBase *seqbase,
                                               SeqCollection *collection))
{
  /* Collect expanded results for each sequence in provided SeqIteratorCollection. */
  SeqCollection *query_matches = SEQ_collection_create(__func__);

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    SEQ_collection_merge(query_matches, SEQ_query_by_reference(seq, seqbase, seq_query_func));
  }

  /* Merge all expanded results in provided SeqIteratorCollection. */
  SEQ_collection_merge(collection, query_matches);
}

/**
 * Duplicate collection
 *
 * \param collection: collection to be duplicated
 * \return duplicate of collection
 */
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

/**
 * Query all strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
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

/**
 * Query all strips in seqbase. This does not include strips nested in meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
SeqCollection *SEQ_query_all_strips(ListBase *seqbase)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}

/**
 * Query all selected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
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

static SeqCollection *query_strips_at_frame(ListBase *seqbase, const int timeline_frame)
{
  SeqCollection *collection = SEQ_collection_create(__func__);

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (SEQ_time_strip_intersects_frame(seq, timeline_frame)) {
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

static bool seq_is_effect_of(const Sequence *seq_effect, const Sequence *possibly_input)
{
  if (seq_effect->seq1 == possibly_input || seq_effect->seq2 == possibly_input ||
      seq_effect->seq3 == possibly_input) {
    return true;
  }
  return false;
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

    if ((seq_iter->type & SEQ_TYPE_EFFECT) != 0 && seq_is_effect_of(seq_iter, seq)) {
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
static void collection_filter_rendered_strips(SeqCollection *collection)
{
  Sequence *seq;

  /* Remove sound strips and muted strips from collection, because these are not rendered.
   * Function #must_render_strip() don't have to check for these strips anymore. */
  SEQ_ITERATOR_FOREACH (seq, collection) {
    if (seq->type == SEQ_TYPE_SOUND_RAM || (seq->flag & SEQ_MUTE) != 0) {
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

/**
 * Query strips that are rendered at \a timeline_frame when \a displayed channel is viewed
 *
 * \param seqbase: ListBase in which strips are queried
 * \param timeline_frame: viewed frame
 * \param displayed_channel: viewed channel. when set to 0, no channel filter is applied
 * \return strip collection
 */
SeqCollection *SEQ_query_rendered_strips(ListBase *seqbase,
                                         const int timeline_frame,
                                         const int displayed_channel)
{
  SeqCollection *collection = query_strips_at_frame(seqbase, timeline_frame);
  if (displayed_channel != 0) {
    collection_filter_channel_up_to_incl(collection, displayed_channel);
  }
  collection_filter_rendered_strips(collection);
  return collection;
}

/**
 * Query all unselected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
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

/**
 * Query all effect strips that are directly or indirectly connected to seq_reference.
 * This includes all effects of seq_reference, strips used by another inputs and their effects, so
 * that whole chain is fully independent of other strips.
 *
 * \param seq_reference: reference strip
 * \param seqbase: ListBase in which strips are queried
 * \param collection: collection to be filled
 */
void SEQ_query_strip_effect_chain(Sequence *seq_reference,
                                  ListBase *seqbase,
                                  SeqCollection *collection)
{
  if (!SEQ_collection_append_strip(seq_reference, collection)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }

  /* Find all strips that seq_reference is connected to. */
  if (seq_reference->type & SEQ_TYPE_EFFECT) {
    if (seq_reference->seq1) {
      SEQ_query_strip_effect_chain(seq_reference->seq1, seqbase, collection);
    }
    if (seq_reference->seq2) {
      SEQ_query_strip_effect_chain(seq_reference->seq2, seqbase, collection);
    }
    if (seq_reference->seq3) {
      SEQ_query_strip_effect_chain(seq_reference->seq3, seqbase, collection);
    }
  }

  /* Find all strips connected to seq_reference. */
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->seq1 == seq_reference || seq_test->seq2 == seq_reference ||
        seq_test->seq3 == seq_reference) {
      SEQ_query_strip_effect_chain(seq_test, seqbase, collection);
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
