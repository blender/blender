/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_ghash.h"

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
  struct GSet *set;
} SeqCollection;

typedef struct SeqIterator {
  GSetIterator gsi;
  SeqCollection *collection;
  bool iterator_initialized;
} SeqIterator;

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
bool SEQ_iterator_ensure(SeqCollection *collection,
                         SeqIterator *iterator,
                         struct Sequence **r_seq);
/**
 * Utility function for SEQ_ITERATOR_FOREACH macro.
 * Yield collection element
 *
 * \param iterator: iterator to be initialized
 *
 * \return collection element or NULL when iteration has ended
 */
struct Sequence *SEQ_iterator_yield(SeqIterator *iterator);

/**
 * Callback format for the for_each function below.
 */
typedef bool (*SeqForEachFunc)(struct Sequence *seq, void *user_data);

/**
 * Utility function to recursively iterate through all sequence strips in a `seqbase` list.
 * Uses callback to do operations on each sequence element.
 * The callback can stop the iteration if needed.
 *
 * \param seqbase: #ListBase of sequences to be iterated over.
 * \param callback: query function callback, returns false if iteration should stop.
 * \param user_data: pointer to user data that can be used in the callback function.
 */
void SEQ_for_each_callback(struct ListBase *seqbase, SeqForEachFunc callback, void *user_data);

/**
 * Create new empty strip collection.
 *
 * \return empty strip collection.
 */
SeqCollection *SEQ_collection_create(const char *name);
/**
 * Duplicate collection
 *
 * \param collection: collection to be duplicated
 * \return duplicate of collection
 */
SeqCollection *SEQ_collection_duplicate(SeqCollection *collection);
/**
 * Return number of items in collection.
 */
uint SEQ_collection_len(const SeqCollection *collection);
/**
 * Check if seq is in collection.
 */
bool SEQ_collection_has_strip(const struct Sequence *seq, const SeqCollection *collection);
/**
 * Add strip to collection.
 *
 * \param seq: strip to be added
 * \param collection: collection to which strip will be added
 * \return false if strip is already in set, otherwise true
 */
bool SEQ_collection_append_strip(struct Sequence *seq, SeqCollection *collection);
/**
 * Remove strip from collection.
 *
 * \param seq: strip to be removed
 * \param collection: collection from which strip will be removed
 * \return true if strip exists in set and it was removed from set, otherwise false
 */
bool SEQ_collection_remove_strip(struct Sequence *seq, SeqCollection *collection);
/**
 * Free strip collection.
 *
 * \param collection: collection to be freed
 */
void SEQ_collection_free(SeqCollection *collection);
/** Quiet compiler warning for free function. */
#define SEQ_collection_free_void_p ((GHashValFreeFP)SEQ_collection_free)

/**
 * Move strips from collection_src to collection_dst. Source collection will be freed.
 *
 * \param collection_dst: destination collection
 * \param collection_src: source collection
 */
void SEQ_collection_merge(SeqCollection *collection_dst, SeqCollection *collection_src);
/**
 * Remove strips from collection that are also in `exclude_elements`. Source collection will be
 * freed.
 *
 * \param collection: collection from which strips are removed
 * \param exclude_elements: collection of strips to be removed
 */
void SEQ_collection_exclude(SeqCollection *collection, SeqCollection *exclude_elements);
/**
 * Expand collection by running SEQ_query() for each strip, which will be used as reference.
 * Results of these queries will be merged into provided collection.
 *
 * \param seqbase: ListBase in which strips are queried
 * \param collection: SeqCollection to be expanded
 * \param seq_query_func: query function callback
 */
void SEQ_collection_expand(const struct Scene *scene,
                           struct ListBase *seqbase,
                           SeqCollection *collection,
                           void seq_query_func(const struct Scene *scene,
                                               struct Sequence *seq_reference,
                                               struct ListBase *seqbase,
                                               SeqCollection *collection));
/**
 * Query strips from seqbase. seq_reference is used by query function as filter condition.
 *
 * \param seq_reference: reference strip for query function
 * \param seqbase: ListBase in which strips are queried
 * \param seq_query_func: query function callback
 * \return strip collection
 */
SeqCollection *SEQ_query_by_reference(struct Sequence *seq_reference,
                                      const struct Scene *scene,
                                      struct ListBase *seqbase,
                                      void seq_query_func(const struct Scene *scene,
                                                          struct Sequence *seq_reference,
                                                          struct ListBase *seqbase,
                                                          SeqCollection *collection));
/**
 * Query all selected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
SeqCollection *SEQ_query_selected_strips(struct ListBase *seqbase);
/**
 * Query all unselected strips in seqbase.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
SeqCollection *SEQ_query_unselected_strips(struct ListBase *seqbase);
/**
 * Query all strips in seqbase. This does not include strips nested in meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
SeqCollection *SEQ_query_all_strips(ListBase *seqbase);
/**
 * Query all strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
SeqCollection *SEQ_query_all_strips_recursive(ListBase *seqbase);
/**
 * Query strips that are rendered at \a timeline_frame when \a displayed channel is viewed
 *
 * \param seqbase: ListBase in which strips are queried
 * \param timeline_frame: viewed frame
 * \param displayed_channel: viewed channel. when set to 0, no channel filter is applied
 * \return strip collection
 */
SeqCollection *SEQ_query_rendered_strips(const struct Scene *scene,
                                         ListBase *channels,
                                         ListBase *seqbase,
                                         int timeline_frame,
                                         int displayed_channel);
/**
 * Query all effect strips that are directly or indirectly connected to seq_reference.
 * This includes all effects of seq_reference, strips used by another inputs and their effects, so
 * that whole chain is fully independent of other strips.
 *
 * \param seq_reference: reference strip
 * \param seqbase: ListBase in which strips are queried
 * \param collection: collection to be filled
 */
void SEQ_query_strip_effect_chain(const struct Scene *scene,
                                  struct Sequence *seq_reference,
                                  struct ListBase *seqbase,
                                  SeqCollection *collection);
void SEQ_filter_selected_strips(SeqCollection *collection);

#ifdef __cplusplus
}
#endif
