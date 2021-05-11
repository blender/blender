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
SeqCollection *SEQ_collection_create(void)
{
  SeqCollection *collection = MEM_callocN(sizeof(SeqCollection), "SeqCollection");
  collection->set = BLI_gset_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "SeqCollection GSet");
  return collection;
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
  SeqCollection *collection = SEQ_collection_create();
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
  if (BLI_gset_lookup(collection->set, seq) != NULL) {
    return false;
  }
  BLI_gset_insert(collection->set, seq);
  return true;
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
  ListBase expand_collections = {0};

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    SeqCollection *expand_collection = SEQ_query_by_reference(seq, seqbase, seq_query_func);
    BLI_addtail(&expand_collections, expand_collection);
  }

  /* Merge all expanded results in provided SeqIteratorCollection. */
  LISTBASE_FOREACH_MUTABLE (SeqCollection *, expand_collection, &expand_collections) {
    BLI_remlink(&expand_collections, expand_collection);
    SEQ_collection_merge(collection, expand_collection);
  }
}

/** \} */

/**
 * Query all strips in seqbase and nested meta strips.
 *
 * \param seqbase: ListBase in which strips are queried
 * \return strip collection
 */
SeqCollection *SEQ_query_all_strips_recursive(ListBase *seqbase)
{
  SeqCollection *collection = SEQ_collection_create();
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_META) {
      SEQ_collection_merge(collection, SEQ_query_all_strips_recursive(&seq->seqbase));
    }
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
  SeqCollection *collection = SEQ_collection_create();
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if ((seq->flag & SELECT) == 0) {
      continue;
    }
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}
