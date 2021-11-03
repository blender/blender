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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_ghash.h"

struct Editing;
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

bool SEQ_iterator_ensure(SeqCollection *collection,
                         SeqIterator *iterator,
                         struct Sequence **r_seq);
struct Sequence *SEQ_iterator_yield(SeqIterator *iterator);

/* Callback format for the for_each function below. */
typedef bool (*SeqForEachFunc)(struct Sequence *seq, void *user_data);

void SEQ_for_each_callback(struct ListBase *seqbase, SeqForEachFunc callback, void *user_data);

SeqCollection *SEQ_collection_create(const char *name);
SeqCollection *SEQ_collection_duplicate(SeqCollection *collection);
uint SEQ_collection_len(const SeqCollection *collection);
bool SEQ_collection_has_strip(const struct Sequence *seq, const SeqCollection *collection);
bool SEQ_collection_append_strip(struct Sequence *seq, SeqCollection *data);
bool SEQ_collection_remove_strip(struct Sequence *seq, SeqCollection *data);
void SEQ_collection_free(SeqCollection *collection);
void SEQ_collection_merge(SeqCollection *collection_dst, SeqCollection *collection_src);
void SEQ_collection_exclude(SeqCollection *collection, SeqCollection *exclude_elements);
void SEQ_collection_expand(struct ListBase *seqbase,
                           SeqCollection *collection,
                           void query_func(struct Sequence *seq_reference,
                                           struct ListBase *seqbase,
                                           SeqCollection *collection));
SeqCollection *SEQ_query_by_reference(struct Sequence *seq_reference,
                                      struct ListBase *seqbase,
                                      void seq_query_func(struct Sequence *seq_reference,
                                                          struct ListBase *seqbase,
                                                          SeqCollection *collection));
SeqCollection *SEQ_query_selected_strips(struct ListBase *seqbase);
SeqCollection *SEQ_query_unselected_strips(struct ListBase *seqbase);
SeqCollection *SEQ_query_all_strips(ListBase *seqbase);
SeqCollection *SEQ_query_all_strips_recursive(ListBase *seqbase);
SeqCollection *SEQ_query_rendered_strips(ListBase *seqbase,
                                         const int timeline_frame,
                                         const int displayed_channel);
void SEQ_query_strip_effect_chain(struct Sequence *seq_reference,
                                  struct ListBase *seqbase,
                                  SeqCollection *collection);
void SEQ_filter_selected_strips(SeqCollection *collection);

/* Utilities to access these as tags. */
int SEQ_query_rendered_strips_to_tag(ListBase *seqbase,
                                     const int timeline_frame,
                                     const int displayed_channel);

#ifdef __cplusplus
}
#endif
