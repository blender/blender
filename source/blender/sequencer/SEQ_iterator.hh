/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_ghash.h"
#include "BLI_vector_set.hh"

struct GSet;
struct GSetIterator;
struct Sequence;

/* Utility macro to construct an unique (within a file) variable name for iterator macro.
 * Use indirect macro evaluation to ensure the `__LINE__` is expanded (rather than being
 * treated as a name token),
 *
 * The `__LINE__` is defined at the invocation of the `SEQ_ITERATOR_FOREACH` and is not changed
 * afterwards. This makes it safe to expand it several times in the `SEQ_ITERATOR_FOREACH`.
 *
 * This allows to have nested `foreach` loops.
 *
 * NOTE: Putting nested loop to a wrapper macro is not supported. */
#define _SEQ_ITERATOR_NAME_JOIN(x, y) x##_##y
#define _SEQ_ITERATOR_NAME_EVALUATE(x, y) _SEQ_ITERATOR_NAME_JOIN(x, y)
#define _SEQ_ITERATOR_NAME(prefix) _SEQ_ITERATOR_NAME_EVALUATE(prefix, __LINE__)

#define SEQ_ITERATOR_FOREACH(var, collection) \
  for (SeqIterator _SEQ_ITERATOR_NAME(iter) = {{{NULL}}}; \
       SEQ_iterator_ensure(collection, &_SEQ_ITERATOR_NAME(iter), &var) && var != NULL; \
       var = SEQ_iterator_yield(&_SEQ_ITERATOR_NAME(iter)))

typedef struct SeqCollection {
  GSet *set;
} SeqCollection;

struct SeqIterator {
  GSetIterator gsi;
  SeqCollection *collection;
  bool iterator_initialized;
};

/**
 * Utility function for SEQ_ITERATOR_FOREACH macro.
 * Ensure, that iterator is initialized. During initialization return pointer to collection element
 * and step #GSet iterator. When this function is called after iterator has been initialized, it
 * will do nothing and return true.
 *
 * \param collection: collection to iterate
 * \param iterator: iterator to be initialized
 * \param r_seq: pointer to Sequence pointer
 *
 * \return false when iterator can not be initialized, true otherwise
 */
bool SEQ_iterator_ensure(SeqCollection *collection, SeqIterator *iterator, Sequence **r_seq);
/**
 * Utility function for SEQ_ITERATOR_FOREACH macro.
 * Yield collection element
 *
 * \param iterator: iterator to be initialized
 *
 * \return collection element or NULL when iteration has ended
 */
Sequence *SEQ_iterator_yield(SeqIterator *iterator);

/**
 * Callback format for the for_each function below.
 */
typedef bool (*SeqForEachFunc)(Sequence *seq, void *user_data);

/**
 * Utility function to recursively iterate through all sequence strips in a `seqbase` list.
 * Uses callback to do operations on each sequence element.
 * The callback can stop the iteration if needed.
 *
 * \param seqbase: #ListBase of sequences to be iterated over.
 * \param callback: query function callback, returns false if iteration should stop.
 * \param user_data: pointer to user data that can be used in the callback function.
 */
void SEQ_for_each_callback(ListBase *seqbase, SeqForEachFunc callback, void *user_data);

/**
 * Expand set by running `seq_query_func()` for each strip, which will be used as reference.
 * Results of these queries will be merged into provided collection.
 *
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be expanded
 * \param seq_query_func: query function callback
 */
void SEQ_iterator_set_expand(const Scene *scene,
                             ListBase *seqbase,
                             blender::VectorSet<Sequence *> &strips,
                             void seq_query_func(const Scene *scene,
                                                 Sequence *seq_reference,
                                                 ListBase *seqbase,
                                                 blender::VectorSet<Sequence *> &strips));
/**
 * Query strips from seqbase. seq_reference is used by query function as filter condition.
 *
 * \param seq_reference: reference strip for query function
 * \param seqbase: ListBase in which strips are queried
 * \param seq_query_func: query function callback
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_by_reference(
    Sequence *seq_reference,
    const Scene *scene,
    ListBase *seqbase,
    void seq_query_func(const Scene *scene,
                        Sequence *seq_reference,
                        ListBase *seqbase,
                        blender::VectorSet<Sequence *> &strips));
/**
 * Query all selected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_selected_strips(ListBase *seqbase);
/**
 * Query all unselected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_unselected_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase. This does not include strips nested in meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_all_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_all_strips_recursive(ListBase *seqbase);

/**
 * Query all effect strips that are directly or indirectly connected to seq_reference.
 * This includes all effects of seq_reference, strips used by another inputs and their effects, so
 * that whole chain is fully independent of other strips.
 *
 * \param seq_reference: reference strip
 * \param seqbase: ListBase in which strips are queried
 * \param strips: set of strips to be filled
 */
void SEQ_query_strip_effect_chain(const Scene *scene,
                                  Sequence *seq_reference,
                                  ListBase *seqbase,
                                  blender::VectorSet<Sequence *> &strips);

/**
 * Query strips that are rendered at \a timeline_frame when \a displayed channel is viewed
 *
 * \param seqbase: ListBase in which strips are queried
 * \param timeline_frame: viewed frame
 * \param displayed_channel: viewed channel. when set to 0, no channel filter is applied
 * \return set of strips
 */
blender::VectorSet<Sequence *> SEQ_query_rendered_strips(const Scene *scene,
                                                         ListBase *channels,
                                                         ListBase *seqbase,
                                                         int timeline_frame,
                                                         int displayed_channel);
